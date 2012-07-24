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

#ifndef ANDROID_SURFACE_FLINGER_H
#define ANDROID_SURFACE_FLINGER_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <cutils/compiler.h>

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/SortedVector.h>
#include <utils/threads.h>

#include <binder/BinderService.h>
#include <binder/IMemory.h>

#include <ui/PixelFormat.h>

#include <gui/IGraphicBufferAlloc.h>
#include <gui/ISurfaceComposer.h>
#include <gui/ISurfaceComposerClient.h>

#include <private/gui/LayerState.h>

#include "Barrier.h"
#include "MessageQueue.h"

namespace android {

// ---------------------------------------------------------------------------

class Client;
class DisplayEventConnection;
class DisplayHardware;
class EventThread;
class Layer;
class LayerBase;
class LayerBaseClient;
class LayerDim;
class LayerScreenshot;
class SurfaceTextureClient;
struct surface_flinger_cblk_t;

// ---------------------------------------------------------------------------

class GraphicBufferAlloc : public BnGraphicBufferAlloc {
public:
    GraphicBufferAlloc();
    virtual ~GraphicBufferAlloc();
    virtual sp<GraphicBuffer> createGraphicBuffer(uint32_t w, uint32_t h,
        PixelFormat format, uint32_t usage, status_t* error);
};

// ---------------------------------------------------------------------------

enum {
    eTransactionNeeded = 0x01, eTraversalNeeded = 0x02
};

class SurfaceFlinger : public BinderService<SurfaceFlinger>,
                       public BnSurfaceComposer,
                       private IBinder::DeathRecipient,
                       private Thread
{
public:
    static char const* getServiceName() {
        return "SurfaceFlinger";
    }

    SurfaceFlinger();

    // post an asynchronous message to the main thread
    status_t postMessageAsync(const sp<MessageBase>& msg, nsecs_t reltime = 0,
        uint32_t flags = 0);

    // post a synchronous message to the main thread
    status_t postMessageSync(const sp<MessageBase>& msg, nsecs_t reltime = 0,
        uint32_t flags = 0);

    // force full composition on all displays
    void repaintEverything();

    // renders content on given display to a texture. thread-safe version.
    status_t renderScreenToTexture(DisplayID dpy, GLuint* textureName,
        GLfloat* uOut, GLfloat* vOut);

    // renders content on given display to a texture, w/o acquiring main lock
    status_t renderScreenToTextureLocked(DisplayID dpy, GLuint* textureName,
        GLfloat* uOut, GLfloat* vOut);

    // returns the default Display
    const DisplayHardware& getDefaultDisplayHardware() const {
        return getDisplayHardware(0);
    }

    // called on the main thread by MessageQueue when an internal message
    // is received
    // TODO: this should be made accessible only to MessageQueue
    void onMessageReceived(int32_t what);

    // utility function to delete a texture on the main thread
    void deleteTextureAsync(GLuint texture);

private:
    friend class Client;
    friend class DisplayEventConnection;
    friend class LayerBase;
    friend class LayerBaseClient;
    friend class Layer;

    // We're reference counted, never destroy SurfaceFlinger directly
    virtual ~SurfaceFlinger();

    /* ------------------------------------------------------------------------
     * Internal data structures
     */

    class LayerVector : public SortedVector<sp<LayerBase> > {
    public:
        LayerVector();
        LayerVector(const LayerVector& rhs);
        virtual int do_compare(const void* lhs, const void* rhs) const;
    };

    struct State {
        State();
        LayerVector layersSortedByZ;
        uint8_t orientation;
        uint8_t orientationFlags;
    };

    /* ------------------------------------------------------------------------
     * IBinder interface
     */
    virtual status_t onTransact(uint32_t code, const Parcel& data,
        Parcel* reply, uint32_t flags);
    virtual status_t dump(int fd, const Vector<String16>& args);

    /* ------------------------------------------------------------------------
     * ISurfaceComposer interface
     */
    virtual sp<ISurfaceComposerClient> createConnection();
    virtual sp<IGraphicBufferAlloc> createGraphicBufferAlloc();
    virtual sp<IMemoryHeap> getCblk() const;
    virtual void bootFinished();
    virtual void setTransactionState(const Vector<ComposerState>& state,
        int orientation, uint32_t flags);
    virtual bool authenticateSurfaceTexture(
        const sp<ISurfaceTexture>& surface) const;
    virtual sp<IDisplayEventConnection> createDisplayEventConnection();
    virtual status_t captureScreen(DisplayID dpy, sp<IMemoryHeap>* heap,
        uint32_t* width, uint32_t* height, PixelFormat* format,
        uint32_t reqWidth, uint32_t reqHeight, uint32_t minLayerZ,
        uint32_t maxLayerZ);
    virtual status_t turnElectronBeamOff(int32_t mode);
    virtual status_t turnElectronBeamOn(int32_t mode);
    // called when screen needs to turn off
    virtual void blank();
    // called when screen is turning back on
    virtual void unblank();
    virtual void connectDisplay(const sp<ISurfaceTexture> display);

    /* ------------------------------------------------------------------------
     * DeathRecipient interface
     */
    virtual void binderDied(const wp<IBinder>& who);

    /* ------------------------------------------------------------------------
     * Thread interface
     */
    virtual bool threadLoop();
    virtual status_t readyToRun();
    virtual void onFirstRef();

    /* ------------------------------------------------------------------------
     * Message handling
     */
    void waitForEvent();
    void signalTransaction();
    void signalLayerUpdate();
    void signalRefresh();

    // called on the main thread in response to screenReleased()
    void onScreenReleased();
    // called on the main thread in response to screenAcquired()
    void onScreenAcquired();

    void handleMessageTransaction();
    void handleMessageInvalidate();
    void handleMessageRefresh();

    Region handleTransaction(uint32_t transactionFlags);
    Region handleTransactionLocked(uint32_t transactionFlags);

    /* handlePageFilp: this is were we latch a new buffer
     * if available and compute the dirty region.
     * The return value is the dirty region expressed in the
     * window manager's coordinate space (or the layer's state
     * space, which is the same thing), in particular the dirty
     * region is independent from a specific display's orientation.
     */
    Region handlePageFlip();

    void handleRefresh();
    void handleWorkList(const DisplayHardware& hw);
    void handleRepaint(const DisplayHardware& hw);

    /* ------------------------------------------------------------------------
     * Transactions
     */
    uint32_t getTransactionFlags(uint32_t flags);
    uint32_t peekTransactionFlags(uint32_t flags);
    uint32_t setTransactionFlags(uint32_t flags);
    void commitTransaction();
    uint32_t setClientStateLocked(const sp<Client>& client,
        const layer_state_t& s);

    /* ------------------------------------------------------------------------
     * Layer management
     */
    sp<ISurface> createLayer(ISurfaceComposerClient::surface_data_t* params,
        const String8& name, const sp<Client>& client, DisplayID display,
        uint32_t w, uint32_t h, PixelFormat format, uint32_t flags);

    sp<Layer> createNormalLayer(const sp<Client>& client, DisplayID display,
        uint32_t w, uint32_t h, uint32_t flags, PixelFormat& format);

    sp<LayerDim> createDimLayer(const sp<Client>& client, DisplayID display,
        uint32_t w, uint32_t h, uint32_t flags);

    sp<LayerScreenshot> createScreenshotLayer(const sp<Client>& client,
        DisplayID display, uint32_t w, uint32_t h, uint32_t flags);

    // called in response to the window-manager calling
    // ISurfaceComposerClient::destroySurface()
    // The specified layer is first placed in a purgatory list
    // until all references from the client are released.
    status_t onLayerRemoved(const sp<Client>& client, SurfaceID sid);

    // called when all clients have released all their references to
    // this layer meaning it is entirely safe to destroy all
    // resources associated to this layer.
    status_t onLayerDestroyed(const wp<LayerBaseClient>& layer);

    // remove a layer from SurfaceFlinger immediately
    status_t removeLayer(const sp<LayerBase>& layer);

    // add a layer to SurfaceFlinger
    ssize_t addClientLayer(const sp<Client>& client,
        const sp<LayerBaseClient>& lbc);

    status_t removeLayer_l(const sp<LayerBase>& layer);
    status_t purgatorizeLayer_l(const sp<LayerBase>& layer);

    /* ------------------------------------------------------------------------
     * Boot animation, on/off animations and screen capture
     */

    void startBootAnim();

    status_t captureScreenImplLocked(DisplayID dpy, sp<IMemoryHeap>* heap,
        uint32_t* width, uint32_t* height, PixelFormat* format,
        uint32_t reqWidth, uint32_t reqHeight, uint32_t minLayerZ,
        uint32_t maxLayerZ);

    status_t turnElectronBeamOffImplLocked(int32_t mode);
    status_t turnElectronBeamOnImplLocked(int32_t mode);
    status_t electronBeamOffAnimationImplLocked();
    status_t electronBeamOnAnimationImplLocked();

    /* ------------------------------------------------------------------------
     * EGL
     */
    static status_t selectConfigForPixelFormat(EGLDisplay dpy,
        EGLint const* attrs, PixelFormat format, EGLConfig* outConfig);
    static EGLConfig selectEGLConfig(EGLDisplay disp, EGLint visualId);
    static EGLContext createGLContext(EGLDisplay disp, EGLConfig config);
    void initializeGL(EGLDisplay display, EGLSurface surface);
    uint32_t getMaxTextureSize() const;
    uint32_t getMaxViewportDims() const;

    /* ------------------------------------------------------------------------
     * Display management
     */
    const DisplayHardware& getDisplayHardware(DisplayID dpy) const {
        return *mDisplayHardwares[dpy];
    }

    /* ------------------------------------------------------------------------
     * Compositing
     */
    void invalidateHwcGeometry();
    void computeVisibleRegions(const LayerVector& currentLayers,
        Region& dirtyRegion, Region& wormholeRegion);
    void postFramebuffer();
    void setupHardwareComposer(const DisplayHardware& hw);
    void composeSurfaces(const DisplayHardware& hw, const Region& dirty);
    void setInvalidateRegion(const Region& reg);
    Region getAndClearInvalidateRegion();
    void drawWormhole() const;
    GLuint getProtectedTexName() const {
        return mProtectedTexName;
    }

    /* ------------------------------------------------------------------------
     * Debugging & dumpsys
     */
    void debugFlashRegions(const DisplayHardware& hw);
    void listLayersLocked(const Vector<String16>& args, size_t& index,
        String8& result, char* buffer, size_t SIZE) const;
    void dumpStatsLocked(const Vector<String16>& args, size_t& index,
        String8& result, char* buffer, size_t SIZE) const;
    void clearStatsLocked(const Vector<String16>& args, size_t& index,
        String8& result, char* buffer, size_t SIZE) const;
    void dumpAllLocked(String8& result, char* buffer, size_t SIZE) const;

    /* ------------------------------------------------------------------------
     * Attributes
     */

    // access must be protected by mStateLock
    mutable Mutex mStateLock;
    State mCurrentState;
    volatile int32_t mTransactionFlags;
    Condition mTransactionCV;
    SortedVector<sp<LayerBase> > mLayerPurgatory;
    bool mTransationPending;
    Vector<sp<LayerBase> > mLayersPendingRemoval;

    // protected by mStateLock (but we could use another lock)
    DisplayHardware* mDisplayHardwares[1];
    bool mLayersRemoved;

    // access must be protected by mInvalidateLock
    mutable Mutex mInvalidateLock;
    Region mInvalidateRegion;

    // constant members (no synchronization needed for access)
    sp<IMemoryHeap> mServerHeap;
    surface_flinger_cblk_t* mServerCblk;
    GLuint mWormholeTexName;
    GLuint mProtectedTexName;
    nsecs_t mBootTime;
    sp<EventThread> mEventThread;
    GLint mMaxViewportDims[2];
    GLint mMaxTextureSize;
    EGLContext mEGLContext;
    EGLConfig mEGLConfig;

    // Can only accessed from the main thread, these members
    // don't need synchronization
    State mDrawingState;
    Region mDirtyRegion;
    Region mDirtyRegionRemovedLayer;
    Region mSwapRegion;
    Region mWormholeRegion;
    bool mVisibleRegionsDirty;
    bool mHwWorkListDirty;
    int32_t mElectronBeamAnimationMode;

    // don't use a lock for these, we don't care
    int mDebugRegion;
    int mDebugDDMS;
    int mDebugDisableHWC;
    int mDebugDisableTransformHint;
    volatile nsecs_t mDebugInSwapBuffers;
    nsecs_t mLastSwapBufferTime;
    volatile nsecs_t mDebugInTransaction;
    nsecs_t mLastTransactionTime;
    bool mBootFinished;

    // these are thread safe
    mutable MessageQueue mEventQueue;
    mutable Barrier mReadyToRunBarrier;

    // protected by mDestroyedLayerLock;
    mutable Mutex mDestroyedLayerLock;
    Vector<LayerBase const *> mDestroyedLayers;

    /* ------------------------------------------------------------------------
     * Feature prototyping
     */

    EGLSurface getExternalDisplaySurface() const;
    sp<SurfaceTextureClient> mExternalDisplayNativeWindow;
    EGLSurface mExternalDisplaySurface;
public:
    surface_flinger_cblk_t* getControlBlock() const;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SURFACE_FLINGER_H
