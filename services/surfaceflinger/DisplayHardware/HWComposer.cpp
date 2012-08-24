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
#define HWC_REMOVE_DEPRECATED_VERSIONS 1

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

static bool hwcHasVersion(const hwc_composer_device_1_t* hwc, uint32_t version) {
    return hwc->common.version >= version;
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
        EventHandler& handler,
        framebuffer_device_t const* fbDev)
    : mFlinger(flinger),
      mModule(0), mHwc(0), mNumDisplays(1),
      mCBContext(new cb_context),
      mEventHandler(handler),
      mVSyncCount(0), mDebugForceFakeVSync(false)
{
    for (size_t i =0 ; i<MAX_DISPLAYS ; i++) {
        mLists[i] = 0;
    }

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
            if (mHwc->common.version < HWC_DEVICE_API_VERSION_1_0) {
                ALOGE("%s device version %#x too old, will not be used",
                        HWC_HARDWARE_COMPOSER, mHwc->common.version);
                hwc_close_1(mHwc);
                mHwc = NULL;
            }
        }

        if (mHwc) {
            if (mHwc->registerProcs) {
                mCBContext->hwc = this;
                mCBContext->procs.invalidate = &hook_invalidate;
                mCBContext->procs.vsync = &hook_vsync;
                memset(mCBContext->procs.zero, 0, sizeof(mCBContext->procs.zero));
                mHwc->registerProcs(mHwc, &mCBContext->procs);
            }

            // always turn vsync off when we start
            needVSyncThread = false;
            mHwc->eventControl(mHwc, 0, HWC_EVENT_VSYNC, 0);

            int period;
            if (mHwc->query(mHwc, HWC_VSYNC_PERIOD, &period) == NO_ERROR) {
                mDisplayData[HWC_DISPLAY_PRIMARY].refresh = nsecs_t(period);
            }

            // these IDs are always reserved
            for (size_t i=0 ; i<HWC_NUM_DISPLAY_TYPES ; i++) {
                mAllocatedDisplayIDs.markBit(i);
                // TODO: we query xdpi / ydpi / refresh
            }

            // the number of displays we actually have depends on the
            // hw composer version
            if (mHwc->common.version == HWC_DEVICE_API_VERSION_1_1) {
                // 1.1 adds support for multiple displays
                mNumDisplays = HWC_NUM_DISPLAY_TYPES;
            } else if (mHwc->common.version > HWC_DEVICE_API_VERSION_1_1) {
                // 1.2 adds support for virtual displays
                mNumDisplays = MAX_DISPLAYS;
            }
        }
    }

    if (fbDev) {
        // if we're here it means we are on version 1.0
        DisplayData& disp(mDisplayData[HWC_DISPLAY_PRIMARY]);
        disp.xdpi = fbDev->xdpi;
        disp.ydpi = fbDev->ydpi;
        if (disp.refresh == 0) {
            disp.refresh = nsecs_t(1e9 / fbDev->fps);
            ALOGW("getting VSYNC period from fb HAL: %lld", disp.refresh);
        }
        if (disp.refresh == 0) {
            disp.refresh = nsecs_t(1e9 / 60.0);
            ALOGW("getting VSYNC period thin air: %lld", mDisplayData[HWC_DISPLAY_PRIMARY].refresh);
        }
    }

    if (needVSyncThread) {
        // we don't have VSYNC support, we need to fake it
        mVSyncThread = new VSyncThread(*this);
    }
}

HWComposer::~HWComposer() {
    mHwc->eventControl(mHwc, 0, EVENT_VSYNC, 0);
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

void HWComposer::hook_invalidate(const struct hwc_procs* procs) {
    cb_context* ctx = reinterpret_cast<cb_context*>(
            const_cast<hwc_procs_t*>(procs));
    ctx->hwc->invalidate();
}

void HWComposer::hook_vsync(const struct hwc_procs* procs, int dpy,
        int64_t timestamp) {
    cb_context* ctx = reinterpret_cast<cb_context*>(
            const_cast<hwc_procs_t*>(procs));
    ctx->hwc->vsync(dpy, timestamp);
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

int32_t HWComposer::allocateDisplayId() {
    if (mAllocatedDisplayIDs.count() >= mNumDisplays) {
        return NO_MEMORY;
    }
    int32_t id = mAllocatedDisplayIDs.firstUnmarkedBit();
    mAllocatedDisplayIDs.markBit(id);
    return id;
}

status_t HWComposer::freeDisplayId(int32_t id) {
    if (id < HWC_NUM_DISPLAY_TYPES) {
        // cannot free the reserved IDs
        return BAD_VALUE;
    }
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id)) {
        return BAD_INDEX;
    }
    mAllocatedDisplayIDs.clearBit(id);
    return NO_ERROR;
}

nsecs_t HWComposer::getRefreshPeriod() const {
    return mDisplayData[HWC_DISPLAY_PRIMARY].refresh;
}

nsecs_t HWComposer::getRefreshTimestamp() const {
    // this returns the last refresh timestamp.
    // if the last one is not available, we estimate it based on
    // the refresh period and whatever closest timestamp we have.
    Mutex::Autolock _l(mLock);
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    return now - ((now - mLastHwVSync) %  mDisplayData[HWC_DISPLAY_PRIMARY].refresh);
}

float HWComposer::getDpiX() const {
    return mDisplayData[HWC_DISPLAY_PRIMARY].xdpi;
}

float HWComposer::getDpiY() const {
    return mDisplayData[HWC_DISPLAY_PRIMARY].ydpi;
}

void HWComposer::eventControl(int event, int enabled) {
    status_t err = NO_ERROR;
    if (mHwc) {
        if (!mDebugForceFakeVSync) {
            err = mHwc->eventControl(mHwc, 0, event, enabled);
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

status_t HWComposer::createWorkList(int32_t id, size_t numLayers) {
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id)) {
        return BAD_INDEX;
    }

    if (mHwc) {
        DisplayData& disp(mDisplayData[id]);
        if (disp.capacity < numLayers) {
            const size_t size = sizeof(hwc_display_contents_1_t)
                    + numLayers * sizeof(hwc_layer_1_t);
            free(disp.list);
            disp.list = (hwc_display_contents_1_t*)malloc(size);
            disp.capacity = numLayers;
        }
        disp.list->flags = HWC_GEOMETRY_CHANGED;
        disp.list->numHwLayers = numLayers;
        disp.list->flipFenceFd = -1;
    }
    return NO_ERROR;
}

status_t HWComposer::prepare() {
    for (size_t i=0 ; i<mNumDisplays ; i++) {
        mLists[i] = mDisplayData[i].list;
        if (mLists[i]) {
            mLists[i]->dpy = EGL_NO_DISPLAY;
            mLists[i]->sur = EGL_NO_SURFACE;
        }
    }
    int err = mHwc->prepare(mHwc, mNumDisplays, mLists);
    if (err == NO_ERROR) {
        // here we're just making sure that "skip" layers are set
        // to HWC_FRAMEBUFFER and we're also counting how many layers
        // we have of each type.
        for (size_t i=0 ; i<mNumDisplays ; i++) {
            DisplayData& disp(mDisplayData[i]);
            disp.hasFbComp = false;
            disp.hasOvComp = false;
            if (disp.list) {
                for (size_t i=0 ; i<disp.list->numHwLayers ; i++) {
                    hwc_layer_1_t& l = disp.list->hwLayers[i];
                    if (l.flags & HWC_SKIP_LAYER) {
                        l.compositionType = HWC_FRAMEBUFFER;
                    }
                    if (l.compositionType == HWC_FRAMEBUFFER) {
                        disp.hasFbComp = true;
                    }
                    if (l.compositionType == HWC_OVERLAY) {
                        disp.hasOvComp = true;
                    }
                }
            }
        }
    }
    return (status_t)err;
}

bool HWComposer::hasHwcComposition(int32_t id) const {
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id))
        return false;
    return mDisplayData[id].hasOvComp;
}

bool HWComposer::hasGlesComposition(int32_t id) const {
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id))
        return false;
    return mDisplayData[id].hasFbComp;
}

status_t HWComposer::commit() {
    int err = NO_ERROR;
    if (mHwc) {
        if (mHwc->common.version == HWC_DEVICE_API_VERSION_1_0) {
            // On version 1.0, the OpenGL ES target surface is communicated
            // by the (dpy, sur) fields and we are guaranteed to have only
            // a single display.
            mLists[0]->dpy = eglGetCurrentDisplay();
            mLists[0]->sur = eglGetCurrentSurface(EGL_DRAW);
        }

        err = mHwc->set(mHwc, mNumDisplays, mLists);

        for (size_t i=0 ; i<mNumDisplays ; i++) {
            DisplayData& disp(mDisplayData[i]);
            if (disp.list) {
                if (disp.list->flipFenceFd != -1) {
                    close(disp.list->flipFenceFd);
                    disp.list->flipFenceFd = -1;
                }
                disp.list->flags &= ~HWC_GEOMETRY_CHANGED;
            }
        }
    }
    return (status_t)err;
}

status_t HWComposer::release() const {
    if (mHwc) {
        mHwc->eventControl(mHwc, 0, HWC_EVENT_VSYNC, 0);
        return (status_t)mHwc->blank(mHwc, 0, 1);
    }
    return NO_ERROR;
}

status_t HWComposer::acquire() const {
    if (mHwc) {
        return (status_t)mHwc->blank(mHwc, 0, 0);
    }
    return NO_ERROR;
}

size_t HWComposer::getNumLayers(int32_t id) const {
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id)) {
        return 0;
    }
    return (mHwc && mDisplayData[id].list) ?
            mDisplayData[id].list->numHwLayers : 0;
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
HWComposer::LayerListIterator HWComposer::getLayerIterator(int32_t id, size_t index) {
    if (uint32_t(id)>31 || !mAllocatedDisplayIDs.hasBit(id)) {
        return LayerListIterator();
    }
    const DisplayData& disp(mDisplayData[id]);
    if (!mHwc || !disp.list || index > disp.list->numHwLayers) {
        return LayerListIterator();
    }
    return LayerListIterator(new HWCLayerVersion1(disp.list->hwLayers), index);
}

/*
 * returns an iterator on the beginning of the layer list
 */
HWComposer::LayerListIterator HWComposer::begin(int32_t id) {
    return getLayerIterator(id, 0);
}

/*
 * returns an iterator on the end of the layer list
 */
HWComposer::LayerListIterator HWComposer::end(int32_t id) {
    return getLayerIterator(id, getNumLayers(id));
}

void HWComposer::dump(String8& result, char* buffer, size_t SIZE,
        const Vector< sp<LayerBase> >& visibleLayersSortedByZ) const {
    if (mHwc) {
        result.append("Hardware Composer state:\n");
        result.appendFormat("  mDebugForceFakeVSync=%d\n", mDebugForceFakeVSync);
        for (size_t i=0 ; i<mNumDisplays ; i++) {
            const DisplayData& disp(mDisplayData[i]);
            if (disp.list) {
                result.appendFormat("  id=%d, numHwLayers=%u, flags=%08x\n",
                        i, disp.list->numHwLayers, disp.list->flags);
                result.append(
                        "   type   |  handle  |   hints  |   flags  | tr | blend |  format  |       source crop         |           frame           name \n"
                        "----------+----------+----------+----------+----+-------+----------+---------------------------+--------------------------------\n");
                //      " ________ | ________ | ________ | ________ | __ | _____ | ________ | [_____,_____,_____,_____] | [_____,_____,_____,_____]
                for (size_t i=0 ; i<disp.list->numHwLayers ; i++) {
                    const hwc_layer_1_t&l = disp.list->hwLayers[i];
                    const sp<LayerBase> layer(visibleLayersSortedByZ[i]);
                    int32_t format = -1;
                    if (layer->getLayer() != NULL) {
                        const sp<GraphicBuffer>& buffer(
                                layer->getLayer()->getActiveBuffer());
                        if (buffer != NULL) {
                            format = buffer->getPixelFormat();
                        }
                    }
                    result.appendFormat(
                            " %8s | %08x | %08x | %08x | %02x | %05x | %08x | [%5d,%5d,%5d,%5d] | [%5d,%5d,%5d,%5d] %s\n",
                            l.compositionType ? "OVERLAY" : "FB",
                                    intptr_t(l.handle), l.hints, l.flags, l.transform, l.blending, format,
                                    l.sourceCrop.left, l.sourceCrop.top, l.sourceCrop.right, l.sourceCrop.bottom,
                                    l.displayFrame.left, l.displayFrame.top, l.displayFrame.right, l.displayFrame.bottom,
                                    layer->getName().string());
                }
            }
        }
    }

    if (mHwc && mHwc->dump) {
        mHwc->dump(mHwc, buffer, SIZE);
        result.append(buffer);
    }
}

// ---------------------------------------------------------------------------

HWComposer::VSyncThread::VSyncThread(HWComposer& hwc)
    : mHwc(hwc), mEnabled(false),
      mNextFakeVSync(0),
      mRefreshPeriod(hwc.getRefreshPeriod())
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
