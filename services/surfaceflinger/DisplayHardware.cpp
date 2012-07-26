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

#include <ui/DisplayInfo.h>
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
#include "LayerBase.h"

// ----------------------------------------------------------------------------
using namespace android;
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

/*
 * Initialize the display to the specified values.
 *
 */

DisplayHardware::DisplayHardware(
        const sp<SurfaceFlinger>& flinger,
        int display,
        const sp<SurfaceTextureClient>& surface,
        EGLConfig config)
    : DisplayHardwareBase(display),
      mFlinger(flinger),
      mDisplayId(display),
      mNativeWindow(surface),
      mFlags(0),
      mSecureLayerVisible(false)
{
    init(config);
}

DisplayHardware::~DisplayHardware() {
}

float DisplayHardware::getDpiX() const {
    return mDpiX;
}

float DisplayHardware::getDpiY() const {
    return mDpiY;
}

float DisplayHardware::getDensity() const {
    return mDensity;
}

float DisplayHardware::getRefreshRate() const {
    return mRefreshRate;
}

int DisplayHardware::getWidth() const {
    return mDisplayWidth;
}

int DisplayHardware::getHeight() const {
    return mDisplayHeight;
}

PixelFormat DisplayHardware::getFormat() const {
    return mFormat;
}

EGLSurface DisplayHardware::getEGLSurface() const {
    return mSurface;
}

status_t DisplayHardware::getInfo(DisplayInfo* info) const {
    info->w = getWidth();
    info->h = getHeight();
    info->xdpi = getDpiX();
    info->ydpi = getDpiY();
    info->fps = getRefreshRate();
    info->density = getDensity();
    info->orientation = getOrientation();
    // TODO: this needs to go away (currently needed only by webkit)
    getPixelFormatInfo(getFormat(), &info->pixelFormatInfo);
    return NO_ERROR;
}

void DisplayHardware::init(EGLConfig config)
{
    ANativeWindow* const window = mNativeWindow.get();

    int concreteType;
    window->query(window, NATIVE_WINDOW_CONCRETE_TYPE, &concreteType);
    if (concreteType == NATIVE_WINDOW_FRAMEBUFFER) {
        mFramebufferSurface = static_cast<FramebufferSurface *>(mNativeWindow.get());
    }

    int format;
    window->query(window, NATIVE_WINDOW_FORMAT, &format);
    mDpiX = window->xdpi;
    mDpiY = window->ydpi;
    if (mFramebufferSurface != NULL) {
        mRefreshRate = mFramebufferSurface->getRefreshRate();
    } else {
        mRefreshRate = 60;
    }
    mRefreshPeriod = nsecs_t(1e9 / mRefreshRate);


    // TODO: Not sure if display density should handled by SF any longer
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

    /*
     * Create our display's surface
     */

    EGLSurface surface;
    EGLint w, h;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    surface = eglCreateWindowSurface(display, config, window, NULL);
    eglQuerySurface(display, surface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &mDisplayHeight);

    if (mFramebufferSurface != NULL) {
        if (mFramebufferSurface->isUpdateOnDemand()) {
            mFlags |= PARTIAL_UPDATES;
            // if we have partial updates, we definitely don't need to
            // preserve the backbuffer, which may be costly.
            eglSurfaceAttrib(display, surface,
                    EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
        }
    }

    mDisplay = display;
    mSurface = surface;
    mFormat  = format;
    mPageFlipCount = 0;

    // initialize the display orientation transform.
    DisplayHardware::setOrientation(ISurfaceComposer::eOrientationDefault);
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
    if (mFramebufferSurface == NULL) {
        return NO_ERROR;
    }
    return mFramebufferSurface->compositionComplete();
}

void DisplayHardware::onVSyncReceived(nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);
    mLastHwVSync = timestamp;
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
        if (mFramebufferSurface != NULL) {
            mFramebufferSurface->setUpdateRectangle(dirty.getBounds());
        }
    }
    
    mPageFlipCount++;
}

uint32_t DisplayHardware::getFlags() const
{
    return mFlags;
}

void DisplayHardware::dump(String8& res) const
{
    if (mFramebufferSurface != NULL) {
        mFramebufferSurface->dump(res);
    }
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

status_t DisplayHardware::setOrientation(int orientation) {
    int w = mDisplayWidth;
    int h = mDisplayHeight;

    DisplayHardware::orientationToTransfrom(
            orientation, w, h, &mGlobalTransform);
    if (orientation & ISurfaceComposer::eOrientationSwapMask) {
        int tmp = w;
        w = h;
        h = tmp;
    }
    mOrientation = orientation;
    return NO_ERROR;
}
