/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/log.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#include "RenderEngine.h"
#include "GLES10RenderEngine.h"
#include "GLES11RenderEngine.h"
#include "GLES20RenderEngine.h"
#include "GLExtensions.h"
#include "Mesh.h"

EGLAPI const char* eglQueryStringImplementationANDROID(EGLDisplay dpy, EGLint name);

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

static bool findExtension(const char* exts, const char* name) {
    if (!exts)
        return false;
    size_t len = strlen(name);

    const char* pos = exts;
    while ((pos = strstr(pos, name)) != NULL) {
        if (pos[len] == '\0' || pos[len] == ' ')
            return true;
        pos += len;
    }

    return false;
}

RenderEngine* RenderEngine::create(EGLDisplay display, int hwcFormat) {
    // EGL_ANDROIDX_no_config_context is an experimental extension with no
    // written specification. It will be replaced by something more formal.
    // SurfaceFlinger is using it to allow a single EGLContext to render to
    // both a 16-bit primary display framebuffer and a 32-bit virtual display
    // framebuffer.
    //
    // The code assumes that ES2 or later is available if this extension is
    // supported.
    EGLConfig config = EGL_NO_CONFIG;
    if (!findExtension(
            eglQueryStringImplementationANDROID(display, EGL_EXTENSIONS),
            "EGL_ANDROIDX_no_config_context")) {
        config = chooseEglConfig(display, hwcFormat);
    }

    EGLint renderableType = 0;
    if (config == EGL_NO_CONFIG) {
        renderableType = EGL_OPENGL_ES2_BIT;
    } else if (!eglGetConfigAttrib(display, config,
            EGL_RENDERABLE_TYPE, &renderableType)) {
        LOG_ALWAYS_FATAL("can't query EGLConfig RENDERABLE_TYPE");
    }
    EGLint contextClientVersion = 0;
    if (renderableType & EGL_OPENGL_ES2_BIT) {
        contextClientVersion = 2;
    } else if (renderableType & EGL_OPENGL_ES_BIT) {
        contextClientVersion = 1;
    } else {
        LOG_ALWAYS_FATAL("no supported EGL_RENDERABLE_TYPEs");
    }

    // Also create our EGLContext
    EGLint contextAttributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, contextClientVersion,      // MUST be first
#ifdef EGL_IMG_context_priority
#ifdef HAS_CONTEXT_PRIORITY
#warning "using EGL_IMG_context_priority"
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
#endif
#endif
            EGL_NONE, EGL_NONE
    };
    EGLContext ctxt = eglCreateContext(display, config, NULL, contextAttributes);

    // if can't create a GL context, we can only abort.
    LOG_ALWAYS_FATAL_IF(ctxt==EGL_NO_CONTEXT, "EGLContext creation failed");


    // now figure out what version of GL did we actually get
    // NOTE: a dummy surface is not needed if KHR_create_context is supported

    EGLConfig dummyConfig = config;
    if (dummyConfig == EGL_NO_CONFIG) {
        dummyConfig = chooseEglConfig(display, hwcFormat);
    }
    EGLint attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE, EGL_NONE };
    EGLSurface dummy = eglCreatePbufferSurface(display, dummyConfig, attribs);
    LOG_ALWAYS_FATAL_IF(dummy==EGL_NO_SURFACE, "can't create dummy pbuffer");
    EGLBoolean success = eglMakeCurrent(display, dummy, dummy, ctxt);
    LOG_ALWAYS_FATAL_IF(!success, "can't make dummy pbuffer current");

    GLExtensions& extensions(GLExtensions::getInstance());
    extensions.initWithGLStrings(
            glGetString(GL_VENDOR),
            glGetString(GL_RENDERER),
            glGetString(GL_VERSION),
            glGetString(GL_EXTENSIONS));

    GlesVersion version = parseGlesVersion( extensions.getVersion() );

    // initialize the renderer while GL is current

    RenderEngine* engine = NULL;
    switch (version) {
    case GLES_VERSION_1_0:
        engine = new GLES10RenderEngine();
        break;
    case GLES_VERSION_1_1:
        engine = new GLES11RenderEngine();
        break;
    case GLES_VERSION_2_0:
    case GLES_VERSION_3_0:
        engine = new GLES20RenderEngine();
        break;
    }
    engine->setEGLHandles(config, ctxt);

    ALOGI("OpenGL ES informations:");
    ALOGI("vendor    : %s", extensions.getVendor());
    ALOGI("renderer  : %s", extensions.getRenderer());
    ALOGI("version   : %s", extensions.getVersion());
    ALOGI("extensions: %s", extensions.getExtension());
    ALOGI("GL_MAX_TEXTURE_SIZE = %zu", engine->getMaxTextureSize());
    ALOGI("GL_MAX_VIEWPORT_DIMS = %zu", engine->getMaxViewportDims());

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, dummy);

    return engine;
}

RenderEngine::RenderEngine() : mEGLContext(EGL_NO_CONTEXT) {
}

RenderEngine::~RenderEngine() {
}

void RenderEngine::setEGLHandles(EGLConfig config, EGLContext ctxt) {
    mEGLConfig = config;
    mEGLContext = ctxt;
}

EGLContext RenderEngine::getEGLConfig() const {
    return mEGLConfig;
}

EGLContext RenderEngine::getEGLContext() const {
    return mEGLContext;
}

void RenderEngine::checkErrors() const {
    do {
        // there could be more than one error flag
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        ALOGE("GL error 0x%04x", int(error));
    } while (true);
}

RenderEngine::GlesVersion RenderEngine::parseGlesVersion(const char* str) {
    int major, minor;
    if (sscanf(str, "OpenGL ES-CM %d.%d", &major, &minor) != 2) {
        if (sscanf(str, "OpenGL ES %d.%d", &major, &minor) != 2) {
            ALOGW("Unable to parse GL_VERSION string: \"%s\"", str);
            return GLES_VERSION_1_0;
        }
    }

    if (major == 1 && minor == 0) return GLES_VERSION_1_0;
    if (major == 1 && minor >= 1) return GLES_VERSION_1_1;
    if (major == 2 && minor >= 0) return GLES_VERSION_2_0;
    if (major == 3 && minor >= 0) return GLES_VERSION_3_0;

    ALOGW("Unrecognized OpenGL ES version: %d.%d", major, minor);
    return GLES_VERSION_1_0;
}

void RenderEngine::fillRegionWithColor(const Region& region, uint32_t height,
        float red, float green, float blue, float alpha) {
    size_t c;
    Rect const* r = region.getArray(&c);
    Mesh mesh(Mesh::TRIANGLES, c*6, 2);
    Mesh::VertexArray<vec2> position(mesh.getPositionArray<vec2>());
    for (size_t i=0 ; i<c ; i++, r++) {
        position[i*6 + 0].x = r->left;
        position[i*6 + 0].y = height - r->top;
        position[i*6 + 1].x = r->left;
        position[i*6 + 1].y = height - r->bottom;
        position[i*6 + 2].x = r->right;
        position[i*6 + 2].y = height - r->bottom;
        position[i*6 + 3].x = r->left;
        position[i*6 + 3].y = height - r->top;
        position[i*6 + 4].x = r->right;
        position[i*6 + 4].y = height - r->bottom;
        position[i*6 + 5].x = r->right;
        position[i*6 + 5].y = height - r->top;
    }
    setupFillWithColor(red, green, blue, alpha);
    drawMesh(mesh);
}

void RenderEngine::clearWithColor(float red, float green, float blue, float alpha) {
    glClearColor(red, green, blue, alpha);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderEngine::setScissor(
        uint32_t left, uint32_t bottom, uint32_t right, uint32_t top) {
    glScissor(left, bottom, right, top);
    glEnable(GL_SCISSOR_TEST);
}

void RenderEngine::disableScissor() {
    glDisable(GL_SCISSOR_TEST);
}

void RenderEngine::genTextures(size_t count, uint32_t* names) {
    glGenTextures(count, names);
}

void RenderEngine::deleteTextures(size_t count, uint32_t const* names) {
    glDeleteTextures(count, names);
}

void RenderEngine::readPixels(size_t l, size_t b, size_t w, size_t h, uint32_t* pixels) {
    glReadPixels(l, b, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void RenderEngine::flush() {
    glFlush();
}

void RenderEngine::dump(String8& result) {
    const GLExtensions& extensions(GLExtensions::getInstance());
    result.appendFormat("GLES: %s, %s, %s\n",
            extensions.getVendor(),
            extensions.getRenderer(),
            extensions.getVersion());
    result.appendFormat("%s\n", extensions.getExtension());
}

// ---------------------------------------------------------------------------

RenderEngine::BindImageAsFramebuffer::BindImageAsFramebuffer(
        RenderEngine& engine, EGLImageKHR image, bool useReadPixels,
        int reqWidth, int reqHeight) : mEngine(engine), mUseReadPixels(useReadPixels)
{
    mEngine.bindImageAsFramebuffer(image, &mTexName, &mFbName, &mStatus,
            useReadPixels, reqWidth, reqHeight);

    ALOGE_IF(mStatus != GL_FRAMEBUFFER_COMPLETE_OES,
            "glCheckFramebufferStatusOES error %d", mStatus);
}

RenderEngine::BindImageAsFramebuffer::~BindImageAsFramebuffer() {
    // back to main framebuffer
    mEngine.unbindFramebuffer(mTexName, mFbName, mUseReadPixels);
}

status_t RenderEngine::BindImageAsFramebuffer::getStatus() const {
    return mStatus == GL_FRAMEBUFFER_COMPLETE_OES ? NO_ERROR : BAD_VALUE;
}

// ---------------------------------------------------------------------------

static status_t selectConfigForAttribute(EGLDisplay dpy, EGLint const* attrs,
        EGLint attribute, EGLint wanted, EGLConfig* outConfig) {
    EGLConfig config = NULL;
    EGLint numConfigs = -1, n = 0;
    eglGetConfigs(dpy, NULL, 0, &numConfigs);
    EGLConfig* const configs = new EGLConfig[numConfigs];
    eglChooseConfig(dpy, attrs, configs, numConfigs, &n);

    if (n) {
        if (attribute != EGL_NONE) {
            for (int i=0 ; i<n ; i++) {
                EGLint value = 0;
                eglGetConfigAttrib(dpy, configs[i], attribute, &value);
                if (wanted == value) {
                    *outConfig = configs[i];
                    delete [] configs;
                    return NO_ERROR;
                }
            }
        } else {
            // just pick the first one
            *outConfig = configs[0];
            delete [] configs;
            return NO_ERROR;
        }
    }
    delete [] configs;
    return NAME_NOT_FOUND;
}

class EGLAttributeVector {
    struct Attribute;
    class Adder;
    friend class Adder;
    KeyedVector<Attribute, EGLint> mList;
    struct Attribute {
        Attribute() {};
        Attribute(EGLint v) : v(v) { }
        EGLint v;
        bool operator < (const Attribute& other) const {
            // this places EGL_NONE at the end
            EGLint lhs(v);
            EGLint rhs(other.v);
            if (lhs == EGL_NONE) lhs = 0x7FFFFFFF;
            if (rhs == EGL_NONE) rhs = 0x7FFFFFFF;
            return lhs < rhs;
        }
    };
    class Adder {
        friend class EGLAttributeVector;
        EGLAttributeVector& v;
        EGLint attribute;
        Adder(EGLAttributeVector& v, EGLint attribute)
            : v(v), attribute(attribute) {
        }
    public:
        void operator = (EGLint value) {
            if (attribute != EGL_NONE) {
                v.mList.add(attribute, value);
            }
        }
        operator EGLint () const { return v.mList[attribute]; }
    };
public:
    EGLAttributeVector() {
        mList.add(EGL_NONE, EGL_NONE);
    }
    void remove(EGLint attribute) {
        if (attribute != EGL_NONE) {
            mList.removeItem(attribute);
        }
    }
    Adder operator [] (EGLint attribute) {
        return Adder(*this, attribute);
    }
    EGLint operator [] (EGLint attribute) const {
       return mList[attribute];
    }
    // cast-operator to (EGLint const*)
    operator EGLint const* () const { return &mList.keyAt(0).v; }
};


static status_t selectEGLConfig(EGLDisplay display, EGLint format,
    EGLint renderableType, EGLConfig* config) {
    // select our EGLConfig. It must support EGL_RECORDABLE_ANDROID if
    // it is to be used with WIFI displays
    status_t err;
    EGLint wantedAttribute;
    EGLint wantedAttributeValue;

    EGLAttributeVector attribs;
    if (renderableType) {
        attribs[EGL_RENDERABLE_TYPE]            = renderableType;
        attribs[EGL_RECORDABLE_ANDROID]         = EGL_TRUE;
        attribs[EGL_SURFACE_TYPE]               = EGL_WINDOW_BIT|EGL_PBUFFER_BIT;
        attribs[EGL_FRAMEBUFFER_TARGET_ANDROID] = EGL_TRUE;
        attribs[EGL_RED_SIZE]                   = 8;
        attribs[EGL_GREEN_SIZE]                 = 8;
        attribs[EGL_BLUE_SIZE]                  = 8;
        wantedAttribute                         = EGL_NONE;
        wantedAttributeValue                    = EGL_NONE;
    } else {
        // if no renderable type specified, fallback to a simplified query
        wantedAttribute                         = EGL_NATIVE_VISUAL_ID;
        wantedAttributeValue                    = format;
    }

    err = selectConfigForAttribute(display, attribs,
            wantedAttribute, wantedAttributeValue, config);
    if (err == NO_ERROR) {
        EGLint caveat;
        if (eglGetConfigAttrib(display, *config, EGL_CONFIG_CAVEAT, &caveat))
            ALOGW_IF(caveat == EGL_SLOW_CONFIG, "EGL_SLOW_CONFIG selected!");
    }

    return err;
}

EGLConfig RenderEngine::chooseEglConfig(EGLDisplay display, int format) {
    status_t err;
    EGLConfig config;

    // First try to get an ES2 config
    err = selectEGLConfig(display, format, EGL_OPENGL_ES2_BIT, &config);
    if (err != NO_ERROR) {
        // If ES2 fails, try ES1
        err = selectEGLConfig(display, format, EGL_OPENGL_ES_BIT, &config);
        if (err != NO_ERROR) {
            // still didn't work, probably because we're on the emulator...
            // try a simplified query
            ALOGW("no suitable EGLConfig found, trying a simpler query");
            err = selectEGLConfig(display, format, 0, &config);
            if (err != NO_ERROR) {
                // this EGL is too lame for android
                LOG_ALWAYS_FATAL("no suitable EGLConfig found, giving up");
            }
        }
    }

    // print some debugging info
    EGLint r,g,b,a;
    eglGetConfigAttrib(display, config, EGL_RED_SIZE,   &r);
    eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(display, config, EGL_BLUE_SIZE,  &b);
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &a);
    ALOGI("EGL information:");
    ALOGI("vendor    : %s", eglQueryString(display, EGL_VENDOR));
    ALOGI("version   : %s", eglQueryString(display, EGL_VERSION));
    ALOGI("extensions: %s", eglQueryString(display, EGL_EXTENSIONS));
    ALOGI("Client API: %s", eglQueryString(display, EGL_CLIENT_APIS)?:"Not Supported");
    ALOGI("EGLSurface: %d-%d-%d-%d, config=%p", r, g, b, a, config);

    return config;
}

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------
