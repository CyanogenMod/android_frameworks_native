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

#ifndef LIBVULKAN_LOADER_H
#define LIBVULKAN_LOADER_H 1

#include <bitset>
#include <vulkan/vulkan.h>
#include "debug_report.h"
#include "driver.h"

struct hwvulkan_device_t;

namespace vulkan {

enum InstanceExtension {
    kKHR_surface,
    kKHR_android_surface,
    kEXT_debug_report,
    kInstanceExtensionCount
};
typedef std::bitset<kInstanceExtensionCount> InstanceExtensionSet;

enum DeviceExtension {
    kKHR_swapchain,
    kANDROID_native_buffer,
    kDeviceExtensionCount
};
typedef std::bitset<kDeviceExtensionCount> DeviceExtensionSet;

// -----------------------------------------------------------------------------
// loader.cpp

bool InitLoader(hwvulkan_device_t* dev);

// clang-format off
VKAPI_ATTR VkResult CreateInstance_Bottom(const VkInstanceCreateInfo* create_info, const VkAllocationCallbacks* allocator, VkInstance* vkinstance);
VKAPI_ATTR VkResult EnumeratePhysicalDevices_Bottom(VkInstance vkinstance, uint32_t* pdev_count, VkPhysicalDevice* pdevs);
VKAPI_ATTR VkResult EnumerateDeviceExtensionProperties_Bottom(VkPhysicalDevice pdev, const char* layer_name, uint32_t* properties_count, VkExtensionProperties* properties);
VKAPI_ATTR void DestroyInstance_Bottom(VkInstance vkinstance, const VkAllocationCallbacks* allocator);
// clang-format on

const VkAllocationCallbacks* GetAllocator(VkInstance instance);
const VkAllocationCallbacks* GetAllocator(VkDevice device);
VkInstance GetDriverInstance(VkInstance instance);
const driver::InstanceDriverTable& GetDriverDispatch(VkInstance instance);
const driver::DeviceDriverTable& GetDriverDispatch(VkDevice device);
const driver::DeviceDriverTable& GetDriverDispatch(VkQueue queue);
DebugReportCallbackList& GetDebugReportCallbacks(VkInstance instance);

// -----------------------------------------------------------------------------
// swapchain.cpp

// clang-format off
VKAPI_ATTR VkResult CreateAndroidSurfaceKHR_Bottom(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
VKAPI_ATTR void DestroySurfaceKHR_Bottom(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult GetPhysicalDeviceSurfaceSupportKHR_Bottom(VkPhysicalDevice pdev, uint32_t queue_family, VkSurfaceKHR surface, VkBool32* pSupported);
VKAPI_ATTR VkResult GetPhysicalDeviceSurfaceCapabilitiesKHR_Bottom(VkPhysicalDevice pdev, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* capabilities);
VKAPI_ATTR VkResult GetPhysicalDeviceSurfaceFormatsKHR_Bottom(VkPhysicalDevice pdev, VkSurfaceKHR surface, uint32_t* count, VkSurfaceFormatKHR* formats);
VKAPI_ATTR VkResult GetPhysicalDeviceSurfacePresentModesKHR_Bottom(VkPhysicalDevice pdev, VkSurfaceKHR surface, uint32_t* count, VkPresentModeKHR* modes);
VKAPI_ATTR VkResult CreateSwapchainKHR_Bottom(VkDevice device, const VkSwapchainCreateInfoKHR* create_info, const VkAllocationCallbacks* allocator, VkSwapchainKHR* swapchain_handle);
VKAPI_ATTR void DestroySwapchainKHR_Bottom(VkDevice device, VkSwapchainKHR swapchain_handle, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult GetSwapchainImagesKHR_Bottom(VkDevice device, VkSwapchainKHR swapchain_handle, uint32_t* count, VkImage* images);
VKAPI_ATTR VkResult AcquireNextImageKHR_Bottom(VkDevice device, VkSwapchainKHR swapchain_handle, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* image_index);
VKAPI_ATTR VkResult QueuePresentKHR_Bottom(VkQueue queue, const VkPresentInfoKHR* present_info);
// clang-format on

// -----------------------------------------------------------------------------
// layers_extensions.cpp

struct Layer;
class LayerRef {
   public:
    LayerRef(Layer* layer);
    LayerRef(LayerRef&& other);
    ~LayerRef();
    LayerRef(const LayerRef&) = delete;
    LayerRef& operator=(const LayerRef&) = delete;

    const char* GetName() const;
    uint32_t GetSpecVersion();

    // provides bool-like behavior
    operator const Layer*() const { return layer_; }

    PFN_vkGetInstanceProcAddr GetGetInstanceProcAddr() const;
    PFN_vkGetDeviceProcAddr GetGetDeviceProcAddr() const;

    bool SupportsExtension(const char* name) const;

   private:
    Layer* layer_;
};

void DiscoverLayers();
uint32_t EnumerateInstanceLayers(uint32_t count, VkLayerProperties* properties);
uint32_t EnumerateDeviceLayers(uint32_t count, VkLayerProperties* properties);
void GetInstanceLayerExtensions(const char* name,
                                const VkExtensionProperties** properties,
                                uint32_t* count);
void GetDeviceLayerExtensions(const char* name,
                              const VkExtensionProperties** properties,
                              uint32_t* count);
LayerRef GetInstanceLayerRef(const char* name);
LayerRef GetDeviceLayerRef(const char* name);

InstanceExtension InstanceExtensionFromName(const char* name);
DeviceExtension DeviceExtensionFromName(const char* name);

}  // namespace vulkan

#endif  // LIBVULKAN_LOADER_H
