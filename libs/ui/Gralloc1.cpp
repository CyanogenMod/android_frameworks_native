/*
 * Copyright 2016 The Android Open Source Project
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

//#define LOG_NDEBUG 0

#include <ui/Gralloc1.h>

#include <vector>

#undef LOG_TAG
#define LOG_TAG GRALLOC1_LOG_TAG

namespace android {

namespace Gralloc1 {

Descriptor::~Descriptor()
{
    int32_t intError = mShimDevice.mFunctions.destroyDescriptor(
            mShimDevice.mDevice, mDeviceId);
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error != GRALLOC1_ERROR_NONE) {
        ALOGE("destroyDescriptor failed: %d", intError);
    }
}

gralloc1_error_t Descriptor::setDimensions(uint32_t width, uint32_t height)
{
    int32_t intError = mShimDevice.mFunctions.setDimensions(mShimDevice.mDevice,
            mDeviceId, width, height);
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error != GRALLOC1_ERROR_NONE) {
        return error;
    }
    mWidth = width;
    mHeight = height;
    return error;
}

template <typename ApiType>
struct Setter {
    typedef int32_t (*Type)(gralloc1_device_t*, gralloc1_buffer_descriptor_t,
            ApiType);
};

template <typename ApiType, typename ValueType>
static inline gralloc1_error_t setHelper(
        typename Setter<ApiType>::Type setter, gralloc1_device_t* device,
        gralloc1_buffer_descriptor_t id, ValueType newValue,
        ValueType* cacheVariable)
{
    int32_t intError = setter(device, id, static_cast<ApiType>(newValue));
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error != GRALLOC1_ERROR_NONE) {
        return error;
    }
    *cacheVariable = newValue;
    return error;
}

gralloc1_error_t Descriptor::setFormat(android_pixel_format_t format)
{
    return setHelper<int32_t>(mShimDevice.mFunctions.setFormat.pfn,
            mShimDevice.mDevice, mDeviceId, format, &mFormat);
}

gralloc1_error_t Descriptor::setProducerUsage(gralloc1_producer_usage_t usage)
{
    return setHelper<uint64_t>(mShimDevice.mFunctions.setProducerUsage.pfn,
            mShimDevice.mDevice, mDeviceId, usage, &mProducerUsage);
}

gralloc1_error_t Descriptor::setConsumerUsage(gralloc1_consumer_usage_t usage)
{
    return setHelper<uint64_t>(mShimDevice.mFunctions.setConsumerUsage.pfn,
            mShimDevice.mDevice, mDeviceId, usage, &mConsumerUsage);
}

Device::Device(gralloc1_device_t* device)
  : mDevice(device),
    mCapabilities(loadCapabilities()),
    mFunctions()
{
    if (!loadFunctions()) {
        ALOGE("Failed to load a required function, aborting");
        abort();
    }
}

bool Device::hasCapability(gralloc1_capability_t capability) const
{
    return mCapabilities.count(capability) > 0;
}

std::string Device::dump()
{
    uint32_t length = 0;
    mFunctions.dump(mDevice, &length, nullptr);

    std::vector<char> output;
    output.resize(length);
    mFunctions.dump(mDevice, &length, output.data());

    return std::string(output.cbegin(), output.cend());
}

std::shared_ptr<Descriptor> Device::createDescriptor()
{
    gralloc1_buffer_descriptor_t descriptorId;
    int32_t intError = mFunctions.createDescriptor(mDevice, &descriptorId);
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error != GRALLOC1_ERROR_NONE) {
        return nullptr;
    }
    auto descriptor = std::make_shared<Descriptor>(*this, descriptorId);
    return descriptor;
}

gralloc1_error_t Device::getStride(buffer_handle_t buffer, uint32_t* outStride)
{
    int32_t intError = mFunctions.getStride(mDevice, buffer, outStride);
    return static_cast<gralloc1_error_t>(intError);
}

static inline bool allocationSucceded(gralloc1_error_t error)
{
    return error == GRALLOC1_ERROR_NONE || error == GRALLOC1_ERROR_NOT_SHARED;
}

gralloc1_error_t Device::allocate(
        const std::vector<std::shared_ptr<const Descriptor>>& descriptors,
        std::vector<buffer_handle_t>* outBuffers)
{
    if (mFunctions.allocate.pfn == nullptr) {
        // Allocation is not supported on this device
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    std::vector<gralloc1_buffer_descriptor_t> deviceIds;
    for (const auto& descriptor : descriptors) {
        deviceIds.emplace_back(descriptor->getDeviceId());
    }

    std::vector<buffer_handle_t> buffers(descriptors.size());
    int32_t intError = mFunctions.allocate(mDevice,
            static_cast<uint32_t>(descriptors.size()), deviceIds.data(),
            buffers.data());
    auto error = static_cast<gralloc1_error_t>(intError);
    if (allocationSucceded(error)) {
        *outBuffers = std::move(buffers);
    }

    return error;
}

gralloc1_error_t Device::allocate(
        const std::shared_ptr<const Descriptor>& descriptor,
        gralloc1_backing_store_t id, buffer_handle_t* outBuffer)
{
    gralloc1_error_t error = GRALLOC1_ERROR_NONE;

    if (hasCapability(GRALLOC1_CAPABILITY_ON_ADAPTER)) {
        buffer_handle_t buffer = nullptr;
        int32_t intError = mFunctions.allocateWithId(mDevice,
                descriptor->getDeviceId(), id, &buffer);
        error = static_cast<gralloc1_error_t>(intError);
        if (allocationSucceded(error)) {
            *outBuffer = buffer;
        }
    } else {
        std::vector<std::shared_ptr<const Descriptor>> descriptors;
        descriptors.emplace_back(descriptor);
        std::vector<buffer_handle_t> buffers;
        error = allocate(descriptors, &buffers);
        if (allocationSucceded(error)) {
            *outBuffer = buffers[0];
        }
    }

    return error;
}

gralloc1_error_t Device::retain(buffer_handle_t buffer)
{
    int32_t intError = mFunctions.retain(mDevice, buffer);
    return static_cast<gralloc1_error_t>(intError);
}

gralloc1_error_t Device::retain(const GraphicBuffer* buffer)
{
    if (hasCapability(GRALLOC1_CAPABILITY_ON_ADAPTER)) {
        return mFunctions.retainGraphicBuffer(mDevice, buffer);
    } else {
        return retain(buffer->getNativeBuffer()->handle);
    }
}

gralloc1_error_t Device::release(buffer_handle_t buffer)
{
    int32_t intError = mFunctions.release(mDevice, buffer);
    return static_cast<gralloc1_error_t>(intError);
}

gralloc1_error_t Device::getNumFlexPlanes(buffer_handle_t buffer,
        uint32_t* outNumPlanes)
{
    uint32_t numPlanes = 0;
    int32_t intError = mFunctions.getNumFlexPlanes(mDevice, buffer, &numPlanes);
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error == GRALLOC1_ERROR_NONE) {
        *outNumPlanes = numPlanes;
    }
    return error;
}

gralloc1_error_t Device::lock(buffer_handle_t buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t* accessRegion, void** outData,
        const sp<Fence>& acquireFence)
{
    ALOGV("Calling lock(%p)", buffer);
    return lockHelper(mFunctions.lock.pfn, buffer, producerUsage,
            consumerUsage, accessRegion, outData, acquireFence);
}

gralloc1_error_t Device::lockFlex(buffer_handle_t buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t* accessRegion,
        struct android_flex_layout* outData,
        const sp<Fence>& acquireFence)
{
    ALOGV("Calling lockFlex(%p)", buffer);
    return lockHelper(mFunctions.lockFlex.pfn, buffer, producerUsage,
            consumerUsage, accessRegion, outData, acquireFence);
}

gralloc1_error_t Device::lockYCbCr(buffer_handle_t buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t* accessRegion,
        struct android_ycbcr* outData,
        const sp<Fence>& acquireFence)
{
    ALOGV("Calling lockYCbCr(%p)", buffer);
    return lockHelper(mFunctions.lockYCbCr.pfn, buffer, producerUsage,
            consumerUsage, accessRegion, outData, acquireFence);
}

gralloc1_error_t Device::unlock(buffer_handle_t buffer, sp<Fence>* outFence)
{
    int32_t fenceFd = -1;
    int32_t intError = mFunctions.unlock(mDevice, buffer, &fenceFd);
    auto error = static_cast<gralloc1_error_t>(intError);
    if (error == GRALLOC1_ERROR_NONE) {
        *outFence = new Fence(fenceFd);
    }
    return error;
}

std::unordered_set<gralloc1_capability_t> Device::loadCapabilities()
{
    std::vector<int32_t> intCapabilities;
    uint32_t numCapabilities = 0;
    mDevice->getCapabilities(mDevice, &numCapabilities, nullptr);

    intCapabilities.resize(numCapabilities);
    mDevice->getCapabilities(mDevice, &numCapabilities, intCapabilities.data());

    std::unordered_set<gralloc1_capability_t> capabilities;
    for (const auto intCapability : intCapabilities) {
        capabilities.emplace(static_cast<gralloc1_capability_t>(intCapability));
    }
    return capabilities;
}

bool Device::loadFunctions()
{
    // Functions which must always be present
    if (!mFunctions.dump.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.createDescriptor.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.destroyDescriptor.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.setConsumerUsage.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.setDimensions.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.setFormat.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.setProducerUsage.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getBackingStore.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getConsumerUsage.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getDimensions.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getFormat.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getProducerUsage.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getStride.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.retain.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.release.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.getNumFlexPlanes.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.lock.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.lockFlex.load(mDevice, true)) {
        return false;
    }
    if (!mFunctions.unlock.load(mDevice, true)) {
        return false;
    }

    if (hasCapability(GRALLOC1_CAPABILITY_ON_ADAPTER)) {
        // These should always be present on the adapter
        if (!mFunctions.retainGraphicBuffer.load(mDevice, true)) {
            return false;
        }
        if (!mFunctions.lockYCbCr.load(mDevice, true)) {
            return false;
        }

        // allocateWithId may not be present if we're only able to map in this
        // process
        mFunctions.allocateWithId.load(mDevice, false);
    } else {
        // allocate may not be present if we're only able to map in this process
        mFunctions.allocate.load(mDevice, false);
    }

    return true;
}

std::unique_ptr<Gralloc1On0Adapter> Loader::mAdapter = nullptr;

Loader::Loader()
  : mDevice(nullptr)
{
    hw_module_t const* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    uint8_t majorVersion = (module->module_api_version >> 8) & 0xFF;
    uint8_t minorVersion = module->module_api_version & 0xFF;
    gralloc1_device_t* device = nullptr;
    if (majorVersion == 1) {
        gralloc1_open(module, &device);
    } else {
        if (!mAdapter) {
            mAdapter = std::make_unique<Gralloc1On0Adapter>(module);
        }
        device = mAdapter->getDevice();
    }
    mDevice = std::make_unique<Gralloc1::Device>(device);
}

Loader::~Loader() {}

std::unique_ptr<Device> Loader::getDevice()
{
    return std::move(mDevice);
}

} // namespace android::Gralloc1

} // namespace android
