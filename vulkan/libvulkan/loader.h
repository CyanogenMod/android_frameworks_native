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
#include "dispatch_gen.h"
#include "debug_report.h"

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

inline const InstanceDispatchTable& GetDispatchTable(VkInstance instance) {
    return **reinterpret_cast<InstanceDispatchTable**>(instance);
}

inline const InstanceDispatchTable& GetDispatchTable(
    VkPhysicalDevice physical_device) {
    return **reinterpret_cast<InstanceDispatchTable**>(physical_device);
}

inline const DeviceDispatchTable& GetDispatchTable(VkDevice device) {
    return **reinterpret_cast<DeviceDispatchTable**>(device);
}

inline const DeviceDispatchTable& GetDispatchTable(VkQueue queue) {
    return **reinterpret_cast<DeviceDispatchTable**>(queue);
}

inline const DeviceDispatchTable& GetDispatchTable(
    VkCommandBuffer command_buffer) {
    return **reinterpret_cast<DeviceDispatchTable**>(command_buffer);
}

// -----------------------------------------------------------------------------
// dispatch_gen.cpp

PFN_vkVoidFunction GetLoaderExportProcAddr(const char* name);
PFN_vkVoidFunction GetLoaderGlobalProcAddr(const char* name);
PFN_vkVoidFunction GetLoaderTopProcAddr(const char* name);
PFN_vkVoidFunction GetLoaderBottomProcAddr(const char* name);
PFN_vkVoidFunction GetDispatchProcAddr(const InstanceDispatchTable& dispatch,
                                       const char* name);
PFN_vkVoidFunction GetDispatchProcAddr(const DeviceDispatchTable& dispatch,
                                       const char* name);
bool LoadInstanceDispatchTable(VkInstance instance,
                               PFN_vkGetInstanceProcAddr get_proc_addr,
                               InstanceDispatchTable& dispatch);
bool LoadDeviceDispatchTable(VkDevice device,
                             PFN_vkGetDeviceProcAddr get_proc_addr,
                             DeviceDispatchTable& dispatch);
bool LoadDriverDispatchTable(VkInstance instance,
                             PFN_vkGetInstanceProcAddr get_proc_addr,
                             const InstanceExtensionSet& extensions,
                             DriverDispatchTable& dispatch);

// -----------------------------------------------------------------------------
// loader.cpp

// clang-format off
VKAPI_ATTR VkResult EnumerateInstanceExtensionProperties_Top(const char* layer_name, uint32_t* count, VkExtensionProperties* properties);
VKAPI_ATTR VkResult EnumerateInstanceLayerProperties_Top(uint32_t* count, VkLayerProperties* properties);
VKAPI_ATTR VkResult CreateInstance_Top(const VkInstanceCreateInfo* create_info, const VkAllocationCallbacks* allocator, VkInstance* instance_out);
VKAPI_ATTR PFN_vkVoidFunction GetInstanceProcAddr_Top(VkInstance instance, const char* name);
VKAPI_ATTR void DestroyInstance_Top(VkInstance instance, const VkAllocationCallbacks* allocator);
VKAPI_ATTR PFN_vkVoidFunction GetDeviceProcAddr_Top(VkDevice drv_device, const char* name);
VKAPI_ATTR void GetDeviceQueue_Top(VkDevice drv_device, uint32_t family, uint32_t index, VkQueue* out_queue);
VKAPI_ATTR VkResult AllocateCommandBuffers_Top(VkDevice device, const VkCommandBufferAllocateInfo* alloc_info, VkCommandBuffer* cmdbufs);
VKAPI_ATTR VkResult EnumerateDeviceLayerProperties_Top(VkPhysicalDevice pdev, uint32_t* properties_count, VkLayerProperties* properties);
VKAPI_ATTR VkResult CreateDevice_Top(VkPhysicalDevice pdev, const VkDeviceCreateInfo* create_info, const VkAllocationCallbacks* allocator, VkDevice* device_out);
VKAPI_ATTR void DestroyDevice_Top(VkDevice drv_device, const VkAllocationCallbacks* allocator);

VKAPI_ATTR VkResult CreateInstance_Bottom(const VkInstanceCreateInfo* create_info, const VkAllocationCallbacks* allocator, VkInstance* vkinstance);
VKAPI_ATTR PFN_vkVoidFunction GetInstanceProcAddr_Bottom(VkInstance, const char* name);
VKAPI_ATTR VkResult EnumeratePhysicalDevices_Bottom(VkInstance vkinstance, uint32_t* pdev_count, VkPhysicalDevice* pdevs);
VKAPI_ATTR void GetPhysicalDeviceProperties_Bottom(VkPhysicalDevice pdev, VkPhysicalDeviceProperties* properties);
VKAPI_ATTR void GetPhysicalDeviceFeatures_Bottom(VkPhysicalDevice pdev, VkPhysicalDeviceFeatures* features);
VKAPI_ATTR void GetPhysicalDeviceMemoryProperties_Bottom(VkPhysicalDevice pdev, VkPhysicalDeviceMemoryProperties* properties);
VKAPI_ATTR void GetPhysicalDeviceQueueFamilyProperties_Bottom(VkPhysicalDevice pdev, uint32_t* properties_count, VkQueueFamilyProperties* properties);
VKAPI_ATTR void GetPhysicalDeviceFormatProperties_Bottom(VkPhysicalDevice pdev, VkFormat format, VkFormatProperties* properties);
VKAPI_ATTR VkResult GetPhysicalDeviceImageFormatProperties_Bottom(VkPhysicalDevice pdev, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties);
VKAPI_ATTR void GetPhysicalDeviceSparseImageFormatProperties_Bottom(VkPhysicalDevice pdev, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* properties_count, VkSparseImageFormatProperties* properties);
VKAPI_ATTR VkResult EnumerateDeviceExtensionProperties_Bottom(VkPhysicalDevice pdev, const char* layer_name, uint32_t* properties_count, VkExtensionProperties* properties);
VKAPI_ATTR VkResult EnumerateDeviceLayerProperties_Bottom(VkPhysicalDevice pdev, uint32_t* properties_count, VkLayerProperties* properties);
VKAPI_ATTR VkResult CreateDevice_Bottom(VkPhysicalDevice pdev, const VkDeviceCreateInfo* create_info, const VkAllocationCallbacks* allocator, VkDevice* device_out);
VKAPI_ATTR void DestroyInstance_Bottom(VkInstance vkinstance, const VkAllocationCallbacks* allocator);
VKAPI_ATTR PFN_vkVoidFunction GetDeviceProcAddr_Bottom(VkDevice vkdevice, const char* name);
// clang-format on

const VkAllocationCallbacks* GetAllocator(VkInstance instance);
const VkAllocationCallbacks* GetAllocator(VkDevice device);
VkInstance GetDriverInstance(VkInstance instance);
const DriverDispatchTable& GetDriverDispatch(VkInstance instance);
const DriverDispatchTable& GetDriverDispatch(VkDevice device);
const DriverDispatchTable& GetDriverDispatch(VkQueue queue);
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

    const char* GetName();
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
