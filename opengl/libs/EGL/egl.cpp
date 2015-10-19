/*
 ** Copyright 2007, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/gralloc.h>
#include <system/window.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <utils/CallStack.h>
#include <utils/String8.h>

#include "../egl_impl.h"

#include "egl_tls.h"
#include "egldefs.h"
#include "Loader.h"

#include "egl_display.h"
#include "egl_object.h"

typedef __eglMustCastToProperFunctionPointerType EGLFuncPointer;

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

egl_connection_t gEGLImpl;
gl_hooks_t gHooks[2];
gl_hooks_t gHooksNoContext;
pthread_key_t gGLWrapperKey = -1;

// ----------------------------------------------------------------------------

void setGLHooksThreadSpecific(gl_hooks_t const *value) {
    setGlThreadSpecific(value);
}

/*****************************************************************************/

static int gl_no_context() {
    if (egl_tls_t::logNoContextCall()) {
        char const* const error = "call to OpenGL ES API with "
                "no current context (logged once per thread)";
        if (LOG_NDEBUG) {
            ALOGE(error);
        } else {
            LOG_ALWAYS_FATAL(error);
        }
        char value[PROPERTY_VALUE_MAX];
        property_get("debug.egl.callstack", value, "0");
        if (atoi(value)) {
            CallStack stack(LOG_TAG);
        }
    }
    return 0;
}

static void early_egl_init(void)
{
    int numHooks = sizeof(gHooksNoContext) / sizeof(EGLFuncPointer);
    EGLFuncPointer *iter = reinterpret_cast<EGLFuncPointer*>(&gHooksNoContext);
    for (int hook = 0; hook < numHooks; ++hook) {
        *(iter++) = reinterpret_cast<EGLFuncPointer>(gl_no_context);
    }

    setGLHooksThreadSpecific(&gHooksNoContext);
}

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static int sEarlyInitState = pthread_once(&once_control, &early_egl_init);

// ----------------------------------------------------------------------------

egl_display_ptr validate_display(EGLDisplay dpy) {
    egl_display_ptr dp = get_display(dpy);
    if (!dp)
        return setError(EGL_BAD_DISPLAY, egl_display_ptr(NULL));
    if (!dp->isReady())
        return setError(EGL_NOT_INITIALIZED, egl_display_ptr(NULL));

    return dp;
}

egl_display_ptr validate_display_connection(EGLDisplay dpy,
        egl_connection_t*& cnx) {
    cnx = NULL;
    egl_display_ptr dp = validate_display(dpy);
    if (!dp)
        return dp;
    cnx = &gEGLImpl;
    if (cnx->dso == 0) {
        return setError(EGL_BAD_CONFIG, egl_display_ptr(NULL));
    }
    return dp;
}

// ----------------------------------------------------------------------------

const GLubyte * egl_get_string_for_current_context(GLenum name) {
    // NOTE: returning NULL here will fall-back to the default
    // implementation.

    EGLContext context = egl_tls_t::getContext();
    if (context == EGL_NO_CONTEXT)
        return NULL;

    egl_context_t const * const c = get_context(context);
    if (c == NULL) // this should never happen, by construction
        return NULL;

    if (name != GL_EXTENSIONS)
        return NULL;

    return (const GLubyte *)c->gl_extensions.string();
}

const GLubyte * egl_get_string_for_current_context(GLenum name, GLuint index) {
    // NOTE: returning NULL here will fall-back to the default
    // implementation.

    EGLContext context = egl_tls_t::getContext();
    if (context == EGL_NO_CONTEXT)
        return NULL;

    egl_context_t const * const c = get_context(context);
    if (c == NULL) // this should never happen, by construction
        return NULL;

    if (name != GL_EXTENSIONS)
        return NULL;

    // if index is out of bounds, assume it will be in the default
    // implementation too, so we don't have to generate a GL error here
    if (index >= c->tokenized_gl_extensions.size())
        return NULL;

    return (const GLubyte *)c->tokenized_gl_extensions.itemAt(index).string();
}

GLint egl_get_num_extensions_for_current_context() {
    // NOTE: returning -1 here will fall-back to the default
    // implementation.

    EGLContext context = egl_tls_t::getContext();
    if (context == EGL_NO_CONTEXT)
        return -1;

    egl_context_t const * const c = get_context(context);
    if (c == NULL) // this should never happen, by construction
        return -1;

    return (GLint)c->tokenized_gl_extensions.size();
}

// ----------------------------------------------------------------------------

// this mutex protects:
//    d->disp[]
//    egl_init_drivers_locked()
//
static EGLBoolean egl_init_drivers_locked() {
    if (sEarlyInitState) {
        // initialized by static ctor. should be set here.
        return EGL_FALSE;
    }

    // get our driver loader
    Loader& loader(Loader::getInstance());

    // dynamically load our EGL implementation
    egl_connection_t* cnx = &gEGLImpl;
    if (cnx->dso == 0) {
        cnx->hooks[egl_connection_t::GLESv1_INDEX] =
                &gHooks[egl_connection_t::GLESv1_INDEX];
        cnx->hooks[egl_connection_t::GLESv2_INDEX] =
                &gHooks[egl_connection_t::GLESv2_INDEX];
        cnx->dso = loader.open(cnx);
    }

    return cnx->dso ? EGL_TRUE : EGL_FALSE;
}

static pthread_mutex_t sInitDriverMutex = PTHREAD_MUTEX_INITIALIZER;

EGLBoolean egl_init_drivers() {
    EGLBoolean res;
    pthread_mutex_lock(&sInitDriverMutex);
    res = egl_init_drivers_locked();
    pthread_mutex_unlock(&sInitDriverMutex);
    return res;
}

static pthread_mutex_t sLogPrintMutex = PTHREAD_MUTEX_INITIALIZER;
static nsecs_t sLogPrintTime = 0;
#define NSECS_DURATION 1000000000

void gl_unimplemented() {
    bool printLog = false;
    nsecs_t now = systemTime();
    pthread_mutex_lock(&sLogPrintMutex);
    if ((now - sLogPrintTime) > NSECS_DURATION) {
        sLogPrintTime = now;
        printLog = true;
    }
    pthread_mutex_unlock(&sLogPrintMutex);
    if (printLog) {
        ALOGE("called unimplemented OpenGL ES API");
        char value[PROPERTY_VALUE_MAX];
        property_get("debug.egl.callstack", value, "0");
        if (atoi(value)) {
            CallStack stack(LOG_TAG);
        }
    }
}

void gl_noop() {
}

// ----------------------------------------------------------------------------

void setGlThreadSpecific(gl_hooks_t const *value) {
    gl_hooks_t const * volatile * tls_hooks = get_tls_hooks();
    tls_hooks[TLS_SLOT_OPENGL_API] = value;
}

// ----------------------------------------------------------------------------
// GL / EGL hooks
// ----------------------------------------------------------------------------

#undef GL_ENTRY
#undef EGL_ENTRY
#define GL_ENTRY(_r, _api, ...) #_api,
#define EGL_ENTRY(_r, _api, ...) #_api,

char const * const gl_names[] = {
    #include "../entries.in"
    NULL
};

char const * const egl_names[] = {
    #include "egl_entries.in"
    NULL
};

#undef GL_ENTRY
#undef EGL_ENTRY


// ----------------------------------------------------------------------------
}; // namespace android
// ----------------------------------------------------------------------------

