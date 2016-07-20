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

#ifndef USE_HWC2
#include "HWComposer_hwc1.h"
#else

#ifndef ANDROID_SF_HWCOMPOSER_H
#define ANDROID_SF_HWCOMPOSER_H

#include "HWC2.h"

#include <stdint.h>
#include <sys/types.h>

#include <ui/Fence.h>

#include <utils/BitSet.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/StrongPointer.h>
#include <utils/Thread.h>
#include <utils/Timers.h>
#include <utils/Vector.h>

#include <memory>
#include <set>
#include <vector>

extern "C" int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *request,
                           struct timespec *remain);

struct framebuffer_device_t;

namespace HWC2 {
    class Device;
    class Display;
}

namespace android {
// ---------------------------------------------------------------------------

class DisplayDevice;
class Fence;
class FloatRect;
class GraphicBuffer;
class HWC2On1Adapter;
class NativeHandle;
class Region;
class String8;
class SurfaceFlinger;

class HWComposer
{
public:
    class EventHandler {
        friend class HWComposer;
        virtual void onVSyncReceived(int32_t disp, nsecs_t timestamp) = 0;
        virtual void onHotplugReceived(int32_t disp, bool connected) = 0;
    protected:
        virtual ~EventHandler() {}
    };

    HWComposer(const sp<SurfaceFlinger>& flinger);

    ~HWComposer();

    void setEventHandler(EventHandler* handler);

    bool hasCapability(HWC2::Capability capability) const;

    // Attempts to allocate a virtual display. If the virtual display is created
    // on the HWC device, outId will contain its HWC ID.
    status_t allocateVirtualDisplay(uint32_t width, uint32_t height,
            android_pixel_format_t* format, int32_t* outId);

    // Attempts to create a new layer on this display
    std::shared_ptr<HWC2::Layer> createLayer(int32_t displayId);

    // Asks the HAL what it can do
    status_t prepare(DisplayDevice& displayDevice);

    status_t setClientTarget(int32_t displayId, const sp<Fence>& acquireFence,
            const sp<GraphicBuffer>& target, android_dataspace_t dataspace);

    // Finalize the layers and present them
    status_t commit(int32_t displayId);

    // set power mode
    status_t setPowerMode(int32_t displayId, int mode);

    // set active config
    status_t setActiveConfig(int32_t displayId, size_t configId);

    // Sets a color transform to be applied to the result of composition
    status_t setColorTransform(int32_t displayId, const mat4& transform);

    // reset state when an external, non-virtual display is disconnected
    void disconnectDisplay(int32_t displayId);

    // does this display have layers handled by HWC
    bool hasDeviceComposition(int32_t displayId) const;

    // does this display have layers handled by GLES
    bool hasClientComposition(int32_t displayId) const;

    // get the retire fence for the previous frame (i.e., corresponding to the
    // last call to presentDisplay
    sp<Fence> getRetireFence(int32_t displayId) const;

    // Get last release fence for the given layer
    sp<Fence> getLayerReleaseFence(int32_t displayId,
            const std::shared_ptr<HWC2::Layer>& layer) const;

    // Set the output buffer and acquire fence for a virtual display.
    // Returns INVALID_OPERATION if displayId is not a virtual display.
    status_t setOutputBuffer(int32_t displayId, const sp<Fence>& acquireFence,
            const sp<GraphicBuffer>& buf);

    // After SurfaceFlinger has retrieved the release fences for all the frames,
    // it can call this to clear the shared pointers in the release fence map
    void clearReleaseFences(int32_t displayId);

    // Returns the HDR capabilities of the given display
    std::unique_ptr<HdrCapabilities> getHdrCapabilities(int32_t displayId);

    // Events handling ---------------------------------------------------------

    void setVsyncEnabled(int32_t disp, HWC2::Vsync enabled);

    // Query display parameters.  Pass in a display index (e.g.
    // HWC_DISPLAY_PRIMARY).
    nsecs_t getRefreshTimestamp(int32_t disp) const;
    bool isConnected(int32_t disp) const;

    // Non-const because it can update configMap inside of mDisplayData
    std::vector<std::shared_ptr<const HWC2::Display::Config>>
            getConfigs(int32_t displayId) const;

    std::shared_ptr<const HWC2::Display::Config>
            getActiveConfig(int32_t displayId) const;

    std::vector<android_color_mode_t> getColorModes(int32_t displayId) const;

    status_t setActiveColorMode(int32_t displayId, android_color_mode_t mode);

    // for debugging ----------------------------------------------------------
    void dump(String8& out) const;

private:
    static const int32_t VIRTUAL_DISPLAY_ID_BASE = 2;

    void loadHwcModule();

    bool isValidDisplay(int32_t displayId) const;
    static void validateChange(HWC2::Composition from, HWC2::Composition to);

    struct cb_context;

    void invalidate(const std::shared_ptr<HWC2::Display>& display);
    void vsync(const std::shared_ptr<HWC2::Display>& display,
            int64_t timestamp);
    void hotplug(const std::shared_ptr<HWC2::Display>& display,
            HWC2::Connection connected);

    struct DisplayData {
        DisplayData();
        ~DisplayData();
        void reset();

        bool hasClientComposition;
        bool hasDeviceComposition;
        std::shared_ptr<HWC2::Display> hwcDisplay;
        HWC2::DisplayRequest displayRequests;
        sp<Fence> lastRetireFence;  // signals when the last set op retires
        std::unordered_map<std::shared_ptr<HWC2::Layer>, sp<Fence>>
                releaseFences;
        buffer_handle_t outbufHandle;
        sp<Fence> outbufAcquireFence;
        mutable std::unordered_map<int32_t,
                std::shared_ptr<const HWC2::Display::Config>> configMap;

        // protected by mVsyncLock
        HWC2::Vsync vsyncEnabled;
    };

    sp<SurfaceFlinger>              mFlinger;
    std::unique_ptr<HWC2On1Adapter> mAdapter;
    std::unique_ptr<HWC2::Device>   mHwcDevice;
    std::vector<DisplayData>        mDisplayData;
    std::set<size_t>                mFreeDisplaySlots;
    std::unordered_map<hwc2_display_t, int32_t> mHwcDisplaySlots;
    // protect mDisplayData from races between prepare and dump
    mutable Mutex mDisplayLock;

    cb_context*                     mCBContext;
    EventHandler*                   mEventHandler;
    size_t                          mVSyncCounts[HWC_NUM_PHYSICAL_DISPLAY_TYPES];
    uint32_t                        mRemainingHwcVirtualDisplays;

    // protected by mLock
    mutable Mutex mLock;
    mutable std::unordered_map<int32_t, nsecs_t> mLastHwVSync;

    // thread-safe
    mutable Mutex mVsyncLock;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SF_HWCOMPOSER_H

#endif // #ifdef USE_HWC2
