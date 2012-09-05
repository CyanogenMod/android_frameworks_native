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

#include "clz.h"
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
        DisplayType type, const wp<IBinder>& displayToken,
        const sp<ANativeWindow>& nativeWindow,
        const sp<FramebufferSurface>& framebufferSurface,
        EGLConfig config)
    : mFlinger(flinger),
      mType(type), mHwcDisplayId(-1),
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
      mLayerStack(0),
      mOrientation()
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
    mViewport.makeInvalid();
    mFrame.makeInvalid();

    // external displays are always considered enabled
    mScreenAcquired = (mType >= DisplayDevice::NUM_DISPLAY_TYPES);

    // get an h/w composer ID
    mHwcDisplayId = mFlinger->allocateHwcDisplayId(mType);

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

EGLBoolean DisplayDevice::makeCurrent(EGLDisplay dpy,
        const sp<const DisplayDevice>& hw, EGLContext ctx) {
    EGLBoolean result = EGL_TRUE;
    EGLSurface sur = eglGetCurrentSurface(EGL_DRAW);
    if (sur != hw->mSurface) {
        result = eglMakeCurrent(dpy, hw->mSurface, hw->mSurface, ctx);
        if (result == EGL_TRUE) {
            GLsizei w = hw->mDisplayWidth;
            GLsizei h = hw->mDisplayHeight;
            glViewport(0, 0, w, h);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            // put the origin in the left-bottom corner
            glOrthof(0, w, 0, h, 0, 1); // l=0, r=w ; b=0, t=h
        }
    }
    return result;
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

const Vector< sp<LayerBase> >& DisplayDevice::getVisibleLayersSortedByZ() const {
    return mVisibleLayersSortedByZ;
}

bool DisplayDevice::getSecureLayerVisible() const {
    return mSecureLayerVisible;
}

Region DisplayDevice::getDirtyRegion(bool repaintEverything) const {
    Region dirty;
    if (repaintEverything) {
        dirty.set(getBounds());
    } else {
        const Transform& planeTransform(mGlobalTransform);
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

void DisplayDevice::setOrientation(int orientation) {
    mOrientation = orientation;
    updateGeometryTransform();
}

void DisplayDevice::setViewport(const Rect& viewport) {
    if (viewport.isValid()) {
        mViewport = viewport;
        updateGeometryTransform();
    }
}

void DisplayDevice::setFrame(const Rect& frame) {
    if (frame.isValid()) {
        mFrame = frame;
        updateGeometryTransform();
    }
}

void DisplayDevice::updateGeometryTransform() {
    int w = mDisplayWidth;
    int h = mDisplayHeight;
    Transform R, S;
    if (DisplayDevice::orientationToTransfrom(
            mOrientation, w, h, &R) == NO_ERROR) {
        dirtyRegion.set(bounds());

        Rect viewport(mViewport);
        Rect frame(mFrame);

        if (!frame.isValid()) {
            // the destination frame can be invalid if it has never been set,
            // in that case we assume the whole display frame.
            frame = Rect(w, h);
        }

        if (viewport.isEmpty()) {
            // viewport can be invalid if it has never been set, in that case
            // we assume the whole display size.
            // it's also invalid to have an empty viewport, so we handle that
            // case in the same way.
            viewport = Rect(w, h);
            if (R.getOrientation() & Transform::ROT_90) {
                // viewport is always specified in the logical orientation
                // of the display (ie: post-rotation).
                swap(viewport.right, viewport.bottom);
            }
        }

        float src_width  = viewport.width();
        float src_height = viewport.height();
        float dst_width  = frame.width();
        float dst_height = frame.height();
        if (src_width != src_height || dst_width != dst_height) {
            float sx = dst_width  / src_width;
            float sy = dst_height / src_height;
            S.set(sx, 0, 0, sy);
        }
        float src_x = viewport.left;
        float src_y = viewport.top;
        float dst_x = frame.left;
        float dst_y = frame.top;
        float tx = dst_x - src_x;
        float ty = dst_y - src_y;
        S.set(tx, ty);

        // rotate first, followed by scaling
        mGlobalTransform = S * R;
    }
}
