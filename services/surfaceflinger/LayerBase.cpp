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
#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <hardware/hardware.h>
#ifdef QCOMHW
#include <gralloc_priv.h>
#endif

#include "clz.h"
#include "LayerBase.h"
#include "SurfaceFlinger.h"
#include "DisplayHardware/DisplayHardware.h"

namespace android {

//Helper
bool isLayerExternalOnly(const sp<Layer>& layer);

// ---------------------------------------------------------------------------

int32_t LayerBase::sSequence = 1;

LayerBase::LayerBase(SurfaceFlinger* flinger, DisplayID display)
    : dpy(display), contentDirty(false),
      sequence(uint32_t(android_atomic_inc(&sSequence))),
      mFlinger(flinger), mFiltering(false),
      mNeedsFiltering(false),
      mOrientation(0),
      mPlaneOrientation(0),
      mTransactionFlags(0),
      mPremultipliedAlpha(true), mName("unnamed"), mDebug(false)
{
    const DisplayHardware& hw(flinger->graphicPlane(0).displayHardware());
    mFlags = hw.getFlags();
}

LayerBase::~LayerBase()
{
}

void LayerBase::setName(const String8& name) {
    mName = name;
}

String8 LayerBase::getName() const {
    return mName;
}

const GraphicPlane& LayerBase::graphicPlane(int dpy) const
{
    return mFlinger->graphicPlane(dpy);
}

GraphicPlane& LayerBase::graphicPlane(int dpy)
{
    return mFlinger->graphicPlane(dpy);
}

void LayerBase::initStates(uint32_t w, uint32_t h, uint32_t flags)
{
    uint32_t layerFlags = 0;
    if (flags & ISurfaceComposer::eHidden)
        layerFlags = ISurfaceComposer::eLayerHidden;

    if (flags & ISurfaceComposer::eNonPremultiplied)
        mPremultipliedAlpha = false;

    mCurrentState.active.w = w;
    mCurrentState.active.h = h;
    mCurrentState.active.crop.makeInvalid();
    mCurrentState.z = 0;
    mCurrentState.alpha = 0xFF;
    mCurrentState.flags = layerFlags;
    mCurrentState.sequence = 0;
    mCurrentState.transform.set(0, 0);
    mCurrentState.requested = mCurrentState.active;

    // drawing state & current state are identical
    mDrawingState = mCurrentState;
}

void LayerBase::commitTransaction() {
    mDrawingState = mCurrentState;
}
void LayerBase::forceVisibilityTransaction() {
    // this can be called without SurfaceFlinger.mStateLock, but if we
    // can atomically increment the sequence number, it doesn't matter.
    android_atomic_inc(&mCurrentState.sequence);
    requestTransaction();
}
bool LayerBase::requestTransaction() {
    int32_t old = setTransactionFlags(eTransactionNeeded);
    return ((old & eTransactionNeeded) == 0);
}
uint32_t LayerBase::getTransactionFlags(uint32_t flags) {
    return android_atomic_and(~flags, &mTransactionFlags) & flags;
}
uint32_t LayerBase::setTransactionFlags(uint32_t flags) {
    return android_atomic_or(flags, &mTransactionFlags);
}

bool LayerBase::setPosition(float x, float y) {
    if (mCurrentState.transform.tx() == x && mCurrentState.transform.ty() == y)
        return false;
    mCurrentState.sequence++;
    mCurrentState.transform.set(x, y);
    requestTransaction();
    return true;
}
bool LayerBase::setLayer(uint32_t z) {
    if (mCurrentState.z == z)
        return false;
    mCurrentState.sequence++;
    mCurrentState.z = z;
    requestTransaction();
    return true;
}
bool LayerBase::setSize(uint32_t w, uint32_t h) {
    if (mCurrentState.requested.w == w && mCurrentState.requested.h == h)
        return false;
    mCurrentState.requested.w = w;
    mCurrentState.requested.h = h;
    requestTransaction();
    return true;
}
bool LayerBase::setAlpha(uint8_t alpha) {
    if (mCurrentState.alpha == alpha)
        return false;
    mCurrentState.sequence++;
    mCurrentState.alpha = alpha;
    requestTransaction();
    return true;
}
bool LayerBase::setMatrix(const layer_state_t::matrix22_t& matrix) {
    mCurrentState.sequence++;
    mCurrentState.transform.set(
            matrix.dsdx, matrix.dsdy, matrix.dtdx, matrix.dtdy);
    requestTransaction();
    return true;
}
bool LayerBase::setTransparentRegionHint(const Region& transparent) {
    mCurrentState.sequence++;
    mCurrentState.transparentRegion = transparent;
    requestTransaction();
    return true;
}
bool LayerBase::setFlags(uint8_t flags, uint8_t mask) {
    const uint32_t newFlags = (mCurrentState.flags & ~mask) | (flags & mask);
    if (mCurrentState.flags == newFlags)
        return false;
    mCurrentState.sequence++;
    mCurrentState.flags = newFlags;
    requestTransaction();
    return true;
}
bool LayerBase::setCrop(const Rect& crop) {
    if (mCurrentState.requested.crop == crop)
        return false;
    mCurrentState.sequence++;
    mCurrentState.requested.crop = crop;
    requestTransaction();
    return true;
}

Rect LayerBase::visibleBounds() const
{
    return mTransformedBounds;
}

void LayerBase::setVisibleRegion(const Region& visibleRegion) {
    // always called from main thread
    visibleRegionScreen = visibleRegion;
}

void LayerBase::setCoveredRegion(const Region& coveredRegion) {
    // always called from main thread
    coveredRegionScreen = coveredRegion;
}

uint32_t LayerBase::doTransaction(uint32_t flags)
{
    const Layer::State& front(drawingState());
    const Layer::State& temp(currentState());

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

void LayerBase::validateVisibility(const Transform& planeTransform)
{
    const Layer::State& s(drawingState());
    const Transform tr(planeTransform * s.transform);
    const bool transformed = tr.transformed();
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    const uint32_t hw_h = hw.getHeight();
    const Rect& crop(s.active.crop);

    Rect win(s.active.w, s.active.h);
    if (!crop.isEmpty()) {
        win.intersect(crop, &win);
    }

    mNumVertices = 4;
    tr.transform(mVertices[0], win.left,  win.top);
    tr.transform(mVertices[1], win.left,  win.bottom);
    tr.transform(mVertices[2], win.right, win.bottom);
    tr.transform(mVertices[3], win.right, win.top);
    for (size_t i=0 ; i<4 ; i++)
        mVertices[i][1] = hw_h - mVertices[i][1];

    if (CC_UNLIKELY(transformed)) {
        // NOTE: here we could also punt if we have too many rectangles
        // in the transparent region
        if (tr.preserveRects()) {
            // transform the transparent region
            transparentRegionScreen = tr.transform(s.transparentRegion);
        } else {
            // transformation too complex, can't do the transparent region
            // optimization.
            transparentRegionScreen.clear();
        }
    } else {
        transparentRegionScreen = s.transparentRegion;
    }

    // cache a few things...
    mOrientation = tr.getOrientation();
    mPlaneOrientation = planeTransform.getOrientation();
    mTransform = tr;
    mTransformedBounds = tr.transform(win);
}

void LayerBase::lockPageFlip(bool& recomputeVisibleRegions) {
}

void LayerBase::unlockPageFlip(
        const Transform& planeTransform, Region& outDirtyRegion) {
}

void LayerBase::setGeometry(hwc_layer_t* hwcl)
{
    hwcl->compositionType = HWC_FRAMEBUFFER;
    hwcl->hints = 0;
    hwcl->flags = HWC_SKIP_LAYER;
    hwcl->transform = 0;
    hwcl->blending = HWC_BLENDING_NONE;

    // this gives us only the "orientation" component of the transform
    const State& s(drawingState());
    const uint32_t finalTransform = s.transform.getOrientation();
    // we can only handle simple transformation
    if (finalTransform & Transform::ROT_INVALID) {
        hwcl->flags = HWC_SKIP_LAYER;
    } else {
        hwcl->transform = finalTransform;
    }

    if (!isOpaque()) {
        hwcl->blending = mPremultipliedAlpha ?
                HWC_BLENDING_PREMULT : HWC_BLENDING_COVERAGE;
    }

    // scaling is already applied in mTransformedBounds
    hwcl->displayFrame.left   = mTransformedBounds.left;
    hwcl->displayFrame.top    = mTransformedBounds.top;
    hwcl->displayFrame.right  = mTransformedBounds.right;
    hwcl->displayFrame.bottom = mTransformedBounds.bottom;
    hwcl->visibleRegionScreen.rects =
            reinterpret_cast<hwc_rect_t const *>(
                    visibleRegionScreen.getArray(
                            &hwcl->visibleRegionScreen.numRects));

    hwcl->sourceCrop.left   = 0;
    hwcl->sourceCrop.top    = 0;
    hwcl->sourceCrop.right  = mTransformedBounds.width();
    hwcl->sourceCrop.bottom = mTransformedBounds.height();
}

void LayerBase::setPerFrameData(hwc_layer_t* hwcl) {
    hwcl->compositionType = HWC_FRAMEBUFFER;
    hwcl->handle = NULL;
}

void LayerBase::setFiltering(bool filtering)
{
    mFiltering = filtering;
}

bool LayerBase::getFiltering() const
{
    return mFiltering;
}

void LayerBase::draw(const Region& clip) const
{
    //Dont draw External-only layers
    if (isLayerExternalOnly(getLayer())) {
        return;
    }
    onDraw(clip);
}

void LayerBase::drawForSreenShot()
{
    //Dont draw External-only layers
    if (isLayerExternalOnly(getLayer())) {
        return;
    }
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    setFiltering(true);
    onDraw( Region(hw.bounds()) );
    setFiltering(false);
}

void LayerBase::clearWithOpenGL(const Region& clip, GLclampf red,
                                GLclampf green, GLclampf blue,
                                GLclampf alpha) const
{
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    const uint32_t fbHeight = hw.getHeight();
    glColor4f(red,green,blue,alpha);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);

    glVertexPointer(2, GL_FLOAT, 0, mVertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, mNumVertices);
}

void LayerBase::clearWithOpenGL(const Region& clip) const
{
    clearWithOpenGL(clip,0,0,0,0);
}

void LayerBase::drawWithOpenGL(const Region& clip) const
{
    const DisplayHardware& hw(graphicPlane(0).displayHardware());
    const uint32_t fbHeight = hw.getHeight();
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

    struct TexCoords {
        GLfloat u;
        GLfloat v;
    };

    Rect crop(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        crop = s.active.crop;
    }
    GLfloat left = GLfloat(crop.left) / GLfloat(s.active.w);
    GLfloat top = GLfloat(crop.top) / GLfloat(s.active.h);
    GLfloat right = GLfloat(crop.right) / GLfloat(s.active.w);
    GLfloat bottom = GLfloat(crop.bottom) / GLfloat(s.active.h);

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

    if (needsDithering()) {
        glEnable(GL_DITHER);
    } else {
        glDisable(GL_DITHER);
    }

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, mVertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_TRIANGLE_FAN, 0, mNumVertices);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_BLEND);
}

void LayerBase::dump(String8& result, char* buffer, size_t SIZE) const
{
    const Layer::State& s(drawingState());

    snprintf(buffer, SIZE,
            "+ %s %p (%s)\n",
            getTypeId(), this, getName().string());
    result.append(buffer);

    s.transparentRegion.dump(result, "transparentRegion");
    transparentRegionScreen.dump(result, "transparentRegionScreen");
    visibleRegionScreen.dump(result, "visibleRegionScreen");

    snprintf(buffer, SIZE,
            "      "
            "z=%9d, pos=(%g,%g), size=(%4d,%4d), crop=(%4d,%4d,%4d,%4d), "
            "isOpaque=%1d, needsDithering=%1d, invalidate=%1d, "
            "alpha=0x%02x, flags=0x%08x, tr=[%.2f, %.2f][%.2f, %.2f]\n",
            s.z, s.transform.tx(), s.transform.ty(), s.active.w, s.active.h,
            s.active.crop.left, s.active.crop.top,
            s.active.crop.right, s.active.crop.bottom,
            isOpaque(), needsDithering(), contentDirty,
            s.alpha, s.flags,
            s.transform[0][0], s.transform[0][1],
            s.transform[1][0], s.transform[1][1]);
    result.append(buffer);
}

void LayerBase::shortDump(String8& result, char* scratch, size_t size) const {
    LayerBase::dump(result, scratch, size);
}

void LayerBase::dumpStats(String8& result, char* scratch, size_t SIZE) const {
}

void LayerBase::clearStats() {
}

// ---------------------------------------------------------------------------

int32_t LayerBaseClient::sIdentity = 1;

LayerBaseClient::LayerBaseClient(SurfaceFlinger* flinger, DisplayID display,
        const sp<Client>& client)
    : LayerBase(flinger, display),
      mHasSurface(false),
      mClientRef(client),
      mIdentity(uint32_t(android_atomic_inc(&sIdentity)))
{
}

LayerBaseClient::~LayerBaseClient()
{
    sp<Client> c(mClientRef.promote());
    if (c != 0) {
        c->detachLayer(this);
    }
}

sp<ISurface> LayerBaseClient::createSurface()
{
    class BSurface : public BnSurface, public LayerCleaner {
        virtual sp<ISurfaceTexture> getSurfaceTexture() const { return 0; }
    public:
        BSurface(const sp<SurfaceFlinger>& flinger,
                const sp<LayerBaseClient>& layer)
            : LayerCleaner(flinger, layer) { }
    };
    sp<ISurface> sur(new BSurface(mFlinger, this));
    return sur;
}

sp<ISurface> LayerBaseClient::getSurface()
{
    sp<ISurface> s;
    Mutex::Autolock _l(mLock);

    LOG_ALWAYS_FATAL_IF(mHasSurface,
            "LayerBaseClient::getSurface() has already been called");

    mHasSurface = true;
    s = createSurface();
    mClientSurfaceBinder = s->asBinder();
    return s;
}

wp<IBinder> LayerBaseClient::getSurfaceBinder() const {
    return mClientSurfaceBinder;
}

wp<IBinder> LayerBaseClient::getSurfaceTextureBinder() const {
    return 0;
}

void LayerBaseClient::dump(String8& result, char* buffer, size_t SIZE) const
{
    LayerBase::dump(result, buffer, SIZE);

    sp<Client> client(mClientRef.promote());
    snprintf(buffer, SIZE,
            "      client=%p, identity=%u\n",
            client.get(), getIdentity());

    result.append(buffer);
}


void LayerBaseClient::shortDump(String8& result, char* scratch, size_t size) const
{
    LayerBaseClient::dump(result, scratch, size);
}

// ---------------------------------------------------------------------------

LayerBaseClient::LayerCleaner::LayerCleaner(const sp<SurfaceFlinger>& flinger,
        const sp<LayerBaseClient>& layer)
    : mFlinger(flinger), mLayer(layer) {
}

LayerBaseClient::LayerCleaner::~LayerCleaner() {
    // destroy client resources
    mFlinger->destroySurface(mLayer);
}

// ---------------------------------------------------------------------------

//Helper for external-only layers
bool isLayerExternalOnly(const sp<Layer>& layer) {
#ifdef QCOMHW
    if(layer != NULL) {
        const sp<GraphicBuffer>& buffer(layer->getActiveBuffer());
        if (buffer != NULL) {
            const int usage = buffer->getUsage();
            if(usage & GRALLOC_USAGE_PRIVATE_EXTERNAL_ONLY)
                return true;
        }
    }
#endif
    return false;
}

}; // namespace android
