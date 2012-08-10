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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

// Uncomment this to remove support for HWC_DEVICE_API_VERSION_0_3 and older
// #define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/Thread.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#include <ui/GraphicBuffer.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include "Layer.h"           // needed only for debugging
#include "LayerBase.h"
#include "HWComposer.h"
#include "SurfaceFlinger.h"

namespace android {

// ---------------------------------------------------------------------------
// Support for HWC_DEVICE_API_VERSION_0_3 and older:
// Since v0.3 is deprecated and support will be dropped soon, as much as
// possible the code is written to target v1.0. When using a v0.3 HWC, we
// allocate v0.3 structures, but assign them to v1.0 pointers.

#if HWC_REMOVE_DEPRECATED_VERSIONS
// We need complete types to satisfy semantic checks, even though the code
// paths that use these won't get executed at runtime (and will likely be dead-
// code-eliminated). When we remove the code to support v0.3 we can remove
// these as well.
typedef hwc_layer_1_t hwc_layer_t;
typedef hwc_display_contents_1_t hwc_layer_list_t;
typedef hwc_composer_device_1_t hwc_composer_device_t;
#endif

// This function assumes we've already rejected HWC's with lower-than-required
// versions. Don't use it for the initial "does HWC meet requirements" check!
static bool hwcHasVersion(const hwc_composer_device_1_t* hwc, uint32_t version) {
    if (HWC_REMOVE_DEPRECATED_VERSIONS &&
            version <= HWC_DEVICE_API_VERSION_1_0) {
        return true;
    } else {
        return hwc->common.version >= version;
    }
}

static bool hwcHasVsyncEvent(const hwc_composer_device_1_t* hwc) {
    return hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_0_3);
}

static size_t sizeofHwcLayerList(const hwc_composer_device_1_t* hwc,
        size_t numLayers) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return sizeof(hwc_display_contents_1_t) + numLayers*sizeof(hwc_layer_1_t);
    } else {
        return sizeof(hwc_layer_list_t) + numLayers*sizeof(hwc_layer_t);
    }
}

static int hwcEventControl(hwc_composer_device_1_t* hwc, int dpy,
        int event, int enabled) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return hwc->methods->eventControl(hwc, dpy, event, enabled);
    } else {
        hwc_composer_device_t* hwc0 = reinterpret_cast<hwc_composer_device_t*>(hwc);
        return hwc0->methods->eventControl(hwc0, event, enabled);
    }
}

static int hwcBlank(hwc_composer_device_1_t* hwc, int dpy, int blank) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return hwc->methods->blank(hwc, dpy, blank);
    } else {
        if (blank) {
            hwc_composer_device_t* hwc0 = reinterpret_cast<hwc_composer_device_t*>(hwc);
            return hwc0->set(hwc0, NULL, NULL, NULL);
        } else {
            // HWC 0.x turns the screen on at the next set()
            return NO_ERROR;
        }
    }
}

static int hwcPrepare(hwc_composer_device_1_t* hwc,
        size_t numDisplays, hwc_display_contents_1_t** displays) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return hwc->prepare(hwc, numDisplays, displays);
    } else {
        hwc_composer_device_t* hwc0 = reinterpret_cast<hwc_composer_device_t*>(hwc);
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(displays[0]);
        // In the past, SurfaceFlinger would pass a NULL list when doing full
        // OpenGL ES composition. I don't know what, if any, dependencies there
        // are on this behavior, so I'm playing it safe and preserving it.
        if (list0->numHwLayers == 0)
            return hwc0->prepare(hwc0, NULL);
        else
            return hwc0->prepare(hwc0, list0);
    }
}

static int hwcSet(hwc_composer_device_1_t* hwc, EGLDisplay dpy, EGLSurface sur,
        size_t numDisplays, hwc_display_contents_1_t** displays) {
    int err;
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        displays[0]->dpy = dpy;
        displays[0]->sur = sur;
        err = hwc->set(hwc, numDisplays, displays);
    } else {
        hwc_composer_device_t* hwc0 = reinterpret_cast<hwc_composer_device_t*>(hwc);
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(displays[0]);
        err = hwc0->set(hwc0, dpy, sur, list0);
    }
    return err;
}

static uint32_t& hwcFlags(hwc_composer_device_1_t* hwc,
        hwc_display_contents_1_t* display) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return display->flags;
    } else {
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(display);
        return list0->flags;
    }
}

static size_t& hwcNumHwLayers(hwc_composer_device_1_t* hwc,
        hwc_display_contents_1_t* display) {
    if (hwcHasVersion(hwc, HWC_DEVICE_API_VERSION_1_0)) {
        return display->numHwLayers;
    } else {
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(display);
        return list0->numHwLayers;
    }
}

// ---------------------------------------------------------------------------

struct HWComposer::cb_context {
    struct callbacks : public hwc_procs_t {
        // these are here to facilitate the transition when adding
        // new callbacks (an implementation can check for NULL before
        // calling a new callback).
        void (*zero[4])(void);
    };
    callbacks procs;
    HWComposer* hwc;
};

// ---------------------------------------------------------------------------

HWComposer::HWComposer(
        const sp<SurfaceFlinger>& flinger,
        EventHandler& handler)
    : mFlinger(flinger),
      mModule(0), mHwc(0), mCapacity(0),
      mNumOVLayers(0), mNumFBLayers(0),
      mCBContext(new cb_context),
      mEventHandler(handler), mRefreshPeriod(0),
      mVSyncCount(0), mDebugForceFakeVSync(false)
{
    for (size_t i = 0; i < MAX_DISPLAYS; i++)
        mLists[i] = NULL;

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sf.no_hw_vsync", value, "0");
    mDebugForceFakeVSync = atoi(value);

    bool needVSyncThread = true;
    int err = hw_get_module(HWC_HARDWARE_MODULE_ID, &mModule);
    ALOGW_IF(err, "%s module not found", HWC_HARDWARE_MODULE_ID);
    if (err == 0) {
        err = hwc_open_1(mModule, &mHwc);
        ALOGE_IF(err, "%s device failed to initialize (%s)",
                HWC_HARDWARE_COMPOSER, strerror(-err));
        if (err == 0) {
            if (HWC_REMOVE_DEPRECATED_VERSIONS &&
                    mHwc->common.version < HWC_DEVICE_API_VERSION_1_0) {
                ALOGE("%s device version %#x too old, will not be used",
                        HWC_HARDWARE_COMPOSER, mHwc->common.version);
                hwc_close_1(mHwc);
                mHwc = NULL;
            }
        }

        if (mHwc) {
            // always turn vsync off when we start
            needVSyncThread = false;
            if (hwcHasVsyncEvent(mHwc)) {
                hwcEventControl(mHwc, 0, HWC_EVENT_VSYNC, 0);

                int period;
                if (mHwc->query(mHwc, HWC_VSYNC_PERIOD, &period) == NO_ERROR) {
                    mRefreshPeriod = nsecs_t(period);
                }
            } else {
                needVSyncThread = true;
            }

            if (mHwc->registerProcs) {
                mCBContext->hwc = this;
                mCBContext->procs.invalidate = &hook_invalidate;
                mCBContext->procs.vsync = &hook_vsync;
                mHwc->registerProcs(mHwc, &mCBContext->procs);
                memset(mCBContext->procs.zero, 0, sizeof(mCBContext->procs.zero));
            }

            // create initial empty display contents for display 0
            createWorkList(MAIN, 0);
        }
    }

    if (mRefreshPeriod == 0) {
        // for compatibility, we attempt to get the refresh rate from
        // the FB HAL if we couldn't get it from the HWC HAL.
        hw_module_t const* module;
        if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
            framebuffer_device_t* fbDev;
            int err = framebuffer_open(module, &fbDev);
            if (!err && fbDev) {
                mRefreshPeriod = nsecs_t(1e9 / fbDev->fps);
                framebuffer_close(fbDev);
            }
        }
        ALOGW("getting VSYNC period from fb HAL: %lld", mRefreshPeriod);
    }

    if (mRefreshPeriod == 0) {
        mRefreshPeriod = nsecs_t(1e9 / 60.0);
        ALOGW("getting VSYNC period thin air: %lld", mRefreshPeriod);
    }

    if (needVSyncThread) {
        // we don't have VSYNC support, we need to fake it
        mVSyncThread = new VSyncThread(*this);
    }
}

HWComposer::~HWComposer() {
    hwcEventControl(mHwc, 0, EVENT_VSYNC, 0);
    for (size_t i = 0; i < MAX_DISPLAYS; i++)
        free(mLists[i]);
    if (mVSyncThread != NULL) {
        mVSyncThread->requestExitAndWait();
    }
    if (mHwc) {
        hwc_close_1(mHwc);
    }
    delete mCBContext;
}

status_t HWComposer::initCheck() const {
    return mHwc ? NO_ERROR : NO_INIT;
}

void HWComposer::hook_invalidate(struct hwc_procs* procs) {
    reinterpret_cast<cb_context *>(procs)->hwc->invalidate();
}

void HWComposer::hook_vsync(struct hwc_procs* procs, int dpy, int64_t timestamp) {
    reinterpret_cast<cb_context *>(procs)->hwc->vsync(dpy, timestamp);
}

void HWComposer::invalidate() {
    mFlinger->repaintEverything();
}

void HWComposer::vsync(int dpy, int64_t timestamp) {
    ATRACE_INT("VSYNC", ++mVSyncCount&1);
    mEventHandler.onVSyncReceived(dpy, timestamp);
    Mutex::Autolock _l(mLock);
    mLastHwVSync = timestamp;
}

nsecs_t HWComposer::getRefreshPeriod() const {
    return mRefreshPeriod;
}

nsecs_t HWComposer::getRefreshTimestamp() const {
    // this returns the last refresh timestamp.
    // if the last one is not available, we estimate it based on
    // the refresh period and whatever closest timestamp we have.
    Mutex::Autolock _l(mLock);
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    return now - ((now - mLastHwVSync) %  mRefreshPeriod);
}

void HWComposer::eventControl(int event, int enabled) {
    status_t err = NO_ERROR;
    if (mHwc && mHwc->common.version >= HWC_DEVICE_API_VERSION_0_3) {
        if (!mDebugForceFakeVSync) {
            err = hwcEventControl(mHwc, 0, event, enabled);
            // error here should not happen -- not sure what we should
            // do if it does.
            ALOGE_IF(err, "eventControl(%d, %d) failed %s",
                    event, enabled, strerror(-err));
        }
    }

    if (err == NO_ERROR && mVSyncThread != NULL) {
        mVSyncThread->setEnabled(enabled);
    }
}

status_t HWComposer::createWorkList(int32_t id, size_t numLayers) { // FIXME: handle multiple displays
    if (uint32_t(id) >= MAX_DISPLAYS)
        return BAD_INDEX;

    if (mHwc) {
        // TODO: must handle multiple displays here
        // mLists[0] is NULL only when this is called from the constructor
        if (!mLists[0] || mCapacity < numLayers) {
            free(mLists[0]);
            size_t size = sizeofHwcLayerList(mHwc, numLayers);
            mLists[0] = (hwc_display_contents_1_t*)malloc(size);
            mCapacity = numLayers;
        }
        hwcFlags(mHwc, mLists[0]) = HWC_GEOMETRY_CHANGED;
        hwcNumHwLayers(mHwc, mLists[0]) = numLayers;
        if (hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_1_0)) {
            mLists[0]->flipFenceFd = -1;
        }
    }
    return NO_ERROR;
}

status_t HWComposer::prepare() const {
    int err = hwcPrepare(mHwc, 1,
            const_cast<hwc_display_contents_1_t**>(mLists));
    if (err == NO_ERROR) {
        size_t numOVLayers = 0;
        size_t numFBLayers = 0;
        size_t count = getNumLayers(0);

        for (size_t i=0 ; i<count ; i++) {
            int compositionType;
            if (hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_1_0)) {
                hwc_layer_1_t* l = &mLists[0]->hwLayers[i];
                if (l->flags & HWC_SKIP_LAYER) {
                    l->compositionType = HWC_FRAMEBUFFER;
                }
                compositionType = l->compositionType;
            } else {
                // mList really has hwc_layer_list_t memory layout
                hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(mLists[0]);
                hwc_layer_t* l = &list0->hwLayers[i];
                if (l->flags & HWC_SKIP_LAYER) {
                    l->compositionType = HWC_FRAMEBUFFER;
                }
                compositionType = l->compositionType;
            }

            switch (compositionType) {
                case HWC_OVERLAY:
                    numOVLayers++;
                    break;
                case HWC_FRAMEBUFFER:
                    numFBLayers++;
                    break;
            }
        }
        mNumOVLayers = numOVLayers;
        mNumFBLayers = numFBLayers;
    }
    return (status_t)err;
}

size_t HWComposer::getLayerCount(int32_t id, int type) const { // FIXME: handle multiple displays
    switch (type) {
        case HWC_OVERLAY:
            return mNumOVLayers;
        case HWC_FRAMEBUFFER:
            return mNumFBLayers;
    }
    return 0;
}

status_t HWComposer::commit(void* fbDisplay, void* fbSurface) const {
    int err = NO_ERROR;
    if (mHwc) {
        err = hwcSet(mHwc, fbDisplay, fbSurface, 1,
                const_cast<hwc_display_contents_1_t**>(mLists));
        if (hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_1_0)) {
            if (mLists[0]->flipFenceFd != -1) {
                close(mLists[0]->flipFenceFd);
                mLists[0]->flipFenceFd = -1;
            }
        }
        hwcFlags(mHwc, mLists[0]) &= ~HWC_GEOMETRY_CHANGED;
    }
    return (status_t)err;
}

status_t HWComposer::release() const {
    if (mHwc) {
        if (hwcHasVsyncEvent(mHwc)) {
            hwcEventControl(mHwc, 0, HWC_EVENT_VSYNC, 0);
        }
        return (status_t)hwcBlank(mHwc, 0, 1);
    }
    return NO_ERROR;
}

status_t HWComposer::acquire() const {
    if (mHwc) {
        return (status_t)hwcBlank(mHwc, 0, 0);
    }
    return NO_ERROR;
}

status_t HWComposer::disable() {
    if (mHwc) {
        hwcNumHwLayers(mHwc, mLists[0]) = 0;
        int err = hwcPrepare(mHwc, 1, mLists);
        return (status_t)err;
    }
    return NO_ERROR;
}

size_t HWComposer::getNumLayers(int32_t id) const { // FIXME: handle multiple displays
    return mHwc ? hwcNumHwLayers(mHwc, mLists[0]) : 0;
}

/*
 * Helper template to implement a concrete HWCLayer
 * This holds the pointer to the concrete hwc layer type
 * and implements the "iterable" side of HWCLayer.
 */
template<typename CONCRETE, typename HWCTYPE>
class Iterable : public HWComposer::HWCLayer {
protected:
    HWCTYPE* const mLayerList;
    HWCTYPE* mCurrentLayer;
    Iterable(HWCTYPE* layer) : mLayerList(layer), mCurrentLayer(layer) { }
    inline HWCTYPE const * getLayer() const { return mCurrentLayer; }
    inline HWCTYPE* getLayer() { return mCurrentLayer; }
    virtual ~Iterable() { }
private:
    // returns a copy of ourselves
    virtual HWComposer::HWCLayer* dup() {
        return new CONCRETE( static_cast<const CONCRETE&>(*this) );
    }
    virtual status_t setLayer(size_t index) {
        mCurrentLayer = &mLayerList[index];
        return NO_ERROR;
    }
};

// #if !HWC_REMOVE_DEPRECATED_VERSIONS
/*
 * Concrete implementation of HWCLayer for HWC_DEVICE_API_VERSION_0_3
 * This implements the HWCLayer side of HWCIterableLayer.
 */
class HWCLayerVersion0 : public Iterable<HWCLayerVersion0, hwc_layer_t> {
public:
    HWCLayerVersion0(hwc_layer_t* layer)
        : Iterable<HWCLayerVersion0, hwc_layer_t>(layer) { }

    virtual int32_t getCompositionType() const {
        return getLayer()->compositionType;
    }
    virtual uint32_t getHints() const {
        return getLayer()->hints;
    }
    virtual int getAndResetReleaseFenceFd() {
        // not supported on VERSION_03
        return -1;
    }
    virtual void setAcquireFenceFd(int fenceFd) {
        if (fenceFd != -1) {
            ALOGE("HWC 0.x can't handle acquire fences");
            close(fenceFd);
        }
    }

    virtual void setDefaultState() {
        getLayer()->compositionType = HWC_FRAMEBUFFER;
        getLayer()->hints = 0;
        getLayer()->flags = HWC_SKIP_LAYER;
        getLayer()->transform = 0;
        getLayer()->blending = HWC_BLENDING_NONE;
        getLayer()->visibleRegionScreen.numRects = 0;
        getLayer()->visibleRegionScreen.rects = NULL;
    }
    virtual void setSkip(bool skip) {
        if (skip) {
            getLayer()->flags |= HWC_SKIP_LAYER;
        } else {
            getLayer()->flags &= ~HWC_SKIP_LAYER;
        }
    }
    virtual void setBlending(uint32_t blending) {
        getLayer()->blending = blending;
    }
    virtual void setTransform(uint32_t transform) {
        getLayer()->transform = transform;
    }
    virtual void setFrame(const Rect& frame) {
        reinterpret_cast<Rect&>(getLayer()->displayFrame) = frame;
    }
    virtual void setCrop(const Rect& crop) {
        reinterpret_cast<Rect&>(getLayer()->sourceCrop) = crop;
    }
    virtual void setVisibleRegionScreen(const Region& reg) {
        getLayer()->visibleRegionScreen.rects =
                reinterpret_cast<hwc_rect_t const *>(
                        reg.getArray(&getLayer()->visibleRegionScreen.numRects));
    }
    virtual void setBuffer(const sp<GraphicBuffer>& buffer) {
        if (buffer == 0 || buffer->handle == 0) {
            getLayer()->compositionType = HWC_FRAMEBUFFER;
            getLayer()->flags |= HWC_SKIP_LAYER;
            getLayer()->handle = 0;
        } else {
            getLayer()->handle = buffer->handle;
        }
    }
};
// #endif // !HWC_REMOVE_DEPRECATED_VERSIONS

/*
 * Concrete implementation of HWCLayer for HWC_DEVICE_API_VERSION_1_0.
 * This implements the HWCLayer side of HWCIterableLayer.
 */
class HWCLayerVersion1 : public Iterable<HWCLayerVersion1, hwc_layer_1_t> {
public:
    HWCLayerVersion1(hwc_layer_1_t* layer)
        : Iterable<HWCLayerVersion1, hwc_layer_1_t>(layer) { }

    virtual int32_t getCompositionType() const {
        return getLayer()->compositionType;
    }
    virtual uint32_t getHints() const {
        return getLayer()->hints;
    }
    virtual int getAndResetReleaseFenceFd() {
        int fd = getLayer()->releaseFenceFd;
        getLayer()->releaseFenceFd = -1;
        return fd;
    }
    virtual void setAcquireFenceFd(int fenceFd) {
        getLayer()->acquireFenceFd = fenceFd;
    }

    virtual void setDefaultState() {
        getLayer()->compositionType = HWC_FRAMEBUFFER;
        getLayer()->hints = 0;
        getLayer()->flags = HWC_SKIP_LAYER;
        getLayer()->transform = 0;
        getLayer()->blending = HWC_BLENDING_NONE;
        getLayer()->visibleRegionScreen.numRects = 0;
        getLayer()->visibleRegionScreen.rects = NULL;
        getLayer()->acquireFenceFd = -1;
        getLayer()->releaseFenceFd = -1;
    }
    virtual void setSkip(bool skip) {
        if (skip) {
            getLayer()->flags |= HWC_SKIP_LAYER;
        } else {
            getLayer()->flags &= ~HWC_SKIP_LAYER;
        }
    }
    virtual void setBlending(uint32_t blending) {
        getLayer()->blending = blending;
    }
    virtual void setTransform(uint32_t transform) {
        getLayer()->transform = transform;
    }
    virtual void setFrame(const Rect& frame) {
        reinterpret_cast<Rect&>(getLayer()->displayFrame) = frame;
    }
    virtual void setCrop(const Rect& crop) {
        reinterpret_cast<Rect&>(getLayer()->sourceCrop) = crop;
    }
    virtual void setVisibleRegionScreen(const Region& reg) {
        getLayer()->visibleRegionScreen.rects =
                reinterpret_cast<hwc_rect_t const *>(
                        reg.getArray(&getLayer()->visibleRegionScreen.numRects));
    }
    virtual void setBuffer(const sp<GraphicBuffer>& buffer) {
        if (buffer == 0 || buffer->handle == 0) {
            getLayer()->compositionType = HWC_FRAMEBUFFER;
            getLayer()->flags |= HWC_SKIP_LAYER;
            getLayer()->handle = 0;
        } else {
            getLayer()->handle = buffer->handle;
        }
    }
};

/*
 * returns an iterator initialized at a given index in the layer list
 */
HWComposer::LayerListIterator HWComposer::getLayerIterator(int32_t id, size_t index) { // FIXME: handle multiple displays
    if (!mHwc || index > hwcNumHwLayers(mHwc, mLists[0]))
        return LayerListIterator();
    if (hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_1_0)) {
        return LayerListIterator(new HWCLayerVersion1(mLists[0]->hwLayers),
                index);
    } else {
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(mLists[0]);
        return LayerListIterator(new HWCLayerVersion0(list0->hwLayers), index);
    }
}

/*
 * returns an iterator on the beginning of the layer list
 */
HWComposer::LayerListIterator HWComposer::begin(int32_t id) { // FIXME: handle multiple displays
    return getLayerIterator(id, 0);
}

/*
 * returns an iterator on the end of the layer list
 */
HWComposer::LayerListIterator HWComposer::end(int32_t id) { // FIXME: handle multiple displays
    return getLayerIterator(id, getNumLayers(id));
}

void HWComposer::dump(String8& result, char* buffer, size_t SIZE,
        const Vector< sp<LayerBase> >& visibleLayersSortedByZ) const {
    if (mHwc) {
        hwc_layer_list_t* list0 = reinterpret_cast<hwc_layer_list_t*>(mLists[0]);

        result.append("Hardware Composer state:\n");
        result.appendFormat("  mDebugForceFakeVSync=%d\n",
                mDebugForceFakeVSync);
        result.appendFormat("  numHwLayers=%u, flags=%08x\n",
                hwcNumHwLayers(mHwc, mLists[0]), hwcFlags(mHwc, mLists[0]));
        result.append(
                "   type   |  handle  |   hints  |   flags  | tr | blend |  format  |       source crop         |           frame           name \n"
                "----------+----------+----------+----------+----+-------+----------+---------------------------+--------------------------------\n");
        //      " ________ | ________ | ________ | ________ | __ | _____ | ________ | [_____,_____,_____,_____] | [_____,_____,_____,_____]
        for (size_t i=0 ; i<hwcNumHwLayers(mHwc, mLists[0]) ; i++) {
            hwc_layer_1_t const* lp;
            if (hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_1_0)) {
                lp = &mLists[0]->hwLayers[i];
            } else {
                // FIXME: here we rely on hwc_layer_1_t and hwc_layer_t having the same layout
                lp = reinterpret_cast<hwc_layer_1_t const*>(&list0->hwLayers[i]);
            }
            const sp<LayerBase> layer(visibleLayersSortedByZ[i]);
            int32_t format = -1;
            if (layer->getLayer() != NULL) {
                const sp<GraphicBuffer>& buffer(layer->getLayer()->getActiveBuffer());
                if (buffer != NULL) {
                    format = buffer->getPixelFormat();
                }
            }
            const hwc_layer_1_t& l(*lp);
            result.appendFormat(
                    " %8s | %08x | %08x | %08x | %02x | %05x | %08x | [%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] %s\n",
                    l.compositionType ? "OVERLAY" : "FB",
                    intptr_t(l.handle), l.hints, l.flags, l.transform, l.blending, format,
                    l.sourceCrop.left, l.sourceCrop.top, l.sourceCrop.right, l.sourceCrop.bottom,
                    l.displayFrame.left, l.displayFrame.top, l.displayFrame.right, l.displayFrame.bottom,
                    layer->getName().string());
        }
    }
    if (mHwc && hwcHasVersion(mHwc, HWC_DEVICE_API_VERSION_0_1) && mHwc->dump) {
        mHwc->dump(mHwc, buffer, SIZE);
        result.append(buffer);
    }
}

// ---------------------------------------------------------------------------

HWComposer::VSyncThread::VSyncThread(HWComposer& hwc)
    : mHwc(hwc), mEnabled(false),
      mNextFakeVSync(0),
      mRefreshPeriod(hwc.mRefreshPeriod)
{
}

void HWComposer::VSyncThread::setEnabled(bool enabled) {
    Mutex::Autolock _l(mLock);
    mEnabled = enabled;
    mCondition.signal();
}

void HWComposer::VSyncThread::onFirstRef() {
    run("VSyncThread", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
}

bool HWComposer::VSyncThread::threadLoop() {
    { // scope for lock
        Mutex::Autolock _l(mLock);
        while (!mEnabled) {
            mCondition.wait(mLock);
        }
    }

    const nsecs_t period = mRefreshPeriod;
    const nsecs_t now = systemTime(CLOCK_MONOTONIC);
    nsecs_t next_vsync = mNextFakeVSync;
    nsecs_t sleep = next_vsync - now;
    if (sleep < 0) {
        // we missed, find where the next vsync should be
        sleep = (period - ((now - next_vsync) % period));
        next_vsync = now + sleep;
    }
    mNextFakeVSync = next_vsync + period;

    struct timespec spec;
    spec.tv_sec  = next_vsync / 1000000000;
    spec.tv_nsec = next_vsync % 1000000000;

    int err;
    do {
        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
    } while (err<0 && errno == EINTR);

    if (err == 0) {
        mHwc.mEventHandler.onVSyncReceived(0, next_vsync);
    }

    return true;
}

// ---------------------------------------------------------------------------
}; // namespace android
