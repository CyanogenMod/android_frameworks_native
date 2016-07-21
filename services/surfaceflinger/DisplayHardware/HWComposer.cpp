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

// #define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "HWComposer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/misc.h>
#include <utils/NativeHandle.h>
#include <utils/String8.h>
#include <utils/Thread.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#include <ui/GraphicBuffer.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <android/configuration.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include "HWComposer.h"
#include "HWC2On1Adapter.h"
#include "HWC2.h"

#include "../Layer.h"           // needed only for debugging
#include "../SurfaceFlinger.h"

namespace android {

#define MIN_HWC_HEADER_VERSION HWC_HEADER_VERSION

// ---------------------------------------------------------------------------

HWComposer::HWComposer(const sp<SurfaceFlinger>& flinger)
    : mFlinger(flinger),
      mAdapter(),
      mHwcDevice(),
      mDisplayData(2),
      mFreeDisplaySlots(),
      mHwcDisplaySlots(),
      mCBContext(),
      mEventHandler(nullptr),
      mVSyncCounts(),
      mRemainingHwcVirtualDisplays(0)
{
    for (size_t i=0 ; i<HWC_NUM_PHYSICAL_DISPLAY_TYPES ; i++) {
        mLastHwVSync[i] = 0;
        mVSyncCounts[i] = 0;
    }

    loadHwcModule();
}

HWComposer::~HWComposer() {}

void HWComposer::setEventHandler(EventHandler* handler)
{
    if (handler == nullptr) {
        ALOGE("setEventHandler: Rejected attempt to clear handler");
        return;
    }

    bool wasNull = (mEventHandler == nullptr);
    mEventHandler = handler;

    if (wasNull) {
        auto hotplugHook = std::bind(&HWComposer::hotplug, this,
                std::placeholders::_1, std::placeholders::_2);
        mHwcDevice->registerHotplugCallback(hotplugHook);
        auto invalidateHook = std::bind(&HWComposer::invalidate, this,
                std::placeholders::_1);
        mHwcDevice->registerRefreshCallback(invalidateHook);
        auto vsyncHook = std::bind(&HWComposer::vsync, this,
                std::placeholders::_1, std::placeholders::_2);
        mHwcDevice->registerVsyncCallback(vsyncHook);
    }
}

// Load and prepare the hardware composer module.  Sets mHwc.
void HWComposer::loadHwcModule()
{
    ALOGV("loadHwcModule");

    hw_module_t const* module;

    if (hw_get_module(HWC_HARDWARE_MODULE_ID, &module) != 0) {
        ALOGE("%s module not found, aborting", HWC_HARDWARE_MODULE_ID);
        abort();
    }

    hw_device_t* device = nullptr;
    int error = module->methods->open(module, HWC_HARDWARE_COMPOSER, &device);
    if (error != 0) {
        ALOGE("Failed to open HWC device (%s), aborting", strerror(-error));
        abort();
    }

    uint32_t majorVersion = (device->version >> 24) & 0xF;
    if (majorVersion == 2) {
        mHwcDevice = std::make_unique<HWC2::Device>(
                reinterpret_cast<hwc2_device_t*>(device));
    } else {
        mAdapter = std::make_unique<HWC2On1Adapter>(
                reinterpret_cast<hwc_composer_device_1_t*>(device));
        uint8_t minorVersion = mAdapter->getHwc1MinorVersion();
        if (minorVersion < 1) {
            ALOGE("Cannot adapt to HWC version %d.%d",
                    static_cast<int32_t>((minorVersion >> 8) & 0xF),
                    static_cast<int32_t>(minorVersion & 0xF));
            abort();
        }
        mHwcDevice = std::make_unique<HWC2::Device>(
                static_cast<hwc2_device_t*>(mAdapter.get()));
    }

    mRemainingHwcVirtualDisplays = mHwcDevice->getMaxVirtualDisplayCount();
}

bool HWComposer::isValidDisplay(int32_t displayId) const {
    return static_cast<size_t>(displayId) < mDisplayData.size() &&
            mDisplayData[displayId].hwcDisplay;
}

void HWComposer::validateChange(HWC2::Composition from, HWC2::Composition to) {
    bool valid = true;
    switch (from) {
        case HWC2::Composition::Client:
            valid = false;
            break;
        case HWC2::Composition::Device:
        case HWC2::Composition::SolidColor:
            valid = (to == HWC2::Composition::Client);
            break;
        case HWC2::Composition::Cursor:
        case HWC2::Composition::Sideband:
            valid = (to == HWC2::Composition::Client ||
                    to == HWC2::Composition::Device);
            break;
        default:
            break;
    }

    if (!valid) {
        ALOGE("Invalid layer type change: %s --> %s", to_string(from).c_str(),
                to_string(to).c_str());
    }
}

void HWComposer::hotplug(const std::shared_ptr<HWC2::Display>& display,
        HWC2::Connection connected) {
    ALOGV("hotplug: %" PRIu64 ", %s", display->getId(),
            to_string(connected).c_str());
    int32_t disp = 0;
    if (!mDisplayData[0].hwcDisplay) {
        ALOGE_IF(connected != HWC2::Connection::Connected, "Assumed primary"
                " display would be connected");
        mDisplayData[0].hwcDisplay = display;
        mHwcDisplaySlots[display->getId()] = 0;
        disp = DisplayDevice::DISPLAY_PRIMARY;
    } else {
        // Disconnect is handled through HWComposer::disconnectDisplay via
        // SurfaceFlinger's onHotplugReceived callback handling
        if (connected == HWC2::Connection::Connected) {
            mDisplayData[1].hwcDisplay = display;
            mHwcDisplaySlots[display->getId()] = 1;
        }
        disp = DisplayDevice::DISPLAY_EXTERNAL;
    }
    mEventHandler->onHotplugReceived(disp,
            connected == HWC2::Connection::Connected);
}

void HWComposer::invalidate(const std::shared_ptr<HWC2::Display>& /*display*/) {
    mFlinger->repaintEverything();
}

void HWComposer::vsync(const std::shared_ptr<HWC2::Display>& display,
        int64_t timestamp) {
    auto displayType = HWC2::DisplayType::Invalid;
    auto error = display->getType(&displayType);
    if (error != HWC2::Error::None) {
        ALOGE("vsync: Failed to determine type of display %" PRIu64,
                display->getId());
        return;
    }

    if (displayType == HWC2::DisplayType::Virtual) {
        ALOGE("Virtual display %" PRIu64 " passed to vsync callback",
                display->getId());
        return;
    }

    if (mHwcDisplaySlots.count(display->getId()) == 0) {
        ALOGE("Unknown physical display %" PRIu64 " passed to vsync callback",
                display->getId());
        return;
    }

    int32_t disp = mHwcDisplaySlots[display->getId()];
    {
        Mutex::Autolock _l(mLock);

        // There have been reports of HWCs that signal several vsync events
        // with the same timestamp when turning the display off and on. This
        // is a bug in the HWC implementation, but filter the extra events
        // out here so they don't cause havoc downstream.
        if (timestamp == mLastHwVSync[disp]) {
            ALOGW("Ignoring duplicate VSYNC event from HWC (t=%" PRId64 ")",
                    timestamp);
            return;
        }

        mLastHwVSync[disp] = timestamp;
    }

    char tag[16];
    snprintf(tag, sizeof(tag), "HW_VSYNC_%1u", disp);
    ATRACE_INT(tag, ++mVSyncCounts[disp] & 1);

    mEventHandler->onVSyncReceived(disp, timestamp);
}

status_t HWComposer::allocateVirtualDisplay(uint32_t width, uint32_t height,
        android_pixel_format_t* format, int32_t *outId) {
    if (mRemainingHwcVirtualDisplays == 0) {
        ALOGE("allocateVirtualDisplay: No remaining virtual displays");
        return NO_MEMORY;
    }

    std::shared_ptr<HWC2::Display> display;
    auto error = mHwcDevice->createVirtualDisplay(width, height, format,
            &display);
    if (error != HWC2::Error::None) {
        ALOGE("allocateVirtualDisplay: Failed to create HWC virtual display");
        return NO_MEMORY;
    }

    size_t displaySlot = 0;
    if (!mFreeDisplaySlots.empty()) {
        displaySlot = *mFreeDisplaySlots.begin();
        mFreeDisplaySlots.erase(displaySlot);
    } else if (mDisplayData.size() < INT32_MAX) {
        // Don't bother allocating a slot larger than we can return
        displaySlot = mDisplayData.size();
        mDisplayData.resize(displaySlot + 1);
    } else {
        ALOGE("allocateVirtualDisplay: Unable to allocate a display slot");
        return NO_MEMORY;
    }

    mDisplayData[displaySlot].hwcDisplay = display;

    --mRemainingHwcVirtualDisplays;
    *outId = static_cast<int32_t>(displaySlot);

    return NO_ERROR;
}

std::shared_ptr<HWC2::Layer> HWComposer::createLayer(int32_t displayId) {
    if (!isValidDisplay(displayId)) {
        ALOGE("Failed to create layer on invalid display %d", displayId);
        return nullptr;
    }
    auto display = mDisplayData[displayId].hwcDisplay;
    std::shared_ptr<HWC2::Layer> layer;
    auto error = display->createLayer(&layer);
    if (error != HWC2::Error::None) {
        ALOGE("Failed to create layer on display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return nullptr;
    }
    return layer;
}

nsecs_t HWComposer::getRefreshTimestamp(int32_t disp) const {
    // this returns the last refresh timestamp.
    // if the last one is not available, we estimate it based on
    // the refresh period and whatever closest timestamp we have.
    Mutex::Autolock _l(mLock);
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    auto vsyncPeriod = getActiveConfig(disp)->getVsyncPeriod();
    return now - ((now - mLastHwVSync[disp]) % vsyncPeriod);
}

bool HWComposer::isConnected(int32_t disp) const {
    if (!isValidDisplay(disp)) {
        ALOGE("isConnected: Attempted to access invalid display %d", disp);
        return false;
    }
    return mDisplayData[disp].hwcDisplay->isConnected();
}

std::vector<std::shared_ptr<const HWC2::Display::Config>>
        HWComposer::getConfigs(int32_t displayId) const {
    if (!isValidDisplay(displayId)) {
        ALOGE("getConfigs: Attempted to access invalid display %d", displayId);
        return {};
    }
    auto& displayData = mDisplayData[displayId];
    auto configs = mDisplayData[displayId].hwcDisplay->getConfigs();
    if (displayData.configMap.empty()) {
        for (size_t i = 0; i < configs.size(); ++i) {
            displayData.configMap[i] = configs[i];
        }
    }
    return configs;
}

std::shared_ptr<const HWC2::Display::Config>
        HWComposer::getActiveConfig(int32_t displayId) const {
    if (!isValidDisplay(displayId)) {
        ALOGE("getActiveConfigs: Attempted to access invalid display %d",
                displayId);
        return nullptr;
    }
    std::shared_ptr<const HWC2::Display::Config> config;
    auto error = mDisplayData[displayId].hwcDisplay->getActiveConfig(&config);
    if (error == HWC2::Error::BadConfig) {
        ALOGV("getActiveConfig: No config active, returning null");
        return nullptr;
    } else if (error != HWC2::Error::None) {
        ALOGE("getActiveConfig failed for display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return nullptr;
    } else if (!config) {
        ALOGE("getActiveConfig returned an unknown config for display %d",
                displayId);
        return nullptr;
    }

    return config;
}

void HWComposer::setVsyncEnabled(int32_t disp, HWC2::Vsync enabled) {
    if (disp < 0 || disp >= HWC_DISPLAY_VIRTUAL) {
        ALOGD("setVsyncEnabled: Ignoring for virtual display %d", disp);
        return;
    }

    if (!isValidDisplay(disp)) {
        ALOGE("setVsyncEnabled: Attempted to access invalid display %d", disp);
        return;
    }

    // NOTE: we use our own internal lock here because we have to call
    // into the HWC with the lock held, and we want to make sure
    // that even if HWC blocks (which it shouldn't), it won't
    // affect other threads.
    Mutex::Autolock _l(mVsyncLock);
    auto& displayData = mDisplayData[disp];
    if (enabled != displayData.vsyncEnabled) {
        ATRACE_CALL();
        auto error = displayData.hwcDisplay->setVsyncEnabled(enabled);
        if (error == HWC2::Error::None) {
            displayData.vsyncEnabled = enabled;

            char tag[16];
            snprintf(tag, sizeof(tag), "HW_VSYNC_ON_%1u", disp);
            ATRACE_INT(tag, enabled == HWC2::Vsync::Enable ? 1 : 0);
        } else {
            ALOGE("setVsyncEnabled: Failed to set vsync to %s on %d/%" PRIu64
                    ": %s (%d)", to_string(enabled).c_str(), disp,
                    mDisplayData[disp].hwcDisplay->getId(),
                    to_string(error).c_str(), static_cast<int32_t>(error));
        }
    }
}

status_t HWComposer::setClientTarget(int32_t displayId,
        const sp<Fence>& acquireFence, const sp<GraphicBuffer>& target,
        android_dataspace_t dataspace) {
    if (!isValidDisplay(displayId)) {
        return BAD_INDEX;
    }

    ALOGV("setClientTarget for display %d", displayId);
    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    buffer_handle_t handle = nullptr;
    if ((target != nullptr) && target->getNativeBuffer()) {
        handle = target->getNativeBuffer()->handle;
    }
    auto error = hwcDisplay->setClientTarget(handle, acquireFence, dataspace);
    if (error != HWC2::Error::None) {
        ALOGE("Failed to set client target for display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t HWComposer::prepare(DisplayDevice& displayDevice) {
    ATRACE_CALL();

    Mutex::Autolock _l(mDisplayLock);
    auto displayId = displayDevice.getHwcDisplayId();
    if (displayId == DisplayDevice::DISPLAY_ID_INVALID) {
        ALOGV("Skipping HWComposer prepare for non-HWC display");
        return NO_ERROR;
    }
    if (!isValidDisplay(displayId)) {
        return BAD_INDEX;
    }

    auto& displayData = mDisplayData[displayId];
    auto& hwcDisplay = displayData.hwcDisplay;
    if (!hwcDisplay->isConnected()) {
        return NO_ERROR;
    }

    uint32_t numTypes = 0;
    uint32_t numRequests = 0;
    auto error = hwcDisplay->validate(&numTypes, &numRequests);
    if (error != HWC2::Error::None && error != HWC2::Error::HasChanges) {
        ALOGE("prepare: validate failed for display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return BAD_INDEX;
    }

    std::unordered_map<std::shared_ptr<HWC2::Layer>, HWC2::Composition>
        changedTypes;
    changedTypes.reserve(numTypes);
    error = hwcDisplay->getChangedCompositionTypes(&changedTypes);
    if (error != HWC2::Error::None) {
        ALOGE("prepare: getChangedCompositionTypes failed on display %d: "
                "%s (%d)", displayId, to_string(error).c_str(),
                static_cast<int32_t>(error));
        return BAD_INDEX;
    }


    displayData.displayRequests = static_cast<HWC2::DisplayRequest>(0);
    std::unordered_map<std::shared_ptr<HWC2::Layer>, HWC2::LayerRequest>
        layerRequests;
    layerRequests.reserve(numRequests);
    error = hwcDisplay->getRequests(&displayData.displayRequests,
            &layerRequests);
    if (error != HWC2::Error::None) {
        ALOGE("prepare: getRequests failed on display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return BAD_INDEX;
    }

    displayData.hasClientComposition = false;
    displayData.hasDeviceComposition = false;
    for (auto& layer : displayDevice.getVisibleLayersSortedByZ()) {
        auto hwcLayer = layer->getHwcLayer(displayId);

        if (changedTypes.count(hwcLayer) != 0) {
            // We pass false so we only update our state and don't call back
            // into the HWC device
            validateChange(layer->getCompositionType(displayId),
                    changedTypes[hwcLayer]);
            layer->setCompositionType(displayId, changedTypes[hwcLayer], false);
        }

        switch (layer->getCompositionType(displayId)) {
            case HWC2::Composition::Client:
                displayData.hasClientComposition = true;
                break;
            case HWC2::Composition::Device:
            case HWC2::Composition::SolidColor:
            case HWC2::Composition::Cursor:
            case HWC2::Composition::Sideband:
                displayData.hasDeviceComposition = true;
                break;
            default:
                break;
        }

        if (layerRequests.count(hwcLayer) != 0 &&
                layerRequests[hwcLayer] ==
                        HWC2::LayerRequest::ClearClientTarget) {
            layer->setClearClientTarget(displayId, true);
        } else {
            if (layerRequests.count(hwcLayer) != 0) {
                ALOGE("prepare: Unknown layer request: %s",
                        to_string(layerRequests[hwcLayer]).c_str());
            }
            layer->setClearClientTarget(displayId, false);
        }
    }

    error = hwcDisplay->acceptChanges();
    if (error != HWC2::Error::None) {
        ALOGE("prepare: acceptChanges failed: %s", to_string(error).c_str());
        return BAD_INDEX;
    }

    return NO_ERROR;
}

bool HWComposer::hasDeviceComposition(int32_t displayId) const {
    if (displayId == DisplayDevice::DISPLAY_ID_INVALID) {
        // Displays without a corresponding HWC display are never composed by
        // the device
        return false;
    }
    if (!isValidDisplay(displayId)) {
        ALOGE("hasDeviceComposition: Invalid display %d", displayId);
        return false;
    }
    return mDisplayData[displayId].hasDeviceComposition;
}

bool HWComposer::hasClientComposition(int32_t displayId) const {
    if (displayId == DisplayDevice::DISPLAY_ID_INVALID) {
        // Displays without a corresponding HWC display are always composed by
        // the client
        return true;
    }
    if (!isValidDisplay(displayId)) {
        ALOGE("hasClientComposition: Invalid display %d", displayId);
        return true;
    }
    return mDisplayData[displayId].hasClientComposition;
}

sp<Fence> HWComposer::getRetireFence(int32_t displayId) const {
    if (!isValidDisplay(displayId)) {
        ALOGE("getRetireFence failed for invalid display %d", displayId);
        return Fence::NO_FENCE;
    }
    return mDisplayData[displayId].lastRetireFence;
}

sp<Fence> HWComposer::getLayerReleaseFence(int32_t displayId,
        const std::shared_ptr<HWC2::Layer>& layer) const {
    if (!isValidDisplay(displayId)) {
        ALOGE("getLayerReleaseFence: Invalid display");
        return Fence::NO_FENCE;
    }
    auto displayFences = mDisplayData[displayId].releaseFences;
    if (displayFences.count(layer) == 0) {
        ALOGV("getLayerReleaseFence: Release fence not found");
        return Fence::NO_FENCE;
    }
    return displayFences[layer];
}

status_t HWComposer::commit(int32_t displayId) {
    ATRACE_CALL();

    if (!isValidDisplay(displayId)) {
        return BAD_INDEX;
    }

    auto& displayData = mDisplayData[displayId];
    auto& hwcDisplay = displayData.hwcDisplay;
    auto error = hwcDisplay->present(&displayData.lastRetireFence);
    if (error != HWC2::Error::None) {
        ALOGE("commit: present failed for display %d: %s (%d)", displayId,
                to_string(error).c_str(), static_cast<int32_t>(error));
        return UNKNOWN_ERROR;
    }

    std::unordered_map<std::shared_ptr<HWC2::Layer>, sp<Fence>> releaseFences;
    error = hwcDisplay->getReleaseFences(&releaseFences);
    if (error != HWC2::Error::None) {
        ALOGE("commit: Failed to get release fences for display %d: %s (%d)",
                displayId, to_string(error).c_str(),
                static_cast<int32_t>(error));
        return UNKNOWN_ERROR;
    }

    displayData.releaseFences = std::move(releaseFences);

    return NO_ERROR;
}

status_t HWComposer::setPowerMode(int32_t displayId, int32_t intMode) {
    ALOGV("setPowerMode(%d, %d)", displayId, intMode);
    if (!isValidDisplay(displayId)) {
        ALOGE("setPowerMode: Bad display");
        return BAD_INDEX;
    }
    if (displayId >= VIRTUAL_DISPLAY_ID_BASE) {
        ALOGE("setPowerMode: Virtual display %d passed in, returning",
                displayId);
        return BAD_INDEX;
    }

    auto mode = static_cast<HWC2::PowerMode>(intMode);
    if (mode == HWC2::PowerMode::Off) {
        setVsyncEnabled(displayId, HWC2::Vsync::Disable);
    }

    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    switch (mode) {
        case HWC2::PowerMode::Off:
        case HWC2::PowerMode::On:
            ALOGV("setPowerMode: Calling HWC %s", to_string(mode).c_str());
            {
                auto error = hwcDisplay->setPowerMode(mode);
                if (error != HWC2::Error::None) {
                    ALOGE("setPowerMode: Unable to set power mode %s for "
                            "display %d: %s (%d)", to_string(mode).c_str(),
                            displayId, to_string(error).c_str(),
                            static_cast<int32_t>(error));
                }
            }
            break;
        case HWC2::PowerMode::Doze:
        case HWC2::PowerMode::DozeSuspend:
            ALOGV("setPowerMode: Calling HWC %s", to_string(mode).c_str());
            {
                bool supportsDoze = false;
                auto error = hwcDisplay->supportsDoze(&supportsDoze);
                if (error != HWC2::Error::None) {
                    ALOGE("setPowerMode: Unable to query doze support for "
                            "display %d: %s (%d)", displayId,
                            to_string(error).c_str(),
                            static_cast<int32_t>(error));
                }
                if (!supportsDoze) {
                    mode = HWC2::PowerMode::On;
                }

                error = hwcDisplay->setPowerMode(mode);
                if (error != HWC2::Error::None) {
                    ALOGE("setPowerMode: Unable to set power mode %s for "
                            "display %d: %s (%d)", to_string(mode).c_str(),
                            displayId, to_string(error).c_str(),
                            static_cast<int32_t>(error));
                }
            }
            break;
        default:
            ALOGV("setPowerMode: Not calling HWC");
            break;
    }

    return NO_ERROR;
}

status_t HWComposer::setActiveConfig(int32_t displayId, size_t configId) {
    if (!isValidDisplay(displayId)) {
        ALOGE("setActiveConfig: Display %d is not valid", displayId);
        return BAD_INDEX;
    }

    auto& displayData = mDisplayData[displayId];
    if (displayData.configMap.count(configId) == 0) {
        ALOGE("setActiveConfig: Invalid config %zd", configId);
        return BAD_INDEX;
    }

    auto error = displayData.hwcDisplay->setActiveConfig(
            displayData.configMap[configId]);
    if (error != HWC2::Error::None) {
        ALOGE("setActiveConfig: Failed to set config %zu on display %d: "
                "%s (%d)", configId, displayId, to_string(error).c_str(),
                static_cast<int32_t>(error));
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

void HWComposer::disconnectDisplay(int displayId) {
    LOG_ALWAYS_FATAL_IF(displayId < 0);
    auto& displayData = mDisplayData[displayId];

    auto displayType = HWC2::DisplayType::Invalid;
    auto error = displayData.hwcDisplay->getType(&displayType);
    if (error != HWC2::Error::None) {
        ALOGE("disconnectDisplay: Failed to determine type of display %d",
                displayId);
        return;
    }

    // If this was a virtual display, add its slot back for reuse by future
    // virtual displays
    if (displayType == HWC2::DisplayType::Virtual) {
        mFreeDisplaySlots.insert(displayId);
        ++mRemainingHwcVirtualDisplays;
    }

    auto hwcId = displayData.hwcDisplay->getId();
    mHwcDisplaySlots.erase(hwcId);
    displayData.reset();
}

status_t HWComposer::setOutputBuffer(int32_t displayId,
        const sp<Fence>& acquireFence, const sp<GraphicBuffer>& buffer) {
    if (!isValidDisplay(displayId)) {
        ALOGE("setOutputBuffer: Display %d is not valid", displayId);
        return BAD_INDEX;
    }

    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    auto displayType = HWC2::DisplayType::Invalid;
    auto error = hwcDisplay->getType(&displayType);
    if (error != HWC2::Error::None) {
        ALOGE("setOutputBuffer: Failed to determine type of display %d",
                displayId);
        return NAME_NOT_FOUND;
    }

    if (displayType != HWC2::DisplayType::Virtual) {
        ALOGE("setOutputBuffer: Display %d is not virtual", displayId);
        return INVALID_OPERATION;
    }

    error = hwcDisplay->setOutputBuffer(buffer, acquireFence);
    if (error != HWC2::Error::None) {
        ALOGE("setOutputBuffer: Failed to set buffer on display %d: %s (%d)",
                displayId, to_string(error).c_str(),
                static_cast<int32_t>(error));
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

void HWComposer::clearReleaseFences(int32_t displayId) {
    if (!isValidDisplay(displayId)) {
        ALOGE("clearReleaseFences: Display %d is not valid", displayId);
        return;
    }
    mDisplayData[displayId].releaseFences.clear();
}

std::unique_ptr<HdrCapabilities> HWComposer::getHdrCapabilities(
        int32_t displayId) {
    if (!isValidDisplay(displayId)) {
        ALOGE("getHdrCapabilities: Display %d is not valid", displayId);
        return nullptr;
    }

    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    std::unique_ptr<HdrCapabilities> capabilities;
    auto error = hwcDisplay->getHdrCapabilities(&capabilities);
    if (error != HWC2::Error::None) {
        ALOGE("getOutputCapabilities: Failed to get capabilities on display %d:"
                " %s (%d)", displayId, to_string(error).c_str(),
                static_cast<int32_t>(error));
        return nullptr;
    }

    return capabilities;
}

// Converts a PixelFormat to a human-readable string.  Max 11 chars.
// (Could use a table of prefab String8 objects.)
/*
static String8 getFormatStr(PixelFormat format) {
    switch (format) {
    case PIXEL_FORMAT_RGBA_8888:    return String8("RGBA_8888");
    case PIXEL_FORMAT_RGBX_8888:    return String8("RGBx_8888");
    case PIXEL_FORMAT_RGB_888:      return String8("RGB_888");
    case PIXEL_FORMAT_RGB_565:      return String8("RGB_565");
    case PIXEL_FORMAT_BGRA_8888:    return String8("BGRA_8888");
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                                    return String8("ImplDef");
    default:
        String8 result;
        result.appendFormat("? %08x", format);
        return result;
    }
}
*/

void HWComposer::dump(String8& result) const {
    // TODO: In order to provide a dump equivalent to HWC1, we need to shadow
    // all the state going into the layers. This is probably better done in
    // Layer itself, but it's going to take a bit of work to get there.
    result.append(mHwcDevice->dump().c_str());
}

// ---------------------------------------------------------------------------

HWComposer::DisplayData::DisplayData()
  : hasClientComposition(false),
    hasDeviceComposition(false),
    hwcDisplay(),
    lastRetireFence(Fence::NO_FENCE),
    outbufHandle(nullptr),
    outbufAcquireFence(Fence::NO_FENCE),
    vsyncEnabled(HWC2::Vsync::Disable) {
    ALOGV("Created new DisplayData");
}

HWComposer::DisplayData::~DisplayData() {
}

void HWComposer::DisplayData::reset() {
    ALOGV("DisplayData reset");
    *this = DisplayData();
}

// ---------------------------------------------------------------------------
}; // namespace android
