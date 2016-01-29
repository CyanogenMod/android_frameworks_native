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
// allocation of the shared_ptr<> control structure to VkAllocationCallbacks.
// The
// platform holds a reference to the ANativeWindow using its embedded reference
// count, and the ANativeWindow implementation holds references to the
// ANativeWindowBuffers using their embedded reference counts, so the
// shared_ptr *must* cooperate with these and hold at least one reference to
// the object using the embedded reference count.

template <typename T>
struct NativeBaseDeleter {
    void operator()(T* obj) { obj->common.decRef(&obj->common); }
};

template <typename Host>
struct AllocScope {};

template <>
struct AllocScope<VkInstance> {
    static const VkSystemAllocationScope kScope =
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE;
};

template <>
struct AllocScope<VkDevice> {
    static const VkSystemAllocationScope kScope =
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE;
};

template <typename T>
class VulkanAllocator {
   public:
    typedef T value_type;

    VulkanAllocator(const VkAllocationCallbacks& allocator,
                    VkSystemAllocationScope scope)
        : allocator_(allocator), scope_(scope) {}

    template <typename U>
    explicit VulkanAllocator(const VulkanAllocator<U>& other)
        : allocator_(other.allocator_), scope_(other.scope_) {}

    T* allocate(size_t n) const {
        T* p = static_cast<T*>(allocator_.pfnAllocation(
            allocator_.pUserData, n * sizeof(T), alignof(T), scope_));
        if (!p)
            throw std::bad_alloc();
        return p;
    }
    void deallocate(T* p, size_t) const noexcept {
        return allocator_.pfnFree(allocator_.pUserData, p);
    }

   private:
    template <typename U>
    friend class VulkanAllocator;
    const VkAllocationCallbacks& allocator_;
    const VkSystemAllocationScope scope_;
};

template <typename T, typename Host>
std::shared_ptr<T> InitSharedPtr(Host host, T* obj) {
    try {
        obj->common.incRef(&obj->common);
        return std::shared_ptr<T>(
            obj, NativeBaseDeleter<T>(),
            VulkanAllocator<T>(*GetAllocator(host), AllocScope<Host>::kScope));
    } catch (std::bad_alloc&) {
        obj->common.decRef(&obj->common);
        return nullptr;
    }
}

// ----------------------------------------------------------------------------

struct Surface {
    std::shared_ptr<ANativeWindow> window;
};

VkSurfaceKHR HandleFromSurface(Surface* surface) {
    return VkSurfaceKHR(reinterpret_cast<uint64_t>(surface));
}

Surface* SurfaceFromHandle(VkSurfaceKHR handle) {
    return reinterpret_cast<Surface*>(handle);
}

struct Swapchain {
    Swapchain(Surface& surface_, uint32_t num_images_)
        : surface(surface_), num_images(num_images_) {}

    Surface& surface;
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
    return reinterpret_cast<Swapchain*>(handle);
}

}  // anonymous namespace

namespace vulkan {

VKAPI_ATTR
VkResult CreateAndroidSurfaceKHR_Bottom(
    VkInstance instance,
    const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* allocator,
    VkSurfaceKHR* out_surface) {
    if (!allocator)
        allocator = GetAllocator(instance);
    void* mem = allocator->pfnAllocation(allocator->pUserData, sizeof(Surface),
                                         alignof(Surface),
                                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Surface* surface = new (mem) Surface;

    surface->window = InitSharedPtr(instance, pCreateInfo->window);
    if (!surface->window) {
        ALOGE("surface creation failed: out of memory");
        surface->~Surface();
        allocator->pfnFree(allocator->pUserData, surface);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // TODO(jessehall): Create and use NATIVE_WINDOW_API_VULKAN.
    int err =
        native_window_api_connect(surface->window.get(), NATIVE_WINDOW_API_EGL);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_api_connect() failed: %s (%d)", strerror(-err),
              err);
        surface->~Surface();
        allocator->pfnFree(allocator->pUserData, surface);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *out_surface = HandleFromSurface(surface);
    return VK_SUCCESS;
}

VKAPI_ATTR
void DestroySurfaceKHR_Bottom(VkInstance instance,
                              VkSurfaceKHR surface_handle,
                              const VkAllocationCallbacks* allocator) {
    Surface* surface = SurfaceFromHandle(surface_handle);
    if (!surface)
        return;
    native_window_api_disconnect(surface->window.get(), NATIVE_WINDOW_API_EGL);
    surface->~Surface();
    if (!allocator)
        allocator = GetAllocator(instance);
    allocator->pfnFree(allocator->pUserData, surface);
}

VKAPI_ATTR
VkResult GetPhysicalDeviceSurfaceSupportKHR_Bottom(VkPhysicalDevice /*pdev*/,
                                                   uint32_t /*queue_family*/,
                                                   VkSurfaceKHR /*surface*/,
                                                   VkBool32* supported) {
    *supported = VK_TRUE;
    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult GetPhysicalDeviceSurfaceCapabilitiesKHR_Bottom(
    VkPhysicalDevice /*pdev*/,
    VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* capabilities) {
    int err;
    ANativeWindow* window = SurfaceFromHandle(surface)->window.get();

    int width, height;
    err = window->query(window, NATIVE_WINDOW_DEFAULT_WIDTH, &width);
    if (err != 0) {
        ALOGE("NATIVE_WINDOW_DEFAULT_WIDTH query failed: %s (%d)",
              strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    err = window->query(window, NATIVE_WINDOW_DEFAULT_HEIGHT, &height);
    if (err != 0) {
        ALOGE("NATIVE_WINDOW_DEFAULT_WIDTH query failed: %s (%d)",
              strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    capabilities->currentExtent =
        VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    // TODO(jessehall): Figure out what the min/max values should be.
    capabilities->minImageCount = 2;
    capabilities->maxImageCount = 3;

    // TODO(jessehall): Figure out what the max extent should be. Maximum
    // texture dimension maybe?
    capabilities->minImageExtent = VkExtent2D{1, 1};
    capabilities->maxImageExtent = VkExtent2D{4096, 4096};

    // TODO(jessehall): We can support all transforms, fix this once
    // implemented.
    capabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    // TODO(jessehall): Implement based on NATIVE_WINDOW_TRANSFORM_HINT.
    capabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    capabilities->maxImageArrayLayers = 1;

    // TODO(jessehall): I think these are right, but haven't thought hard about
    // it. Do we need to query the driver for support of any of these?
    // Currently not included:
    // - VK_IMAGE_USAGE_GENERAL: maybe? does this imply cpu mappable?
    // - VK_IMAGE_USAGE_DEPTH_STENCIL_BIT: definitely not
    // - VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT: definitely not
    capabilities->supportedUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult GetPhysicalDeviceSurfaceFormatsKHR_Bottom(
    VkPhysicalDevice /*pdev*/,
    VkSurfaceKHR /*surface*/,
    uint32_t* count,
    VkSurfaceFormatKHR* formats) {
    // TODO(jessehall): Fill out the set of supported formats. Longer term, add
    // a new gralloc method to query whether a (format, usage) pair is
    // supported, and check that for each gralloc format that corresponds to a
    // Vulkan format. Shorter term, just add a few more formats to the ones
    // hardcoded below.

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

VKAPI_ATTR
VkResult GetPhysicalDeviceSurfacePresentModesKHR_Bottom(
    VkPhysicalDevice /*pdev*/,
    VkSurfaceKHR /*surface*/,
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

VKAPI_ATTR
VkResult CreateSwapchainKHR_Bottom(VkDevice device,
                                   const VkSwapchainCreateInfoKHR* create_info,
                                   const VkAllocationCallbacks* allocator,
                                   VkSwapchainKHR* swapchain_handle) {
    int err;
    VkResult result = VK_SUCCESS;

    if (!allocator)
        allocator = GetAllocator(device);

    ALOGV_IF(create_info->imageArrayLayers != 1,
             "Swapchain imageArrayLayers (%u) != 1 not supported",
             create_info->imageArrayLayers);

    ALOGE_IF(create_info->imageFormat != VK_FORMAT_R8G8B8A8_UNORM,
             "swapchain formats other than R8G8B8A8_UNORM not yet implemented");
    ALOGE_IF(create_info->imageColorSpace != VK_COLORSPACE_SRGB_NONLINEAR_KHR,
             "color spaces other than SRGB_NONLINEAR not yet implemented");
    ALOGE_IF(create_info->oldSwapchain,
             "swapchain re-creation not yet implemented");
    ALOGE_IF(create_info->preTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
             "swapchain preTransform not yet implemented");
    ALOGE_IF(create_info->presentMode != VK_PRESENT_MODE_FIFO_KHR,
             "present modes other than FIFO are not yet implemented");

    // -- Configure the native window --

    Surface& surface = *SurfaceFromHandle(create_info->surface);
    const DriverDispatchTable& dispatch = GetDriverDispatch(device);

    err = native_window_set_buffers_dimensions(
        surface.window.get(), static_cast<int>(create_info->imageExtent.width),
        static_cast<int>(create_info->imageExtent.height));
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_buffers_dimensions(%d,%d) failed: %s (%d)",
              create_info->imageExtent.width, create_info->imageExtent.height,
              strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    err = native_window_set_scaling_mode(
        surface.window.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_scaling_mode(SCALE_TO_WINDOW) failed: %s (%d)",
              strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t min_undequeued_buffers;
    err = surface.window->query(
        surface.window.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
        reinterpret_cast<int*>(&min_undequeued_buffers));
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("window->query failed: %s (%d)", strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    uint32_t num_images =
        (create_info->minImageCount - 1) + min_undequeued_buffers;
    err = native_window_set_buffer_count(surface.window.get(), num_images);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err),
              err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    int gralloc_usage = 0;
    // TODO(jessehall): Remove conditional once all drivers have been updated
    if (dispatch.GetSwapchainGrallocUsageANDROID) {
        result = dispatch.GetSwapchainGrallocUsageANDROID(
            device, create_info->imageFormat, create_info->imageUsage,
            &gralloc_usage);
        if (result != VK_SUCCESS) {
            ALOGE("vkGetSwapchainGrallocUsageANDROID failed: %d", result);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    } else {
        gralloc_usage = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
    }
    err = native_window_set_usage(surface.window.get(), gralloc_usage);
    if (err != 0) {
        // TODO(jessehall): Improve error reporting. Can we enumerate possible
        // errors and translate them to valid Vulkan result codes?
        ALOGE("native_window_set_usage failed: %s (%d)", strerror(-err), err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // -- Allocate our Swapchain object --
    // After this point, we must deallocate the swapchain on error.

    void* mem = allocator->pfnAllocation(allocator->pUserData,
                                         sizeof(Swapchain), alignof(Swapchain),
                                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Swapchain* swapchain = new (mem) Swapchain(surface, num_images);

    // -- Dequeue all buffers and create a VkImage for each --
    // Any failures during or after this must cancel the dequeued buffers.

    VkNativeBufferANDROID image_native_buffer = {
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
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = create_info->imageUsage,
        .flags = 0,
        .sharingMode = create_info->imageSharingMode,
        .queueFamilyIndexCount = create_info->queueFamilyIndexCount,
        .pQueueFamilyIndices = create_info->pQueueFamilyIndices,
    };

    for (uint32_t i = 0; i < num_images; i++) {
        Swapchain::Image& img = swapchain->images[i];

        ANativeWindowBuffer* buffer;
        err = surface.window->dequeueBuffer(surface.window.get(), &buffer,
                                            &img.dequeue_fence);
        if (err != 0) {
            // TODO(jessehall): Improve error reporting. Can we enumerate
            // possible errors and translate them to valid Vulkan result codes?
            ALOGE("dequeueBuffer[%u] failed: %s (%d)", i, strerror(-err), err);
            result = VK_ERROR_INITIALIZATION_FAILED;
            break;
        }
        img.buffer = InitSharedPtr(device, buffer);
        if (!img.buffer) {
            ALOGE("swapchain creation failed: out of memory");
            surface.window->cancelBuffer(surface.window.get(), buffer,
                                         img.dequeue_fence);
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            break;
        }
        img.dequeued = true;

        image_create.extent =
            VkExtent3D{static_cast<uint32_t>(img.buffer->width),
                       static_cast<uint32_t>(img.buffer->height),
                       1};
        image_native_buffer.handle = img.buffer->handle;
        image_native_buffer.stride = img.buffer->stride;
        image_native_buffer.format = img.buffer->format;
        image_native_buffer.usage = img.buffer->usage;

        result =
            dispatch.CreateImage(device, &image_create, nullptr, &img.image);
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
            surface.window->cancelBuffer(surface.window.get(), img.buffer.get(),
                                         img.dequeue_fence);
            img.dequeue_fence = -1;
            img.dequeued = false;
        }
        if (result != VK_SUCCESS) {
            if (img.image)
                dispatch.DestroyImage(device, img.image, nullptr);
        }
    }

    if (result != VK_SUCCESS) {
        swapchain->~Swapchain();
        allocator->pfnFree(allocator->pUserData, swapchain);
        return result;
    }

    *swapchain_handle = HandleFromSwapchain(swapchain);
    return VK_SUCCESS;
}

VKAPI_ATTR
void DestroySwapchainKHR_Bottom(VkDevice device,
                                VkSwapchainKHR swapchain_handle,
                                const VkAllocationCallbacks* allocator) {
    const DriverDispatchTable& dispatch = GetDriverDispatch(device);
    Swapchain* swapchain = SwapchainFromHandle(swapchain_handle);
    const std::shared_ptr<ANativeWindow>& window = swapchain->surface.window;

    for (uint32_t i = 0; i < swapchain->num_images; i++) {
        Swapchain::Image& img = swapchain->images[i];
        if (img.dequeued) {
            window->cancelBuffer(window.get(), img.buffer.get(),
                                 img.dequeue_fence);
            img.dequeue_fence = -1;
            img.dequeued = false;
        }
        if (img.image) {
            dispatch.DestroyImage(device, img.image, nullptr);
        }
    }

    if (!allocator)
        allocator = GetAllocator(device);
    swapchain->~Swapchain();
    allocator->pfnFree(allocator->pUserData, swapchain);
}

VKAPI_ATTR
VkResult GetSwapchainImagesKHR_Bottom(VkDevice,
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

VKAPI_ATTR
VkResult AcquireNextImageKHR_Bottom(VkDevice device,
                                    VkSwapchainKHR swapchain_handle,
                                    uint64_t timeout,
                                    VkSemaphore semaphore,
                                    VkFence vk_fence,
                                    uint32_t* image_index) {
    Swapchain& swapchain = *SwapchainFromHandle(swapchain_handle);
    ANativeWindow* window = swapchain.surface.window.get();
    VkResult result;
    int err;

    ALOGW_IF(
        timeout != UINT64_MAX,
        "vkAcquireNextImageKHR: non-infinite timeouts not yet implemented");

    ANativeWindowBuffer* buffer;
    int fence_fd;
    err = window->dequeueBuffer(window, &buffer, &fence_fd);
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
            swapchain.images[idx].dequeue_fence = fence_fd;
            break;
        }
    }
    if (idx == swapchain.num_images) {
        ALOGE("dequeueBuffer returned unrecognized buffer");
        window->cancelBuffer(window, buffer, fence_fd);
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    int fence_clone = -1;
    if (fence_fd != -1) {
        fence_clone = dup(fence_fd);
        if (fence_clone == -1) {
            ALOGE("dup(fence) failed, stalling until signalled: %s (%d)",
                  strerror(errno), errno);
            sync_wait(fence_fd, -1 /* forever */);
        }
    }

    result = GetDriverDispatch(device).AcquireImageANDROID(
        device, swapchain.images[idx].image, fence_clone, semaphore, vk_fence);
    if (result != VK_SUCCESS) {
        // NOTE: we're relying on AcquireImageANDROID to close fence_clone,
        // even if the call fails. We could close it ourselves on failure, but
        // that would create a race condition if the driver closes it on a
        // failure path: some other thread might create an fd with the same
        // number between the time the driver closes it and the time we close
        // it. We must assume one of: the driver *always* closes it even on
        // failure, or *never* closes it on failure.
        window->cancelBuffer(window, buffer, fence_fd);
        swapchain.images[idx].dequeued = false;
        swapchain.images[idx].dequeue_fence = -1;
        return result;
    }

    *image_index = idx;
    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult QueuePresentKHR_Bottom(VkQueue queue,
                                const VkPresentInfoKHR* present_info) {
    ALOGV_IF(present_info->sType != VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
             "vkQueuePresentKHR: invalid VkPresentInfoKHR structure type %d",
             present_info->sType);
    ALOGV_IF(present_info->pNext, "VkPresentInfo::pNext != NULL");

    const DriverDispatchTable& dispatch = GetDriverDispatch(queue);
    VkResult final_result = VK_SUCCESS;
    for (uint32_t sc = 0; sc < present_info->swapchainCount; sc++) {
        Swapchain& swapchain =
            *SwapchainFromHandle(present_info->pSwapchains[sc]);
        ANativeWindow* window = swapchain.surface.window.get();
        uint32_t image_idx = present_info->pImageIndices[sc];
        Swapchain::Image& img = swapchain.images[image_idx];
        VkResult result;
        int err;

        int fence = -1;
        result = dispatch.QueueSignalReleaseImageANDROID(
            queue, present_info->waitSemaphoreCount,
            present_info->pWaitSemaphores, img.image, &fence);
        if (result != VK_SUCCESS) {
            ALOGE("QueueSignalReleaseImageANDROID failed: %d", result);
            if (present_info->pResults)
                present_info->pResults[sc] = result;
            if (final_result == VK_SUCCESS)
                final_result = result;
            // TODO(jessehall): What happens to the buffer here? Does the app
            // still own it or not, i.e. should we cancel the buffer? Hard to
            // do correctly without synchronizing, though I guess we could wait
            // for the queue to idle.
            continue;
        }

        err = window->queueBuffer(window, img.buffer.get(), fence);
        if (err != 0) {
            // TODO(jessehall): What now? We should probably cancel the buffer,
            // I guess?
            ALOGE("queueBuffer failed: %s (%d)", strerror(-err), err);
            if (present_info->pResults)
                present_info->pResults[sc] = result;
            if (final_result == VK_SUCCESS)
                final_result = VK_ERROR_INITIALIZATION_FAILED;
            continue;
        }

        if (img.dequeue_fence != -1) {
            close(img.dequeue_fence);
            img.dequeue_fence = -1;
        }
        img.dequeued = false;

        if (present_info->pResults)
            present_info->pResults[sc] = VK_SUCCESS;
    }

    return final_result;
}

}  // namespace vulkan
