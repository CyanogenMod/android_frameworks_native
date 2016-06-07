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

#ifndef ANDROID_LAYER_H
#define ANDROID_LAYER_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/Timers.h>

#include <ui/FrameStats.h>
#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>
#include <ui/Region.h>

#include <gui/ISurfaceComposerClient.h>

#include <private/gui/LayerState.h>

#include <list>

#include "FrameTracker.h"
#include "Client.h"
#include "MonitoredProducer.h"
#include "SurfaceFlinger.h"
#include "SurfaceFlingerConsumer.h"
#include "Transform.h"

#include "DisplayHardware/HWComposer.h"
#include "DisplayHardware/FloatRect.h"
#include "RenderEngine/Mesh.h"
#include "RenderEngine/Texture.h"

namespace android {

// ---------------------------------------------------------------------------

class Client;
class Colorizer;
class DisplayDevice;
class GraphicBuffer;
class SurfaceFlinger;

// ---------------------------------------------------------------------------

/*
 * A new BufferQueue and a new SurfaceFlingerConsumer are created when the
 * Layer is first referenced.
 *
 * This also implements onFrameAvailable(), which notifies SurfaceFlinger
 * that new data has arrived.
 */
class Layer : public SurfaceFlingerConsumer::ContentsChangedListener {
    static int32_t sSequence;

public:
    mutable bool contentDirty;
    // regions below are in window-manager space
    Region visibleRegion;
    Region coveredRegion;
    Region visibleNonTransparentRegion;
    Region surfaceDamageRegion;

    // Layer serial number.  This gives layers an explicit ordering, so we
    // have a stable sort order when their layer stack and Z-order are
    // the same.
    int32_t sequence;

    enum { // flags for doTransaction()
        eDontUpdateGeometryState = 0x00000001,
        eVisibleRegion = 0x00000002,
    };

    struct Geometry {
        uint32_t w;
        uint32_t h;
        Transform transform;

        inline bool operator ==(const Geometry& rhs) const {
          return (w == rhs.w && h == rhs.h);
        }
        inline bool operator !=(const Geometry& rhs) const {
            return !operator ==(rhs);
        }
    };

    struct State {
        Geometry active;
        Geometry requested;
        uint32_t z;
        uint32_t layerStack;
#ifdef USE_HWC2
        float alpha;
#else
        uint8_t alpha;
#endif
        uint8_t flags;
        uint8_t mask;
        uint8_t reserved[2];
        int32_t sequence; // changes when visible regions can change
        bool modified;

        Rect crop;
        Rect finalCrop;

        // If set, defers this state update until the Layer identified by handle
        // receives a frame with the given frameNumber
        sp<IBinder> handle;
        uint64_t frameNumber;

        // the transparentRegion hint is a bit special, it's latched only
        // when we receive a buffer -- this is because it's "content"
        // dependent.
        Region activeTransparentRegion;
        Region requestedTransparentRegion;
    };

    // -----------------------------------------------------------------------

    Layer(SurfaceFlinger* flinger, const sp<Client>& client,
            const String8& name, uint32_t w, uint32_t h, uint32_t flags);

    virtual ~Layer();

    // the this layer's size and format
    status_t setBuffers(uint32_t w, uint32_t h, PixelFormat format, uint32_t flags);

    // modify current state
    bool setPosition(float x, float y, bool immediate);
    bool setLayer(uint32_t z);
    bool setSize(uint32_t w, uint32_t h);
#ifdef USE_HWC2
    bool setAlpha(float alpha);
#else
    bool setAlpha(uint8_t alpha);
#endif
    bool setMatrix(const layer_state_t::matrix22_t& matrix);
    bool setTransparentRegionHint(const Region& transparent);
    bool setFlags(uint8_t flags, uint8_t mask);
    bool setCrop(const Rect& crop);
    bool setFinalCrop(const Rect& crop);
    bool setLayerStack(uint32_t layerStack);
    void deferTransactionUntil(const sp<IBinder>& handle, uint64_t frameNumber);
    bool setOverrideScalingMode(int32_t overrideScalingMode);

    // If we have received a new buffer this frame, we will pass its surface
    // damage down to hardware composer. Otherwise, we must send a region with
    // one empty rect.
    void useSurfaceDamage();
    void useEmptyDamage();

    uint32_t getTransactionFlags(uint32_t flags);
    uint32_t setTransactionFlags(uint32_t flags);

    void computeGeometry(const sp<const DisplayDevice>& hw, Mesh& mesh,
            bool useIdentityTransform) const;
    Rect computeBounds(const Region& activeTransparentRegion) const;
    Rect computeBounds() const;

    class Handle;
    sp<IBinder> getHandle();
    sp<IGraphicBufferProducer> getProducer() const;
    const String8& getName() const;

    int32_t getSequence() const { return sequence; }

    // -----------------------------------------------------------------------
    // Virtuals

    virtual const char* getTypeId() const { return "Layer"; }

    /*
     * isOpaque - true if this surface is opaque
     *
     * This takes into account the buffer format (i.e. whether or not the
     * pixel format includes an alpha channel) and the "opaque" flag set
     * on the layer.  It does not examine the current plane alpha value.
     */
    virtual bool isOpaque(const Layer::State& s) const;

    /*
     * isSecure - true if this surface is secure, that is if it prevents
     * screenshots or VNC servers.
     */
    virtual bool isSecure() const;

    /*
     * isProtected - true if the layer may contain protected content in the
     * GRALLOC_USAGE_PROTECTED sense.
     */
    virtual bool isProtected() const;

    /*
     * isVisible - true if this layer is visible, false otherwise
     */
    virtual bool isVisible() const;

    /*
     * isFixedSize - true if content has a fixed size
     */
    virtual bool isFixedSize() const;

protected:
    /*
     * onDraw - draws the surface.
     */
    virtual void onDraw(const sp<const DisplayDevice>& hw, const Region& clip,
            bool useIdentityTransform) const;

public:
    // -----------------------------------------------------------------------

#ifdef USE_HWC2
    void setGeometry(const sp<const DisplayDevice>& displayDevice);
    void forceClientComposition(int32_t hwcId);
    void setPerFrameData(const sp<const DisplayDevice>& displayDevice);

    // callIntoHwc exists so we can update our local state and call
    // acceptDisplayChanges without unnecessarily updating the device's state
    void setCompositionType(int32_t hwcId, HWC2::Composition type,
            bool callIntoHwc = true);
    HWC2::Composition getCompositionType(int32_t hwcId) const;

    void setClearClientTarget(int32_t hwcId, bool clear);
    bool getClearClientTarget(int32_t hwcId) const;

    void updateCursorPosition(const sp<const DisplayDevice>& hw);
#else
    void setGeometry(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);
    void setPerFrameData(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);
    void setAcquireFence(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);

    Rect getPosition(const sp<const DisplayDevice>& hw);
#endif

    /*
     * called after page-flip
     */
#ifdef USE_HWC2
    void onLayerDisplayed(const sp<Fence>& releaseFence);
#else
    void onLayerDisplayed(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface* layer);
#endif

    bool shouldPresentNow(const DispSync& dispSync) const;

    /*
     * called before composition.
     * returns true if the layer has pending updates.
     */
    bool onPreComposition();

    /*
     *  called after composition.
     */
    void onPostComposition();

#ifdef USE_HWC2
    // If a buffer was replaced this frame, release the former buffer
    void releasePendingBuffer();
#endif

    /*
     * draw - performs some global clipping optimizations
     * and calls onDraw().
     */
    void draw(const sp<const DisplayDevice>& hw, const Region& clip) const;
    void draw(const sp<const DisplayDevice>& hw, bool useIdentityTransform) const;
    void draw(const sp<const DisplayDevice>& hw) const;

    /*
     * doTransaction - process the transaction. This is a good place to figure
     * out which attributes of the surface have changed.
     */
    uint32_t doTransaction(uint32_t transactionFlags);

    /*
     * setVisibleRegion - called to set the new visible region. This gives
     * a chance to update the new visible region or record the fact it changed.
     */
    void setVisibleRegion(const Region& visibleRegion);

    /*
     * setCoveredRegion - called when the covered region changes. The covered
     * region corresponds to any area of the surface that is covered
     * (transparently or not) by another surface.
     */
    void setCoveredRegion(const Region& coveredRegion);

    /*
     * setVisibleNonTransparentRegion - called when the visible and
     * non-transparent region changes.
     */
    void setVisibleNonTransparentRegion(const Region&
            visibleNonTransparentRegion);

    /*
     * latchBuffer - called each time the screen is redrawn and returns whether
     * the visible regions need to be recomputed (this is a fairly heavy
     * operation, so this should be set only if needed). Typically this is used
     * to figure out if the content or size of a surface has changed.
     */
    Region latchBuffer(bool& recomputeVisibleRegions);

    bool isPotentialCursor() const { return mPotentialCursor;}

    /*
     * called with the state lock when the surface is removed from the
     * current list
     */
    void onRemoved();


    // Updates the transform hint in our SurfaceFlingerConsumer to match
    // the current orientation of the display device.
    void updateTransformHint(const sp<const DisplayDevice>& hw) const;

    /*
     * returns the rectangle that crops the content of the layer and scales it
     * to the layer's size.
     */
    Rect getContentCrop() const;

    /*
     * Returns if a frame is queued.
     */
    bool hasQueuedFrame() const { return mQueuedFrames > 0 ||
            mSidebandStreamChanged || mAutoRefresh; }

#ifdef USE_HWC2
    // -----------------------------------------------------------------------

    bool hasHwcLayer(int32_t hwcId) {
        if (mHwcLayers.count(hwcId) == 0) {
            return false;
        }
        if (mHwcLayers[hwcId].layer->isAbandoned()) {
            ALOGI("Erasing abandoned layer %s on %d", mName.string(), hwcId);
            mHwcLayers.erase(hwcId);
            return false;
        }
        return true;
    }

    std::shared_ptr<HWC2::Layer> getHwcLayer(int32_t hwcId) {
        if (mHwcLayers.count(hwcId) == 0) {
            return nullptr;
        }
        return mHwcLayers[hwcId].layer;
    }

    void setHwcLayer(int32_t hwcId, std::shared_ptr<HWC2::Layer>&& layer) {
        if (layer) {
            mHwcLayers[hwcId].layer = layer;
        } else {
            mHwcLayers.erase(hwcId);
        }
    }

#endif
    // -----------------------------------------------------------------------

    void clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip) const;
    void setFiltering(bool filtering);
    bool getFiltering() const;

    // only for debugging
    inline const sp<GraphicBuffer>& getActiveBuffer() const { return mActiveBuffer; }

    inline  const State&    getDrawingState() const { return mDrawingState; }
    inline  const State&    getCurrentState() const { return mCurrentState; }
    inline  State&          getCurrentState()       { return mCurrentState; }


    /* always call base class first */
    void dump(String8& result, Colorizer& colorizer) const;
    void dumpFrameStats(String8& result) const;
    void clearFrameStats();
    void logFrameStats();
    void getFrameStats(FrameStats* outStats) const;

    void getFenceData(String8* outName, uint64_t* outFrameNumber,
            bool* outIsGlesComposition, nsecs_t* outPostedTime,
            sp<Fence>* outAcquireFence, sp<Fence>* outPrevReleaseFence) const;

protected:
    // constant
    sp<SurfaceFlinger> mFlinger;

    virtual void onFirstRef();

    /*
     * Trivial class, used to ensure that mFlinger->onLayerDestroyed(mLayer)
     * is called.
     */
    class LayerCleaner {
        sp<SurfaceFlinger> mFlinger;
        wp<Layer> mLayer;
    protected:
        ~LayerCleaner();
    public:
        LayerCleaner(const sp<SurfaceFlinger>& flinger, const sp<Layer>& layer);
    };


private:
    // Interface implementation for SurfaceFlingerConsumer::ContentsChangedListener
    virtual void onFrameAvailable(const BufferItem& item) override;
    virtual void onFrameReplaced(const BufferItem& item) override;
    virtual void onSidebandStreamChanged() override;

    void commitTransaction(const State& stateToCommit);

    // needsLinearFiltering - true if this surface's state requires filtering
    bool needsFiltering(const sp<const DisplayDevice>& hw) const;

    uint32_t getEffectiveUsage(uint32_t usage) const;
    FloatRect computeCrop(const sp<const DisplayDevice>& hw) const;
    bool isCropped() const;
    static bool getOpacityForFormat(uint32_t format);

    // drawing
    void clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip,
            float r, float g, float b, float alpha) const;
    void drawWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip,
            bool useIdentityTransform) const;

    // Temporary - Used only for LEGACY camera mode.
    uint32_t getProducerStickyTransform() const;

    // -----------------------------------------------------------------------

    class SyncPoint
    {
    public:
        SyncPoint(uint64_t frameNumber) : mFrameNumber(frameNumber),
                mFrameIsAvailable(false), mTransactionIsApplied(false) {}

        uint64_t getFrameNumber() const {
            return mFrameNumber;
        }

        bool frameIsAvailable() const {
            return mFrameIsAvailable;
        }

        void setFrameAvailable() {
            mFrameIsAvailable = true;
        }

        bool transactionIsApplied() const {
            return mTransactionIsApplied;
        }

        void setTransactionApplied() {
            mTransactionIsApplied = true;
        }

    private:
        const uint64_t mFrameNumber;
        std::atomic<bool> mFrameIsAvailable;
        std::atomic<bool> mTransactionIsApplied;
    };

    // SyncPoints which will be signaled when the correct frame is at the head
    // of the queue and dropped after the frame has been latched. Protected by
    // mLocalSyncPointMutex.
    Mutex mLocalSyncPointMutex;
    std::list<std::shared_ptr<SyncPoint>> mLocalSyncPoints;

    // SyncPoints which will be signaled and then dropped when the transaction
    // is applied
    std::list<std::shared_ptr<SyncPoint>> mRemoteSyncPoints;

    uint64_t getHeadFrameNumber() const;

    // Returns false if the relevant frame has already been latched
    bool addSyncPoint(const std::shared_ptr<SyncPoint>& point);

    void pushPendingState();
    void popPendingState(State* stateToCommit);
    bool applyPendingStates(State* stateToCommit);

    // Returns mCurrentScaling mode (originating from the
    // Client) or mOverrideScalingMode mode (originating from
    // the Surface Controller) if set.
    uint32_t getEffectiveScalingMode() const;
public:
    void notifyAvailableFrames();
private:

    // -----------------------------------------------------------------------

    // constants
    sp<SurfaceFlingerConsumer> mSurfaceFlingerConsumer;
    sp<IGraphicBufferProducer> mProducer;
    uint32_t mTextureName;      // from GLES
    bool mPremultipliedAlpha;
    String8 mName;
    PixelFormat mFormat;

    // these are protected by an external lock
    State mCurrentState;
    State mDrawingState;
    volatile int32_t mTransactionFlags;

    // Accessed from main thread and binder threads
    Mutex mPendingStateMutex;
    Vector<State> mPendingStates;

    // thread-safe
    volatile int32_t mQueuedFrames;
    volatile int32_t mSidebandStreamChanged; // used like an atomic boolean
    FrameTracker mFrameTracker;

    // main thread
    sp<GraphicBuffer> mActiveBuffer;
    sp<NativeHandle> mSidebandStream;
    Rect mCurrentCrop;
    uint32_t mCurrentTransform;
    uint32_t mCurrentScalingMode;
    // We encode unset as -1.
    int32_t mOverrideScalingMode;
    bool mCurrentOpacity;
    std::atomic<uint64_t> mCurrentFrameNumber;
    bool mRefreshPending;
    bool mFrameLatencyNeeded;
    // Whether filtering is forced on or not
    bool mFiltering;
    // Whether filtering is needed b/c of the drawingstate
    bool mNeedsFiltering;
    // The mesh used to draw the layer in GLES composition mode
    mutable Mesh mMesh;
    // The texture used to draw the layer in GLES composition mode
    mutable Texture mTexture;

#ifdef USE_HWC2
    // HWC items, accessed from the main thread
    struct HWCInfo {
        HWCInfo()
          : layer(),
            forceClientComposition(false),
            compositionType(HWC2::Composition::Invalid),
            clearClientTarget(false) {}

        std::shared_ptr<HWC2::Layer> layer;
        bool forceClientComposition;
        HWC2::Composition compositionType;
        bool clearClientTarget;
    };
    std::unordered_map<int32_t, HWCInfo> mHwcLayers;
#else
    bool mIsGlesComposition;
#endif

    // page-flip thread (currently main thread)
    bool mProtectedByApp; // application requires protected path to external sink

    // protected by mLock
    mutable Mutex mLock;
    // Set to true once we've returned this surface's handle
    mutable bool mHasSurface;
    const wp<Client> mClientRef;

    // This layer can be a cursor on some displays.
    bool mPotentialCursor;

    // Local copy of the queued contents of the incoming BufferQueue
    mutable Mutex mQueueItemLock;
    Condition mQueueItemCondition;
    Vector<BufferItem> mQueueItems;
    std::atomic<uint64_t> mLastFrameNumberReceived;
    bool mUpdateTexImageFailed; // This is only modified from the main thread

    bool mAutoRefresh;
    bool mFreezePositionUpdates;
};

// ---------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_LAYER_H
