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
#ifdef BOARD_EGL_NEEDS_LEGACY_FB
#include <ui/FramebufferNativeWindow.h>
#endif

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
        DisplayType type,
        bool isSecure,
        const wp<IBinder>& displayToken,
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
      mIsSecure(isSecure),
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
#ifndef BOARD_EGL_NEEDS_LEGACY_FB
    ANativeWindow* const window = mNativeWindow.get();
#else
    ANativeWindow* const window = new FramebufferNativeWindow();
#endif

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

    // Name the display.  The name will be replaced shortly if the display
    // was created with createDisplay().
    switch (mType) {
        case DISPLAY_PRIMARY:
            mDisplayName = "Built-in Screen";
            break;
        case DISPLAY_EXTERNAL:
            mDisplayName = "HDMI Screen";
            break;
        default:
            mDisplayName = "Virtual Screen";    // e.g. Overlay #n
            break;
    }

    // initialize the display orientation transform.
    setProjection(DisplayState::eOrientationDefault, mViewport, mFrame);
}

void DisplayDevice::setDisplayName(const String8& displayName) {
    if (!displayName.isEmpty()) {
        // never override the name with an empty name
        mDisplayName = displayName;
    }
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

void DisplayDevice::swapBuffers(HWComposer& hwc) const {
    EGLBoolean success = EGL_TRUE;
    if (hwc.initCheck() != NO_ERROR) {
        // no HWC, we call eglSwapBuffers()
        success = eglSwapBuffers(mDisplay, mSurface);
    } else {
        // We have a valid HWC, but not all displays can use it, in particular
        // the virtual displays are on their own.
        // TODO: HWC 1.2 will allow virtual displays
        if (mType >= DisplayDevice::DISPLAY_VIRTUAL) {
            // always call eglSwapBuffers() for virtual displays
            success = eglSwapBuffers(mDisplay, mSurface);
        } else if (hwc.supportsFramebufferTarget()) {
            // as of hwc 1.1 we always call eglSwapBuffers if we have some
            // GLES layers
            if (hwc.hasGlesComposition(mType)) {
                success = eglSwapBuffers(mDisplay, mSurface);
            }
        } else {
            // HWC doesn't have the framebuffer target, we don't call
            // eglSwapBuffers(), since this is handled by HWComposer::commit().
        }
    }

    if (!success) {
        EGLint error = eglGetError();
        if (error == EGL_CONTEXT_LOST ||
                mType == DisplayDevice::DISPLAY_PRIMARY) {
            LOG_ALWAYS_FATAL("eglSwapBuffers(%p, %p) failed with 0x%08x",
                    mDisplay, mSurface, error);
        }
    }
}

void DisplayDevice::onSwapBuffersCompleted(HWComposer& hwc) const {
    if (hwc.initCheck() == NO_ERROR) {
        if (hwc.supportsFramebufferTarget()) {
            int fd = hwc.getAndResetReleaseFenceFd(mType);
            mFramebufferSurface->setReleaseFenceFd(fd);
        }
    }
}

uint32_t DisplayDevice::getFlags() const
{
    return mFlags;
}

EGLBoolean DisplayDevice::makeCurrent(EGLDisplay dpy,
        const sp<const DisplayDevice>& hw, EGLContext ctx) {
    EGLBoolean result = EGL_TRUE;
    EGLSurface sur = eglGetCurrentSurface(EGL_DRAW);
    if (sur != hw->mSurface) {
        result = eglMakeCurrent(dpy, hw->mSurface, hw->mSurface, ctx);
        if (result == EGL_TRUE) {
            setViewportAndProjection(hw);
        }
    }
    return result;
}

void DisplayDevice::setViewportAndProjection(const sp<const DisplayDevice>& hw) {
    GLsizei w = hw->mDisplayWidth;
    GLsizei h = hw->mDisplayHeight;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // put the origin in the left-bottom corner
    glOrthof(0, w, 0, h, 0, 1); // l=0, r=w ; b=0, t=h
    glMatrixMode(GL_MODELVIEW);
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
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.sf.hwrotation", value, "0");
    int additionalRot = atoi(value);

    if (additionalRot) {
        additionalRot /= 90;
        if (orientation == DisplayState::eOrientationUnchanged) {
            orientation = additionalRot;
        } else {
            orientation += additionalRot;
            orientation %= 4;
        }
    }

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

void DisplayDevice::setProjection(int orientation,
        const Rect& viewport, const Rect& frame) {
    mOrientation = orientation;
    mViewport = viewport;
    mFrame = frame;
    updateGeometryTransform();
}

void DisplayDevice::updateGeometryTransform() {
    int w = mDisplayWidth;
    int h = mDisplayHeight;
    Transform TL, TP, R, S;
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
        if (src_width != dst_width || src_height != dst_height) {
            float sx = dst_width  / src_width;
            float sy = dst_height / src_height;
            S.set(sx, 0, 0, sy);
        }

        float src_x = viewport.left;
        float src_y = viewport.top;
        float dst_x = frame.left;
        float dst_y = frame.top;
        TL.set(-src_x, -src_y);
        TP.set(dst_x, dst_y);

        // The viewport and frame are both in the logical orientation.
        // Apply the logical translation, scale to physical size, apply the
        // physical translation and finally rotate to the physical orientation.
        mGlobalTransform = R * TP * S * TL;

        const uint8_t type = mGlobalTransform.getType();
        mNeedsFiltering = (!mGlobalTransform.preserveRects() ||
                (type >= Transform::SCALE));
    }
}

void DisplayDevice::dump(String8& result, char* buffer, size_t SIZE) const {
    const Transform& tr(mGlobalTransform);
    snprintf(buffer, SIZE,
        "+ DisplayDevice: %s\n"
        "   type=%x, layerStack=%u, (%4dx%4d), ANativeWindow=%p, orient=%2d (type=%08x), "
        "flips=%u, isSecure=%d, secureVis=%d, acquired=%d, numLayers=%u\n"
        "   v:[%d,%d,%d,%d], f:[%d,%d,%d,%d], "
        "transform:[[%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f]]\n",
        mDisplayName.string(), mType,
        mLayerStack, mDisplayWidth, mDisplayHeight, mNativeWindow.get(),
        mOrientation, tr.getType(), getPageFlipCount(),
        mIsSecure, mSecureLayerVisible, mScreenAcquired, mVisibleLayersSortedByZ.size(),
        mViewport.left, mViewport.top, mViewport.right, mViewport.bottom,
        mFrame.left, mFrame.top, mFrame.right, mFrame.bottom,
        tr[0][0], tr[1][0], tr[2][0],
        tr[0][1], tr[1][1], tr[2][1],
        tr[0][2], tr[1][2], tr[2][2]);

    result.append(buffer);

    String8 fbtargetDump;
    if (mFramebufferSurface != NULL) {
        mFramebufferSurface->dump(fbtargetDump);
        result.append(fbtargetDump);
    }
}
