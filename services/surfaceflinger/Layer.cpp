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
#include "DisplayDevice.h"
#include "GLExtensions.h"
#include "Layer.h"
#include "SurfaceFlinger.h"
#include "SurfaceTextureLayer.h"

#include "DisplayHardware/HWComposer.h"

#ifdef QCOM_BSP
#include <gralloc_priv.h>
#endif

#define DEBUG_RESIZE    0

namespace android {

// ---------------------------------------------------------------------------

int32_t Layer::sSequence = 1;

Layer::Layer(SurfaceFlinger* flinger, const sp<Client>& client,
        const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    :   contentDirty(false),
        sequence(uint32_t(android_atomic_inc(&sSequence))),
        mFlinger(flinger),
        mTextureName(-1U),
        mPremultipliedAlpha(true),
        mName("unnamed"),
        mDebug(false),
        mFormat(PIXEL_FORMAT_NONE),
        mGLExtensions(GLExtensions::getInstance()),
        mOpaqueLayer(true),
        mTransactionFlags(0),
        mQueuedFrames(0),
        mCurrentTransform(0),
        mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
        mCurrentOpacity(true),
        mRefreshPending(false),
        mFrameLatencyNeeded(false),
        mFiltering(false),
        mNeedsFiltering(false),
        mSecure(false),
        mProtectedByApp(false),
        mHasSurface(false),
        mClientRef(client)
{
    mCurrentCrop.makeInvalid();
    glGenTextures(1, &mTextureName);

    uint32_t layerFlags = 0;
    if (flags & ISurfaceComposerClient::eHidden)
        layerFlags = layer_state_t::eLayerHidden;

    if (flags & ISurfaceComposerClient::eNonPremultiplied)
        mPremultipliedAlpha = false;

    mName = name;

    mCurrentState.active.w = w;
    mCurrentState.active.h = h;
    mCurrentState.active.crop.makeInvalid();
    mCurrentState.z = 0;
    mCurrentState.alpha = 0xFF;
    mCurrentState.layerStack = 0;
    mCurrentState.flags = layerFlags;
    mCurrentState.sequence = 0;
    mCurrentState.transform.set(0, 0);
    mCurrentState.requested = mCurrentState.active;

    // drawing state & current state are identical
    mDrawingState = mCurrentState;
}

void Layer::onFirstRef()
{
    // Creates a custom BufferQueue for SurfaceFlingerConsumer to use
    sp<BufferQueue> bq = new SurfaceTextureLayer(mFlinger);
    mSurfaceFlingerConsumer = new SurfaceFlingerConsumer(mTextureName, true,
            GL_TEXTURE_EXTERNAL_OES, false, bq);

    mSurfaceFlingerConsumer->setConsumerUsageBits(getEffectiveUsage(0));
    mSurfaceFlingerConsumer->setFrameAvailableListener(this);
    mSurfaceFlingerConsumer->setSynchronousMode(true);
    mSurfaceFlingerConsumer->setName(mName);

#ifdef TARGET_DISABLE_TRIPLE_BUFFERING
#warning "disabling triple buffering"
    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(2);
#else
    mSurfaceFlingerConsumer->setDefaultMaxBufferCount(3);
#endif

    const sp<const DisplayDevice> hw(mFlinger->getDefaultDisplayDevice());
    updateTransformHint(hw);
}

Layer::~Layer() {
    sp<Client> c(mClientRef.promote());
    if (c != 0) {
        c->detachLayer(this);
    }
    mFlinger->deleteTextureAsync(mTextureName);
}

// ---------------------------------------------------------------------------
// callbacks
// ---------------------------------------------------------------------------

void Layer::onLayerDisplayed(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface* layer) {
    if (layer) {
        layer->onDisplayed();
        mSurfaceFlingerConsumer->setReleaseFence(layer->getAndResetReleaseFence());
    }
}

void Layer::onFrameAvailable() {
    android_atomic_inc(&mQueuedFrames);
    mFlinger->signalLayerUpdate();
}

// called with SurfaceFlinger::mStateLock from the drawing thread after
// the layer has been remove from the current state list (and just before
// it's removed from the drawing state list)
void Layer::onRemoved() {
    mSurfaceFlingerConsumer->abandon();
}

// ---------------------------------------------------------------------------
// set-up
// ---------------------------------------------------------------------------

String8 Layer::getName() const {
    return mName;
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

    uint32_t const maxSurfaceDims = min(
            mFlinger->getMaxTextureSize(), mFlinger->getMaxViewportDims());

    // never allow a surface larger than what our underlying GL implementation
    // can handle.
    if ((uint32_t(w)>maxSurfaceDims) || (uint32_t(h)>maxSurfaceDims)) {
        ALOGE("dimensions too large %u x %u", uint32_t(w), uint32_t(h));
        return BAD_VALUE;
    }

    mFormat = format;

    mSecure = (flags & ISurfaceComposerClient::eSecure) ? true : false;
    mProtectedByApp = (flags & ISurfaceComposerClient::eProtectedByApp) ? true : false;
    mOpaqueLayer = (flags & ISurfaceComposerClient::eOpaque);
    mCurrentOpacity = getOpacityForFormat(format);

    mSurfaceFlingerConsumer->setDefaultBufferSize(w, h);
    mSurfaceFlingerConsumer->setDefaultBufferFormat(format);
    mSurfaceFlingerConsumer->setConsumerUsageBits(getEffectiveUsage(0));

    return NO_ERROR;
}

sp<IBinder> Layer::getHandle() {
    Mutex::Autolock _l(mLock);

    LOG_ALWAYS_FATAL_IF(mHasSurface,
            "Layer::getHandle() has already been called");

    mHasSurface = true;

    /*
     * The layer handle is just a BBinder object passed to the client
     * (remote process) -- we don't keep any reference on our side such that
     * the dtor is called when the remote side let go of its reference.
     *
     * LayerCleaner ensures that mFlinger->onLayerDestroyed() is called for
     * this layer when the handle is destroyed.
     */

    class Handle : public BBinder, public LayerCleaner {
        wp<const Layer> mOwner;
    public:
        Handle(const sp<SurfaceFlinger>& flinger, const sp<Layer>& layer)
            : LayerCleaner(flinger, layer), mOwner(layer) {
        }
    };

    return new Handle(mFlinger, this);
}

sp<BufferQueue> Layer::getBufferQueue() const {
    return mSurfaceFlingerConsumer->getBufferQueue();
}

//virtual sp<IGraphicBufferProducer> getSurfaceTexture() const {
//    sp<IGraphicBufferProducer> res;
//    sp<const Layer> that( mOwner.promote() );
//    if (that != NULL) {
//        res = that->mSurfaceFlingerConsumer->getBufferQueue();
//    }
//    return res;
//}

// ---------------------------------------------------------------------------
// h/w composer set-up
// ---------------------------------------------------------------------------

Rect Layer::getContentCrop() const {
    // this is the crop rectangle that applies to the buffer
    // itself (as opposed to the window)
    Rect crop;
    if (!mCurrentCrop.isEmpty()) {
        // if the buffer crop is defined, we use that
        crop = mCurrentCrop;
    } else if (mActiveBuffer != NULL) {
        // otherwise we use the whole buffer
        crop = mActiveBuffer->getBounds();
    } else {
        // if we don't have a buffer yet, we use an empty/invalid crop
        crop.makeInvalid();
    }
    return crop;
}

uint32_t Layer::getContentTransform() const {
    return mCurrentTransform;
}

Rect Layer::computeBounds() const {
    const Layer::State& s(drawingState());
    Rect win(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        win.intersect(s.active.crop, &win);
    }
    return win;
}

Rect Layer::computeCrop(const sp<const DisplayDevice>& hw) const {
    /*
     * The way we compute the crop (aka. texture coordinates when we have a
     * Layer) produces a different output from the GL code in
     * drawWithOpenGL() due to HWC being limited to integers. The difference
     * can be large if getContentTransform() contains a large scale factor.
     * See comments in drawWithOpenGL() for more details.
     */

    // the content crop is the area of the content that gets scaled to the
    // layer's size.
    Rect crop(getContentCrop());

    // the active.crop is the area of the window that gets cropped, but not
    // scaled in any ways.
    const State& s(drawingState());

    // apply the projection's clipping to the window crop in
    // layerstack space, and convert-back to layer space.
    // if there are no window scaling (or content scaling) involved,
    // this operation will map to full pixels in the buffer.
    // NOTE: should we revert to GL composition if a scaling is involved
    // since it cannot be represented in the HWC API?
    Rect activeCrop(s.transform.transform(s.active.crop));
    activeCrop.intersect(hw->getViewport(), &activeCrop);
    activeCrop = s.transform.inverse().transform(activeCrop);

    // paranoia: make sure the window-crop is constrained in the
    // window's bounds
    activeCrop.intersect(Rect(s.active.w, s.active.h), &activeCrop);

    if (!activeCrop.isEmpty()) {
        // Transform the window crop to match the buffer coordinate system,
        // which means using the inverse of the current transform set on the
        // SurfaceFlingerConsumer.
        uint32_t invTransform = getContentTransform();
        int winWidth = s.active.w;
        int winHeight = s.active.h;
        if (invTransform & NATIVE_WINDOW_TRANSFORM_ROT_90) {
            invTransform ^= NATIVE_WINDOW_TRANSFORM_FLIP_V |
                    NATIVE_WINDOW_TRANSFORM_FLIP_H;
            winWidth = s.active.h;
            winHeight = s.active.w;
        }
        const Rect winCrop = activeCrop.transform(
                invTransform, s.active.w, s.active.h);

        // the code below essentially performs a scaled intersection
        // of crop and winCrop
        float xScale = float(crop.width()) / float(winWidth);
        float yScale = float(crop.height()) / float(winHeight);

        int insetL = int(ceilf( winCrop.left                * xScale));
        int insetT = int(ceilf( winCrop.top                 * yScale));
        int insetR = int(ceilf((winWidth  - winCrop.right ) * xScale));
        int insetB = int(ceilf((winHeight - winCrop.bottom) * yScale));

        crop.left   += insetL;
        crop.top    += insetT;
        crop.right  -= insetR;
        crop.bottom -= insetB;
    }
    return crop;
}

void Layer::setGeometry(
    const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer)
{
    layer.setDefaultState();

    // enable this layer
    layer.setSkip(false);

    if (isSecure() && !hw->isSecure()) {
        layer.setSkip(true);
    }

    // this gives us only the "orientation" component of the transform
    const State& s(drawingState());
    if (!isOpaque() || s.alpha != 0xFF) {
        layer.setBlending(mPremultipliedAlpha ?
                HWC_BLENDING_PREMULT :
                HWC_BLENDING_COVERAGE);
    }

    // apply the layer's transform, followed by the display's global transform
    // here we're guaranteed that the layer's transform preserves rects
    Rect frame(s.transform.transform(computeBounds()));
    frame.intersect(hw->getViewport(), &frame);
    const Transform& tr(hw->getTransform());
    layer.setFrame(tr.transform(frame));
#ifdef QCOM_BSP
    // set dest_rect to frame buffer width and height, if external_only flag
    // for the layer is enabled.
    if(isExtOnly()) {
        uint32_t w = hw->getWidth();
        uint32_t h = hw->getHeight();
        layer.setFrame(Rect(w,h));
    }
#endif
    layer.setCrop(computeCrop(hw));
    layer.setPlaneAlpha(s.alpha);

    /*
     * Transformations are applied in this order:
     * 1) buffer orientation/flip/mirror
     * 2) state transformation (window manager)
     * 3) layer orientation (screen orientation)
     * (NOTE: the matrices are multiplied in reverse order)
     */

    const Transform bufferOrientation(mCurrentTransform);
    const Transform transform(tr * s.transform * bufferOrientation);

    // this gives us only the "orientation" component of the transform
    const uint32_t orientation = transform.getOrientation();
    if (orientation & Transform::ROT_INVALID) {
        // we can only handle simple transformation
        layer.setSkip(true);
    } else {
        layer.setTransform(orientation);
    }
}

void Layer::setPerFrameData(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
    // we have to set the visible region on every frame because
    // we currently free it during onLayerDisplayed(), which is called
    // after HWComposer::commit() -- every frame.
    // Apply this display's projection's viewport to the visible region
    // before giving it to the HWC HAL.
    const Transform& tr = hw->getTransform();
    Region visible = tr.transform(visibleRegion.intersect(hw->getViewport()));
    layer.setVisibleRegionScreen(visible);

    // NOTE: buffer can be NULL if the client never drew into this
    // layer yet, or if we ran out of memory
    layer.setBuffer(mActiveBuffer);
}

void Layer::setAcquireFence(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
    int fenceFd = -1;

    // TODO: there is a possible optimization here: we only need to set the
    // acquire fence the first time a new buffer is acquired on EACH display.

    if (layer.getCompositionType() == HWC_OVERLAY) {
        sp<Fence> fence = mSurfaceFlingerConsumer->getCurrentFence();
        if (fence->isValid()) {
            fenceFd = fence->dup();
            if (fenceFd == -1) {
                ALOGW("failed to dup layer fence, skipping sync: %d", errno);
            }
        }
    }
    layer.setAcquireFenceFd(fenceFd);
}

// ---------------------------------------------------------------------------
// drawing...
// ---------------------------------------------------------------------------

void Layer::draw(const sp<const DisplayDevice>& hw, const Region& clip) const {
    onDraw(hw, clip);
}

void Layer::draw(const sp<const DisplayDevice>& hw) {
    onDraw( hw, Region(hw->bounds()) );
}

void Layer::onDraw(const sp<const DisplayDevice>& hw, const Region& clip) const
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
            const sp<Layer>& layer(drawingLayers[i]);
            if (layer.get() == static_cast<Layer const*>(this))
                break;
            under.orSelf( hw->getTransform().transform(layer->visibleRegion) );
        }
        // if not everything below us is covered, we plug the holes!
        Region holes(clip.subtract(under));
        if (!holes.isEmpty()) {
            clearWithOpenGL(hw, holes, 0, 0, 0, 1);
        }
        return;
    }

    // Bind the current buffer to the GL texture, and wait for it to be
    // ready for us to draw into.
    status_t err = mSurfaceFlingerConsumer->bindTextureImage();
    if (err != NO_ERROR) {
        ALOGW("onDraw: bindTextureImage failed (err=%d)", err);
        // Go ahead and draw the buffer anyway; no matter what we do the screen
        // is probably going to have something visibly wrong.
    }

    bool blackOutLayer = isProtected() || (isSecure() && !hw->isSecure());

    if (!blackOutLayer) {
        // TODO: we could be more subtle with isFixedSize()
        const bool useFiltering = getFiltering() || needsFiltering(hw) || isFixedSize();

        // Query the texture matrix given our current filtering mode.
        float textureMatrix[16];
        mSurfaceFlingerConsumer->setFilteringEnabled(useFiltering);
        mSurfaceFlingerConsumer->getTransformMatrix(textureMatrix);

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

    drawWithOpenGL(hw, clip);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
}


void Layer::clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip,
        GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) const
{
    const uint32_t fbHeight = hw->getHeight();
    glColor4f(red,green,blue,alpha);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    LayerMesh mesh;
    computeGeometry(hw, &mesh);

    glVertexPointer(2, GL_FLOAT, 0, mesh.getVertices());
    glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.getVertexCount());
}

void Layer::clearWithOpenGL(
        const sp<const DisplayDevice>& hw, const Region& clip) const {
    clearWithOpenGL(hw, clip, 0,0,0,0);
}

void Layer::drawWithOpenGL(
        const sp<const DisplayDevice>& hw, const Region& clip) const {
    const uint32_t fbHeight = hw->getHeight();
    const State& s(drawingState());

    GLenum src = mPremultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
    if (CC_UNLIKELY(s.alpha < 0xFF)) {
        const GLfloat alpha = s.alpha * (1.0f/255.0f);
        if (mPremultipliedAlpha) {
            glColor4f(alpha, alpha, alpha, alpha);
        } else {
            glColor4f(1, 1, 1, alpha);
        }
        glEnable(GL_BLEND);
        glBlendFunc(src, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glColor4f(1, 1, 1, 1);
        glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        if (!isOpaque()) {
            glEnable(GL_BLEND);
            glBlendFunc(src, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
    }

    LayerMesh mesh;
    computeGeometry(hw, &mesh);

    // TODO: we probably want to generate the texture coords with the mesh
    // here we assume that we only have 4 vertices

    struct TexCoords {
        GLfloat u;
        GLfloat v;
    };


    /*
     * NOTE: the way we compute the texture coordinates here produces
     * different results than when we take the HWC path -- in the later case
     * the "source crop" is rounded to texel boundaries.
     * This can produce significantly different results when the texture
     * is scaled by a large amount.
     *
     * The GL code below is more logical (imho), and the difference with
     * HWC is due to a limitation of the HWC API to integers -- a question
     * is suspend is wether we should ignore this problem or revert to
     * GL composition when a buffer scaling is applied (maybe with some
     * minimal value)? Or, we could make GL behave like HWC -- but this feel
     * like more of a hack.
     */
    const Rect win(computeBounds());

    GLfloat left   = GLfloat(win.left)   / GLfloat(s.active.w);
    GLfloat top    = GLfloat(win.top)    / GLfloat(s.active.h);
    GLfloat right  = GLfloat(win.right)  / GLfloat(s.active.w);
    GLfloat bottom = GLfloat(win.bottom) / GLfloat(s.active.h);

    TexCoords texCoords[4];
    texCoords[0].u = left;
    texCoords[0].v = top;
    texCoords[1].u = left;
    texCoords[1].v = bottom;
    texCoords[2].u = right;
    texCoords[2].v = bottom;
    texCoords[3].u = right;
    texCoords[3].v = top;
    for (int i = 0; i < 4; i++) {
        texCoords[i].v = 1.0f - texCoords[i].v;
    }

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glVertexPointer(2, GL_FLOAT, 0, mesh.getVertices());
    glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.getVertexCount());

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_BLEND);
}

void Layer::setFiltering(bool filtering) {
    mFiltering = filtering;
}

bool Layer::getFiltering() const {
    return mFiltering;
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

// ----------------------------------------------------------------------------
// local state
// ----------------------------------------------------------------------------

void Layer::computeGeometry(const sp<const DisplayDevice>& hw, LayerMesh* mesh) const
{
    const Layer::State& s(drawingState());
    const Transform tr(hw->getTransform() * s.transform);
    const uint32_t hw_h = hw->getHeight();
    Rect win(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        win.intersect(s.active.crop, &win);
    }
    if (mesh) {
        tr.transform(mesh->mVertices[0], win.left,  win.top);
        tr.transform(mesh->mVertices[1], win.left,  win.bottom);
        tr.transform(mesh->mVertices[2], win.right, win.bottom);
        tr.transform(mesh->mVertices[3], win.right, win.top);
        for (size_t i=0 ; i<4 ; i++) {
            mesh->mVertices[i][1] = hw_h - mesh->mVertices[i][1];
        }
    }
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

bool Layer::isFixedSize() const {
    return mCurrentScalingMode != NATIVE_WINDOW_SCALING_MODE_FREEZE;
}

bool Layer::isCropped() const {
    return !mCurrentCrop.isEmpty();
}

bool Layer::needsFiltering(const sp<const DisplayDevice>& hw) const {
    return mNeedsFiltering || hw->needsFiltering();
}

void Layer::setVisibleRegion(const Region& visibleRegion) {
    // always called from main thread
    this->visibleRegion = visibleRegion;
}

void Layer::setCoveredRegion(const Region& coveredRegion) {
    // always called from main thread
    this->coveredRegion = coveredRegion;
}

void Layer::setVisibleNonTransparentRegion(const Region&
        setVisibleNonTransparentRegion) {
    // always called from main thread
    this->visibleNonTransparentRegion = setVisibleNonTransparentRegion;
}

// ----------------------------------------------------------------------------
// transaction
// ----------------------------------------------------------------------------

uint32_t Layer::doTransaction(uint32_t flags) {
    ATRACE_CALL();

    const Layer::State& front(drawingState());
    const Layer::State& temp(currentState());

    const bool sizeChanged = (temp.requested.w != front.requested.w) ||
                             (temp.requested.h != front.requested.h);

    if (sizeChanged) {
        // the size changed, we need to ask our client to request a new buffer
        ALOGD_IF(DEBUG_RESIZE,
                "doTransaction: geometry (layer=%p '%s'), tr=%02x, scalingMode=%d\n"
                "  current={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n"
                "  drawing={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n",
                this, (const char*) getName(), mCurrentTransform, mCurrentScalingMode,
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
        mSurfaceFlingerConsumer->setDefaultBufferSize(
                temp.requested.w, temp.requested.h);
    }

    if (!isFixedSize()) {

        const bool resizePending = (temp.requested.w != temp.active.w) ||
                                   (temp.requested.h != temp.active.h);

        if (resizePending) {
            // don't let Layer::doTransaction update the drawing state
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

    // always set active to requested, unless we're asked not to
    // this is used by Layer, which special cases resizes.
    if (flags & eDontUpdateGeometryState)  {
    } else {
        Layer::State& editTemp(currentState());
        editTemp.active = temp.requested;
    }

    if (front.active != temp.active) {
        // invalidate and recompute the visible regions if needed
        flags |= Layer::eVisibleRegion;
    }

    if (temp.sequence != front.sequence) {
        // invalidate and recompute the visible regions if needed
        flags |= eVisibleRegion;
        this->contentDirty = true;

        // we may use linear filtering, if the matrix scales us
        const uint8_t type = temp.transform.getType();
        mNeedsFiltering = (!temp.transform.preserveRects() ||
                (type >= Transform::SCALE));
    }

    // Commit the transaction
    commitTransaction();
    return flags;
}

void Layer::commitTransaction() {
    mDrawingState = mCurrentState;
}

uint32_t Layer::getTransactionFlags(uint32_t flags) {
    return android_atomic_and(~flags, &mTransactionFlags) & flags;
}

uint32_t Layer::setTransactionFlags(uint32_t flags) {
    return android_atomic_or(flags, &mTransactionFlags);
}

bool Layer::setPosition(float x, float y) {
    if (mCurrentState.transform.tx() == x && mCurrentState.transform.ty() == y)
        return false;
    mCurrentState.sequence++;
    mCurrentState.transform.set(x, y);
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setLayer(uint32_t z) {
    if (mCurrentState.z == z)
        return false;
    mCurrentState.sequence++;
    mCurrentState.z = z;
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setSize(uint32_t w, uint32_t h) {
    if (mCurrentState.requested.w == w && mCurrentState.requested.h == h)
        return false;
    mCurrentState.requested.w = w;
    mCurrentState.requested.h = h;
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setAlpha(uint8_t alpha) {
    if (mCurrentState.alpha == alpha)
        return false;
    mCurrentState.sequence++;
    mCurrentState.alpha = alpha;
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setMatrix(const layer_state_t::matrix22_t& matrix) {
    mCurrentState.sequence++;
    mCurrentState.transform.set(
            matrix.dsdx, matrix.dsdy, matrix.dtdx, matrix.dtdy);
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setTransparentRegionHint(const Region& transparent) {
    mCurrentState.requestedTransparentRegion = transparent;
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setFlags(uint8_t flags, uint8_t mask) {
    const uint32_t newFlags = (mCurrentState.flags & ~mask) | (flags & mask);
    if (mCurrentState.flags == newFlags)
        return false;
    mCurrentState.sequence++;
    mCurrentState.flags = newFlags;
    setTransactionFlags(eTransactionNeeded);
    return true;
}
bool Layer::setCrop(const Rect& crop) {
    if (mCurrentState.requested.crop == crop)
        return false;
    mCurrentState.sequence++;
    mCurrentState.requested.crop = crop;
    setTransactionFlags(eTransactionNeeded);
    return true;
}

bool Layer::setLayerStack(uint32_t layerStack) {
    if (mCurrentState.layerStack == layerStack)
        return false;
    mCurrentState.sequence++;
    mCurrentState.layerStack = layerStack;
    setTransactionFlags(eTransactionNeeded);
    return true;
}

// ----------------------------------------------------------------------------
// pageflip handling...
// ----------------------------------------------------------------------------

bool Layer::onPreComposition() {
    mRefreshPending = false;
    return mQueuedFrames > 0;
}

void Layer::onPostComposition() {
    if (mFrameLatencyNeeded) {
        nsecs_t desiredPresentTime = mSurfaceFlingerConsumer->getTimestamp();
        mFrameTracker.setDesiredPresentTime(desiredPresentTime);

        sp<Fence> frameReadyFence = mSurfaceFlingerConsumer->getCurrentFence();
        if (frameReadyFence->isValid()) {
            mFrameTracker.setFrameReadyFence(frameReadyFence);
        } else {
            // There was no fence for this frame, so assume that it was ready
            // to be presented at the desired present time.
            mFrameTracker.setFrameReadyTime(desiredPresentTime);
        }

        const HWComposer& hwc = mFlinger->getHwComposer();
        sp<Fence> presentFence = hwc.getDisplayFence(HWC_DISPLAY_PRIMARY);
        if (presentFence->isValid()) {
            mFrameTracker.setActualPresentFence(presentFence);
        } else {
            // The HWC doesn't support present fences, so use the refresh
            // timestamp instead.
            nsecs_t presentTime = hwc.getRefreshTimestamp(HWC_DISPLAY_PRIMARY);
            mFrameTracker.setActualPresentTime(presentTime);
        }

        mFrameTracker.advanceFrame();
        mFrameLatencyNeeded = false;
    }
}

bool Layer::isVisible() const {
    const Layer::State& s(mDrawingState);
    return !(s.flags & layer_state_t::eLayerHidden) && s.alpha
            && (mActiveBuffer != NULL);
}

Region Layer::latchBuffer(bool& recomputeVisibleRegions)
{
    ATRACE_CALL();

    Region outDirtyRegion;
    if (mQueuedFrames > 0) {

        // if we've already called updateTexImage() without going through
        // a composition step, we have to skip this layer at this point
        // because we cannot call updateTeximage() without a corresponding
        // compositionComplete() call.
        // we'll trigger an update in onPreComposition().
        if (mRefreshPending) {
            return outDirtyRegion;
        }

        // Capture the old state of the layer for comparisons later
        const bool oldOpacity = isOpaque();
        sp<GraphicBuffer> oldActiveBuffer = mActiveBuffer;

        // signal another event if we have more frames pending
        if (android_atomic_dec(&mQueuedFrames) > 1) {
            mFlinger->signalLayerUpdate();
        }

        struct Reject : public SurfaceFlingerConsumer::BufferRejecter {
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
                            "latchBuffer/reject: buffer (%ux%u, tr=%02x), scalingMode=%d\n"
                            "  drawing={ active   ={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }\n"
                            "            requested={ wh={%4u,%4u} crop={%4d,%4d,%4d,%4d} (%4d,%4d) }}\n",
                            bufWidth, bufHeight, item.mTransform, item.mScalingMode,
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

                // if the transparent region has changed (this test is
                // conservative, but that's fine, worst case we're doing
                // a bit of extra work), we latch the new one and we
                // trigger a visible-region recompute.
                if (!front.activeTransparentRegion.isTriviallyEqual(
                        front.requestedTransparentRegion)) {
                    front.activeTransparentRegion = front.requestedTransparentRegion;

                    // We also need to update the current state so that
                    // we don't end-up overwriting the drawing state with
                    // this stale current state during the next transaction
                    //
                    // NOTE: We don't need to hold the transaction lock here
                    // because State::active is only accessed from this thread.
                    current.activeTransparentRegion = front.activeTransparentRegion;

                    // recompute visible region
                    recomputeVisibleRegions = true;
                }

                return false;
            }
        };


        Reject r(mDrawingState, currentState(), recomputeVisibleRegions);

        if (mSurfaceFlingerConsumer->updateTexImage(&r) != NO_ERROR) {
            // something happened!
            recomputeVisibleRegions = true;
            return outDirtyRegion;
        }

        // update the active buffer
        mActiveBuffer = mSurfaceFlingerConsumer->getCurrentBuffer();
        if (mActiveBuffer == NULL) {
            // this can only happen if the very first buffer was rejected.
            return outDirtyRegion;
        }

        mRefreshPending = true;
        mFrameLatencyNeeded = true;
        if (oldActiveBuffer == NULL) {
             // the first time we receive a buffer, we need to trigger a
             // geometry invalidation.
            recomputeVisibleRegions = true;
         }

        Rect crop(mSurfaceFlingerConsumer->getCurrentCrop());
        const uint32_t transform(mSurfaceFlingerConsumer->getCurrentTransform());
        const uint32_t scalingMode(mSurfaceFlingerConsumer->getCurrentScalingMode());
        if ((crop != mCurrentCrop) ||
            (transform != mCurrentTransform) ||
            (scalingMode != mCurrentScalingMode))
        {
            mCurrentCrop = crop;
            mCurrentTransform = transform;
            mCurrentScalingMode = scalingMode;
            recomputeVisibleRegions = true;
        }

        if (oldActiveBuffer != NULL) {
            uint32_t bufWidth  = mActiveBuffer->getWidth();
            uint32_t bufHeight = mActiveBuffer->getHeight();
            if (bufWidth != uint32_t(oldActiveBuffer->width) ||
                bufHeight != uint32_t(oldActiveBuffer->height)) {
                recomputeVisibleRegions = true;
            }
        }

        mCurrentOpacity = getOpacityForFormat(mActiveBuffer->format);
        if (oldOpacity != isOpaque()) {
            recomputeVisibleRegions = true;
        }

        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterx(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // FIXME: postedRegion should be dirty & bounds
        const Layer::State& front(drawingState());
        Region dirtyRegion(Rect(front.active.w, front.active.h));

        // transform the dirty region to window-manager space
        outDirtyRegion = (front.transform.transform(dirtyRegion));
    }
    return outDirtyRegion;
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

void Layer::updateTransformHint(const sp<const DisplayDevice>& hw) const {
    uint32_t orientation = 0;
    if (!mFlinger->mDebugDisableTransformHint) {
        // The transform hint is used to improve performance, but we can
        // only have a single transform hint, it cannot
        // apply to all displays.
        const Transform& planeTransform(hw->getTransform());
        orientation = planeTransform.getOrientation();
        if (orientation & Transform::ROT_INVALID) {
            orientation = 0;
        }
    }
    mSurfaceFlingerConsumer->setTransformHint(orientation);
}

// ----------------------------------------------------------------------------
// debugging
// ----------------------------------------------------------------------------

void Layer::dump(String8& result, char* buffer, size_t SIZE) const
{
    const Layer::State& s(drawingState());

    snprintf(buffer, SIZE,
            "+ %s %p (%s)\n",
            getTypeId(), this, getName().string());
    result.append(buffer);

    s.activeTransparentRegion.dump(result, "transparentRegion");
    visibleRegion.dump(result, "visibleRegion");
    sp<Client> client(mClientRef.promote());

    snprintf(buffer, SIZE,
            "      "
            "layerStack=%4d, z=%9d, pos=(%g,%g), size=(%4d,%4d), crop=(%4d,%4d,%4d,%4d), "
            "isOpaque=%1d, invalidate=%1d, "
            "alpha=0x%02x, flags=0x%08x, tr=[%.2f, %.2f][%.2f, %.2f]\n"
            "      client=%p\n",
            s.layerStack, s.z, s.transform.tx(), s.transform.ty(), s.active.w, s.active.h,
            s.active.crop.left, s.active.crop.top,
            s.active.crop.right, s.active.crop.bottom,
            isOpaque(), contentDirty,
            s.alpha, s.flags,
            s.transform[0][0], s.transform[0][1],
            s.transform[1][0], s.transform[1][1],
            client.get());
    result.append(buffer);

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
            " queued-frames=%d, mRefreshPending=%d\n",
            mFormat, w0, h0, s0,f0,
            mQueuedFrames, mRefreshPending);

    result.append(buffer);

    if (mSurfaceFlingerConsumer != 0) {
        mSurfaceFlingerConsumer->dump(result, "            ", buffer, SIZE);
    }
}


void Layer::shortDump(String8& result, char* scratch, size_t size) const {
    Layer::dump(result, scratch, size);
}

void Layer::dumpStats(String8& result, char* buffer, size_t SIZE) const {
    mFrameTracker.dump(result);
}

void Layer::clearStats() {
    mFrameTracker.clear();
}

// ---------------------------------------------------------------------------

Layer::LayerCleaner::LayerCleaner(const sp<SurfaceFlinger>& flinger,
        const sp<Layer>& layer)
    : mFlinger(flinger), mLayer(layer) {
}

Layer::LayerCleaner::~LayerCleaner() {
    // destroy client resources
    mFlinger->onLayerDestroyed(mLayer);
}

#ifdef QCOM_BSP
bool Layer::isExtOnly() const
{
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if (activeBuffer != 0) {
        uint32_t usage = activeBuffer->getUsage();
        if(usage & GRALLOC_USAGE_PRIVATE_EXTERNAL_ONLY)
            return true;
    }
    return false;
}

bool Layer::isIntOnly() const
{
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if (activeBuffer != 0) {
        uint32_t usage = activeBuffer->getUsage();
        if(usage & GRALLOC_USAGE_PRIVATE_INTERNAL_ONLY)
            return true;
    }
    return false;
}
#endif
// ---------------------------------------------------------------------------


}; // namespace android
