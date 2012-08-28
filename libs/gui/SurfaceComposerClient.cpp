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

#define LOG_TAG "SurfaceComposerClient"

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/Singleton.h>
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include <utils/threads.h>

#include <binder/IMemory.h>
#include <binder/IServiceManager.h>

#include <ui/DisplayInfo.h>

#include <gui/ISurface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/ISurfaceComposerClient.h>
#include <gui/SurfaceComposerClient.h>

#include <private/gui/ComposerService.h>
#include <private/gui/LayerState.h>

namespace android {
// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(ComposerService);

ComposerService::ComposerService()
: Singleton<ComposerService>() {
    const String16 name("SurfaceFlinger");
    while (getService(name, &mComposerService) != NO_ERROR) {
        usleep(250000);
    }
}

sp<ISurfaceComposer> ComposerService::getComposerService() {
    return ComposerService::getInstance().mComposerService;
}

// ---------------------------------------------------------------------------

static inline
int compare_type(const ComposerState& lhs, const ComposerState& rhs) {
    if (lhs.client < rhs.client)  return -1;
    if (lhs.client > rhs.client)  return 1;
    if (lhs.state.surface < rhs.state.surface)  return -1;
    if (lhs.state.surface > rhs.state.surface)  return 1;
    return 0;
}

static inline
int compare_type(const DisplayState& lhs, const DisplayState& rhs) {
    return compare_type(lhs.token, rhs.token);
}

class Composer : public Singleton<Composer>
{
    friend class Singleton<Composer>;

    mutable Mutex               mLock;
    SortedVector<ComposerState> mComposerStates;
    SortedVector<DisplayState > mDisplayStates;
    uint32_t                    mForceSynchronous;

    Composer() : Singleton<Composer>(),
        mForceSynchronous(0)
    { }

    void closeGlobalTransactionImpl(bool synchronous);

    layer_state_t* getLayerStateLocked(
            const sp<SurfaceComposerClient>& client, SurfaceID id);

    DisplayState& getDisplayStateLocked(const sp<IBinder>& token);

public:
    sp<IBinder> createDisplay();
    sp<IBinder> getBuiltInDisplay(int32_t id);

    status_t setPosition(const sp<SurfaceComposerClient>& client, SurfaceID id,
            float x, float y);
    status_t setSize(const sp<SurfaceComposerClient>& client, SurfaceID id,
            uint32_t w, uint32_t h);
    status_t setLayer(const sp<SurfaceComposerClient>& client, SurfaceID id,
            int32_t z);
    status_t setFlags(const sp<SurfaceComposerClient>& client, SurfaceID id,
            uint32_t flags, uint32_t mask);
    status_t setTransparentRegionHint(
            const sp<SurfaceComposerClient>& client, SurfaceID id,
            const Region& transparentRegion);
    status_t setAlpha(const sp<SurfaceComposerClient>& client, SurfaceID id,
            float alpha);
    status_t setMatrix(const sp<SurfaceComposerClient>& client, SurfaceID id,
            float dsdx, float dtdx, float dsdy, float dtdy);
    status_t setOrientation(int orientation);
    status_t setCrop(const sp<SurfaceComposerClient>& client, SurfaceID id,
            const Rect& crop);
    status_t setLayerStack(const sp<SurfaceComposerClient>& client,
            SurfaceID id, uint32_t layerStack);

    void setDisplaySurface(const sp<IBinder>& token, const sp<ISurfaceTexture>& surface);
    void setDisplayLayerStack(const sp<IBinder>& token, uint32_t layerStack);
    void setDisplayOrientation(const sp<IBinder>& token, uint32_t orientation);
    void setDisplayViewport(const sp<IBinder>& token, const Rect& viewport);
    void setDisplayFrame(const sp<IBinder>& token, const Rect& frame);

    static void closeGlobalTransaction(bool synchronous) {
        Composer::getInstance().closeGlobalTransactionImpl(synchronous);
    }
};

ANDROID_SINGLETON_STATIC_INSTANCE(Composer);

// ---------------------------------------------------------------------------

sp<IBinder> Composer::createDisplay() {
    return ComposerService::getComposerService()->createDisplay();
}

sp<IBinder> Composer::getBuiltInDisplay(int32_t id) {
    return ComposerService::getComposerService()->getBuiltInDisplay(id);
}

void Composer::closeGlobalTransactionImpl(bool synchronous) {
    sp<ISurfaceComposer> sm(ComposerService::getComposerService());

    Vector<ComposerState> transaction;
    Vector<DisplayState> displayTransaction;
    uint32_t flags = 0;

    { // scope for the lock
        Mutex::Autolock _l(mLock);
        transaction = mComposerStates;
        mComposerStates.clear();

        displayTransaction = mDisplayStates;
        mDisplayStates.clear();

        if (synchronous || mForceSynchronous) {
            flags |= ISurfaceComposer::eSynchronous;
        }
        mForceSynchronous = false;
    }

   sm->setTransactionState(transaction, displayTransaction, flags);
}

layer_state_t* Composer::getLayerStateLocked(
        const sp<SurfaceComposerClient>& client, SurfaceID id) {

    ComposerState s;
    s.client = client->mClient;
    s.state.surface = id;

    ssize_t index = mComposerStates.indexOf(s);
    if (index < 0) {
        // we don't have it, add an initialized layer_state to our list
        index = mComposerStates.add(s);
    }

    ComposerState* const out = mComposerStates.editArray();
    return &(out[index].state);
}

status_t Composer::setPosition(const sp<SurfaceComposerClient>& client,
        SurfaceID id, float x, float y) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::ePositionChanged;
    s->x = x;
    s->y = y;
    return NO_ERROR;
}

status_t Composer::setSize(const sp<SurfaceComposerClient>& client,
        SurfaceID id, uint32_t w, uint32_t h) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eSizeChanged;
    s->w = w;
    s->h = h;

    // Resizing a surface makes the transaction synchronous.
    mForceSynchronous = true;

    return NO_ERROR;
}

status_t Composer::setLayer(const sp<SurfaceComposerClient>& client,
        SurfaceID id, int32_t z) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eLayerChanged;
    s->z = z;
    return NO_ERROR;
}

status_t Composer::setFlags(const sp<SurfaceComposerClient>& client,
        SurfaceID id, uint32_t flags,
        uint32_t mask) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eVisibilityChanged;
    s->flags &= ~mask;
    s->flags |= (flags & mask);
    s->mask |= mask;
    return NO_ERROR;
}

status_t Composer::setTransparentRegionHint(
        const sp<SurfaceComposerClient>& client, SurfaceID id,
        const Region& transparentRegion) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eTransparentRegionChanged;
    s->transparentRegion = transparentRegion;
    return NO_ERROR;
}

status_t Composer::setAlpha(const sp<SurfaceComposerClient>& client,
        SurfaceID id, float alpha) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eAlphaChanged;
    s->alpha = alpha;
    return NO_ERROR;
}

status_t Composer::setLayerStack(const sp<SurfaceComposerClient>& client,
        SurfaceID id, uint32_t layerStack) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eLayerStackChanged;
    s->layerStack = layerStack;
    return NO_ERROR;
}

status_t Composer::setMatrix(const sp<SurfaceComposerClient>& client,
        SurfaceID id, float dsdx, float dtdx,
        float dsdy, float dtdy) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eMatrixChanged;
    layer_state_t::matrix22_t matrix;
    matrix.dsdx = dsdx;
    matrix.dtdx = dtdx;
    matrix.dsdy = dsdy;
    matrix.dtdy = dtdy;
    s->matrix = matrix;
    return NO_ERROR;
}

status_t Composer::setCrop(const sp<SurfaceComposerClient>& client,
        SurfaceID id, const Rect& crop) {
    Mutex::Autolock _l(mLock);
    layer_state_t* s = getLayerStateLocked(client, id);
    if (!s)
        return BAD_INDEX;
    s->what |= layer_state_t::eCropChanged;
    s->crop = crop;
    return NO_ERROR;
}

// ---------------------------------------------------------------------------

DisplayState& Composer::getDisplayStateLocked(const sp<IBinder>& token) {
    DisplayState s;
    s.token = token;
    ssize_t index = mDisplayStates.indexOf(s);
    if (index < 0) {
        // we don't have it, add an initialized layer_state to our list
        s.what = 0;
        index = mDisplayStates.add(s);
    }
    return mDisplayStates.editItemAt(index);
}

void Composer::setDisplaySurface(const sp<IBinder>& token,
        const sp<ISurfaceTexture>& surface) {
    Mutex::Autolock _l(mLock);
    DisplayState& s(getDisplayStateLocked(token));
    s.surface = surface;
    s.what |= DisplayState::eSurfaceChanged;
}

void Composer::setDisplayLayerStack(const sp<IBinder>& token,
        uint32_t layerStack) {
    Mutex::Autolock _l(mLock);
    DisplayState& s(getDisplayStateLocked(token));
    s.layerStack = layerStack;
    s.what |= DisplayState::eLayerStackChanged;
}

void Composer::setDisplayOrientation(const sp<IBinder>& token,
        uint32_t orientation) {
    Mutex::Autolock _l(mLock);
    DisplayState& s(getDisplayStateLocked(token));
    s.orientation = orientation;
    s.what |= DisplayState::eOrientationChanged;
    mForceSynchronous = true; // TODO: do we actually still need this?
}

// FIXME: get rid of this eventually
status_t Composer::setOrientation(int orientation) {
    sp<ISurfaceComposer> sm(ComposerService::getComposerService());
    sp<IBinder> token(sm->getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain));
    Composer::setDisplayOrientation(token, orientation);
    return NO_ERROR;
}

void Composer::setDisplayViewport(const sp<IBinder>& token,
        const Rect& viewport) {
    Mutex::Autolock _l(mLock);
    DisplayState& s(getDisplayStateLocked(token));
    s.viewport = viewport;
    s.what |= DisplayState::eViewportChanged;
}

void Composer::setDisplayFrame(const sp<IBinder>& token,
        const Rect& frame) {
    Mutex::Autolock _l(mLock);
    DisplayState& s(getDisplayStateLocked(token));
    s.frame = frame;
    s.what |= DisplayState::eFrameChanged;
}

// ---------------------------------------------------------------------------

SurfaceComposerClient::SurfaceComposerClient()
    : mStatus(NO_INIT), mComposer(Composer::getInstance())
{
}

void SurfaceComposerClient::onFirstRef() {
    sp<ISurfaceComposer> sm(ComposerService::getComposerService());
    if (sm != 0) {
        sp<ISurfaceComposerClient> conn = sm->createConnection();
        if (conn != 0) {
            mClient = conn;
            mStatus = NO_ERROR;
        }
    }
}

SurfaceComposerClient::~SurfaceComposerClient() {
    dispose();
}

status_t SurfaceComposerClient::initCheck() const {
    return mStatus;
}

sp<IBinder> SurfaceComposerClient::connection() const {
    return (mClient != 0) ? mClient->asBinder() : 0;
}

status_t SurfaceComposerClient::linkToComposerDeath(
        const sp<IBinder::DeathRecipient>& recipient,
        void* cookie, uint32_t flags) {
    sp<ISurfaceComposer> sm(ComposerService::getComposerService());
    return sm->asBinder()->linkToDeath(recipient, cookie, flags);
}

void SurfaceComposerClient::dispose() {
    // this can be called more than once.
    sp<ISurfaceComposerClient> client;
    Mutex::Autolock _lm(mLock);
    if (mClient != 0) {
        client = mClient; // hold ref while lock is held
        mClient.clear();
    }
    mStatus = NO_INIT;
}

sp<SurfaceControl> SurfaceComposerClient::createSurface(
        const String8& name,
        uint32_t w,
        uint32_t h,
        PixelFormat format,
        uint32_t flags)
{
    sp<SurfaceControl> result;
    if (mStatus == NO_ERROR) {
        ISurfaceComposerClient::surface_data_t data;
        sp<ISurface> surface = mClient->createSurface(&data, name,
                w, h, format, flags);
        if (surface != 0) {
            result = new SurfaceControl(this, surface, data);
        }
    }
    return result;
}

sp<IBinder> SurfaceComposerClient::createDisplay() {
    return Composer::getInstance().createDisplay();
}

sp<IBinder> SurfaceComposerClient::getBuiltInDisplay(int32_t id) {
    return Composer::getInstance().getBuiltInDisplay(id);
}

status_t SurfaceComposerClient::destroySurface(SurfaceID sid) {
    if (mStatus != NO_ERROR)
        return mStatus;
    status_t err = mClient->destroySurface(sid);
    return err;
}

inline Composer& SurfaceComposerClient::getComposer() {
    return mComposer;
}

// ----------------------------------------------------------------------------

void SurfaceComposerClient::openGlobalTransaction() {
    // Currently a no-op
}

void SurfaceComposerClient::closeGlobalTransaction(bool synchronous) {
    Composer::closeGlobalTransaction(synchronous);
}

// ----------------------------------------------------------------------------

status_t SurfaceComposerClient::setCrop(SurfaceID id, const Rect& crop) {
    return getComposer().setCrop(this, id, crop);
}

status_t SurfaceComposerClient::setPosition(SurfaceID id, float x, float y) {
    return getComposer().setPosition(this, id, x, y);
}

status_t SurfaceComposerClient::setSize(SurfaceID id, uint32_t w, uint32_t h) {
    return getComposer().setSize(this, id, w, h);
}

status_t SurfaceComposerClient::setLayer(SurfaceID id, int32_t z) {
    return getComposer().setLayer(this, id, z);
}

status_t SurfaceComposerClient::hide(SurfaceID id) {
    return getComposer().setFlags(this, id,
            layer_state_t::eLayerHidden,
            layer_state_t::eLayerHidden);
}

status_t SurfaceComposerClient::show(SurfaceID id, int32_t) {
    return getComposer().setFlags(this, id,
            0,
            layer_state_t::eLayerHidden);
}

status_t SurfaceComposerClient::setFlags(SurfaceID id, uint32_t flags,
        uint32_t mask) {
    return getComposer().setFlags(this, id, flags, mask);
}

status_t SurfaceComposerClient::setTransparentRegionHint(SurfaceID id,
        const Region& transparentRegion) {
    return getComposer().setTransparentRegionHint(this, id, transparentRegion);
}

status_t SurfaceComposerClient::setAlpha(SurfaceID id, float alpha) {
    return getComposer().setAlpha(this, id, alpha);
}

status_t SurfaceComposerClient::setLayerStack(SurfaceID id, uint32_t layerStack) {
    return getComposer().setLayerStack(this, id, layerStack);
}

status_t SurfaceComposerClient::setMatrix(SurfaceID id, float dsdx, float dtdx,
        float dsdy, float dtdy) {
    return getComposer().setMatrix(this, id, dsdx, dtdx, dsdy, dtdy);
}

// ----------------------------------------------------------------------------

void SurfaceComposerClient::setDisplaySurface(const sp<IBinder>& token,
        const sp<ISurfaceTexture>& surface) {
    Composer::getInstance().setDisplaySurface(token, surface);
}

void SurfaceComposerClient::setDisplayLayerStack(const sp<IBinder>& token,
        uint32_t layerStack) {
    Composer::getInstance().setDisplayLayerStack(token, layerStack);
}

void SurfaceComposerClient::setDisplayOrientation(const sp<IBinder>& token,
        uint32_t orientation) {
    Composer::getInstance().setDisplayOrientation(token, orientation);
}

void SurfaceComposerClient::setDisplayViewport(const sp<IBinder>& token,
        const Rect& viewport) {
    Composer::getInstance().setDisplayViewport(token, viewport);
}

void SurfaceComposerClient::setDisplayFrame(const sp<IBinder>& token,
        const Rect& frame) {
    Composer::getInstance().setDisplayFrame(token, frame);
}

// ----------------------------------------------------------------------------

status_t SurfaceComposerClient::getDisplayInfo(
        const sp<IBinder>& display, DisplayInfo* info)
{
    return ComposerService::getComposerService()->getDisplayInfo(display, info);
}

// ----------------------------------------------------------------------------

ScreenshotClient::ScreenshotClient()
    : mWidth(0), mHeight(0), mFormat(PIXEL_FORMAT_NONE) {
}

status_t ScreenshotClient::update(const sp<IBinder>& display) {
    sp<ISurfaceComposer> s(ComposerService::getComposerService());
    if (s == NULL) return NO_INIT;
    mHeap = 0;
    return s->captureScreen(display, &mHeap,
            &mWidth, &mHeight, &mFormat, 0, 0,
            0, -1UL);
}

status_t ScreenshotClient::update(const sp<IBinder>& display,
        uint32_t reqWidth, uint32_t reqHeight) {
    sp<ISurfaceComposer> s(ComposerService::getComposerService());
    if (s == NULL) return NO_INIT;
    mHeap = 0;
    return s->captureScreen(display, &mHeap,
            &mWidth, &mHeight, &mFormat, reqWidth, reqHeight,
            0, -1UL);
}

status_t ScreenshotClient::update(const sp<IBinder>& display,
        uint32_t reqWidth, uint32_t reqHeight,
        uint32_t minLayerZ, uint32_t maxLayerZ) {
    sp<ISurfaceComposer> s(ComposerService::getComposerService());
    if (s == NULL) return NO_INIT;
    mHeap = 0;
    return s->captureScreen(display, &mHeap,
            &mWidth, &mHeight, &mFormat, reqWidth, reqHeight,
            minLayerZ, maxLayerZ);
}

void ScreenshotClient::release() {
    mHeap = 0;
}

void const* ScreenshotClient::getPixels() const {
    return mHeap->getBase();
}

uint32_t ScreenshotClient::getWidth() const {
    return mWidth;
}

uint32_t ScreenshotClient::getHeight() const {
    return mHeight;
}

PixelFormat ScreenshotClient::getFormat() const {
    return mFormat;
}

uint32_t ScreenshotClient::getStride() const {
    return mWidth;
}

size_t ScreenshotClient::getSize() const {
    return mHeap->getSize();
}

// ----------------------------------------------------------------------------
}; // namespace android
