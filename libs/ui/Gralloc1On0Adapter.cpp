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

#undef LOG_TAG
#define LOG_TAG "Gralloc1On0Adapter"
//#define LOG_NDEBUG 0

#include <hardware/gralloc.h>

#include <ui/Gralloc1On0Adapter.h>

#include <utils/Log.h>

#include <inttypes.h>

template <typename PFN, typename T>
static gralloc1_function_pointer_t asFP(T function)
{
    static_assert(std::is_same<PFN, T>::value, "Incompatible function pointer");
    return reinterpret_cast<gralloc1_function_pointer_t>(function);
}

namespace android {

Gralloc1On0Adapter::Gralloc1On0Adapter(const hw_module_t* module)
  : mModule(reinterpret_cast<const gralloc_module_t*>(module)),
    mMinorVersion(mModule->common.module_api_version & 0xFF),
    mDevice(nullptr)
{
    ALOGV("Constructing");
    getCapabilities = getCapabilitiesHook;
    getFunction = getFunctionHook;
    int error = ::gralloc_open(&(mModule->common), &mDevice);
    if (error) {
        ALOGE("Failed to open gralloc0 module: %d", error);
    }
    ALOGV("Opened gralloc0 device %p", mDevice);
}

Gralloc1On0Adapter::~Gralloc1On0Adapter()
{
    ALOGV("Destructing");
    if (mDevice) {
        ALOGV("Closing gralloc0 device %p", mDevice);
        ::gralloc_close(mDevice);
    }
}

void Gralloc1On0Adapter::doGetCapabilities(uint32_t* outCount,
        int32_t* outCapabilities)
{
    if (outCapabilities == nullptr) {
        *outCount = 1;
        return;
    }
    if (*outCount >= 1) {
        *outCapabilities = GRALLOC1_CAPABILITY_ON_ADAPTER;
        *outCount = 1;
    }
}

gralloc1_function_pointer_t Gralloc1On0Adapter::doGetFunction(
        int32_t intDescriptor)
{
    constexpr auto lastDescriptor =
            static_cast<int32_t>(GRALLOC1_LAST_ADAPTER_FUNCTION);
    if (intDescriptor < 0 || intDescriptor > lastDescriptor) {
        ALOGE("Invalid function descriptor");
        return nullptr;
    }

    auto descriptor =
            static_cast<gralloc1_function_descriptor_t>(intDescriptor);
    switch (descriptor) {
        case GRALLOC1_FUNCTION_DUMP:
            return asFP<GRALLOC1_PFN_DUMP>(dumpHook);
        case GRALLOC1_FUNCTION_CREATE_DESCRIPTOR:
            return asFP<GRALLOC1_PFN_CREATE_DESCRIPTOR>(createDescriptorHook);
        case GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR:
            return asFP<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(destroyDescriptorHook);
        case GRALLOC1_FUNCTION_SET_CONSUMER_USAGE:
            return asFP<GRALLOC1_PFN_SET_CONSUMER_USAGE>(setConsumerUsageHook);
        case GRALLOC1_FUNCTION_SET_DIMENSIONS:
            return asFP<GRALLOC1_PFN_SET_DIMENSIONS>(setDimensionsHook);
        case GRALLOC1_FUNCTION_SET_FORMAT:
            return asFP<GRALLOC1_PFN_SET_FORMAT>(setFormatHook);
        case GRALLOC1_FUNCTION_SET_PRODUCER_USAGE:
            return asFP<GRALLOC1_PFN_SET_PRODUCER_USAGE>(setProducerUsageHook);
        case GRALLOC1_FUNCTION_GET_BACKING_STORE:
            return asFP<GRALLOC1_PFN_GET_BACKING_STORE>(
                    bufferHook<decltype(&Buffer::getBackingStore),
                    &Buffer::getBackingStore, gralloc1_backing_store_t*>);
        case GRALLOC1_FUNCTION_GET_CONSUMER_USAGE:
            return asFP<GRALLOC1_PFN_GET_CONSUMER_USAGE>(getConsumerUsageHook);
        case GRALLOC1_FUNCTION_GET_DIMENSIONS:
            return asFP<GRALLOC1_PFN_GET_DIMENSIONS>(
                    bufferHook<decltype(&Buffer::getDimensions),
                    &Buffer::getDimensions, uint32_t*, uint32_t*>);
        case GRALLOC1_FUNCTION_GET_FORMAT:
            return asFP<GRALLOC1_PFN_GET_FORMAT>(
                    bufferHook<decltype(&Buffer::getFormat),
                    &Buffer::getFormat, int32_t*>);
        case GRALLOC1_FUNCTION_GET_PRODUCER_USAGE:
            return asFP<GRALLOC1_PFN_GET_PRODUCER_USAGE>(getProducerUsageHook);
        case GRALLOC1_FUNCTION_GET_STRIDE:
            return asFP<GRALLOC1_PFN_GET_STRIDE>(
                    bufferHook<decltype(&Buffer::getStride),
                    &Buffer::getStride, uint32_t*>);
        case GRALLOC1_FUNCTION_ALLOCATE:
            // Not provided, since we'll use ALLOCATE_WITH_ID
            return nullptr;
        case GRALLOC1_FUNCTION_ALLOCATE_WITH_ID:
            if (mDevice != nullptr) {
                return asFP<GRALLOC1_PFN_ALLOCATE_WITH_ID>(allocateWithIdHook);
            } else {
                return nullptr;
            }
        case GRALLOC1_FUNCTION_RETAIN:
            return asFP<GRALLOC1_PFN_RETAIN>(
                    managementHook<&Gralloc1On0Adapter::retain>);
        case GRALLOC1_FUNCTION_RELEASE:
            return asFP<GRALLOC1_PFN_RELEASE>(
                    managementHook<&Gralloc1On0Adapter::release>);
        case GRALLOC1_FUNCTION_RETAIN_GRAPHIC_BUFFER:
            return asFP<GRALLOC1_PFN_RETAIN_GRAPHIC_BUFFER>(
                    retainGraphicBufferHook);
        case GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES:
            return asFP<GRALLOC1_PFN_GET_NUM_FLEX_PLANES>(
                    bufferHook<decltype(&Buffer::getNumFlexPlanes),
                    &Buffer::getNumFlexPlanes, uint32_t*>);
        case GRALLOC1_FUNCTION_LOCK:
            return asFP<GRALLOC1_PFN_LOCK>(
                    lockHook<void*, &Gralloc1On0Adapter::lock>);
        case GRALLOC1_FUNCTION_LOCK_FLEX:
            return asFP<GRALLOC1_PFN_LOCK_FLEX>(
                    lockHook<struct android_flex_layout,
                    &Gralloc1On0Adapter::lockFlex>);
        case GRALLOC1_FUNCTION_LOCK_YCBCR:
            return asFP<GRALLOC1_PFN_LOCK_YCBCR>(
                    lockHook<struct android_ycbcr,
                    &Gralloc1On0Adapter::lockYCbCr>);
        case GRALLOC1_FUNCTION_UNLOCK:
            return asFP<GRALLOC1_PFN_UNLOCK>(unlockHook);
#ifdef EXYNOS4_ENHANCEMENTS
        case GRALLOC1_FUNCTION_GETPHYS:
            return asFP<GRALLOC1_PFN_GETPHYS>(getphysHook);
#endif
        case GRALLOC1_FUNCTION_INVALID:
            ALOGE("Invalid function descriptor");
            return nullptr;
    }

    ALOGE("Unknown function descriptor: %d", intDescriptor);
    return nullptr;
}

void Gralloc1On0Adapter::dump(uint32_t* outSize, char* outBuffer)
{
    ALOGV("dump(%u (%p), %p", outSize ? *outSize : 0, outSize, outBuffer);

    if (!mDevice->dump) {
        // dump is optional on gralloc0 implementations
        *outSize = 0;
        return;
    }

    if (!outBuffer) {
        constexpr int32_t BUFFER_LENGTH = 4096;
        char buffer[BUFFER_LENGTH] = {};
        mDevice->dump(mDevice, buffer, BUFFER_LENGTH);
        buffer[BUFFER_LENGTH - 1] = 0; // Ensure the buffer is null-terminated
        size_t actualLength = std::strlen(buffer);
        mCachedDump.resize(actualLength);
        std::copy_n(buffer, actualLength, mCachedDump.begin());
        *outSize = static_cast<uint32_t>(actualLength);
    } else {
        *outSize = std::min(*outSize,
                static_cast<uint32_t>(mCachedDump.size()));
        outBuffer = std::copy_n(mCachedDump.cbegin(), *outSize, outBuffer);
    }
}

gralloc1_error_t Gralloc1On0Adapter::createDescriptor(
        gralloc1_buffer_descriptor_t* outDescriptor)
{
    auto descriptorId = sNextBufferDescriptorId++;
    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    mDescriptors.emplace(descriptorId,
            std::make_shared<Descriptor>(this, descriptorId));

    ALOGV("Created descriptor %" PRIu64, descriptorId);

    *outDescriptor = descriptorId;
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::destroyDescriptor(
        gralloc1_buffer_descriptor_t descriptor)
{
    ALOGV("Destroying descriptor %" PRIu64, descriptor);

    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    if (mDescriptors.count(descriptor) == 0) {
        return GRALLOC1_ERROR_BAD_DESCRIPTOR;
    }

    mDescriptors.erase(descriptor);
    return GRALLOC1_ERROR_NONE;
}

Gralloc1On0Adapter::Buffer::Buffer(buffer_handle_t handle,
        gralloc1_backing_store_t store, const Descriptor& descriptor,
        uint32_t stride, bool wasAllocated)
  : mHandle(handle),
    mReferenceCount(1),
    mStore(store),
    mDescriptor(descriptor),
    mStride(stride),
    mWasAllocated(wasAllocated) {}

gralloc1_error_t Gralloc1On0Adapter::allocate(
        const std::shared_ptr<Descriptor>& descriptor,
        gralloc1_backing_store_t store,
        buffer_handle_t* outBufferHandle)
{
    ALOGV("allocate(%" PRIu64 ", %#" PRIx64 ")", descriptor->id, store);

    // If this function is being called, it's because we handed out its function
    // pointer, which only occurs when mDevice has been loaded successfully and
    // we are permitted to allocate

    int usage = static_cast<int>(descriptor->producerUsage) |
            static_cast<int>(descriptor->consumerUsage);
    buffer_handle_t handle = nullptr;
    int stride = 0;
    ALOGV("Calling alloc(%p, %u, %u, %i, %u)", mDevice, descriptor->width,
            descriptor->height, descriptor->format, usage);
    auto error = mDevice->alloc(mDevice,
            static_cast<int>(descriptor->width),
            static_cast<int>(descriptor->height), descriptor->format,
            usage, &handle, &stride);
    if (error != 0) {
        ALOGE("gralloc0 allocation failed: %d (%s)", error,
                strerror(-error));
        return GRALLOC1_ERROR_NO_RESOURCES;
    }

    *outBufferHandle = handle;
    auto buffer = std::make_shared<Buffer>(handle, store, *descriptor, stride,
            true);

    std::lock_guard<std::mutex> lock(mBufferMutex);
    mBuffers.emplace(handle, std::move(buffer));

    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::allocateWithIdHook(
        gralloc1_device_t* device, gralloc1_buffer_descriptor_t descriptorId,
        gralloc1_backing_store_t store, buffer_handle_t* outBuffer)
{
    auto adapter = getAdapter(device);

    auto descriptor = adapter->getDescriptor(descriptorId);
    if (!descriptor) {
        return GRALLOC1_ERROR_BAD_DESCRIPTOR;
    }

    buffer_handle_t bufferHandle = nullptr;
    auto error = adapter->allocate(descriptor, store, &bufferHandle);
    if (error != GRALLOC1_ERROR_NONE) {
        return error;
    }

    *outBuffer = bufferHandle;
    return error;
}

#ifdef EXYNOS4_ENHANCEMENTS
gralloc1_error_t Gralloc1On0Adapter::getphys(
        gralloc1_device_t* device,
        buffer_handle_t handle,
        void **paddr)
{
    gralloc1_error_t err;
    auto adapter = getAdapter(device);
    int res = mModule->getphys(mModule, handle, paddr);

    if (res) {
        ALOGE("getphys(%p) fail %d(%s)", handle, res, strerror(-res));
        err = GRALLOC1_ERROR_UNDEFINED;
    } else {
        err = GRALLOC1_ERROR_NONE;
    }

    return err;
}
#endif

gralloc1_error_t Gralloc1On0Adapter::retain(
        const std::shared_ptr<Buffer>& buffer)
{
    buffer->retain();
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::release(
        const std::shared_ptr<Buffer>& buffer)
{
    if (!buffer->release()) {
        return GRALLOC1_ERROR_NONE;
    }

    buffer_handle_t handle = buffer->getHandle();
    if (buffer->wasAllocated()) {
        ALOGV("Calling free(%p)", handle);
        int result = mDevice->free(mDevice, handle);
        if (result != 0) {
            ALOGE("gralloc0 free failed: %d", result);
        }
    } else {
        ALOGV("Calling unregisterBuffer(%p)", handle);
        int result = mModule->unregisterBuffer(mModule, handle);
        if (result != 0) {
            ALOGE("gralloc0 unregister failed: %d", result);
        }
    }

    std::lock_guard<std::mutex> lock(mBufferMutex);
    mBuffers.erase(handle);
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::retain(
        const android::GraphicBuffer* graphicBuffer)
{
    ALOGV("retainGraphicBuffer(%p, %#" PRIx64 ")",
            graphicBuffer->getNativeBuffer()->handle, graphicBuffer->getId());

    buffer_handle_t handle = graphicBuffer->getNativeBuffer()->handle;
    std::lock_guard<std::mutex> lock(mBufferMutex);
    if (mBuffers.count(handle) != 0) {
        mBuffers[handle]->retain();
        return GRALLOC1_ERROR_NONE;
    }

    ALOGV("Calling registerBuffer(%p)", handle);
    int result = mModule->registerBuffer(mModule, handle);
    if (result != 0) {
        ALOGE("gralloc0 register failed: %d", result);
        return GRALLOC1_ERROR_NO_RESOURCES;
    }

    Descriptor descriptor{this, sNextBufferDescriptorId++};
    descriptor.setDimensions(graphicBuffer->getWidth(),
            graphicBuffer->getHeight());
    descriptor.setFormat(graphicBuffer->getPixelFormat());
    descriptor.setProducerUsage(
            static_cast<gralloc1_producer_usage_t>(graphicBuffer->getUsage()));
    descriptor.setConsumerUsage(
            static_cast<gralloc1_consumer_usage_t>(graphicBuffer->getUsage()));
    auto buffer = std::make_shared<Buffer>(handle,
            static_cast<gralloc1_backing_store_t>(graphicBuffer->getId()),
            descriptor, graphicBuffer->getStride(), false);
    mBuffers.emplace(handle, std::move(buffer));
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::lock(
        const std::shared_ptr<Buffer>& buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t& accessRegion, void** outData,
        const sp<Fence>& acquireFence)
{
    if (mMinorVersion >= 3) {
        int result = mModule->lockAsync(mModule, buffer->getHandle(),
                static_cast<int32_t>(producerUsage | consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData, acquireFence->dup());
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    } else {
        acquireFence->waitForever("Gralloc1On0Adapter::lock");
        int result = mModule->lock(mModule, buffer->getHandle(),
                static_cast<int32_t>(producerUsage | consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData);
        ALOGV("gralloc0 lock returned %d", result);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    }
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::lockFlex(
        const std::shared_ptr<Buffer>& /*buffer*/,
        gralloc1_producer_usage_t /*producerUsage*/,
        gralloc1_consumer_usage_t /*consumerUsage*/,
        const gralloc1_rect_t& /*accessRegion*/,
        struct android_flex_layout* /*outData*/,
        const sp<Fence>& /*acquireFence*/)
{
    // TODO
    return GRALLOC1_ERROR_UNSUPPORTED;
}

gralloc1_error_t Gralloc1On0Adapter::lockYCbCr(
        const std::shared_ptr<Buffer>& buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t& accessRegion, struct android_ycbcr* outData,
        const sp<Fence>& acquireFence)
{
    if (mMinorVersion >= 3 && mModule->lockAsync_ycbcr) {
        int result = mModule->lockAsync_ycbcr(mModule, buffer->getHandle(),
                static_cast<int>(producerUsage | consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData, acquireFence->dup());
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    } else if (mModule->lock_ycbcr) {
        acquireFence->waitForever("Gralloc1On0Adapter::lockYCbCr");
        int result = mModule->lock_ycbcr(mModule, buffer->getHandle(),
                static_cast<int>(producerUsage | consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData);
        ALOGV("gralloc0 lockYCbCr returned %d", result);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    } else {
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::unlock(
        const std::shared_ptr<Buffer>& buffer,
        sp<Fence>* outReleaseFence)
{
    if (mMinorVersion >= 3) {
        int fenceFd = -1;
        int result = mModule->unlockAsync(mModule, buffer->getHandle(),
                &fenceFd);
        if (result != 0) {
            close(fenceFd);
            ALOGE("gralloc0 unlockAsync failed: %d", result);
        } else {
            *outReleaseFence = new Fence(fenceFd);
        }
    } else {
        int result = mModule->unlock(mModule, buffer->getHandle());
        if (result != 0) {
            ALOGE("gralloc0 unlock failed: %d", result);
        }
    }
    return GRALLOC1_ERROR_NONE;
}

std::shared_ptr<Gralloc1On0Adapter::Descriptor>
Gralloc1On0Adapter::getDescriptor(gralloc1_buffer_descriptor_t descriptorId)
{
    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    if (mDescriptors.count(descriptorId) == 0) {
        return nullptr;
    }

    return mDescriptors[descriptorId];
}

std::shared_ptr<Gralloc1On0Adapter::Buffer> Gralloc1On0Adapter::getBuffer(
        buffer_handle_t bufferHandle)
{
    std::lock_guard<std::mutex> lock(mBufferMutex);
    if (mBuffers.count(bufferHandle) == 0) {
        return nullptr;
    }

    return mBuffers[bufferHandle];
}

std::atomic<gralloc1_buffer_descriptor_t>
        Gralloc1On0Adapter::sNextBufferDescriptorId(1);

} // namespace android
