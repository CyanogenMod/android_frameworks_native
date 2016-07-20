/*
 * Copyright 2015 The Android Open Source Project
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

#ifndef ANDROID_SF_HWC2_ON_1_ADAPTER_H
#define ANDROID_SF_HWC2_ON_1_ADAPTER_H

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11

#include <ui/Fence.h>

#include <atomic>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct hwc_composer_device_1;
struct hwc_display_contents_1;
struct hwc_layer_1;

namespace android {

class HWC2On1Adapter : public hwc2_device_t
{
public:
    HWC2On1Adapter(struct hwc_composer_device_1* hwc1Device);
    ~HWC2On1Adapter();

    struct hwc_composer_device_1* getHwc1Device() const { return mHwc1Device; }
    uint8_t getHwc1MinorVersion() const { return mHwc1MinorVersion; }

private:
    static inline HWC2On1Adapter* getAdapter(hwc2_device_t* device) {
        return static_cast<HWC2On1Adapter*>(device);
    }

    // getCapabilities

    void doGetCapabilities(uint32_t* outCount,
            int32_t* /*hwc2_capability_t*/ outCapabilities);
    static void getCapabilitiesHook(hwc2_device_t* device, uint32_t* outCount,
            int32_t* /*hwc2_capability_t*/ outCapabilities) {
        getAdapter(device)->doGetCapabilities(outCount, outCapabilities);
    }

    // getFunction

    hwc2_function_pointer_t doGetFunction(HWC2::FunctionDescriptor descriptor);
    static hwc2_function_pointer_t getFunctionHook(hwc2_device_t* device,
            int32_t intDesc) {
        auto descriptor = static_cast<HWC2::FunctionDescriptor>(intDesc);
        return getAdapter(device)->doGetFunction(descriptor);
    }

    // Device functions

    HWC2::Error createVirtualDisplay(uint32_t width, uint32_t height,
            hwc2_display_t* outDisplay);
    static int32_t createVirtualDisplayHook(hwc2_device_t* device,
            uint32_t width, uint32_t height, int32_t* /*format*/,
            hwc2_display_t* outDisplay) {
        // HWC1 implementations cannot override the buffer format requested by
        // the consumer
        auto error = getAdapter(device)->createVirtualDisplay(width, height,
                outDisplay);
        return static_cast<int32_t>(error);
    }

    HWC2::Error destroyVirtualDisplay(hwc2_display_t display);
    static int32_t destroyVirtualDisplayHook(hwc2_device_t* device,
            hwc2_display_t display) {
        auto error = getAdapter(device)->destroyVirtualDisplay(display);
        return static_cast<int32_t>(error);
    }

    std::string mDumpString;
    void dump(uint32_t* outSize, char* outBuffer);
    static void dumpHook(hwc2_device_t* device, uint32_t* outSize,
            char* outBuffer) {
        getAdapter(device)->dump(outSize, outBuffer);
    }

    uint32_t getMaxVirtualDisplayCount();
    static uint32_t getMaxVirtualDisplayCountHook(hwc2_device_t* device) {
        return getAdapter(device)->getMaxVirtualDisplayCount();
    }

    HWC2::Error registerCallback(HWC2::Callback descriptor,
            hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer);
    static int32_t registerCallbackHook(hwc2_device_t* device,
            int32_t intDesc, hwc2_callback_data_t callbackData,
            hwc2_function_pointer_t pointer) {
        auto descriptor = static_cast<HWC2::Callback>(intDesc);
        auto error = getAdapter(device)->registerCallback(descriptor,
                callbackData, pointer);
        return static_cast<int32_t>(error);
    }

    // Display functions

    class Layer;

    class SortLayersByZ {
        public:
            bool operator()(const std::shared_ptr<Layer>& lhs,
                    const std::shared_ptr<Layer>& rhs);
    };

    class DisplayContentsDeleter {
        public:
            void operator()(struct hwc_display_contents_1* contents);
    };

    class DeferredFence {
        public:
            DeferredFence()
              : mMutex(),
                mFences({Fence::NO_FENCE, Fence::NO_FENCE}) {}

            void add(int32_t fenceFd) {
                mFences.emplace(new Fence(fenceFd));
                mFences.pop();
            }

            const sp<Fence>& get() const {
                return mFences.front();
            }

        private:
            mutable std::mutex mMutex;
            std::queue<sp<Fence>> mFences;
    };

    class FencedBuffer {
        public:
            FencedBuffer() : mBuffer(nullptr), mFence(Fence::NO_FENCE) {}

            void setBuffer(buffer_handle_t buffer) { mBuffer = buffer; }
            void setFence(int fenceFd) { mFence = new Fence(fenceFd); }

            buffer_handle_t getBuffer() const { return mBuffer; }
            int getFence() const { return mFence->dup(); }

        private:
            buffer_handle_t mBuffer;
            sp<Fence> mFence;
    };

    class Display {
        public:
            typedef std::unique_ptr<hwc_display_contents_1,
                    DisplayContentsDeleter> HWC1Contents;

            Display(HWC2On1Adapter& device, HWC2::DisplayType type);

            hwc2_display_t getId() const { return mId; }
            HWC2On1Adapter& getDevice() const { return mDevice; }

            // Does not require locking because it is set before adding the
            // Displays to the Adapter's list of displays
            void setHwc1Id(int32_t id) { mHwc1Id = id; }
            int32_t getHwc1Id() const { return mHwc1Id; }

            void incDirty() { ++mDirtyCount; }
            void decDirty() { --mDirtyCount; }
            bool isDirty() const { return mDirtyCount > 0 || mZIsDirty; }

            // HWC2 Display functions
            HWC2::Error acceptChanges();
            HWC2::Error createLayer(hwc2_layer_t* outLayerId);
            HWC2::Error destroyLayer(hwc2_layer_t layerId);
            HWC2::Error getActiveConfig(hwc2_config_t* outConfigId);
            HWC2::Error getAttribute(hwc2_config_t configId,
                    HWC2::Attribute attribute, int32_t* outValue);
            HWC2::Error getChangedCompositionTypes(uint32_t* outNumElements,
                    hwc2_layer_t* outLayers, int32_t* outTypes);
            HWC2::Error getColorModes(uint32_t* outNumModes, int32_t* outModes);
            HWC2::Error getConfigs(uint32_t* outNumConfigs,
                    hwc2_config_t* outConfigIds);
            HWC2::Error getDozeSupport(int32_t* outSupport);
            HWC2::Error getHdrCapabilities(uint32_t* outNumTypes,
                    int32_t* outTypes, float* outMaxLuminance,
                    float* outMaxAverageLuminance, float* outMinLuminance);
            HWC2::Error getName(uint32_t* outSize, char* outName);
            HWC2::Error getReleaseFences(uint32_t* outNumElements,
                    hwc2_layer_t* outLayers, int32_t* outFences);
            HWC2::Error getRequests(int32_t* outDisplayRequests,
                    uint32_t* outNumElements, hwc2_layer_t* outLayers,
                    int32_t* outLayerRequests);
            HWC2::Error getType(int32_t* outType);
            HWC2::Error present(int32_t* outRetireFence);
            HWC2::Error setActiveConfig(hwc2_config_t configId);
            HWC2::Error setClientTarget(buffer_handle_t target,
                    int32_t acquireFence, int32_t dataspace,
                    hwc_region_t damage);
            HWC2::Error setColorMode(android_color_mode_t mode);
            HWC2::Error setColorTransform(android_color_transform_t hint);
            HWC2::Error setOutputBuffer(buffer_handle_t buffer,
                    int32_t releaseFence);
            HWC2::Error setPowerMode(HWC2::PowerMode mode);
            HWC2::Error setVsyncEnabled(HWC2::Vsync enabled);
            HWC2::Error validate(uint32_t* outNumTypes,
                    uint32_t* outNumRequests);

            HWC2::Error updateLayerZ(hwc2_layer_t layerId, uint32_t z);

            // Read configs from HWC1 device
            void populateConfigs();

            // Set configs for a virtual display
            void populateConfigs(uint32_t width, uint32_t height);

            bool prepare();
            HWC1Contents cloneRequestedContents() const;
            void setReceivedContents(HWC1Contents contents);
            bool hasChanges() const;
            HWC2::Error set(hwc_display_contents_1& hwcContents);
            void addRetireFence(int fenceFd);
            void addReleaseFences(const hwc_display_contents_1& hwcContents);

            bool hasColorTransform() const;

            std::string dump() const;

        private:
            class Config {
                public:
                    Config(Display& display)
                      : mDisplay(display),
                        mAttributes() {}

                    bool isOnDisplay(const Display& display) const {
                        return display.getId() == mDisplay.getId();
                    }

                    void setAttribute(HWC2::Attribute attribute, int32_t value);
                    int32_t getAttribute(HWC2::Attribute attribute) const;

                    void setHwc1Id(uint32_t id);
                    bool hasHwc1Id(uint32_t id) const;
                    HWC2::Error getColorModeForHwc1Id(uint32_t id,
                            android_color_mode_t *outMode) const;
                    HWC2::Error getHwc1IdForColorMode(android_color_mode_t mode,
                            uint32_t* outId) const;

                    void setId(hwc2_config_t id) { mId = id; }
                    hwc2_config_t getId() const { return mId; }

                    // Attempts to merge two configs that differ only in color
                    // mode. Returns whether the merge was successful
                    bool merge(const Config& other);

                    std::set<android_color_mode_t> getColorModes() const;

                    // splitLine divides the output into two lines suitable for
                    // dumpsys SurfaceFlinger
                    std::string toString(bool splitLine = false) const;

                private:
                    Display& mDisplay;
                    hwc2_config_t mId;
                    std::unordered_map<HWC2::Attribute, int32_t> mAttributes;

                    // Maps from color transform to HWC1 config ID
                    std::unordered_map<android_color_mode_t, uint32_t> mHwc1Ids;
            };

            class Changes {
                public:
                    uint32_t getNumTypes() const {
                        return static_cast<uint32_t>(mTypeChanges.size());
                    }

                    uint32_t getNumLayerRequests() const {
                        return static_cast<uint32_t>(mLayerRequests.size());
                    }

                    const std::unordered_map<hwc2_layer_t, HWC2::Composition>&
                            getTypeChanges() const {
                        return mTypeChanges;
                    }

                    const std::unordered_map<hwc2_layer_t, HWC2::LayerRequest>&
                            getLayerRequests() const {
                        return mLayerRequests;
                    }

                    int32_t getDisplayRequests() const {
                        int32_t requests = 0;
                        for (auto request : mDisplayRequests) {
                            requests |= static_cast<int32_t>(request);
                        }
                        return requests;
                    }

                    void addTypeChange(hwc2_layer_t layerId,
                            HWC2::Composition type) {
                        mTypeChanges.insert({layerId, type});
                    }

                    void clearTypeChanges() { mTypeChanges.clear(); }

                    void addLayerRequest(hwc2_layer_t layerId,
                            HWC2::LayerRequest request) {
                        mLayerRequests.insert({layerId, request});
                    }

                private:
                    std::unordered_map<hwc2_layer_t, HWC2::Composition>
                            mTypeChanges;
                    std::unordered_map<hwc2_layer_t, HWC2::LayerRequest>
                            mLayerRequests;
                    std::unordered_set<HWC2::DisplayRequest> mDisplayRequests;
            };

            std::shared_ptr<const Config>
                    getConfig(hwc2_config_t configId) const;

            void populateColorModes();
            void initializeActiveConfig();

            void reallocateHwc1Contents();
            void assignHwc1LayerIds();

            void updateTypeChanges(const struct hwc_layer_1& hwc1Layer,
                    const Layer& layer);
            void updateLayerRequests(const struct hwc_layer_1& hwc1Layer,
                    const Layer& layer);

            void prepareFramebufferTarget();

            static std::atomic<hwc2_display_t> sNextId;
            const hwc2_display_t mId;
            HWC2On1Adapter& mDevice;

            std::atomic<size_t> mDirtyCount;

            // The state of this display should only be modified from
            // SurfaceFlinger's main loop, with the exception of when dump is
            // called. To prevent a bad state from crashing us during a dump
            // call, all public calls into Display must acquire this mutex.
            //
            // It is recursive because we don't want to deadlock in validate
            // (or present) when we call HWC2On1Adapter::prepareAllDisplays
            // (or setAllDisplays), which calls back into Display functions
            // which require locking.
            mutable std::recursive_mutex mStateMutex;

            bool mZIsDirty;
            HWC1Contents mHwc1RequestedContents;
            HWC1Contents mHwc1ReceivedContents;
            DeferredFence mRetireFence;

            // Will only be non-null after the layer has been validated but
            // before it has been presented
            std::unique_ptr<Changes> mChanges;

            int32_t mHwc1Id;

            std::vector<std::shared_ptr<Config>> mConfigs;
            std::shared_ptr<const Config> mActiveConfig;
            std::set<android_color_mode_t> mColorModes;
            android_color_mode_t mActiveColorMode;
            std::string mName;
            HWC2::DisplayType mType;
            HWC2::PowerMode mPowerMode;
            HWC2::Vsync mVsyncEnabled;

            FencedBuffer mClientTarget;
            FencedBuffer mOutputBuffer;

            bool mHasColorTransform;

            std::multiset<std::shared_ptr<Layer>, SortLayersByZ> mLayers;
            std::unordered_map<size_t, std::shared_ptr<Layer>> mHwc1LayerMap;
    };

    template <typename ...Args>
    static int32_t callDisplayFunction(hwc2_device_t* device,
            hwc2_display_t displayId, HWC2::Error (Display::*member)(Args...),
            Args... args) {
        auto display = getAdapter(device)->getDisplay(displayId);
        if (!display) {
            return static_cast<int32_t>(HWC2::Error::BadDisplay);
        }
        auto error = ((*display).*member)(std::forward<Args>(args)...);
        return static_cast<int32_t>(error);
    }

    template <typename MF, MF memFunc, typename ...Args>
    static int32_t displayHook(hwc2_device_t* device, hwc2_display_t displayId,
            Args... args) {
        return HWC2On1Adapter::callDisplayFunction(device, displayId, memFunc,
                std::forward<Args>(args)...);
    }

    static int32_t getDisplayAttributeHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_config_t config,
            int32_t intAttribute, int32_t* outValue) {
        auto attribute = static_cast<HWC2::Attribute>(intAttribute);
        return callDisplayFunction(device, display, &Display::getAttribute,
                config, attribute, outValue);
    }

    static int32_t setColorTransformHook(hwc2_device_t* device,
            hwc2_display_t display, const float* /*matrix*/,
            int32_t /*android_color_transform_t*/ intHint) {
        // We intentionally throw away the matrix, because if the hint is
        // anything other than IDENTITY, we have to fall back to client
        // composition anyway
        auto hint = static_cast<android_color_transform_t>(intHint);
        return callDisplayFunction(device, display, &Display::setColorTransform,
                hint);
    }

    static int32_t setColorModeHook(hwc2_device_t* device,
            hwc2_display_t display, int32_t /*android_color_mode_t*/ intMode) {
        auto mode = static_cast<android_color_mode_t>(intMode);
        return callDisplayFunction(device, display, &Display::setColorMode, mode);
    }

    static int32_t setPowerModeHook(hwc2_device_t* device,
            hwc2_display_t display, int32_t intMode) {
        auto mode = static_cast<HWC2::PowerMode>(intMode);
        return callDisplayFunction(device, display, &Display::setPowerMode,
                mode);
    }

    static int32_t setVsyncEnabledHook(hwc2_device_t* device,
            hwc2_display_t display, int32_t intEnabled) {
        auto enabled = static_cast<HWC2::Vsync>(intEnabled);
        return callDisplayFunction(device, display, &Display::setVsyncEnabled,
                enabled);
    }

    // Layer functions

    template <typename T>
    class LatchedState {
        public:
            LatchedState(Layer& parent, T initialValue)
              : mParent(parent),
                mPendingValue(initialValue),
                mValue(initialValue) {}

            void setPending(T value) {
                if (value == mPendingValue) {
                    return;
                }
                if (mPendingValue == mValue) {
                    mParent.incDirty();
                } else if (value == mValue) {
                    mParent.decDirty();
                }
                mPendingValue = value;
            }

            T getValue() const { return mValue; }
            T getPendingValue() const { return mPendingValue; }

            bool isDirty() const { return mPendingValue != mValue; }

            void latch() {
                if (isDirty()) {
                    mValue = mPendingValue;
                    mParent.decDirty();
                }
            }

        private:
            Layer& mParent;
            T mPendingValue;
            T mValue;
    };

    class Layer {
        public:
            Layer(Display& display);

            bool operator==(const Layer& other) { return mId == other.mId; }
            bool operator!=(const Layer& other) { return !(*this == other); }

            hwc2_layer_t getId() const { return mId; }
            Display& getDisplay() const { return mDisplay; }

            void incDirty() { if (mDirtyCount++ == 0) mDisplay.incDirty(); }
            void decDirty() { if (--mDirtyCount == 0) mDisplay.decDirty(); }
            bool isDirty() const { return mDirtyCount > 0; }

            // HWC2 Layer functions
            HWC2::Error setBuffer(buffer_handle_t buffer, int32_t acquireFence);
            HWC2::Error setCursorPosition(int32_t x, int32_t y);
            HWC2::Error setSurfaceDamage(hwc_region_t damage);

            // HWC2 Layer state functions
            HWC2::Error setBlendMode(HWC2::BlendMode mode);
            HWC2::Error setColor(hwc_color_t color);
            HWC2::Error setCompositionType(HWC2::Composition type);
            HWC2::Error setDataspace(android_dataspace_t dataspace);
            HWC2::Error setDisplayFrame(hwc_rect_t frame);
            HWC2::Error setPlaneAlpha(float alpha);
            HWC2::Error setSidebandStream(const native_handle_t* stream);
            HWC2::Error setSourceCrop(hwc_frect_t crop);
            HWC2::Error setTransform(HWC2::Transform transform);
            HWC2::Error setVisibleRegion(hwc_region_t visible);
            HWC2::Error setZ(uint32_t z);

            HWC2::Composition getCompositionType() const {
                return mCompositionType.getValue();
            }
            uint32_t getZ() const { return mZ; }

            void addReleaseFence(int fenceFd);
            const sp<Fence>& getReleaseFence() const;

            void setHwc1Id(size_t id) { mHwc1Id = id; }
            size_t getHwc1Id() const { return mHwc1Id; }

            void applyState(struct hwc_layer_1& hwc1Layer, bool applyAllState);

            std::string dump() const;

        private:
            void applyCommonState(struct hwc_layer_1& hwc1Layer,
                    bool applyAllState);
            void applySolidColorState(struct hwc_layer_1& hwc1Layer,
                    bool applyAllState);
            void applySidebandState(struct hwc_layer_1& hwc1Layer,
                    bool applyAllState);
            void applyBufferState(struct hwc_layer_1& hwc1Layer);
            void applyCompositionType(struct hwc_layer_1& hwc1Layer,
                    bool applyAllState);

            static std::atomic<hwc2_layer_t> sNextId;
            const hwc2_layer_t mId;
            Display& mDisplay;
            size_t mDirtyCount;

            FencedBuffer mBuffer;
            std::vector<hwc_rect_t> mSurfaceDamage;

            LatchedState<HWC2::BlendMode> mBlendMode;
            LatchedState<hwc_color_t> mColor;
            LatchedState<HWC2::Composition> mCompositionType;
            LatchedState<hwc_rect_t> mDisplayFrame;
            LatchedState<float> mPlaneAlpha;
            LatchedState<const native_handle_t*> mSidebandStream;
            LatchedState<hwc_frect_t> mSourceCrop;
            LatchedState<HWC2::Transform> mTransform;
            LatchedState<std::vector<hwc_rect_t>> mVisibleRegion;
            uint32_t mZ;

            DeferredFence mReleaseFence;

            size_t mHwc1Id;
            bool mHasUnsupportedDataspace;
            bool mHasUnsupportedPlaneAlpha;
    };

    template <typename ...Args>
    static int32_t callLayerFunction(hwc2_device_t* device,
            hwc2_display_t displayId, hwc2_layer_t layerId,
            HWC2::Error (Layer::*member)(Args...), Args... args) {
        auto result = getAdapter(device)->getLayer(displayId, layerId);
        auto error = std::get<HWC2::Error>(result);
        if (error == HWC2::Error::None) {
            auto layer = std::get<Layer*>(result);
            error = ((*layer).*member)(std::forward<Args>(args)...);
        }
        return static_cast<int32_t>(error);
    }

    template <typename MF, MF memFunc, typename ...Args>
    static int32_t layerHook(hwc2_device_t* device, hwc2_display_t displayId,
            hwc2_layer_t layerId, Args... args) {
        return HWC2On1Adapter::callLayerFunction(device, displayId, layerId,
                memFunc, std::forward<Args>(args)...);
    }

    // Layer state functions

    static int32_t setLayerBlendModeHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_layer_t layer, int32_t intMode) {
        auto mode = static_cast<HWC2::BlendMode>(intMode);
        return callLayerFunction(device, display, layer,
                &Layer::setBlendMode, mode);
    }

    static int32_t setLayerCompositionTypeHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_layer_t layer, int32_t intType) {
        auto type = static_cast<HWC2::Composition>(intType);
        return callLayerFunction(device, display, layer,
                &Layer::setCompositionType, type);
    }

    static int32_t setLayerDataspaceHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_layer_t layer, int32_t intDataspace) {
        auto dataspace = static_cast<android_dataspace_t>(intDataspace);
        return callLayerFunction(device, display, layer, &Layer::setDataspace,
                dataspace);
    }

    static int32_t setLayerTransformHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_layer_t layer, int32_t intTransform) {
        auto transform = static_cast<HWC2::Transform>(intTransform);
        return callLayerFunction(device, display, layer, &Layer::setTransform,
                transform);
    }

    static int32_t setLayerZOrderHook(hwc2_device_t* device,
            hwc2_display_t display, hwc2_layer_t layer, uint32_t z) {
        return callDisplayFunction(device, display, &Display::updateLayerZ,
                layer, z);
    }

    // Adapter internals

    void populateCapabilities();
    Display* getDisplay(hwc2_display_t id);
    std::tuple<Layer*, HWC2::Error> getLayer(hwc2_display_t displayId,
            hwc2_layer_t layerId);
    void populatePrimary();

    bool prepareAllDisplays();
    std::vector<struct hwc_display_contents_1*> mHwc1Contents;
    HWC2::Error setAllDisplays();

    void hwc1Invalidate();
    void hwc1Vsync(int hwc1DisplayId, int64_t timestamp);
    void hwc1Hotplug(int hwc1DisplayId, int connected);

    // These are set in the constructor and before any asynchronous events are
    // possible

    struct hwc_composer_device_1* const mHwc1Device;
    const uint8_t mHwc1MinorVersion;
    bool mHwc1SupportsVirtualDisplays;

    class Callbacks;
    const std::unique_ptr<Callbacks> mHwc1Callbacks;

    std::unordered_set<HWC2::Capability> mCapabilities;

    // These are only accessed from the main SurfaceFlinger thread (not from
    // callbacks or dump

    std::map<hwc2_layer_t, std::shared_ptr<Layer>> mLayers;
    std::shared_ptr<Display> mHwc1VirtualDisplay;

    // These are potentially accessed from multiple threads, and are protected
    // by this mutex. This needs to be recursive, since the HWC1 implementation
    // can call back into the invalidate callback on the same thread that is
    // calling prepare.
    std::recursive_timed_mutex mStateMutex;

    struct CallbackInfo {
        hwc2_callback_data_t data;
        hwc2_function_pointer_t pointer;
    };
    std::unordered_map<HWC2::Callback, CallbackInfo> mCallbacks;
    bool mHasPendingInvalidate;
    std::vector<std::pair<int, int64_t>> mPendingVsyncs;
    std::vector<std::pair<int, int>> mPendingHotplugs;

    std::map<hwc2_display_t, std::shared_ptr<Display>> mDisplays;
    std::unordered_map<int, hwc2_display_t> mHwc1DisplayMap;
};

} // namespace android

#endif
