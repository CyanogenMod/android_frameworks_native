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

#ifndef ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H
#define ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H

#include <stdint.h>
#include <sys/types.h>

#include <binder/IBinder.h>

#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/SortedVector.h>
#include <utils/threads.h>

#include <ui/PixelFormat.h>

#include <gui/Surface.h>

namespace android {

// ---------------------------------------------------------------------------

class DisplayInfo;
class Composer;
class IMemoryHeap;
class ISurfaceComposerClient;
class ISurfaceTexture;
class Region;

// ---------------------------------------------------------------------------

class SurfaceComposerClient : public RefBase
{
    friend class Composer;
public:    
                SurfaceComposerClient();
    virtual     ~SurfaceComposerClient();

    // Always make sure we could initialize
    status_t    initCheck() const;

    // Return the connection of this client
    sp<IBinder> connection() const;
    
    // Forcibly remove connection before all references have gone away.
    void        dispose();

    // callback when the composer is dies
    status_t linkToComposerDeath(const sp<IBinder::DeathRecipient>& recipient,
            void* cookie = NULL, uint32_t flags = 0);

    // Get information about a display
    static status_t getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info);

    /* triggers screen off and waits for it to complete */
    static void blankDisplay(const sp<IBinder>& display);

    /* triggers screen on and waits for it to complete */
    static void unblankDisplay(const sp<IBinder>& display);
    // TODO: Remove me.  Do not use.
    // This is a compatibility shim for one product whose drivers are depending on
    // this legacy function (when they shouldn't).
    static status_t getDisplayInfo(int32_t displayId, DisplayInfo* info);

#if defined(ICS_CAMERA_BLOB) || defined(MR0_CAMERA_BLOB)
    static ssize_t getDisplayWidth(int32_t displayId);
    static ssize_t getDisplayHeight(int32_t displayId);
    static ssize_t getDisplayOrientation(int32_t displayId);
#endif

    // ------------------------------------------------------------------------
    // surface creation / destruction

    //! Create a surface
    sp<SurfaceControl> createSurface(
            const String8& name,// name of the surface
            uint32_t w,         // width in pixel
            uint32_t h,         // height in pixel
            PixelFormat format, // pixel-format desired
            uint32_t flags = 0  // usage flags
    );

    //! Create a display
    static sp<IBinder> createDisplay(const String8& displayName, bool secure);

    //! Get the token for the existing default displays.
    //! Possible values for id are eDisplayIdMain and eDisplayIdHdmi.
    static sp<IBinder> getBuiltInDisplay(int32_t id);

    // ------------------------------------------------------------------------
    // Composer parameters
    // All composer parameters must be changed within a transaction
    // several surfaces can be updated in one transaction, all changes are
    // committed at once when the transaction is closed.
    // closeGlobalTransaction() requires an IPC with the server.

    //! Open a composer transaction on all active SurfaceComposerClients.
    static void openGlobalTransaction();

    //! Close a composer transaction on all active SurfaceComposerClients.
    static void closeGlobalTransaction(bool synchronous = false);

    static int setOrientation(int32_t dpy, int orientation, uint32_t flags);

    //! Flag the currently open transaction as an animation transaction.
    static void setAnimationTransaction();

    status_t    hide(SurfaceID id);
    status_t    show(SurfaceID id);
    status_t    setFlags(SurfaceID id, uint32_t flags, uint32_t mask);
    status_t    setTransparentRegionHint(SurfaceID id, const Region& transparent);
    status_t    setLayer(SurfaceID id, int32_t layer);
    status_t    setAlpha(SurfaceID id, float alpha=1.0f);
    status_t    setMatrix(SurfaceID id, float dsdx, float dtdx, float dsdy, float dtdy);
    status_t    setPosition(SurfaceID id, float x, float y);
    status_t    setSize(SurfaceID id, uint32_t w, uint32_t h);
    status_t    setCrop(SurfaceID id, const Rect& crop);
    status_t    setLayerStack(SurfaceID id, uint32_t layerStack);
    status_t    destroySurface(SurfaceID sid);

    static void setDisplaySurface(const sp<IBinder>& token,
            const sp<ISurfaceTexture>& surface);
    static void setDisplayLayerStack(const sp<IBinder>& token,
            uint32_t layerStack);

    /* setDisplayProjection() defines the projection of layer stacks
     * to a given display.
     *
     * - orientation defines the display's orientation.
     * - layerStackRect defines which area of the window manager coordinate
     * space will be used.
     * - displayRect defines where on the display will layerStackRect be
     * mapped to. displayRect is specified post-orientation, that is
     * it uses the orientation seen by the end-user.
     */
    static void setDisplayProjection(const sp<IBinder>& token,
            uint32_t orientation,
            const Rect& layerStackRect,
            const Rect& displayRect);

private:
    virtual void onFirstRef();
    Composer& getComposer();

    mutable     Mutex                       mLock;
                status_t                    mStatus;
                sp<ISurfaceComposerClient>  mClient;
                Composer&                   mComposer;
};

// ---------------------------------------------------------------------------

class ScreenshotClient
{
    sp<IMemoryHeap> mHeap;
    uint32_t mWidth;
    uint32_t mHeight;
    PixelFormat mFormat;
public:
    ScreenshotClient();

    // TODO: Remove me.  Do not use.
    // This is a compatibility shim for one product whose drivers are depending on
    // this legacy function (when they shouldn't).
    status_t update();

    // frees the previous screenshot and capture a new one
    status_t update(const sp<IBinder>& display);
    status_t update(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight);
    status_t update(const sp<IBinder>& display,
            uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ);

    // release memory occupied by the screenshot
    void release();

    // pixels are valid until this object is freed or
    // release() or update() is called
    void const* getPixels() const;

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    PixelFormat getFormat() const;
    uint32_t getStride() const;
    // size of allocated memory in bytes
    size_t getSize() const;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_SURFACE_COMPOSER_CLIENT_H
