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

#include <hardware/hwcomposer_defs.h>

#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/StrongPointer.h>
#include <utils/Thread.h>
#include <utils/Timers.h>
#include <utils/Vector.h>

extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);

struct hwc_composer_device_1;
struct hwc_layer_list_1;
struct hwc_procs;

namespace android {
// ---------------------------------------------------------------------------

class GraphicBuffer;
class LayerBase;
class String8;
class SurfaceFlinger;

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

    // Asks the HAL what it can do
    status_t prepare() const;

    // disable hwc until next createWorkList
    status_t disable();

    // commits the list
    status_t commit(void* fbDisplay, void* fbSurface) const;

    // release hardware resources and blank screen
    status_t release() const;

    // acquire hardware resources and unblank screen
    status_t acquire() const;

    // create a work list for numLayers layer. sets HWC_GEOMETRY_CHANGED.
    status_t createWorkList(size_t numLayers);

    // get the layer array created by createWorkList()
    size_t getNumLayers() const;

    // get number of layers of the given type as updated in prepare().
    // type is HWC_OVERLAY or HWC_FRAMEBUFFER
    size_t getLayerCount(int type) const;

    // needed forward declarations
    class LayerListIterator;

    /*
     * Interface to hardware composer's layers functionality.
     * This abstracts the HAL interface to layers which can evolve in
     * incompatible ways from one release to another.
     * The idea is that we could extend this interface as we add
     * features to h/w composer.
     */
    class HWCLayerInterface {
    protected:
        virtual ~HWCLayerInterface() { }
    public:
        virtual int32_t getCompositionType() const = 0;
        virtual uint32_t getHints() const = 0;
        virtual int getAndResetReleaseFenceFd() = 0;
        virtual void setDefaultState() = 0;
        virtual void setSkip(bool skip) = 0;
        virtual void setBlending(uint32_t blending) = 0;
        virtual void setTransform(uint32_t transform) = 0;
        virtual void setFrame(const Rect& frame) = 0;
        virtual void setCrop(const Rect& crop) = 0;
        virtual void setVisibleRegionScreen(const Region& reg) = 0;
        virtual void setBuffer(const sp<GraphicBuffer>& buffer) = 0;
        virtual void setAcquireFenceFd(int fenceFd) = 0;
    };

    /*
     * Interface used to implement an iterator to a list
     * of HWCLayer.
     */
    class HWCLayer : public HWCLayerInterface {
        friend class LayerListIterator;
        // select the layer at the given index
        virtual status_t setLayer(size_t index) = 0;
        virtual HWCLayer* dup() = 0;
        static HWCLayer* copy(HWCLayer *rhs) {
            return rhs ? rhs->dup() : NULL;
        }
    protected:
        virtual ~HWCLayer() { }
    };

    /*
     * Iterator through a HWCLayer list.
     * This behaves more or less like a forward iterator.
     */
    class LayerListIterator {
        friend struct HWComposer;
        HWCLayer* const mLayerList;
        size_t mIndex;

        LayerListIterator() : mLayerList(NULL), mIndex(0) { }

        LayerListIterator(HWCLayer* layer, size_t index)
            : mLayerList(layer), mIndex(index) { }

        // we don't allow assignment, because we don't need it for now
        LayerListIterator& operator = (const LayerListIterator& rhs);

    public:
        // copy operators
        LayerListIterator(const LayerListIterator& rhs)
            : mLayerList(HWCLayer::copy(rhs.mLayerList)), mIndex(rhs.mIndex) {
        }

        ~LayerListIterator() { delete mLayerList; }

        // pre-increment
        LayerListIterator& operator++() {
            mLayerList->setLayer(++mIndex);
            return *this;
        }

        // dereference
        HWCLayerInterface& operator * () { return *mLayerList; }
        HWCLayerInterface* operator -> () { return mLayerList; }

        // comparison
        bool operator == (const LayerListIterator& rhs) const {
            return mIndex == rhs.mIndex;
        }
        bool operator != (const LayerListIterator& rhs) const {
            return !operator==(rhs);
        }
    };

    // Returns an iterator to the beginning of the layer list
    LayerListIterator begin();

    // Returns an iterator to the end of the layer list
    LayerListIterator end();


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
    LayerListIterator getLayerIterator(size_t index);

    struct cb_context;

    static void hook_invalidate(struct hwc_procs* procs);
    static void hook_vsync(struct hwc_procs* procs, int dpy, int64_t timestamp);

    inline void invalidate();
    inline void vsync(int dpy, int64_t timestamp);

    sp<SurfaceFlinger>              mFlinger;
    hw_module_t const*              mModule;
    struct hwc_composer_device_1*   mHwc;
    struct hwc_layer_list_1*        mList;
    size_t                          mCapacity;
    mutable size_t                  mNumOVLayers;
    mutable size_t                  mNumFBLayers;
    cb_context*                     mCBContext;
    EventHandler&                   mEventHandler;
    nsecs_t                         mRefreshPeriod;
    size_t                          mVSyncCount;
    sp<VSyncThread>                 mVSyncThread;
    bool                            mDebugForceFakeVSync;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SF_HWCOMPOSER_H
