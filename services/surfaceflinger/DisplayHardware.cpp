/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cutils/properties.h>

#include <utils/RefBase.h>
#include <utils/Log.h>

#include <ui/PixelFormat.h>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <hardware/gralloc.h>

#include "DisplayHardware/FramebufferSurface.h"
#include "DisplayHardware/DisplayHardwareBase.h"
#include "DisplayHardware/HWComposer.h"

#include "DisplayHardware.h"
#include "GLExtensions.h"
#include "SurfaceFlinger.h"

using namespace android;


static __attribute__((noinline))
void checkGLErrors()
{
    do {
        // there could be more than one error flag
        GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        ALOGE("GL error 0x%04x", int(error));
    } while(true);
}

static __attribute__((noinline))
void checkEGLErrors(const char* token)
{
    struct EGLUtils {
        static const char *strerror(EGLint err) {
            switch (err){
                case EGL_SUCCESS:           return "EGL_SUCCESS";
                case EGL_NOT_INITIALIZED:   return "EGL_NOT_INITIALIZED";
                case EGL_BAD_ACCESS:        return "EGL_BAD_ACCESS";
                case EGL_BAD_ALLOC:         return "EGL_BAD_ALLOC";
                case EGL_BAD_ATTRIBUTE:     return "EGL_BAD_ATTRIBUTE";
                case EGL_BAD_CONFIG:        return "EGL_BAD_CONFIG";
                case EGL_BAD_CONTEXT:       return "EGL_BAD_CONTEXT";
                case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
                case EGL_BAD_DISPLAY:       return "EGL_BAD_DISPLAY";
                case EGL_BAD_MATCH:         return "EGL_BAD_MATCH";
                case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
                case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
                case EGL_BAD_PARAMETER:     return "EGL_BAD_PARAMETER";
                case EGL_BAD_SURFACE:       return "EGL_BAD_SURFACE";
                case EGL_CONTEXT_LOST:      return "EGL_CONTEXT_LOST";
                default: return "UNKNOWN";
            }
        }
    };

    EGLint error = eglGetError();
    if (error && error != EGL_SUCCESS) {
        ALOGE("%s: EGL error 0x%04x (%s)",
                token, int(error), EGLUtils::strerror(error));
    }
}

/*
 * Initialize the display to the specified values.
 *
 */

DisplayHardware::DisplayHardware(
        const sp<SurfaceFlinger>& flinger,
        uint32_t dpy)
    : DisplayHardwareBase(flinger, dpy),
      mFlinger(flinger), mFlags(0), mHwc(0), mSecureLayerVisible(false)
{
    init(dpy);
}

DisplayHardware::~DisplayHardware()
{
    fini();
}

float DisplayHardware::getDpiX() const          { return mDpiX; }
float DisplayHardware::getDpiY() const          { return mDpiY; }
float DisplayHardware::getDensity() const       { return mDensity; }
float DisplayHardware::getRefreshRate() const   { return mRefreshRate; }
int DisplayHardware::getWidth() const           { return mDisplayWidth; }
int DisplayHardware::getHeight() const          { return mDisplayHeight; }
PixelFormat DisplayHardware::getFormat() const  { return mFormat; }
uint32_t DisplayHardware::getMaxTextureSize() const { return mMaxTextureSize; }

uint32_t DisplayHardware::getMaxViewportDims() const {
    return mMaxViewportDims[0] < mMaxViewportDims[1] ?
            mMaxViewportDims[0] : mMaxViewportDims[1];
}

static status_t selectConfigForPixelFormat(
        EGLDisplay dpy,
        EGLint const* attrs,
        PixelFormat format,
        EGLConfig* outConfig)
{
    EGLConfig config = NULL;
    EGLint numConfigs = -1, n=0;
    eglGetConfigs(dpy, NULL, 0, &numConfigs);
    EGLConfig* const configs = new EGLConfig[numConfigs];
    eglChooseConfig(dpy, attrs, configs, numConfigs, &n);
    for (int i=0 ; i<n ; i++) {
        EGLint nativeVisualId = 0;
        eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &nativeVisualId);
        if (nativeVisualId>0 && format == nativeVisualId) {
            *outConfig = configs[i];
            delete [] configs;
            return NO_ERROR;
        }
    }
    delete [] configs;
    return NAME_NOT_FOUND;
}


void DisplayHardware::init(uint32_t dpy)
{
    mNativeWindow = new FramebufferSurface();
    framebuffer_device_t const * fbDev = mNativeWindow->getDevice();
    if (!fbDev) {
        ALOGE("Display subsystem failed to initialize. check logs. exiting...");
        exit(0);
    }

    int format;
    ANativeWindow const * const window = mNativeWindow.get();
    window->query(window, NATIVE_WINDOW_FORMAT, &format);
    mDpiX = mNativeWindow->xdpi;
    mDpiY = mNativeWindow->ydpi;
    mRefreshRate = fbDev->fps;

    if (mDpiX == 0 || mDpiY == 0) {
        ALOGE("invalid screen resolution from fb HAL (xdpi=%f, ydpi=%f), "
               "defaulting to 160 dpi", mDpiX, mDpiY);
        mDpiX = mDpiY = 160;
    }

    class Density {
        static int getDensityFromProperty(char const* propName) {
            char property[PROPERTY_VALUE_MAX];
            int density = 0;
            if (property_get(propName, property, NULL) > 0) {
                density = atoi(property);
            }
            return density;
        }
    public:
        static int getEmuDensity() {
            return getDensityFromProperty("qemu.sf.lcd_density"); }
        static int getBuildDensity()  {
            return getDensityFromProperty("ro.sf.lcd_density"); }
    };


    // The density of the device is provided by a build property
    mDensity = Density::getBuildDensity() / 160.0f;

    if (mDensity == 0) {
        // the build doesn't provide a density -- this is wrong!
        // use xdpi instead
        ALOGE("ro.sf.lcd_density must be defined as a build property");
        mDensity = mDpiX / 160.0f;
    }

    if (Density::getEmuDensity()) {
        // if "qemu.sf.lcd_density" is specified, it overrides everything
        mDpiX = mDpiY = mDensity = Density::getEmuDensity();
        mDensity /= 160.0f;
    }



    /* FIXME: this is a temporary HACK until we are able to report the refresh rate
     * properly from the HAL. The WindowManagerService now relies on this value.
     */
#ifndef REFRESH_RATE
    mRefreshRate = fbDev->fps;
#else
    mRefreshRate = REFRESH_RATE;
#warning "refresh rate set via makefile to REFRESH_RATE"
#endif

    mRefreshPeriod = nsecs_t(1e9 / mRefreshRate);

    EGLint w, h, dummy;
    EGLint numConfigs=0;
    EGLSurface surface;
    EGLContext context;
    EGLBoolean result;
    status_t err;

    // initialize EGL
    EGLint attribs[] = {
            EGL_SURFACE_TYPE,           EGL_WINDOW_BIT,
            EGL_RECORDABLE_ANDROID,     EGL_TRUE,
            EGL_NONE
    };

    // TODO: all the extensions below should be queried through
    // eglGetProcAddress().

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, NULL, NULL);
    eglGetConfigs(display, NULL, 0, &numConfigs);

    EGLConfig config = NULL;
    err = selectConfigForPixelFormat(display, attribs, format, &config);
    if (err) {
        // maybe we failed because of EGL_RECORDABLE_ANDROID
        ALOGW("couldn't find an EGLConfig with EGL_RECORDABLE_ANDROID");
        attribs[2] = EGL_NONE;
        err = selectConfigForPixelFormat(display, attribs, format, &config);
    }

    ALOGE_IF(err, "couldn't find an EGLConfig matching the screen format");
    
    EGLint r,g,b,a;
    eglGetConfigAttrib(display, config, EGL_RED_SIZE,   &r);
    eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(display, config, EGL_BLUE_SIZE,  &b);
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &a);

    if (mNativeWindow->isUpdateOnDemand()) {
        mFlags |= PARTIAL_UPDATES;
    }
    
    if (eglGetConfigAttrib(display, config, EGL_CONFIG_CAVEAT, &dummy) == EGL_TRUE) {
        if (dummy == EGL_SLOW_CONFIG)
            mFlags |= SLOW_CONFIG;
    }

    /*
     * Create our main surface
     */

    surface = eglCreateWindowSurface(display, config, mNativeWindow.get(), NULL);
    eglQuerySurface(display, surface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &mDisplayHeight);

    if (mFlags & PARTIAL_UPDATES) {
        // if we have partial updates, we definitely don't need to
        // preserve the backbuffer, which may be costly.
        eglSurfaceAttrib(display, surface,
                EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
    }

    /*
     * Create our OpenGL ES context
     */
    
    EGLint contextAttributes[] = {
#ifdef EGL_IMG_context_priority
#ifdef HAS_CONTEXT_PRIORITY
#warning "using EGL_IMG_context_priority"
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
#endif
#endif
        EGL_NONE, EGL_NONE
    };
    context = eglCreateContext(display, config, NULL, contextAttributes);

    mDisplay = display;
    mConfig  = config;
    mSurface = surface;
    mContext = context;
    mFormat  = fbDev->format;
    mPageFlipCount = 0;

    /*
     * Gather OpenGL ES extensions
     */

    result = eglMakeCurrent(display, surface, surface, context);
    if (!result) {
        ALOGE("Couldn't create a working GLES context. check logs. exiting...");
        exit(0);
    }

    GLExtensions& extensions(GLExtensions::getInstance());
    extensions.initWithGLStrings(
            glGetString(GL_VENDOR),
            glGetString(GL_RENDERER),
            glGetString(GL_VERSION),
            glGetString(GL_EXTENSIONS),
            eglQueryString(display, EGL_VENDOR),
            eglQueryString(display, EGL_VERSION),
            eglQueryString(display, EGL_EXTENSIONS));

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

    ALOGI("EGL informations:");
    ALOGI("# of configs : %d", numConfigs);
    ALOGI("vendor    : %s", extensions.getEglVendor());
    ALOGI("version   : %s", extensions.getEglVersion());
    ALOGI("extensions: %s", extensions.getEglExtension());
    ALOGI("Client API: %s", eglQueryString(display, EGL_CLIENT_APIS)?:"Not Supported");
    ALOGI("EGLSurface: %d-%d-%d-%d, config=%p", r, g, b, a, config);

    ALOGI("OpenGL informations:");
    ALOGI("vendor    : %s", extensions.getVendor());
    ALOGI("renderer  : %s", extensions.getRenderer());
    ALOGI("version   : %s", extensions.getVersion());
    ALOGI("extensions: %s", extensions.getExtension());
    ALOGI("GL_MAX_TEXTURE_SIZE = %d", mMaxTextureSize);
    ALOGI("GL_MAX_VIEWPORT_DIMS = %d x %d", mMaxViewportDims[0], mMaxViewportDims[1]);
    ALOGI("flags = %08x", mFlags);

    // Unbind the context from this thread
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);


    // initialize the H/W composer
    mHwc = new HWComposer(mFlinger, *this, mRefreshPeriod);
    if (mHwc->initCheck() == NO_ERROR) {
        mHwc->setFrameBuffer(mDisplay, mSurface);
    }


    // initialize the display orientation transform.
    // it's a constant that should come from the display driver.
    int displayOrientation = ISurfaceComposer::eOrientationDefault;
    char property[PROPERTY_VALUE_MAX];
    if (property_get("ro.sf.hwrotation", property, NULL) > 0) {
        //displayOrientation
        switch (atoi(property)) {
        case 90:
            displayOrientation = ISurfaceComposer::eOrientation90;
            break;
        case 270:
            displayOrientation = ISurfaceComposer::eOrientation270;
            break;
        }
    }

    w = mDisplayWidth;
    h = mDisplayHeight;
    DisplayHardware::orientationToTransfrom(displayOrientation, w, h,
            &mDisplayTransform);
    if (displayOrientation & ISurfaceComposer::eOrientationSwapMask) {
        mLogicalDisplayWidth = h;
        mLogicalDisplayHeight = w;
    } else {
        mLogicalDisplayWidth = w;
        mLogicalDisplayHeight = h;
    }
    DisplayHardware::setOrientation(ISurfaceComposer::eOrientationDefault);
}

void DisplayHardware::setVSyncHandler(const sp<VSyncHandler>& handler) {
    Mutex::Autolock _l(mLock);
    mVSyncHandler = handler;
}

void DisplayHardware::eventControl(int event, int enabled) {
    if (event == EVENT_VSYNC) {
        mPowerHAL.vsyncHint(enabled);
    }
    mHwc->eventControl(event, enabled);
}

void DisplayHardware::onVSyncReceived(int dpy, nsecs_t timestamp) {
    sp<VSyncHandler> handler;
    { // scope for the lock
        Mutex::Autolock _l(mLock);
        mLastHwVSync = timestamp;
        if (mVSyncHandler != NULL) {
            handler = mVSyncHandler.promote();
        }
    }

    if (handler != NULL) {
        handler->onVSyncReceived(dpy, timestamp);
    }
}

HWComposer& DisplayHardware::getHwComposer() const {
    return *mHwc;
}

/*
 * Clean up.  Throw out our local state.
 *
 * (It's entirely possible we'll never get here, since this is meant
 * for real hardware, which doesn't restart.)
 */

void DisplayHardware::fini()
{
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(mDisplay);
}

void DisplayHardware::releaseScreen() const
{
    DisplayHardwareBase::releaseScreen();
    if (mHwc->initCheck() == NO_ERROR) {
        mHwc->release();
    }
}

void DisplayHardware::acquireScreen() const
{
    if (mHwc->initCheck() == NO_ERROR) {
        mHwc->acquire();
    }
    DisplayHardwareBase::acquireScreen();
}

uint32_t DisplayHardware::getPageFlipCount() const {
    return mPageFlipCount;
}

nsecs_t DisplayHardware::getRefreshTimestamp() const {
    // this returns the last refresh timestamp.
    // if the last one is not available, we estimate it based on
    // the refresh period and whatever closest timestamp we have.
    Mutex::Autolock _l(mLock);
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    return now - ((now - mLastHwVSync) %  mRefreshPeriod);
}

nsecs_t DisplayHardware::getRefreshPeriod() const {
    return mRefreshPeriod;
}

status_t DisplayHardware::compositionComplete() const {
    return mNativeWindow->compositionComplete();
}

void DisplayHardware::flip(const Region& dirty) const
{
    checkGLErrors();

    EGLDisplay dpy = mDisplay;
    EGLSurface surface = mSurface;

#ifdef EGL_ANDROID_swap_rectangle    
    if (mFlags & SWAP_RECTANGLE) {
        const Region newDirty(dirty.intersect(bounds()));
        const Rect b(newDirty.getBounds());
        eglSetSwapRectangleANDROID(dpy, surface,
                b.left, b.top, b.width(), b.height());
    } 
#endif
    
    if (mFlags & PARTIAL_UPDATES) {
        mNativeWindow->setUpdateRectangle(dirty.getBounds());
    }
    
    mPageFlipCount++;

    if (mHwc->initCheck() == NO_ERROR) {
        mHwc->commit();
    } else {
        eglSwapBuffers(dpy, surface);
    }
    checkEGLErrors("eglSwapBuffers");

    // for debugging
    //glClearColor(1,0,0,0);
    //glClear(GL_COLOR_BUFFER_BIT);
}

uint32_t DisplayHardware::getFlags() const
{
    return mFlags;
}

void DisplayHardware::makeCurrent() const
{
    eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
}

void DisplayHardware::dump(String8& res) const
{
    mNativeWindow->dump(res);
}

// ----------------------------------------------------------------------------

void DisplayHardware::setVisibleLayersSortedByZ(const Vector< sp<LayerBase> >& layers) {
    mVisibleLayersSortedByZ = layers;
    size_t count = layers.size();
    for (size_t i=0 ; i<count ; i++) {
        if (layers[i]->isSecure()) {
            mSecureLayerVisible = true;
        }
    }
}

Vector< sp<LayerBase> > DisplayHardware::getVisibleLayersSortedByZ() const {
    return mVisibleLayersSortedByZ;
}

bool DisplayHardware::getSecureLayerVisible() const {
    return mSecureLayerVisible;
}

// ----------------------------------------------------------------------------

status_t DisplayHardware::orientationToTransfrom(
        int orientation, int w, int h, Transform* tr)
{
    uint32_t flags = 0;
    switch (orientation) {
    case ISurfaceComposer::eOrientationDefault:
        flags = Transform::ROT_0;
        break;
    case ISurfaceComposer::eOrientation90:
        flags = Transform::ROT_90;
        break;
    case ISurfaceComposer::eOrientation180:
        flags = Transform::ROT_180;
        break;
    case ISurfaceComposer::eOrientation270:
        flags = Transform::ROT_270;
        break;
    default:
        return BAD_VALUE;
    }
    tr->set(flags, w, h);
    return NO_ERROR;
}

status_t DisplayHardware::setOrientation(int orientation)
{
    // If the rotation can be handled in hardware, this is where
    // the magic should happen.

    const int w = mLogicalDisplayWidth;
    const int h = mLogicalDisplayHeight;
    mUserDisplayWidth = w;
    mUserDisplayHeight = h;

    Transform orientationTransform;
    DisplayHardware::orientationToTransfrom(orientation, w, h,
            &orientationTransform);
    if (orientation & ISurfaceComposer::eOrientationSwapMask) {
        mUserDisplayWidth = h;
        mUserDisplayHeight = w;
    }

    mOrientation = orientation;
    mGlobalTransform = mDisplayTransform * orientationTransform;
    return NO_ERROR;
}
