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

#include "clz.h"
#include "Client.h"
#include "LayerBase.h"
#include "Layer.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"

namespace android {

// ---------------------------------------------------------------------------

int32_t LayerBase::sSequence = 1;

LayerBase::LayerBase(SurfaceFlinger* flinger)
    : contentDirty(false),
      sequence(uint32_t(android_atomic_inc(&sSequence))),
      mFlinger(flinger), mFiltering(false),
      mNeedsFiltering(false),
      mTransactionFlags(0),
      mPremultipliedAlpha(true), mName("unnamed"), mDebug(false)
{
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

void LayerBase::initStates(uint32_t w, uint32_t h, uint32_t flags)
{
    uint32_t layerFlags = 0;
    if (flags & ISurfaceComposerClient::eHidden)
        layerFlags = layer_state_t::eLayerHidden;

    if (flags & ISurfaceComposerClient::eNonPremultiplied)
        mPremultipliedAlpha = false;

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

bool LayerBase::needsFiltering(const sp<const DisplayDevice>& hw) const {
    return mNeedsFiltering || hw->needsFiltering();
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

bool LayerBase::setLayerStack(uint32_t layerStack) {
    if (mCurrentState.layerStack == layerStack)
        return false;
    mCurrentState.sequence++;
    mCurrentState.layerStack = layerStack;
    requestTransaction();
    return true;
}

void LayerBase::setVisibleRegion(const Region& visibleRegion) {
    // always called from main thread
    this->visibleRegion = visibleRegion;
}

void LayerBase::setCoveredRegion(const Region& coveredRegion) {
    // always called from main thread
    this->coveredRegion = coveredRegion;
}

void LayerBase::setVisibleNonTransparentRegion(const Region&
        setVisibleNonTransparentRegion) {
    // always called from main thread
    this->visibleNonTransparentRegion = setVisibleNonTransparentRegion;
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

void LayerBase::computeGeometry(const sp<const DisplayDevice>& hw, LayerMesh* mesh) const
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

Rect LayerBase::computeBounds() const {
    const Layer::State& s(drawingState());
    Rect win(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        win.intersect(s.active.crop, &win);
    }
    return s.transform.transform(win);
}

Region LayerBase::latchBuffer(bool& recomputeVisibleRegions) {
    Region result;
    return result;
}

void LayerBase::setGeometry(
    const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer)
{
    layer.setDefaultState();

    // this gives us only the "orientation" component of the transform
    const State& s(drawingState());
    const uint32_t finalTransform = s.transform.getOrientation();
    // we can only handle simple transformation
    if (finalTransform & Transform::ROT_INVALID) {
        layer.setTransform(0);
    } else {
        layer.setTransform(finalTransform);
    }

    if (!isOpaque()) {
        layer.setBlending(mPremultipliedAlpha ?
                HWC_BLENDING_PREMULT :
                HWC_BLENDING_COVERAGE);
    }

    const Transform& tr = hw->getTransform();
    Rect transformedBounds(computeBounds());
    transformedBounds = tr.transform(transformedBounds);

    // scaling is already applied in transformedBounds
    layer.setFrame(transformedBounds);
    layer.setCrop(transformedBounds.getBounds());
}

void LayerBase::setPerFrameData(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
    layer.setPerFrameDefaultState();
    // we have to set the visible region on every frame because
    // we currently free it during onLayerDisplayed(), which is called
    // after HWComposer::commit() -- every frame.
    const Transform& tr = hw->getTransform();
    layer.setVisibleRegionScreen(tr.transform(visibleRegion));
}

void LayerBase::setAcquireFence(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
    layer.setAcquireFenceFd(-1);
}

void LayerBase::onLayerDisplayed(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface* layer) {
    if (layer) {
        layer->onDisplayed();
    }
}

void LayerBase::setFiltering(bool filtering)
{
    mFiltering = filtering;
}

bool LayerBase::getFiltering() const
{
    return mFiltering;
}

bool LayerBase::isVisible() const {
    const Layer::State& s(mDrawingState);
    return !(s.flags & layer_state_t::eLayerHidden) && s.alpha;
}

void LayerBase::draw(const sp<const DisplayDevice>& hw, const Region& clip) const
{
    onDraw(hw, clip);
}

void LayerBase::draw(const sp<const DisplayDevice>& hw)
{
    onDraw( hw, Region(hw->bounds()) );
}

void LayerBase::clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip,
        GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) const
{
    const uint32_t fbHeight = hw->getHeight();
    glColor4f(red,green,blue,alpha);

    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);

    LayerMesh mesh;
    computeGeometry(hw, &mesh);

    glVertexPointer(2, GL_FLOAT, 0, mesh.getVertices());
    glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.getVertexCount());
}

void LayerBase::clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip) const
{
    clearWithOpenGL(hw, clip, 0,0,0,0);
}

void LayerBase::drawWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip) const
{
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

    Rect win(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        win.intersect(s.active.crop, &win);
    }

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

    if (needsDithering()) {
        glEnable(GL_DITHER);
    } else {
        glDisable(GL_DITHER);
    }

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glVertexPointer(2, GL_FLOAT, 0, mesh.getVertices());
    glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.getVertexCount());

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
    visibleRegion.dump(result, "visibleRegion");

    snprintf(buffer, SIZE,
            "      "
            "layerStack=%4d, z=%9d, pos=(%g,%g), size=(%4d,%4d), crop=(%4d,%4d,%4d,%4d), "
            "isOpaque=%1d, needsDithering=%1d, invalidate=%1d, "
            "alpha=0x%02x, flags=0x%08x, tr=[%.2f, %.2f][%.2f, %.2f]\n",
            s.layerStack, s.z, s.transform.tx(), s.transform.ty(), s.active.w, s.active.h,
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

sp<LayerBaseClient> LayerBase::getLayerBaseClient() const {
    return 0;
}

sp<Layer> LayerBase::getLayer() const {
    return 0;
}

// ---------------------------------------------------------------------------

int32_t LayerBaseClient::sIdentity = 1;

LayerBaseClient::LayerBaseClient(SurfaceFlinger* flinger,
        const sp<Client>& client)
    : LayerBase(flinger),
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
    mFlinger->onLayerDestroyed(mLayer);
}

// ---------------------------------------------------------------------------

}; // namespace android
