/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ANDROID_SF_HWCOMPOSER_H
#define ANDROID_SF_HWCOMPOSER_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>

#include <hardware/hwcomposer.h>

#include <utils/StrongPointer.h>
#include <utils/Vector.h>

extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);

namespace android {
// ---------------------------------------------------------------------------

class String8;
class SurfaceFlinger;
class LayerBase;

class HWComposer
{
public:
    class EventHandler {
        friend class HWComposer;
        virtual void onVSyncReceived(int dpy, nsecs_t timestamp) = 0;
    protected:
        virtual ~EventHandler() {}
    };

    HWComposer(const sp<SurfaceFlinger>& flinger,
            EventHandler& handler, nsecs_t refreshPeriod);
    ~HWComposer();

    status_t initCheck() const;

    // tells the HAL what the framebuffer is
    void setFrameBuffer(EGLDisplay dpy, EGLSurface sur);

    // create a work list for numLayers layer. sets HWC_GEOMETRY_CHANGED.
    status_t createWorkList(size_t numLayers);

    // Asks the HAL what it can do
    status_t prepare() const;

    // disable hwc until next createWorkList
    status_t disable();

    // commits the list
    status_t commit() const;

    // release hardware resources
    status_t release() const;

    // get the layer array created by createWorkList()
    size_t getNumLayers() const;
    hwc_layer_t* getLayers() const;

    // get number of layers of the given type as updated in prepare().
    // type is HWC_OVERLAY or HWC_FRAMEBUFFER
    size_t getLayerCount(int type) const;

    // Events handling ---------------------------------------------------------

    enum {
        EVENT_VSYNC = HWC_EVENT_VSYNC
    };

    void eventControl(int event, int enabled);

    // this class is only used to fake the VSync event on systems that don't
    // have it.
    class VSyncThread : public Thread {
        HWComposer& mHwc;
        mutable Mutex mLock;
        Condition mCondition;
        bool mEnabled;
        mutable nsecs_t mNextFakeVSync;
        nsecs_t mRefreshPeriod;
        virtual void onFirstRef();
        virtual bool threadLoop();
    public:
        VSyncThread(HWComposer& hwc);
        void setEnabled(bool enabled);
    };

    friend class VSyncThread;

    // for debugging ----------------------------------------------------------
    void dump(String8& out, char* scratch, size_t SIZE,
            const Vector< sp<LayerBase> >& visibleLayersSortedByZ) const;

private:

    struct callbacks : public hwc_procs_t {
        // these are here to facilitate the transition when adding
        // new callbacks (an implementation can check for NULL before
        // calling a new callback).
        void (*zero[4])(void);
    };

    struct cb_context {
        callbacks procs;
        HWComposer* hwc;
    };

    static void hook_invalidate(struct hwc_procs* procs);
    static void hook_vsync(struct hwc_procs* procs, int dpy, int64_t timestamp);

    inline void invalidate();
    inline void vsync(int dpy, int64_t timestamp);

    sp<SurfaceFlinger>      mFlinger;
    hw_module_t const*      mModule;
    hwc_composer_device_t*  mHwc;
    hwc_layer_list_t*       mList;
    size_t                  mCapacity;
    mutable size_t          mNumOVLayers;
    mutable size_t          mNumFBLayers;
    hwc_display_t           mDpy;
    hwc_surface_t           mSur;
    cb_context              mCBContext;
    EventHandler&           mEventHandler;
    nsecs_t                 mRefreshPeriod;
    size_t                  mVSyncCount;
    sp<VSyncThread>         mVSyncThread;
    bool                    mDebugForceFakeVSync;
};


// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SF_HWCOMPOSER_H
