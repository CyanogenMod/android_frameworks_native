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

#include <hardware/hwcomposer_defs.h>

#include <private/gui/LayerState.h>

#include "Barrier.h"
#include "MessageQueue.h"

#include "DisplayHardware/HWComposer.h"

namespace android {

// ---------------------------------------------------------------------------

class Client;
class DisplayEventConnection;
class DisplayDevice;
class EventThread;
class Layer;
class LayerBase;
class LayerBaseClient;
class LayerDim;
class LayerScreenshot;
class SurfaceTextureClient;

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
                       private Thread,
                       private HWComposer::EventHandler
{
public:
    static char const* getServiceName() {
        return "SurfaceFlinger";
    }

    SurfaceFlinger();

    enum {
        EVENT_VSYNC = HWC_EVENT_VSYNC
    };

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
    const DisplayDevice& getDefaultDisplayDevice() const {
        return getDisplayDevice(0);
    }

    // utility function to delete a texture on the main thread
    void deleteTextureAsync(GLuint texture);


    // enable/disable h/w composer event
    // TODO: this should be made accessible only to EventThread
    void eventControl(int event, int enabled);

    // called on the main thread by MessageQueue when an internal message
    // is received
    // TODO: this should be made accessible only to MessageQueue
    void onMessageReceived(int32_t what);

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
    virtual void bootFinished();
    virtual void setTransactionState(const Vector<ComposerState>& state,
            const Vector<DisplayState>& displays, uint32_t flags);
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
    virtual status_t getDisplayInfo(DisplayID dpy, DisplayInfo* info);
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
     * HWComposer::EventHandler interface
     */
    virtual void onVSyncReceived(int dpy, nsecs_t timestamp);

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

    void handleTransaction(uint32_t transactionFlags);
    void handleTransactionLocked(uint32_t transactionFlags);

    /* handlePageFilp: this is were we latch a new buffer
     * if available and compute the dirty region.
     */
    void handlePageFlip();

    void handleRefresh();
    void handleRepaint(const DisplayDevice& hw, const Region& dirtyRegion);

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
     * Display and layer stack management
     */
    const DisplayDevice& getDisplayDevice(DisplayID dpy) const {
        return *mDisplayDevices[dpy];
    }

    // mark a region of a layer stack dirty. this updates the dirty
    // region of all screens presenting this layer stack.
    void invalidateLayerStack(uint32_t layerStack, const Region& dirty);

    /* ------------------------------------------------------------------------
     * H/W composer
     */

    HWComposer& getHwComposer() const { return *mHwc; }

        /* ------------------------------------------------------------------------
     * Compositing
     */
    void invalidateHwcGeometry();
    void computeVisibleRegions(const LayerVector& currentLayers,
            uint32_t layerStack,
            Region& dirtyRegion, Region& opaqueRegion);
    void postFramebuffer();
    void composeSurfaces(const DisplayDevice& hw, const Region& dirty);
    void drawWormhole(const Region& region) const;
    GLuint getProtectedTexName() const {
        return mProtectedTexName;
    }

    /* ------------------------------------------------------------------------
     * Debugging & dumpsys
     */
    void debugFlashRegions(const DisplayDevice& hw, const Region& dirtyReg);
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
    DisplayDevice* mDisplayDevices[1];
    bool mLayersRemoved;

    // access must be protected by mInvalidateLock
    volatile int32_t mRepaintEverything;

    // constant members (no synchronization needed for access)
    HWComposer* mHwc;
    GLuint mProtectedTexName;
    nsecs_t mBootTime;
    sp<EventThread> mEventThread;
    GLint mMaxViewportDims[2];
    GLint mMaxTextureSize;
    EGLContext mEGLContext;
    EGLConfig mEGLConfig;
    EGLDisplay mEGLDisplay;

    // Can only accessed from the main thread, these members
    // don't need synchronization
    State mDrawingState;
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
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SURFACE_FLINGER_H
