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

// #define LOG_NDEBUG 0

#include <algorithm>
#include <memory>

#include <gui/BufferQueue.h>
#include <log/log.h>
#include <sync/sync.h>

#include "loader.h"

using namespace vulkan;

// TODO(jessehall): Currently we don't have a good error code for when a native
// window operation fails. Just returning INITIALIZATION_FAILED for now. Later
// versions (post SDK 0.9) of the API/extension have a better error code.
// When updating to that version, audit all error returns.

namespace {

// ----------------------------------------------------------------------------
// These functions/classes form an adaptor that allows objects to be refcounted
// by both android::sp<> and std::shared_ptr<> simultaneously, and delegates
// allocation of the shared_ptr<> control structure to VkAllocCallbacks. The
// platform holds a reference to the ANativeWindow using its embedded reference
// count, and the ANativeWindow implementation holds references to the
// ANativeWindowBuffers using their embedded reference counts, so the
// shared_ptr *must* cooperate with these and hold at least one reference to
// the object using the embedded reference count.

template <typename T>
struct NativeBaseDeleter {
    void operator()(T* obj) { obj->common.decRef(&obj->common); }
};

template <typename T>
class VulkanAllocator {
   public:
    typedef T value_type;

    explicit VulkanAllocator(VkDevice device) : device_(device) {}

    template <typename U>
    explicit VulkanAllocator(const VulkanAllocator<U>& other)
        : device_(other.device_) {}

    T* allocate(size_t n) const {
        return static_cast<T*>(AllocDeviceMem(
            device_, n * sizeof(T), alignof(T), VK_SYSTEM_ALLOC_TYPE_INTERNAL));
    }
    void deallocate(T* p, size_t) const { return FreeDeviceMem(device_, p); }

   private:
    template <typename U>
    friend class VulkanAllocator;
    VkDevice device_;
};

template <typename T>
std::shared_ptr<T> InitSharedPtr(VkDevice device, T* obj) {
    obj->common.incRef(&obj->common);
    return std::shared_ptr<T>(obj, NativeBaseDeleter<T>(),
                              VulkanAllocator<T>(device));
}

// ----------------------------------------------------------------------------

struct Swapchain {
    Swapchain(std::shared_ptr<ANativeWindow> window_, uint32_t num_images_)
        : window(window_), num_images(num_images_) {}

    std::shared_ptr<ANativeWindow> window;
    uint32_t num_images;

    struct Image {
        Image() : image(VK_NULL_HANDLE), dequeue_fence(-1), dequeued(false) {}
        VkImage image;
        std::shared_ptr<ANativeWindowBuffer> buffer;
        // The fence is only valid when the buffer is dequeued, and should be
        // -1 any other time. When valid, we own the fd, and must ensure it is
        // closed: either by closing it explicitly when queueing the buffer,
        // or by passing ownership e.g. to ANativeWindow::cancelBuffer().
        int dequeue_fence;
        bool dequeued;
    } images[android::BufferQueue::NUM_BUFFER_SLOTS];
};

VkSwapchainKHR HandleFromSwapchain(Swapchain* swapchain) {
    return VkSwapchainKHR(reinterpret_cast<uint64_t>(swapchain));
}

Swapchain* SwapchainFromHandle(VkSwapchainKHR handle) {
    return reinterpret_cast<Swapchain*>(handle.handle);
}

}  // anonymous namespace

namespace vulkan {

VkResult GetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice /*pdev*/,
    uint32_t /*queue_family*/,
    const VkSurfaceDescriptionKHR* surface_desc,
    VkBool32* supported) {
// TODO(jessehall): Fix the header, preferrably upstream, so values added to
// existing enums don't trigger warnings like this.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
    ALOGE_IF(
        surface_desc->sType != VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_KHR,
        "vkGetPhysicalDeviceSurfaceSupportKHR: pSurfaceDescription->sType=%#x "
        "not supported",
        surface_desc->sType);
#pragma clang diagnostic pop

    const VkSurfaceDescriptionWindowKHR* window_desc =
        reinterpret_cast<const VkSurfaceDescriptionWindowKHR*>(surface_desc);

    // TODO(jessehall): Also check whether the physical device exports the
    // VK_EXT_ANDROID_native_buffer extension. For now, assume it does.
    *supported = (window_desc->platform == VK_PLATFORM_ANDROID_KHR &&
                  !window_desc->pPlatformHandle &&
                  static_cast<ANativeWindow*>(window_desc->pPlatformWindow)
                          ->common.magic == ANDROID_NATIVE_WINDOW_MAGIC);

    return VK_SUCCESS;
}

VkResult GetSurfacePropertiesKHR(VkDevice /*device*/,
                                 const VkSurfaceDescriptionKHR* surface_desc,
                                 VkSurfacePropertiesKHR* properties) {
    const VkSurfaceDescriptionWindowKHR* window_desc =
        reinterpret_cast<const VkSurfaceDescriptionWindowKHR*>(surface_desc);
    ANativeWindow* window =
        static_cast<ANativeWindow*>(window_desc->pPlatformWindow);

    int err;

    // TODO(jessehall): Currently the window must be connected for several
    // queries -- including default dimensions -- to work, since Surface caches
    // the queried values at connect() and queueBuffer(), and query() returns
    // those cached values.
    //
    // The proposed refactoring to create a VkSurface object (bug 14596) will
    // give us a place to connect once per window. If that doesn't end up
    // happening, we'll probably need to maintain an internal list of windows
    // that have swapchains created for them, search that list here, and
    // only temporarily connect if the window doesn't have a swapchain.

    bool disconnect = true;
    err = native_window_api_connect(window, NATIVE_WINDOW_API_EGL);
    if (err == -EINVAL) {
        // This is returned if the window is already connected, among other
        // things. We'll just assume we're already connected and charge ahead.
        // See TODO above, this is not cool.
        ALOGW(
            "vkGetSurfacePropertiesKHR: native_window_api_connect returned "
            "-EINVAL, assuming already connected");
        err = 0;
        disconnect = false;
    } else if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    int width, height;
    err = window->query(window, NATIVE_WINDOW_DEFAULT_WIDTH, &width);
    if (err != 0) {
        ALOGE("NATIVE_WINDOW_DEFAULT_WIDTH query failed: %s (%d)",
              strerror(-err), err);
        if (disconnect)
            native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    err = window->query(window, NATIVE_WINDOW_DEFAULT_HEIGHT, &height);
    if (err != 0) {
        ALOGE("NATIVE_WINDOW_DEFAULT_WIDTH query failed: %s (%d)",
              strerror(-err), err);
        if (disconnect)
            native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (disconnect)
        native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);

    properties->currentExtent = VkExtent2D{width, height};

    // TODO(jessehall): Figure out what the min/max values should be.
    properties->minImageCount = 2;
    properties->maxImageCount = 3;

    // TODO(jessehall): Figure out what the max extent should be. Maximum
    // texture dimension maybe?
    properties->minImageExtent = VkExtent2D{1, 1};
    properties->maxImageExtent = VkExtent2D{4096, 4096};

    // TODO(jessehall): We can support all transforms, fix this once
    // implemented.
    properties->supportedTransforms = VK_SURFACE_TRANSFORM_NONE_BIT_KHR;

    // TODO(jessehall): Implement based on NATIVE_WINDOW_TRANSFORM_HINT.
    properties->currentTransform = VK_SURFACE_TRANSFORM_NONE_KHR;

    properties->maxImageArraySize = 1;

    // TODO(jessehall): I think these are right, but haven't thought hard about
    // it. Do we need to query the driver for support of any of these?
    // Currently not included:
    // - VK_IMAGE_USAGE_GENERAL: maybe? does this imply cpu mappable?
    // - VK_IMAGE_USAGE_DEPTH_STENCIL_BIT: definitely not
    // - VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT: definitely not
    properties->supportedUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT |
        VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    return VK_SUCCESS;
}

VkResult GetSurfaceFormatsKHR(VkDevice /*device*/,
                              const VkSurfaceDescriptionKHR* /*surface_desc*/,
                              uint32_t* count,
                              VkSurfaceFormatKHR* formats) {
    // TODO(jessehall): Fill out the set of supported formats. Open question
    // whether we should query the driver for support -- how does it know what
    // the consumer can support? Should we support formats that don't
    // correspond to gralloc formats?

    const VkSurfaceFormatKHR kFormats[] = {
        {VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_R8G8B8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR},
    };
    const uint32_t kNumFormats = sizeof(kFormats) / sizeof(kFormats[0]);

    VkResult result = VK_SUCCESS;
    if (formats) {
        if (*count < kNumFormats)
            result = VK_INCOMPLETE;
        std::copy(kFormats, kFormats + std::min(*count, kNumFormats), formats);
    }
    *count = kNumFormats;
    return result;
}

VkResult GetSurfacePresentModesKHR(
    VkDevice /*device*/,
    const VkSurfaceDescriptionKHR* /*surface_desc*/,
    uint32_t* count,
    VkPresentModeKHR* modes) {
    const VkPresentModeKHR kModes[] = {
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR,
    };
    const uint32_t kNumModes = sizeof(kModes) / sizeof(kModes[0]);

    VkResult result = VK_SUCCESS;
    if (modes) {
        if (*count < kNumModes)
            result = VK_INCOMPLETE;
        std::copy(kModes, kModes + std::min(*count, kNumModes), modes);
    }
    *count = kNumModes;
    return result;
}

VkResult CreateSwapchainKHR(VkDevice device,
                            const VkSwapchainCreateInfoKHR* create_info,
                            VkSwapchainKHR* swapchain_handle) {
    int err;
    VkResult result = VK_SUCCESS;

    ALOGV_IF(create_info->imageArraySize != 1,
             "Swapchain imageArraySize (%u) != 1 not supported",
             create_info->imageArraySize);

    ALOGE_IF(create_info->imageFormat != VK_FORMAT_R8G8B8A8_UNORM,
             "swapchain formats other than R8G8B8A8_UNORM not yet implemented");
    ALOGE_IF(create_info->imageColorSpace != VK_COLORSPACE_SRGB_NONLINEAR_KHR,
             "color spaces other than SRGB_NONLINEAR not yet implemented");
    ALOGE_IF(create_info->oldSwapchain,
             "swapchain re-creation not yet implemented");
    ALOGE_IF(create_info->preTransform != VK_SURFACE_TRANSFORM_NONE_KHR,
             "swapchain preTransform not yet implemented");
    ALOGE_IF(create_info->presentMode != VK_PRESENT_MODE_FIFO_KHR,
             "present modes other than FIFO are not yet implemented");

    // -- Configure the native window --
    // Failure paths from here on need to disconnect the window.

    const DeviceVtbl& driver_vtbl = GetDriverVtbl(device);

    std::shared_ptr<ANativeWindow> window = InitSharedPtr(
        device, static_cast<ANativeWindow*>(
                    reinterpret_cast<const VkSurfaceDescriptionWindowKHR*>(
                        create_info->pSurfaceDescription)->pPlatformWindow));

    // TODO(jessehall): Create and use NATIVE_WINDOW_API_VULKAN.
    err = native_window_api_connect(window.get(), NATIVE_WINDOW_API_EGL);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_api_connect() failed: %s (%d)", strerror(-err),
              err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    err = native_window_set_buffers_dimensions(window.get(),
                                               create_info->imageExtent.width,
                                               create_info->imageExtent.height);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_buffers_dimensions(%d,%d) failed: %s (%d)",
              create_info->imageExtent.width, create_info->imageExtent.height,
              strerror(-err), err);
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    err = native_window_set_scaling_mode(
        window.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_scaling_mode(SCALE_TO_WINDOW) failed: %s (%d)",
              strerror(-err), err);
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t min_undequeued_buffers;
    err = window->query(window.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                        reinterpret_cast<int*>(&min_undequeued_buffers));
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("window->query failed: %s (%d)", strerror(-err), err);
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    uint32_t num_images =
        (create_info->minImageCount - 1) + min_undequeued_buffers;
    err = native_window_set_buffer_count(window.get(), num_images);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err),
              err);
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    int gralloc_usage = 0;
    // TODO(jessehall): Remove conditional once all drivers have been updated
    if (driver_vtbl.GetSwapchainGrallocUsageANDROID) {
        result = driver_vtbl.GetSwapchainGrallocUsageANDROID(
            device, create_info->imageFormat, create_info->imageUsageFlags,
            &gralloc_usage);
        if (result != VK_SUCCESS) {
            ALOGE("vkGetSwapchainGrallocUsageANDROID failed: %d", result);
            native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    } else {
        gralloc_usage = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
    }
    err = native_window_set_usage(window.get(), gralloc_usage);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_usage failed: %s (%d)", strerror(-err), err);
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // -- Allocate our Swapchain object --
    // After this point, we must deallocate the swapchain on error.

    void* mem = AllocDeviceMem(device, sizeof(Swapchain), alignof(Swapchain),
                               VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
    if (!mem) {
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    Swapchain* swapchain = new (mem) Swapchain(window, num_images);

    // -- Dequeue all buffers and create a VkImage for each --
    // Any failures during or after this must cancel the dequeued buffers.

    VkNativeBufferANDROID image_native_buffer = {
// TODO(jessehall): Figure out how to make extension headers not horrible.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
        .sType = VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID,
#pragma clang diagnostic pop
        .pNext = nullptr,
    };
    VkImageCreateInfo image_create = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &image_native_buffer,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,  // TODO(jessehall)
        .extent = {0, 0, 1},
        .mipLevels = 1,
        .arraySize = 1,
        .samples = 1,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = create_info->imageUsageFlags,
        .flags = 0,
        .sharingMode = create_info->sharingMode,
        .queueFamilyCount = create_info->queueFamilyCount,
        .pQueueFamilyIndices = create_info->pQueueFamilyIndices,
    };

    for (uint32_t i = 0; i < num_images; i++) {
        Swapchain::Image& img = swapchain->images[i];

        ANativeWindowBuffer* buffer;
        err = window->dequeueBuffer(window.get(), &buffer, &img.dequeue_fence);
        if (err != 0) {
            // TODO(jessehall): Improve error reporting. Can we enumerate
            // possible errors and translate them to valid Vulkan result codes?
            ALOGE("dequeueBuffer[%u] failed: %s (%d)", i, strerror(-err), err);
            result = VK_ERROR_INITIALIZATION_FAILED;
            break;
        }
        img.buffer = InitSharedPtr(device, buffer);
        img.dequeued = true;

        image_create.extent =
            VkExtent3D{img.buffer->width, img.buffer->height, 1};
        image_native_buffer.handle = img.buffer->handle;
        image_native_buffer.stride = img.buffer->stride;
        image_native_buffer.format = img.buffer->format;
        image_native_buffer.usage = img.buffer->usage;

        result = driver_vtbl.CreateImage(device, &image_create, &img.image);
        if (result != VK_SUCCESS) {
            ALOGD("vkCreateImage w/ native buffer failed: %u", result);
            break;
        }
    }

    // -- Cancel all buffers, returning them to the queue --
    // If an error occurred before, also destroy the VkImage and release the
    // buffer reference. Otherwise, we retain a strong reference to the buffer.
    //
    // TODO(jessehall): The error path here is the same as DestroySwapchain,
    // but not the non-error path. Should refactor/unify.
    for (uint32_t i = 0; i < num_images; i++) {
        Swapchain::Image& img = swapchain->images[i];
        if (img.dequeued) {
            window->cancelBuffer(window.get(), img.buffer.get(),
                                 img.dequeue_fence);
            img.dequeue_fence = -1;
            img.dequeued = false;
        }
        if (result != VK_SUCCESS) {
            if (img.image)
                driver_vtbl.DestroyImage(device, img.image);
        }
    }

    if (result != VK_SUCCESS) {
        native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
        swapchain->~Swapchain();
        FreeDeviceMem(device, swapchain);
        return result;
    }

    *swapchain_handle = HandleFromSwapchain(swapchain);
    return VK_SUCCESS;
}

VkResult DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain_handle) {
    const DeviceVtbl& driver_vtbl = GetDriverVtbl(device);
    Swapchain* swapchain = SwapchainFromHandle(swapchain_handle);
    const std::shared_ptr<ANativeWindow>& window = swapchain->window;

    for (uint32_t i = 0; i < swapchain->num_images; i++) {
        Swapchain::Image& img = swapchain->images[i];
        if (img.dequeued) {
            window->cancelBuffer(window.get(), img.buffer.get(),
                                 img.dequeue_fence);
            img.dequeue_fence = -1;
            img.dequeued = false;
        }
        if (img.image) {
            driver_vtbl.DestroyImage(device, img.image);
        }
    }

    native_window_api_disconnect(window.get(), NATIVE_WINDOW_API_EGL);
    swapchain->~Swapchain();
    FreeDeviceMem(device, swapchain);

    return VK_SUCCESS;
}

VkResult GetSwapchainImagesKHR(VkDevice,
                               VkSwapchainKHR swapchain_handle,
                               uint32_t* count,
                               VkImage* images) {
    Swapchain& swapchain = *SwapchainFromHandle(swapchain_handle);
    VkResult result = VK_SUCCESS;
    if (images) {
        uint32_t n = swapchain.num_images;
        if (*count < swapchain.num_images) {
            n = *count;
            result = VK_INCOMPLETE;
        }
        for (uint32_t i = 0; i < n; i++)
            images[i] = swapchain.images[i].image;
    }
    *count = swapchain.num_images;
    return result;
}

VkResult AcquireNextImageKHR(VkDevice device,
                             VkSwapchainKHR swapchain_handle,
                             uint64_t timeout,
                             VkSemaphore semaphore,
                             uint32_t* image_index) {
    Swapchain& swapchain = *SwapchainFromHandle(swapchain_handle);
    VkResult result;
    int err;

    ALOGW_IF(
        timeout != UINT64_MAX,
        "vkAcquireNextImageKHR: non-infinite timeouts not yet implemented");

    ANativeWindowBuffer* buffer;
    int fence;
    err = swapchain.window->dequeueBuffer(swapchain.window.get(), &buffer,
                                          &fence);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("dequeueBuffer failed: %s (%d)", strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t idx;
    for (idx = 0; idx < swapchain.num_images; idx++) {
        if (swapchain.images[idx].buffer.get() == buffer) {
            swapchain.images[idx].dequeued = true;
            swapchain.images[idx].dequeue_fence = fence;
            break;
        }
    }
    if (idx == swapchain.num_images) {
        ALOGE("dequeueBuffer returned unrecognized buffer");
        swapchain.window->cancelBuffer(swapchain.window.get(), buffer, fence);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
        return VK_ERROR_OUT_OF_DATE_KHR;
#pragma clang diagnostic pop
    }

    int fence_clone = -1;
    if (fence != -1) {
        fence_clone = dup(fence);
        if (fence_clone == -1) {
            ALOGE("dup(fence) failed, stalling until signalled: %s (%d)",
                  strerror(errno), errno);
            sync_wait(fence, -1 /* forever */);
        }
    }

    const DeviceVtbl& driver_vtbl = GetDriverVtbl(device);
    if (driver_vtbl.AcquireImageANDROID) {
        result = driver_vtbl.AcquireImageANDROID(
            device, swapchain.images[idx].image, fence_clone, semaphore);
    } else {
        ALOG_ASSERT(driver_vtbl.ImportNativeFenceANDROID,
                    "Have neither vkAcquireImageANDROID nor "
                    "vkImportNativeFenceANDROID");
        result = driver_vtbl.ImportNativeFenceANDROID(device, semaphore,
                                                      fence_clone);
    }
    if (result != VK_SUCCESS) {
        // NOTE: we're relying on AcquireImageANDROID to close fence_clone,
        // even if the call fails. We could close it ourselves on failure, but
        // that would create a race condition if the driver closes it on a
        // failure path: some other thread might create an fd with the same
        // number between the time the driver closes it and the time we close
        // it. We must assume one of: the driver *always* closes it even on
        // failure, or *never* closes it on failure.
        swapchain.window->cancelBuffer(swapchain.window.get(), buffer, fence);
        swapchain.images[idx].dequeued = false;
        swapchain.images[idx].dequeue_fence = -1;
        return result;
    }

    *image_index = idx;
    return VK_SUCCESS;
}

VkResult QueuePresentKHR(VkQueue queue, VkPresentInfoKHR* present_info) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
    ALOGV_IF(present_info->sType != VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
             "vkQueuePresentKHR: invalid VkPresentInfoKHR structure type %d",
             present_info->sType);
#pragma clang diagnostic pop
    ALOGV_IF(present_info->pNext, "VkPresentInfo::pNext != NULL");

    const DeviceVtbl& driver_vtbl = GetDriverVtbl(queue);
    VkResult final_result = VK_SUCCESS;
    for (uint32_t sc = 0; sc < present_info->swapchainCount; sc++) {
        Swapchain& swapchain =
            *SwapchainFromHandle(present_info->swapchains[sc]);
        uint32_t image_idx = present_info->imageIndices[sc];
        Swapchain::Image& img = swapchain.images[image_idx];
        VkResult result;
        int err;

        int fence = -1;
        if (driver_vtbl.QueueSignalReleaseImageANDROID) {
            result = driver_vtbl.QueueSignalReleaseImageANDROID(
                queue, img.image, &fence);
        } else {
            ALOG_ASSERT(driver_vtbl.QueueSignalNativeFenceANDROID,
                        "Have neither vkQueueSignalReleaseImageANDROID nor "
                        "vkQueueSignalNativeFenceANDROID");
            result = driver_vtbl.QueueSignalNativeFenceANDROID(queue, &fence);
        }
        if (result != VK_SUCCESS) {
            ALOGE("QueueSignalReleaseImageANDROID failed: %d", result);
            if (final_result == VK_SUCCESS)
                final_result = result;
            // TODO(jessehall): What happens to the buffer here? Does the app
            // still own it or not, i.e. should we cancel the buffer? Hard to
            // do correctly without synchronizing, though I guess we could wait
            // for the queue to idle.
            continue;
        }

        err = swapchain.window->queueBuffer(swapchain.window.get(),
                                            img.buffer.get(), fence);
        if (err != 0) {
            // TODO(jessehall): What now? We should probably cancel the buffer,
            // I guess?
            ALOGE("queueBuffer failed: %s (%d)", strerror(-err), err);
            if (final_result == VK_SUCCESS)
                final_result = VK_ERROR_INITIALIZATION_FAILED;
            continue;
        }

        if (img.dequeue_fence != -1) {
            close(img.dequeue_fence);
            img.dequeue_fence = -1;
        }
        img.dequeued = false;
    }

    return final_result;
}

}  // namespace vulkan
