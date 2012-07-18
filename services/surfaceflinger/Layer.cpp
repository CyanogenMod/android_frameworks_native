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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <math.h>

#include <cutils/compiler.h>
#include <cutils/native_handle.h>
#include <cutils/properties.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/StopWatch.h>
#include <utils/Trace.h>

#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>

#include <gui/Surface.h>

#include "clz.h"
#include "DisplayHardware/DisplayHardware.h"
#include "DisplayHardware/HWComposer.h"
#include "GLExtensions.h"
#include "Layer.h"
#include "SurfaceFlinger.h"
#include "SurfaceTextureLayer.h"

#define DEBUG_RESIZE    0

namespace android {

// ---------------------------------------------------------------------------

Layer::Layer(SurfaceFlinger* flinger,
        DisplayID display, const sp<Client>& client)
    :   LayerBaseClient(flinger, display, client),
        mTextureName(-1U),
        mQueuedFrames(0),
        mCurrentTransform(0),
        mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
        mCurrentOpacity(true),
        mRefreshPending(false),
        mFrameLatencyNeeded(false),
        mFrameLatencyOffset(0),
        mFormat(PIXEL_FORMAT_NONE),
        mGLExtensions(GLExtensions::getInstance()),
        mOpaqueLayer(true),
        mNeedsDithering(false),
        mSecure(false),
        mProtectedByApp(false)
{
    mCurrentCrop.makeInvalid();
    glGenTextures(1, &mTextureName);
}

void Layer::onLayerDisplayed() {
    if (mFrameLatencyNeeded) {
        const DisplayHardware& hw(graphicPlane(0).displayHardware());
        mFrameStats[mFrameLatencyOffset].timestamp = mSurfaceTexture->getTimestamp();
        mFrameStats[mFrameLatencyOffset].set = systemTime();
        mFrameStats[mFrameLatencyOffset].vsync = hw.getRefreshTimestamp();
        mFrameLatencyOffset = (mFrameLatencyOffset + 1) % 128;
        mFrameLatencyNeeded = false;
    }
}

void Layer::onFirstRef()
{
    LayerBaseClient::onFirstRef();

    struct FrameQueuedListener : public SurfaceTexture::FrameAvailableListener {
        FrameQueuedListener(Layer* layer) : mLayer(layer) { }
    private:
        wp<Layer> mLayer;
        virtual void onFrameAvailable() {
            sp<Layer> that(mLayer.promote());
            if (that != 0) {
                that->onFrameQueued();
            }
        }
    };

    // Creates a custom BufferQueue for SurfaceTexture to use
    sp<BufferQueue> bq = new SurfaceTextureLayer();
    mSurfaceTexture = new SurfaceTexture(mTextureName, true,
            GL_TEXTURE_EXTERNAL_OES, false, bq);

    mSurfaceTexture->setConsumerUsageBits(getEffectiveUsage(0));
    mSurfaceTexture->setFrameAvailableListener(new FrameQueuedListener(this));
    mSurfaceTexture->setSynchronousMode(true);

#ifdef TARGET_DISABLE_TRIPLE_BUFFERING
#warning "disabling triple buffering"
    mSurfaceTexture->setBufferCountServer(2);
#else
    mSurfaceTexture->setBufferCountServer(3);
#endif
}

Layer::~Layer()
{
    mFlinger->postMessageAsync(
            new SurfaceFlinger::MessageDestroyGLTexture(mTextureName) );
}

void Layer::onFrameQueued() {
    android_atomic_inc(&mQueuedFrames);
    mFlinger->signalLayerUpdate();
}

// called with SurfaceFlinger::mStateLock as soon as the layer is entered
// in the purgatory list
void Layer::onRemoved()
{
    mSurfaceTexture->abandon();
}

void Layer::setName(const String8& name) {
    LayerBase::setName(name);
    mSurfaceTexture->setName(name);
}

void Layer::validateVisibility(const Transform& globalTransform) {
    LayerBase::validateVisibility(globalTransform);

    // This optimization allows the SurfaceTexture to bake in
    // the rotation so hardware overlays can be used
    mSurfaceTexture->setTransformHint(getTransformHint());
}

sp<ISurface> Layer::createSurface()
{
    class BSurface : public BnSurface, public LayerCleaner {
        wp<const Layer> mOwner;
        virtual sp<ISurfaceTexture> getSurfaceTexture() const {
            sp<ISurfaceTexture> res;
            sp<const Layer> that( mOwner.promote() );
            if (that != NULL) {
                res = that->mSurfaceTexture->getBufferQueue();
            }
            return res;
        }
    public:
        BSurface(const sp<SurfaceFlinger>& flinger,
                const sp<Layer>& layer)
            : LayerCleaner(flinger, layer), mOwner(layer) { }
    };
    sp<ISurface> sur(new BSurface(mFlinger, this));
    return sur;
}

wp<IBinder> Layer::getSurfaceTextureBinder() const
{
    return mSurfaceTexture->getBufferQueue()->asBinder();
}

status_t Layer::setBuffers( uint32_t w, uint32_t h,
                            PixelFormat format, uint32_t flags)
{
    // this surfaces pixel format
    PixelFormatInfo info;
    status_t err = getPixelFormatInfo(format, &info);
    if (err) {
        ALOGE("unsupported pixelformat %d", format);
        return err;
    }

    // the display's pixel format
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    uint32_t const maxSurfaceDims = min(
            hw.getMaxTextureSize(), hw.getMaxViewportDims());

    // never allow a surface larger than what our underlying GL implementation
    // can handle.
    if ((uint32_t(w)>maxSurfaceDims) || (uint32_t(h)>maxSurfaceDims)) {
        ALOGE("dimensions too large %u x %u", uint32_t(w), uint32_t(h));
        return BAD_VALUE;
    }

    PixelFormatInfo displayInfo;
    getPixelFormatInfo(hw.getFormat(), &displayInfo);
    const uint32_t hwFlags = hw.getFlags();
    
    mFormat = format;

    mSecure = (flags & ISurfaceComposer::eSecure) ? true : false;
    mProtectedByApp = (flags & ISurfaceComposer::eProtectedByApp) ? true : false;
    mOpaqueLayer = (flags & ISurfaceComposer::eOpaque);
    mCurrentOpacity = getOpacityForFormat(format);

    mSurfaceTexture->setDefaultBufferSize(w, h);
    mSurfaceTexture->setDefaultBufferFormat(format);
    mSurfaceTexture->setConsumerUsageBits(getEffectiveUsage(0));

    int useDither = mFlinger->getUseDithering();
    if (useDither) {
        if (useDither == 2) {
            mNeedsDithering = true;
        }
        else {
            // we use the red index
            int displayRedSize = displayInfo.getSize(PixelFormatInfo::INDEX_RED);
            int layerRedsize = info.getSize(PixelFormatInfo::INDEX_RED);
            mNeedsDithering = (layerRedsize > displayRedSize);
        }
    } else {
        mNeedsDithering = false;
    }

    return NO_ERROR;
}

Rect Layer::computeBufferCrop() const {
    // Start with the SurfaceTexture's buffer crop...
    Rect crop;
    if (!mCurrentCrop.isEmpty()) {
        crop = mCurrentCrop;
    } else  if (mActiveBuffer != NULL){
        crop = Rect(mActiveBuffer->getWidth(), mActiveBuffer->getHeight());
    } else {
        crop = Rect(mTransformedBounds.width(), mTransformedBounds.height());
    }

    // ... then reduce that in the same proportions as the window crop reduces
    // the window size.
    const State& s(drawingState());
    if (!s.active.crop.isEmpty()) {
        // Transform the window crop to match the buffer coordinate system,
        // which means using the inverse of the current transform set on the
        // SurfaceTexture.
        uint32_t invTransform = mCurrentTransform;
        int winWidth = s.active.w;
        int winHeight = s.active.h;
        if (invTransform & NATIVE_WINDOW_TRANSFORM_ROT_90) {
            invTransform ^= NATIVE_WINDOW_TRANSFORM_FLIP_V |
                    NATIVE_WINDOW_TRANSFORM_FLIP_H;
            winWidth = s.active.h;
            winHeight = s.active.w;
        }
        Rect winCrop = s.active.crop.transform(invTransform,
                s.active.w, s.active.h);

        float xScale = float(crop.width()) / float(winWidth);
        float yScale = float(crop.height()) / float(winHeight);
        crop.left += int(ceilf(float(winCrop.left) * xScale));
        crop.top += int(ceilf(float(winCrop.top) * yScale));
        crop.right -= int(ceilf(float(winWidth - winCrop.right) * xScale));
        crop.bottom -= int(ceilf(float(winHeight - winCrop.bottom) * yScale));
    }

    return crop;
}

void Layer::setGeometry(hwc_layer_t* hwcl)
{
    LayerBaseClient::setGeometry(hwcl);

    hwcl->flags &= ~HWC_SKIP_LAYER;

    // we can't do alpha-fade with the hwc HAL
    const State& s(drawingState());
    if (s.alpha < 0xFF) {
        hwcl->flags = HWC_SKIP_LAYER;
    }

    /*
     * Transformations are applied in this order:
     * 1) buffer orientation/flip/mirror
     * 2) state transformation (window manager)
     * 3) layer orientation (screen orientation)
     * mTransform is already the composition of (2) and (3)
     * (NOTE: the matrices are multiplied in reverse order)
     */

    const Transform bufferOrientation(mCurrentTransform);
    const Transform tr(mTransform * bufferOrientation);

    // this gives us only the "orientation" component of the transform
    const uint32_t finalTransform = tr.getOrientation();

    // we can only handle simple transformation
    if (finalTransform & Transform::ROT_INVALID) {
        hwcl->flags = HWC_SKIP_LAYER;
    } else {
        hwcl->transform = finalTransform;
    }

    Rect crop = computeBufferCrop();
    hwcl->sourceCrop.left   = crop.left;
    hwcl->sourceCrop.top    = crop.top;
    hwcl->sourceCrop.right  = crop.right;
    hwcl->sourceCrop.bottom = crop.bottom;
}

void Layer::setPerFrameData(hwc_layer_t* hwcl) {
    const sp<GraphicBuffer>& buffer(mActiveBuffer);
    if (buffer == NULL) {
        // this can happen if the client never drew into this layer yet,
        // or if we ran out of memory. In that case, don't let
        // HWC handle it.
        hwcl->flags |= HWC_SKIP_LAYER;
        hwcl->handle = NULL;
    } else {
        hwcl->handle = buffer->handle;
    }
}

void Layer::onDraw(const Region& clip) const
{
    ATRACE_CALL();

    if (CC_UNLIKELY(mActiveBuffer == 0)) {
        // the texture has not been created yet, this Layer has
        // in fact never been drawn into. This happens frequently with
        // SurfaceView because the WindowManager can't know when the client
        // has drawn the first time.

        // If there is nothing under us, we paint the screen in black, otherwise
        // we just skip this update.

        // figure out if there is something below us
        Region under;
        const SurfaceFlinger::LayerVector& drawingLayers(
                mFlinger->mDrawingState.layersSortedByZ);
        const size_t count = drawingLayers.size();
        for (size_t i=0 ; i<count ; ++i) {
            const sp<LayerBase>& layer(drawingLayers[i]);
            if (layer.get() == static_cast<LayerBase const*>(this))
                break;
            under.orSelf(layer->visibleRegionScreen);
        }
        // if not everything below us is covered, we plug the holes!
        Region holes(clip.subtract(under));
        if (!holes.isEmpty()) {
            clearWithOpenGL(holes, 0, 0, 0, 1);
        }
        return;
    }

    if (!isProtected()) {
        // TODO: we could be more subtle with isFixedSize()
        const bool useFiltering = getFiltering() || needsFiltering() || isFixedSize();

        // Query the texture matrix given our current filtering mode.
        float textureMatrix[16];
        mSurfaceTexture->setFilteringEnabled(useFiltering);
        mSurfaceTexture->getTransformMatrix(textureMatrix);

        // Set things up for texturing.
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, mTextureName);
        GLenum filter = GL_NEAREST;
        if (useFiltering) {
            filter = GL_LINEAR;
        }
        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, filter);
        glMatrixMode(GL_TEXTURE);
        glLoadMatrixf(textureMatrix);
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_EXTERNAL_OES);
    } else {
        glBindTexture(GL_TEXTURE_2D, mFlinger->getProtectedTexName());
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_TEXTURE_EXTERNAL_OES);
        glEnable(GL_TEXTURE_2D);
    }

    drawWithOpenGL(clip);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
}

// As documented in libhardware header, formats in the range
// 0x100 - 0x1FF are specific to the HAL implementation, and
// are known to have no alpha channel
// TODO: move definition for device-specific range into
// hardware.h, instead of using hard-coded values here.
#define HARDWARE_IS_DEVICE_FORMAT(f) ((f) >= 0x100 && (f) <= 0x1FF)

bool Layer::getOpacityForFormat(uint32_t format)
{
    if (HARDWARE_IS_DEVICE_FORMAT(format)) {
        return true;
    }
    PixelFormatInfo info;
    status_t err = getPixelFormatInfo(PixelFormat(format), &info);
    // in case of error (unknown format), we assume no blending
    return (err || info.h_alpha <= info.l_alpha);
}


bool Layer::isOpaque() const
{
    // if we don't have a buffer yet, we're translucent regardless of the
    // layer's opaque flag.
    if (mActiveBuffer == 0) {
        return false;
    }

    // if the layer has the opaque flag, then we're always opaque,
    // otherwise we use the current buffer's format.
    return mOpaqueLayer || mCurrentOpacity;
}

bool Layer::isProtected() const
{
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    return (activeBuffer != 0) &&
            (activeBuffer->getUsage() & GRALLOC_USAGE_PROTECTED);
}

uint32_t Layer::doTransaction(uint32_t flags)
{
    ATRACE_CALL();

    const Layer::State& front(drawingState());
    const Layer::State& temp(currentState());

    const bool sizeChanged = (temp.requested.w != front.requested.w) ||
                             (temp.requested.h != front.requested.h);

    if (sizeChanged) {
        // the size changed, we need to ask our client to request a new buffer
        ALOGD_IF(DEBUG_RESIZE,
                "doTransaction: geometry (layer=%p), scalingMode=%d\n"
                "  current={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n"
                "  drawing={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n",
                this, mCurrentScalingMode,
                temp.active.w, temp.active.h,
                temp.active.crop.left,
                temp.active.crop.top,
                temp.active.crop.right,
                temp.active.crop.bottom,
                temp.active.crop.getWidth(),
                temp.active.crop.getHeight(),
                temp.requested.w, temp.requested.h,
                temp.requested.crop.left,
                temp.requested.crop.top,
                temp.requested.crop.right,
                temp.requested.crop.bottom,
                temp.requested.crop.getWidth(),
                temp.requested.crop.getHeight(),
                front.active.w, front.active.h,
                front.active.crop.left,
                front.active.crop.top,
                front.active.crop.right,
                front.active.crop.bottom,
                front.active.crop.getWidth(),
                front.active.crop.getHeight(),
                front.requested.w, front.requested.h,
                front.requested.crop.left,
                front.requested.crop.top,
                front.requested.crop.right,
                front.requested.crop.bottom,
                front.requested.crop.getWidth(),
                front.requested.crop.getHeight());

        // record the new size, form this point on, when the client request
        // a buffer, it'll get the new size.
        mSurfaceTexture->setDefaultBufferSize(
                temp.requested.w, temp.requested.h);
    }

    if (!isFixedSize()) {

        const bool resizePending = (temp.requested.w != temp.active.w) ||
                                   (temp.requested.h != temp.active.h);

        if (resizePending) {
            // don't let LayerBase::doTransaction update the drawing state
            // if we have a pending resize, unless we are in fixed-size mode.
            // the drawing state will be updated only once we receive a buffer
            // with the correct size.
            //
            // in particular, we want to make sure the clip (which is part
            // of the geometry state) is latched together with the size but is
            // latched immediately when no resizing is involved.

            flags |= eDontUpdateGeometryState;
        }
    }

    return LayerBase::doTransaction(flags);
}

bool Layer::isFixedSize() const {
    return mCurrentScalingMode != NATIVE_WINDOW_SCALING_MODE_FREEZE;
}

bool Layer::isCropped() const {
    return !mCurrentCrop.isEmpty();
}

// ----------------------------------------------------------------------------
// pageflip handling...
// ----------------------------------------------------------------------------

bool Layer::onPreComposition() {
    mRefreshPending = false;
    return mQueuedFrames > 0;
}

void Layer::lockPageFlip(bool& recomputeVisibleRegions)
{
    ATRACE_CALL();

    if (mQueuedFrames > 0) {

        // if we've already called updateTexImage() without going through
        // a composition step, we have to skip this layer at this point
        // because we cannot call updateTeximage() without a corresponding
        // compositionComplete() call.
        // we'll trigger an update in onPreComposition().
        if (mRefreshPending) {
            mPostedDirtyRegion.clear();
            return;
        }

        // Capture the old state of the layer for comparisons later
        const bool oldOpacity = isOpaque();
        sp<GraphicBuffer> oldActiveBuffer = mActiveBuffer;

        // signal another event if we have more frames pending
        if (android_atomic_dec(&mQueuedFrames) > 1) {
            mFlinger->signalLayerUpdate();
        }

        struct Reject : public SurfaceTexture::BufferRejecter {
            Layer::State& front;
            Layer::State& current;
            bool& recomputeVisibleRegions;
            Reject(Layer::State& front, Layer::State& current,
                    bool& recomputeVisibleRegions)
                : front(front), current(current),
                  recomputeVisibleRegions(recomputeVisibleRegions) {
            }

            virtual bool reject(const sp<GraphicBuffer>& buf,
                    const BufferQueue::BufferItem& item) {
                if (buf == NULL) {
                    return false;
                }

                uint32_t bufWidth  = buf->getWidth();
                uint32_t bufHeight = buf->getHeight();

                // check that we received a buffer of the right size
                // (Take the buffer's orientation into account)
                if (item.mTransform & Transform::ROT_90) {
                    swap(bufWidth, bufHeight);
                }


                bool isFixedSize = item.mScalingMode != NATIVE_WINDOW_SCALING_MODE_FREEZE;
                if (front.active != front.requested) {

                    if (isFixedSize ||
                            (bufWidth == front.requested.w &&
                             bufHeight == front.requested.h))
                    {
                        // Here we pretend the transaction happened by updating the
                        // current and drawing states. Drawing state is only accessed
                        // in this thread, no need to have it locked
                        front.active = front.requested;

                        // We also need to update the current state so that
                        // we don't end-up overwriting the drawing state with
                        // this stale current state during the next transaction
                        //
                        // NOTE: We don't need to hold the transaction lock here
                        // because State::active is only accessed from this thread.
                        current.active = front.active;

                        // recompute visible region
                        recomputeVisibleRegions = true;
                    }

                    ALOGD_IF(DEBUG_RESIZE,
                            "lockPageFlip: (layer=%p), buffer (%ux%u, tr=%02x), scalingMode=%d\n"
                            "  drawing={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                            "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n",
                            this, bufWidth, bufHeight, item.mTransform, item.mScalingMode,
                            front.active.w, front.active.h,
                            front.active.crop.left,
                            front.active.crop.top,
                            front.active.crop.right,
                            front.active.crop.bottom,
                            front.active.crop.getWidth(),
                            front.active.crop.getHeight(),
                            front.requested.w, front.requested.h,
                            front.requested.crop.left,
                            front.requested.crop.top,
                            front.requested.crop.right,
                            front.requested.crop.bottom,
                            front.requested.crop.getWidth(),
                            front.requested.crop.getHeight());
                }

                if (!isFixedSize) {
                    if (front.active.w != bufWidth ||
                        front.active.h != bufHeight) {
                        // reject this buffer
                        return true;
                    }
                }
                return false;
            }
        };


        Reject r(mDrawingState, currentState(), recomputeVisibleRegions);

        if (mSurfaceTexture->updateTexImage(&r) < NO_ERROR) {
            // something happened!
            recomputeVisibleRegions = true;
            return;
        }

        // update the active buffer
        mActiveBuffer = mSurfaceTexture->getCurrentBuffer();
        if (mActiveBuffer == NULL) {
            // this can only happen if the very first buffer was rejected.
            return;
        }

        mRefreshPending = true;
        mFrameLatencyNeeded = true;
        if (oldActiveBuffer == NULL) {
             // the first time we receive a buffer, we need to trigger a
             // geometry invalidation.
             mFlinger->invalidateHwcGeometry();
         }

        Rect crop(mSurfaceTexture->getCurrentCrop());
        const uint32_t transform(mSurfaceTexture->getCurrentTransform());
        const uint32_t scalingMode(mSurfaceTexture->getCurrentScalingMode());
        if ((crop != mCurrentCrop) ||
            (transform != mCurrentTransform) ||
            (scalingMode != mCurrentScalingMode))
        {
            mCurrentCrop = crop;
            mCurrentTransform = transform;
            mCurrentScalingMode = scalingMode;
            mFlinger->invalidateHwcGeometry();
        }

        if (oldActiveBuffer != NULL) {
            uint32_t bufWidth  = mActiveBuffer->getWidth();
            uint32_t bufHeight = mActiveBuffer->getHeight();
            if (bufWidth != uint32_t(oldActiveBuffer->width) ||
                bufHeight != uint32_t(oldActiveBuffer->height)) {
                mFlinger->invalidateHwcGeometry();
            }
        }

        mCurrentOpacity = getOpacityForFormat(mActiveBuffer->format);
        if (oldOpacity != isOpaque()) {
            recomputeVisibleRegions = true;
        }

        // FIXME: mPostedDirtyRegion = dirty & bounds
        const Layer::State& front(drawingState());
        mPostedDirtyRegion.set(front.active.w, front.active.h);

        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void Layer::unlockPageFlip(
        const Transform& planeTransform, Region& outDirtyRegion)
{
    ATRACE_CALL();

    Region postedRegion(mPostedDirtyRegion);
    if (!postedRegion.isEmpty()) {
        mPostedDirtyRegion.clear();
        if (!visibleRegionScreen.isEmpty()) {
            // The dirty region is given in the layer's coordinate space
            // transform the dirty region by the surface's transformation
            // and the global transformation.
            const Layer::State& s(drawingState());
            const Transform tr(planeTransform * s.transform);
            postedRegion = tr.transform(postedRegion);

            // At this point, the dirty region is in screen space.
            // Make sure it's constrained by the visible region (which
            // is in screen space as well).
            postedRegion.andSelf(visibleRegionScreen);
            outDirtyRegion.orSelf(postedRegion);
        }
    }
}

void Layer::dump(String8& result, char* buffer, size_t SIZE) const
{
    LayerBaseClient::dump(result, buffer, SIZE);

    sp<const GraphicBuffer> buf0(mActiveBuffer);
    uint32_t w0=0, h0=0, s0=0, f0=0;
    if (buf0 != 0) {
        w0 = buf0->getWidth();
        h0 = buf0->getHeight();
        s0 = buf0->getStride();
        f0 = buf0->format;
    }
    snprintf(buffer, SIZE,
            "      "
            "format=%2d, activeBuffer=[%4ux%4u:%4u,%3X],"
            " transform-hint=0x%02x, queued-frames=%d, mRefreshPending=%d\n",
            mFormat, w0, h0, s0,f0,
            getTransformHint(), mQueuedFrames, mRefreshPending);

    result.append(buffer);

    if (mSurfaceTexture != 0) {
        mSurfaceTexture->dump(result, "            ", buffer, SIZE);
    }
}

void Layer::dumpStats(String8& result, char* buffer, size_t SIZE) const
{
    LayerBaseClient::dumpStats(result, buffer, SIZE);
    const size_t o = mFrameLatencyOffset;
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    const nsecs_t period = hw.getRefreshPeriod();
    result.appendFormat("%lld\n", period);
    for (size_t i=0 ; i<128 ; i++) {
        const size_t index = (o+i) % 128;
        const nsecs_t time_app   = mFrameStats[index].timestamp;
        const nsecs_t time_set   = mFrameStats[index].set;
        const nsecs_t time_vsync = mFrameStats[index].vsync;
        result.appendFormat("%lld\t%lld\t%lld\n",
                time_app,
                time_vsync,
                time_set);
    }
    result.append("\n");
}

void Layer::clearStats()
{
    LayerBaseClient::clearStats();
    memset(mFrameStats, 0, sizeof(mFrameStats));
}

uint32_t Layer::getEffectiveUsage(uint32_t usage) const
{
    // TODO: should we do something special if mSecure is set?
    if (mProtectedByApp) {
        // need a hardware-protected path to external video sink
        usage |= GraphicBuffer::USAGE_PROTECTED;
    }
    usage |= GraphicBuffer::USAGE_HW_COMPOSER;
    return usage;
}

uint32_t Layer::getTransformHint() const {
    uint32_t orientation = 0;
    if (!mFlinger->mDebugDisableTransformHint) {
        orientation = getPlaneOrientation();
        if (orientation & Transform::ROT_INVALID) {
            orientation = 0;
        }
    }
    return orientation;
}

// ---------------------------------------------------------------------------


}; // namespace android
