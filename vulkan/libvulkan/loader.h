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

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_ext_khr_swapchain.h>
#include <vulkan/vk_ext_khr_device_swapchain.h>
#include <vulkan/vk_ext_android_native_buffer.h>

namespace vulkan {

// TODO(jessehall): The InstanceVtbl and DeviceVtbl both have a set of
// functions used in the app->layers/loader interface, and a different set of
// functions used only in the loader->driver interface. We should probably
// split them into two structures: one used for dispatch of application calls,
// and one to hold the driver entry points.

struct InstanceVtbl {
    // clang-format off
    VkInstance instance;

    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;

    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;

    // Layers and loader only, not implemented by drivers
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    // clang-format on
};

struct DeviceVtbl {
    void* device;

    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkAllocMemory AllocMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkResetFences ResetFences;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateEvent CreateEvent;
    PFN_vkDestroyEvent DestroyEvent;
    PFN_vkGetEventStatus GetEventStatus;
    PFN_vkSetEvent SetEvent;
    PFN_vkResetEvent ResetEvent;
    PFN_vkCreateQueryPool CreateQueryPool;
    PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkGetQueryPoolResults GetQueryPoolResults;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkCreateBufferView CreateBufferView;
    PFN_vkDestroyBufferView DestroyBufferView;
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreateShader CreateShader;
    PFN_vkDestroyShader DestroyShader;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkGetPipelineCacheSize GetPipelineCacheSize;
    PFN_vkGetPipelineCacheData GetPipelineCacheData;
    PFN_vkMergePipelineCaches MergePipelineCaches;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkCreateComputePipelines CreateComputePipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkResetDescriptorPool ResetDescriptorPool;
    PFN_vkAllocDescriptorSets AllocDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocCommandBuffers AllocCommandBuffers;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;

    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkQueueBindSparseBufferMemory QueueBindSparseBufferMemory;
    PFN_vkQueueBindSparseImageOpaqueMemory QueueBindSparseImageOpaqueMemory;
    PFN_vkQueueBindSparseImageMemory QueueBindSparseImageMemory;
    PFN_vkQueueSignalSemaphore QueueSignalSemaphore;
    PFN_vkQueueWaitSemaphore QueueWaitSemaphore;

    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdSetLineWidth CmdSetLineWidth;
    PFN_vkCmdSetDepthBias CmdSetDepthBias;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
    PFN_vkCmdSetStencilReference CmdSetStencilReference;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdDrawIndirect CmdDrawIndirect;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
    PFN_vkCmdDispatch CmdDispatch;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdClearColorImage CmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
    PFN_vkCmdClearAttachments CmdClearAttachments;
    PFN_vkCmdResolveImage CmdResolveImage;
    PFN_vkCmdSetEvent CmdSetEvent;
    PFN_vkCmdResetEvent CmdResetEvent;
    PFN_vkCmdWaitEvents CmdWaitEvents;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdBeginQuery CmdBeginQuery;
    PFN_vkCmdEndQuery CmdEndQuery;
    PFN_vkCmdResetQueryPool CmdResetQueryPool;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdNextSubpass CmdNextSubpass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdExecuteCommands CmdExecuteCommands;

    // Layers and loader only, not implemented by drivers
    PFN_vkGetSurfacePropertiesKHR GetSurfacePropertiesKHR;
    PFN_vkGetSurfaceFormatsKHR GetSurfaceFormatsKHR;
    PFN_vkGetSurfacePresentModesKHR GetSurfacePresentModesKHR;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;

    // Implemented only by drivers, not by layers or the loader
    PFN_vkGetSwapchainGrallocUsageANDROID GetSwapchainGrallocUsageANDROID;
    PFN_vkAcquireImageANDROID AcquireImageANDROID;
    PFN_vkQueueSignalReleaseImageANDROID QueueSignalReleaseImageANDROID;
    PFN_vkImportNativeFenceANDROID ImportNativeFenceANDROID;
    PFN_vkQueueSignalNativeFenceANDROID QueueSignalNativeFenceANDROID;
};

// -----------------------------------------------------------------------------
// loader.cpp

VkResult EnumerateInstanceExtensionProperties(
    const char* layer_name,
    uint32_t* count,
    VkExtensionProperties* properties);
VkResult EnumerateInstanceLayerProperties(uint32_t* count,
                                          VkLayerProperties* properties);
VkResult CreateInstance(const VkInstanceCreateInfo* create_info,
                        VkInstance* instance);
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* name);
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice drv_device, const char* name);
void GetDeviceQueue(VkDevice drv_device,
                    uint32_t family,
                    uint32_t index,
                    VkQueue* out_queue);
VkResult AllocCommandBuffers(VkDevice device,
                             const VkCmdBufferAllocInfo* alloc_info,
                             VkCmdBuffer* cmdbuffers);
VkResult DestroyDevice(VkDevice drv_device);

void* AllocDeviceMem(VkDevice device,
                     size_t size,
                     size_t align,
                     VkSystemAllocType type);
void FreeDeviceMem(VkDevice device, void* ptr);
const DeviceVtbl& GetDriverVtbl(VkDevice device);
const DeviceVtbl& GetDriverVtbl(VkQueue queue);

// -----------------------------------------------------------------------------
// get_proc_addr.cpp

PFN_vkVoidFunction GetGlobalInstanceProcAddr(const char* name);
PFN_vkVoidFunction GetGlobalDeviceProcAddr(const char* name);
PFN_vkVoidFunction GetSpecificInstanceProcAddr(const InstanceVtbl* vtbl,
                                               const char* name);
PFN_vkVoidFunction GetSpecificDeviceProcAddr(const DeviceVtbl* vtbl,
                                             const char* name);

bool LoadInstanceVtbl(VkInstance instance,
                      VkInstance next_instance,
                      PFN_vkGetInstanceProcAddr get_proc_addr,
                      InstanceVtbl& vtbl);
bool LoadDeviceVtbl(VkDevice device,
                    VkDevice next_device,
                    PFN_vkGetDeviceProcAddr get_proc_addr,
                    DeviceVtbl& vtbl);

// -----------------------------------------------------------------------------
// swapchain.cpp

VkResult GetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice pdev,
    uint32_t queue_family,
    const VkSurfaceDescriptionKHR* surface_desc,
    VkBool32* supported);
VkResult GetSurfacePropertiesKHR(VkDevice device,
                                 const VkSurfaceDescriptionKHR* surface_desc,
                                 VkSurfacePropertiesKHR* properties);
VkResult GetSurfaceFormatsKHR(VkDevice device,
                              const VkSurfaceDescriptionKHR* surface_desc,
                              uint32_t* count,
                              VkSurfaceFormatKHR* formats);
VkResult GetSurfacePresentModesKHR(VkDevice device,
                                   const VkSurfaceDescriptionKHR* surface_desc,
                                   uint32_t* count,
                                   VkPresentModeKHR* modes);
VkResult CreateSwapchainKHR(VkDevice device,
                            const VkSwapchainCreateInfoKHR* create_info,
                            VkSwapchainKHR* swapchain_handle);
VkResult DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain_handle);
VkResult GetSwapchainImagesKHR(VkDevice device,
                               VkSwapchainKHR swapchain_handle,
                               uint32_t* count,
                               VkImage* images);
VkResult AcquireNextImageKHR(VkDevice device,
                             VkSwapchainKHR swapchain_handle,
                             uint64_t timeout,
                             VkSemaphore semaphore,
                             uint32_t* image_index);
VkResult QueuePresentKHR(VkQueue queue, VkPresentInfoKHR* present_info);

}  // namespace vulkan

#endif  // LIBVULKAN_LOADER_H
