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

#include <gui/SurfaceTextureClient.h>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <hardware/gralloc.h>

#include "DisplayHardware/FramebufferSurface.h"
#include "DisplayHardware/HWComposer.h"

#include "DisplayDevice.h"
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

// ----------------------------------------------------------------------------

/*
 * Initialize the display to the specified values.
 *
 */

DisplayDevice::DisplayDevice(
        const sp<SurfaceFlinger>& flinger,
        int display,
        const sp<ANativeWindow>& nativeWindow,
        const sp<FramebufferSurface>& framebufferSurface,
        EGLConfig config)
    : mFlinger(flinger),
      mId(display),
      mNativeWindow(nativeWindow),
      mFramebufferSurface(framebufferSurface),
      mDisplay(EGL_NO_DISPLAY),
      mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT),
      mDisplayWidth(), mDisplayHeight(), mFormat(),
      mFlags(),
      mPageFlipCount(),
      mSecureLayerVisible(false),
      mScreenAcquired(false),
      mOrientation(),
      mLayerStack(0)
{
    init(config);
}

DisplayDevice::~DisplayDevice() {
    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }
}

bool DisplayDevice::isValid() const {
    return mFlinger != NULL;
}

int DisplayDevice::getWidth() const {
    return mDisplayWidth;
}

int DisplayDevice::getHeight() const {
    return mDisplayHeight;
}

PixelFormat DisplayDevice::getFormat() const {
    return mFormat;
}

EGLSurface DisplayDevice::getEGLSurface() const {
    return mSurface;
}

void DisplayDevice::init(EGLConfig config)
{
    ANativeWindow* const window = mNativeWindow.get();

    int format;
    window->query(window, NATIVE_WINDOW_FORMAT, &format);

    /*
     * Create our display's surface
     */

    EGLSurface surface;
    EGLint w, h;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    surface = eglCreateWindowSurface(display, config, window, NULL);
    eglQuerySurface(display, surface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(display, surface, EGL_HEIGHT, &mDisplayHeight);

    mDisplay = display;
    mSurface = surface;
    mFormat  = format;
    mPageFlipCount = 0;

    // external displays are always considered enabled
    mScreenAcquired = mId >= DisplayDevice::DISPLAY_ID_COUNT;

    // initialize the display orientation transform.
    DisplayDevice::setOrientation(DisplayState::eOrientationDefault);
}

uint32_t DisplayDevice::getPageFlipCount() const {
    return mPageFlipCount;
}

status_t DisplayDevice::compositionComplete() const {
    if (mFramebufferSurface == NULL) {
        return NO_ERROR;
    }
    return mFramebufferSurface->compositionComplete();
}

void DisplayDevice::flip(const Region& dirty) const
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
    
    mPageFlipCount++;
}

uint32_t DisplayDevice::getFlags() const
{
    return mFlags;
}

void DisplayDevice::dump(String8& res) const
{
    if (mFramebufferSurface != NULL) {
        mFramebufferSurface->dump(res);
    }
}

void DisplayDevice::makeCurrent(const sp<const DisplayDevice>& hw, EGLContext ctx) {
    EGLSurface sur = eglGetCurrentSurface(EGL_DRAW);
    if (sur != hw->mSurface) {
        EGLDisplay dpy = eglGetCurrentDisplay();
        eglMakeCurrent(dpy, hw->mSurface, hw->mSurface, ctx);
    }
}

// ----------------------------------------------------------------------------

void DisplayDevice::setVisibleLayersSortedByZ(const Vector< sp<LayerBase> >& layers) {
    mVisibleLayersSortedByZ = layers;
    mSecureLayerVisible = false;
    size_t count = layers.size();
    for (size_t i=0 ; i<count ; i++) {
        if (layers[i]->isSecure()) {
            mSecureLayerVisible = true;
        }
    }
}

Vector< sp<LayerBase> > DisplayDevice::getVisibleLayersSortedByZ() const {
    return mVisibleLayersSortedByZ;
}

bool DisplayDevice::getSecureLayerVisible() const {
    return mSecureLayerVisible;
}

Region DisplayDevice::getDirtyRegion(bool repaintEverything) const {
    Region dirty;
    const Transform& planeTransform(mGlobalTransform);
    if (repaintEverything) {
        dirty.set(getBounds());
    } else {
        dirty = planeTransform.transform(this->dirtyRegion);
        dirty.andSelf(getBounds());
    }
    return dirty;
}

// ----------------------------------------------------------------------------

bool DisplayDevice::canDraw() const {
    return mScreenAcquired;
}

void DisplayDevice::releaseScreen() const {
    mScreenAcquired = false;
}

void DisplayDevice::acquireScreen() const {
    mScreenAcquired = true;
}

bool DisplayDevice::isScreenAcquired() const {
    return mScreenAcquired;
}

// ----------------------------------------------------------------------------

void DisplayDevice::setLayerStack(uint32_t stack) {
    mLayerStack = stack;
    dirtyRegion.set(bounds());
}

// ----------------------------------------------------------------------------

status_t DisplayDevice::orientationToTransfrom(
        int orientation, int w, int h, Transform* tr)
{
    uint32_t flags = 0;
    switch (orientation) {
    case DisplayState::eOrientationDefault:
        flags = Transform::ROT_0;
        break;
    case DisplayState::eOrientation90:
        flags = Transform::ROT_90;
        break;
    case DisplayState::eOrientation180:
        flags = Transform::ROT_180;
        break;
    case DisplayState::eOrientation270:
        flags = Transform::ROT_270;
        break;
    default:
        return BAD_VALUE;
    }
    tr->set(flags, w, h);
    return NO_ERROR;
}

status_t DisplayDevice::setOrientation(int orientation) {
    int w = mDisplayWidth;
    int h = mDisplayHeight;

    DisplayDevice::orientationToTransfrom(
            orientation, w, h, &mGlobalTransform);
    if (orientation & DisplayState::eOrientationSwapMask) {
        int tmp = w;
        w = h;
        h = tmp;
    }
    mOrientation = orientation;
    dirtyRegion.set(bounds());
    return NO_ERROR;
}
