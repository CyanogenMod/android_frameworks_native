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

#include "loader.h"
#define LOG_NDEBUG 0
#include <log/log.h>

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
    if (surface_desc->sType != VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_KHR)
        return VK_ERROR_INVALID_VALUE;
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
VkResult GetSurfacePropertiesKHR(VkDevice device,
                                 const VkSurfaceDescriptionKHR* surface_desc,
                                 VkSurfacePropertiesKHR* properties) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetSurfaceFormatsKHR(VkDevice device,
                              const VkSurfaceDescriptionKHR* surface_desc,
                              uint32_t* count,
                              VkSurfaceFormatKHR* formats) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetSurfacePresentModesKHR(VkDevice device,
                                   const VkSurfaceDescriptionKHR* surface_desc,
                                   uint32_t* count,
                                   VkPresentModeKHR* modes) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateSwapchainKHR(VkDevice device,
                            const VkSwapchainCreateInfoKHR* create_info,
                            VkSwapchainKHR* swapchain) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetSwapchainImagesKHR(VkDevice device,
                               VkSwapchainKHR swapchain,
                               uint32_t* count,
                               VkImage* image) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult AcquireNextImageKHR(VkDevice device,
                             VkSwapchainKHR swapchain,
                             uint64_t timeout,
                             VkSemaphore semaphore,
                             uint32_t* image_index) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueuePresentKHR(VkQueue queue, VkPresentInfoKHR* present_info) {
    ALOGV("TODO: %s", __FUNCTION__);
    return VK_SUCCESS;
}
#pragma clang diagnostic pop

}  // namespace vulkan
