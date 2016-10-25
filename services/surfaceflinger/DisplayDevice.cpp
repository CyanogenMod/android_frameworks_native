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

// #define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "DisplayDevice"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cutils/properties.h>

#include <utils/RefBase.h>
#include <utils/Log.h>

#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>

#include <gui/Surface.h>

#include <hardware/gralloc.h>

#include "DisplayHardware/DisplaySurface.h"
#include "DisplayHardware/HWComposer.h"
#ifdef USE_HWC2
#include "DisplayHardware/HWC2.h"
#endif
#include "RenderEngine/RenderEngine.h"

#include "clz.h"
#include "DisplayDevice.h"
#include "SurfaceFlinger.h"
#include "Layer.h"

// ----------------------------------------------------------------------------
using namespace android;
// ----------------------------------------------------------------------------

#ifdef EGL_ANDROID_swap_rectangle
static constexpr bool kEGLAndroidSwapRectangle = true;
#else
static constexpr bool kEGLAndroidSwapRectangle = false;
#endif

#if !defined(EGL_EGLEXT_PROTOTYPES) || !defined(EGL_ANDROID_swap_rectangle)
// Dummy implementation in case it is missing.
inline void eglSetSwapRectangleANDROID (EGLDisplay, EGLSurface, EGLint, EGLint, EGLint, EGLint) {
}
#endif

/*
 * Initialize the display to the specified values.
 *
 */

uint32_t DisplayDevice::sPrimaryDisplayOrientation = 0;

DisplayDevice::DisplayDevice(
        const sp<SurfaceFlinger>& flinger,
        DisplayType type,
        int32_t hwcId,
#ifndef USE_HWC2
        int format,
#endif
        bool isSecure,
        const wp<IBinder>& displayToken,
        const sp<DisplaySurface>& displaySurface,
        const sp<IGraphicBufferProducer>& producer,
        EGLConfig config)
    : lastCompositionHadVisibleLayers(false),
      mFlinger(flinger),
      mType(type),
      mHwcDisplayId(hwcId),
      mDisplayToken(displayToken),
      mDisplaySurface(displaySurface),
      mDisplay(EGL_NO_DISPLAY),
      mSurface(EGL_NO_SURFACE),
      mDisplayWidth(),
      mDisplayHeight(),
#ifndef USE_HWC2
      mFormat(),
#endif
      mFlags(),
      mPageFlipCount(),
      mIsSecure(isSecure),
      mLayerStack(NO_LAYER_STACK),
      mOrientation(),
      mPowerMode(HWC_POWER_MODE_OFF),
      mActiveConfig(0)
{
    Surface* surface;
    mNativeWindow = surface = new Surface(producer, false);
    ANativeWindow* const window = mNativeWindow.get();
    char property[PROPERTY_VALUE_MAX];

    /*
     * Create our display's surface
     */

    EGLSurface eglSurface;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (config == EGL_NO_CONFIG) {
#ifdef USE_HWC2
        config = RenderEngine::chooseEglConfig(display, PIXEL_FORMAT_RGBA_8888);
#else
        config = RenderEngine::chooseEglConfig(display, format);
#endif
    }
    eglSurface = eglCreateWindowSurface(display, config, window, NULL);
    eglQuerySurface(display, eglSurface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(display, eglSurface, EGL_HEIGHT, &mDisplayHeight);

    // Make sure that composition can never be stalled by a virtual display
    // consumer that isn't processing buffers fast enough. We have to do this
    // in two places:
    // * Here, in case the display is composed entirely by HWC.
    // * In makeCurrent(), using eglSwapInterval. Some EGL drivers set the
    //   window's swap interval in eglMakeCurrent, so they'll override the
    //   interval we set here.
    if (mType >= DisplayDevice::DISPLAY_VIRTUAL)
        window->setSwapInterval(window, 0);

    mConfig = config;
    mDisplay = display;
    mSurface = eglSurface;
#ifndef USE_HWC2
    mFormat = format;
#endif
    mPageFlipCount = 0;
    mViewport.makeInvalid();
    mFrame.makeInvalid();

    // virtual displays are always considered enabled
    mPowerMode = (mType >= DisplayDevice::DISPLAY_VIRTUAL) ?
                  HWC_POWER_MODE_NORMAL : HWC_POWER_MODE_OFF;

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

    mPanelMountFlip = 0;
    // 1: H-Flip, 2: V-Flip, 3: 180 (HV Flip)
    property_get("ro.panel.mountflip", property, "0");
    mPanelMountFlip = atoi(property);

    // initialize the display orientation transform.
    setProjection(DisplayState::eOrientationDefault, mViewport, mFrame);

#ifdef NUM_FRAMEBUFFER_SURFACE_BUFFERS
    surface->allocateBuffers();
#endif
}

DisplayDevice::~DisplayDevice() {
    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }
}

void DisplayDevice::disconnect(HWComposer& hwc) {
    if (mHwcDisplayId >= 0) {
        hwc.disconnectDisplay(mHwcDisplayId);
#ifndef USE_HWC2
        if (mHwcDisplayId >= DISPLAY_VIRTUAL)
            hwc.freeDisplayId(mHwcDisplayId);
#endif
        mHwcDisplayId = -1;
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

#ifndef USE_HWC2
PixelFormat DisplayDevice::getFormat() const {
    return mFormat;
}
#endif

EGLSurface DisplayDevice::getEGLSurface() const {
    return mSurface;
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

#ifndef USE_HWC2
status_t DisplayDevice::compositionComplete() const {
    return mDisplaySurface->compositionComplete();
}
#endif

void DisplayDevice::flip(const Region& dirty) const
{
    mFlinger->getRenderEngine().checkErrors();

    if (kEGLAndroidSwapRectangle) {
        if (mFlags & SWAP_RECTANGLE) {
            const Region newDirty(dirty.intersect(bounds()));
            const Rect b(newDirty.getBounds());
            eglSetSwapRectangleANDROID(mDisplay, mSurface,
                    b.left, b.top, b.width(), b.height());
        }
    }

    mPageFlipCount++;
}

status_t DisplayDevice::beginFrame(bool mustRecompose) const {
    return mDisplaySurface->beginFrame(mustRecompose);
}

#ifdef USE_HWC2
status_t DisplayDevice::prepareFrame(HWComposer& hwc) {
    status_t error = hwc.prepare(*this);
    if (error != NO_ERROR) {
        return error;
    }

    DisplaySurface::CompositionType compositionType;
    bool hasClient = hwc.hasClientComposition(mHwcDisplayId);
    bool hasDevice = hwc.hasDeviceComposition(mHwcDisplayId);
    if (hasClient && hasDevice) {
        compositionType = DisplaySurface::COMPOSITION_MIXED;
    } else if (hasClient) {
        compositionType = DisplaySurface::COMPOSITION_GLES;
    } else if (hasDevice) {
        compositionType = DisplaySurface::COMPOSITION_HWC;
    } else {
        // Nothing to do -- when turning the screen off we get a frame like
        // this. Call it a HWC frame since we won't be doing any GLES work but
        // will do a prepare/set cycle.
        compositionType = DisplaySurface::COMPOSITION_HWC;
    }
    return mDisplaySurface->prepareFrame(compositionType);
}
#else
status_t DisplayDevice::prepareFrame(const HWComposer& hwc) const {
    DisplaySurface::CompositionType compositionType;
    bool haveGles = hwc.hasGlesComposition(mHwcDisplayId);
    bool haveHwc = hwc.hasHwcComposition(mHwcDisplayId);
    if (haveGles && haveHwc) {
        compositionType = DisplaySurface::COMPOSITION_MIXED;
    } else if (haveGles) {
        compositionType = DisplaySurface::COMPOSITION_GLES;
    } else if (haveHwc) {
        compositionType = DisplaySurface::COMPOSITION_HWC;
    } else {
        // Nothing to do -- when turning the screen off we get a frame like
        // this. Call it a HWC frame since we won't be doing any GLES work but
        // will do a prepare/set cycle.
        compositionType = DisplaySurface::COMPOSITION_HWC;
    }
    return mDisplaySurface->prepareFrame(compositionType);
}
#endif

void DisplayDevice::swapBuffers(HWComposer& hwc) const {
#ifdef USE_HWC2
    if (hwc.hasClientComposition(mHwcDisplayId)) {
#else
    // We need to call eglSwapBuffers() if:
    //  (1) we don't have a hardware composer, or
    //  (2) we did GLES composition this frame, and either
    //    (a) we have framebuffer target support (not present on legacy
    //        devices, where HWComposer::commit() handles things); or
    //    (b) this is a virtual display
    if (hwc.initCheck() != NO_ERROR ||
            (hwc.hasGlesComposition(mHwcDisplayId) &&
             (hwc.supportsFramebufferTarget() || mType >= DISPLAY_VIRTUAL))) {
#endif
        EGLBoolean success = eglSwapBuffers(mDisplay, mSurface);
        if (!success) {
            EGLint error = eglGetError();
            if (error == EGL_CONTEXT_LOST ||
                    mType == DisplayDevice::DISPLAY_PRIMARY) {
                LOG_ALWAYS_FATAL("eglSwapBuffers(%p, %p) failed with 0x%08x",
                        mDisplay, mSurface, error);
            } else {
                ALOGE("eglSwapBuffers(%p, %p) failed with 0x%08x",
                        mDisplay, mSurface, error);
            }
        }
    }

    status_t result = mDisplaySurface->advanceFrame();
    if (result != NO_ERROR) {
        ALOGE("[%s] failed pushing new frame to HWC: %d",
                mDisplayName.string(), result);
    }
}

#ifdef USE_HWC2
void DisplayDevice::onSwapBuffersCompleted() const {
    mDisplaySurface->onFrameCommitted();
}
#else
void DisplayDevice::onSwapBuffersCompleted(HWComposer& hwc) const {
    if (hwc.initCheck() == NO_ERROR) {
        mDisplaySurface->onFrameCommitted();
    }
}
#endif

uint32_t DisplayDevice::getFlags() const
{
    return mFlags;
}

EGLBoolean DisplayDevice::makeCurrent(EGLDisplay dpy, EGLContext ctx) const {
    EGLBoolean result = EGL_TRUE;
    EGLSurface sur = eglGetCurrentSurface(EGL_DRAW);
    if (sur != mSurface) {
        result = eglMakeCurrent(dpy, mSurface, mSurface, ctx);
        if (result == EGL_TRUE) {
            if (mType >= DisplayDevice::DISPLAY_VIRTUAL)
                eglSwapInterval(dpy, 0);
        }
    }
    setViewportAndProjection();
    return result;
}

void DisplayDevice::setViewportAndProjection() const {
    size_t w = mDisplayWidth;
    size_t h = mDisplayHeight;
    Rect sourceCrop(0, 0, w, h);
    mFlinger->getRenderEngine().setViewportAndProjection(w, h, sourceCrop, h,
        false, Transform::ROT_0);
}

const sp<Fence>& DisplayDevice::getClientTargetAcquireFence() const {
    return mDisplaySurface->getClientTargetAcquireFence();
}

// ----------------------------------------------------------------------------

void DisplayDevice::setVisibleLayersSortedByZ(const Vector< sp<Layer> >& layers) {
    mVisibleLayersSortedByZ = layers;
}

const Vector< sp<Layer> >& DisplayDevice::getVisibleLayersSortedByZ() const {
    return mVisibleLayersSortedByZ;
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
void DisplayDevice::setPowerMode(int mode) {
    mPowerMode = mode;
}

int DisplayDevice::getPowerMode()  const {
    return mPowerMode;
}

bool DisplayDevice::isDisplayOn() const {
    return (mPowerMode != HWC_POWER_MODE_OFF);
}

// ----------------------------------------------------------------------------
void DisplayDevice::setActiveConfig(int mode) {
    mActiveConfig = mode;
}

int DisplayDevice::getActiveConfig()  const {
    return mActiveConfig;
}

// ----------------------------------------------------------------------------
#ifdef USE_HWC2
void DisplayDevice::setActiveColorMode(android_color_mode_t mode) {
    mActiveColorMode = mode;
}

android_color_mode_t DisplayDevice::getActiveColorMode() const {
    return mActiveColorMode;
}
#endif

// ----------------------------------------------------------------------------

void DisplayDevice::setLayerStack(uint32_t stack) {
    mLayerStack = stack;
    dirtyRegion.set(bounds());
}

// ----------------------------------------------------------------------------

uint32_t DisplayDevice::getOrientationTransform() const {
    uint32_t transform = 0;
    switch (mOrientation) {
        case DisplayState::eOrientationDefault:
            transform = Transform::ROT_0;
            break;
        case DisplayState::eOrientation90:
            transform = Transform::ROT_90;
            break;
        case DisplayState::eOrientation180:
            transform = Transform::ROT_180;
            break;
        case DisplayState::eOrientation270:
            transform = Transform::ROT_270;
            break;
    }
    return transform;
}

status_t DisplayDevice::orientationToTransfrom(
        int orientation, int w, int h, Transform* tr)
{
    uint32_t flags = 0;
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.sf.hwrotation", value, "0");
    int additionalRot = atoi(value);

    if (additionalRot && mType == DISPLAY_PRIMARY) {
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

    if (DISPLAY_PRIMARY == mHwcDisplayId) {
        flags = flags ^ getPanelMountFlip();
    }

    tr->set(flags, w, h);
    return NO_ERROR;
}

void DisplayDevice::setDisplaySize(const int newWidth, const int newHeight) {
    dirtyRegion.set(getBounds());

    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }

    mDisplaySurface->resizeBuffers(newWidth, newHeight);

    ANativeWindow* const window = mNativeWindow.get();
    mSurface = eglCreateWindowSurface(mDisplay, mConfig, window, NULL);
    eglQuerySurface(mDisplay, mSurface, EGL_WIDTH,  &mDisplayWidth);
    eglQuerySurface(mDisplay, mSurface, EGL_HEIGHT, &mDisplayHeight);

    LOG_FATAL_IF(mDisplayWidth != newWidth,
                "Unable to set new width to %d", newWidth);
    LOG_FATAL_IF(mDisplayHeight != newHeight,
                "Unable to set new height to %d", newHeight);
}

void DisplayDevice::setProjection(int orientation,
        const Rect& newViewport, const Rect& newFrame) {
    Rect viewport(newViewport);
    Rect frame(newFrame);

    const int w = mDisplayWidth;
    const int h = mDisplayHeight;

    Transform R;
    DisplayDevice::orientationToTransfrom(orientation, w, h, &R);

    if (!frame.isValid()) {
        // the destination frame can be invalid if it has never been set,
        // in that case we assume the whole display frame.
        char value[PROPERTY_VALUE_MAX];
        property_get("ro.sf.hwrotation", value, "0");
        int additionalRot = atoi(value);

        if (additionalRot == 90 || additionalRot == 270) {
            frame = Rect(h, w);
        } else {
            frame = Rect(w, h);
        }
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

    dirtyRegion.set(getBounds());

    Transform TL, TP, S;
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

    mScissor = mGlobalTransform.transform(viewport);
    if (mScissor.isEmpty()) {
        mScissor = getBounds();
    }

    mOrientation = orientation;
    if (mType == DisplayType::DISPLAY_PRIMARY) {
        uint32_t transform = 0;
        switch (mOrientation) {
            case DisplayState::eOrientationDefault:
                transform = Transform::ROT_0;
                break;
            case DisplayState::eOrientation90:
                transform = Transform::ROT_90;
                break;
            case DisplayState::eOrientation180:
                transform = Transform::ROT_180;
                break;
            case DisplayState::eOrientation270:
                transform = Transform::ROT_270;
                break;
        }
        sPrimaryDisplayOrientation = transform;
    }
    mViewport = viewport;
    mFrame = frame;
}

uint32_t DisplayDevice::getPrimaryDisplayOrientationTransform() {
    return sPrimaryDisplayOrientation;
}

void DisplayDevice::dump(String8& result) const {
    const Transform& tr(mGlobalTransform);
    result.appendFormat(
        "+ DisplayDevice: %s\n"
        "   type=%x, hwcId=%d, layerStack=%u, (%4dx%4d), ANativeWindow=%p, orient=%2d (type=%08x), "
        "flips=%u, isSecure=%d, powerMode=%d, activeConfig=%d, numLayers=%zu\n"
        "   v:[%d,%d,%d,%d], f:[%d,%d,%d,%d], s:[%d,%d,%d,%d],"
        "transform:[[%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f][%0.3f,%0.3f,%0.3f]]\n",
        mDisplayName.string(), mType, mHwcDisplayId,
        mLayerStack, mDisplayWidth, mDisplayHeight, mNativeWindow.get(),
        mOrientation, tr.getType(), getPageFlipCount(),
        mIsSecure, mPowerMode, mActiveConfig,
        mVisibleLayersSortedByZ.size(),
        mViewport.left, mViewport.top, mViewport.right, mViewport.bottom,
        mFrame.left, mFrame.top, mFrame.right, mFrame.bottom,
        mScissor.left, mScissor.top, mScissor.right, mScissor.bottom,
        tr[0][0], tr[1][0], tr[2][0],
        tr[0][1], tr[1][1], tr[2][1],
        tr[0][2], tr[1][2], tr[2][2]);

    String8 surfaceDump;
    mDisplaySurface->dumpAsString(surfaceDump);
    result.append(surfaceDump);
}
