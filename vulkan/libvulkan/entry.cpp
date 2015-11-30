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

// This file is generated. Do not edit manually!
// To regenerate: $ apic template ../api/vulkan.api entry.cpp.tmpl
// Requires apic from https://android.googlesource.com/platform/tools/gpu/.

#include "loader.h"
using namespace vulkan;

// clang-format off

namespace {
    inline const InstanceVtbl& GetVtbl(VkInstance instance) {
        return **reinterpret_cast<InstanceVtbl**>(instance);
    }
    inline const InstanceVtbl& GetVtbl(VkPhysicalDevice physicalDevice) {
        return **reinterpret_cast<InstanceVtbl**>(physicalDevice);
    }
    inline const DeviceVtbl& GetVtbl(VkDevice device) {
        return **reinterpret_cast<DeviceVtbl**>(device);
    }
    inline const DeviceVtbl& GetVtbl(VkQueue queue) {
        return **reinterpret_cast<DeviceVtbl**>(queue);
    }
    inline const DeviceVtbl& GetVtbl(VkCommandBuffer commandBuffer) {
        return **reinterpret_cast<DeviceVtbl**>(commandBuffer);
    }
} // namespace

__attribute__((visibility("default")))
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    return vulkan::CreateInstance(pCreateInfo, pAllocator, pInstance);
}

__attribute__((visibility("default")))
void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(instance).DestroyInstance(instance, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    return GetVtbl(instance).EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    return vulkan::GetDeviceProcAddr(device, pName);
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    return vulkan::GetInstanceProcAddr(instance, pName);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceProperties(physicalDevice, pProperties);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    GetVtbl(physicalDevice).GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

__attribute__((visibility("default")))
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    return GetVtbl(physicalDevice).CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

__attribute__((visibility("default")))
void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    vulkan::DestroyDevice(device, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    return vulkan::EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    return vulkan::EnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    return GetVtbl(physicalDevice).EnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    return GetVtbl(physicalDevice).EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    vulkan::GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

__attribute__((visibility("default")))
VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    return GetVtbl(queue).QueueSubmit(queue, submitCount, pSubmits, fence);
}

__attribute__((visibility("default")))
VkResult vkQueueWaitIdle(VkQueue queue) {
    return GetVtbl(queue).QueueWaitIdle(queue);
}

__attribute__((visibility("default")))
VkResult vkDeviceWaitIdle(VkDevice device) {
    return GetVtbl(device).DeviceWaitIdle(device);
}

__attribute__((visibility("default")))
VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    return GetVtbl(device).AllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

__attribute__((visibility("default")))
void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).FreeMemory(device, memory, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    return GetVtbl(device).MapMemory(device, memory, offset, size, flags, ppData);
}

__attribute__((visibility("default")))
void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    GetVtbl(device).UnmapMemory(device, memory);
}

__attribute__((visibility("default")))
VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return GetVtbl(device).FlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return GetVtbl(device).InvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
void vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    GetVtbl(device).GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

__attribute__((visibility("default")))
void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    GetVtbl(device).GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return GetVtbl(device).BindBufferMemory(device, buffer, memory, memoryOffset);
}

__attribute__((visibility("default")))
void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    GetVtbl(device).GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return GetVtbl(device).BindImageMemory(device, image, memory, memoryOffset);
}

__attribute__((visibility("default")))
void vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    GetVtbl(device).GetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    return GetVtbl(queue).QueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

__attribute__((visibility("default")))
VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    return GetVtbl(device).CreateFence(device, pCreateInfo, pAllocator, pFence);
}

__attribute__((visibility("default")))
void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyFence(device, fence, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    return GetVtbl(device).ResetFences(device, fenceCount, pFences);
}

__attribute__((visibility("default")))
VkResult vkGetFenceStatus(VkDevice device, VkFence fence) {
    return GetVtbl(device).GetFenceStatus(device, fence);
}

__attribute__((visibility("default")))
VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    return GetVtbl(device).WaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

__attribute__((visibility("default")))
VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    return GetVtbl(device).CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

__attribute__((visibility("default")))
void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroySemaphore(device, semaphore, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    return GetVtbl(device).CreateEvent(device, pCreateInfo, pAllocator, pEvent);
}

__attribute__((visibility("default")))
void vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyEvent(device, event, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkGetEventStatus(VkDevice device, VkEvent event) {
    return GetVtbl(device).GetEventStatus(device, event);
}

__attribute__((visibility("default")))
VkResult vkSetEvent(VkDevice device, VkEvent event) {
    return GetVtbl(device).SetEvent(device, event);
}

__attribute__((visibility("default")))
VkResult vkResetEvent(VkDevice device, VkEvent event) {
    return GetVtbl(device).ResetEvent(device, event);
}

__attribute__((visibility("default")))
VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    return GetVtbl(device).CreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

__attribute__((visibility("default")))
void vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyQueryPool(device, queryPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    return GetVtbl(device).GetQueryPoolResults(device, queryPool, startQuery, queryCount, dataSize, pData, stride, flags);
}

__attribute__((visibility("default")))
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    return GetVtbl(device).CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

__attribute__((visibility("default")))
void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyBuffer(device, buffer, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    return GetVtbl(device).CreateBufferView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
void vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyBufferView(device, bufferView, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    return GetVtbl(device).CreateImage(device, pCreateInfo, pAllocator, pImage);
}

__attribute__((visibility("default")))
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyImage(device, image, pAllocator);
}

__attribute__((visibility("default")))
void vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    GetVtbl(device).GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

__attribute__((visibility("default")))
VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    return GetVtbl(device).CreateImageView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyImageView(device, imageView, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    return GetVtbl(device).CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

__attribute__((visibility("default")))
void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyShaderModule(device, shaderModule, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    return GetVtbl(device).CreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

__attribute__((visibility("default")))
void vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipelineCache(device, pipelineCache, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    return GetVtbl(device).GetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

__attribute__((visibility("default")))
VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    return GetVtbl(device).MergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}

__attribute__((visibility("default")))
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipeline(device, pipeline, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    return GetVtbl(device).CreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

__attribute__((visibility("default")))
void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    return GetVtbl(device).CreateSampler(device, pCreateInfo, pAllocator, pSampler);
}

__attribute__((visibility("default")))
void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroySampler(device, sampler, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    return GetVtbl(device).CreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

__attribute__((visibility("default")))
void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    return GetVtbl(device).CreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

__attribute__((visibility("default")))
void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyDescriptorPool(device, descriptorPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    return GetVtbl(device).ResetDescriptorPool(device, descriptorPool, flags);
}

__attribute__((visibility("default")))
VkResult vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    return GetVtbl(device).AllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

__attribute__((visibility("default")))
VkResult vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    return GetVtbl(device).FreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

__attribute__((visibility("default")))
void vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    GetVtbl(device).UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

__attribute__((visibility("default")))
VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    return GetVtbl(device).CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

__attribute__((visibility("default")))
void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyFramebuffer(device, framebuffer, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    return GetVtbl(device).CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

__attribute__((visibility("default")))
void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyRenderPass(device, renderPass, pAllocator);
}

__attribute__((visibility("default")))
void vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    GetVtbl(device).GetRenderAreaGranularity(device, renderPass, pGranularity);
}

__attribute__((visibility("default")))
VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    return GetVtbl(device).CreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

__attribute__((visibility("default")))
void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    GetVtbl(device).DestroyCommandPool(device, commandPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    return GetVtbl(device).ResetCommandPool(device, commandPool, flags);
}

__attribute__((visibility("default")))
VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    return GetVtbl(device).AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

__attribute__((visibility("default")))
void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    GetVtbl(device).FreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

__attribute__((visibility("default")))
VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    return GetVtbl(commandBuffer).BeginCommandBuffer(commandBuffer, pBeginInfo);
}

__attribute__((visibility("default")))
VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return GetVtbl(commandBuffer).EndCommandBuffer(commandBuffer);
}

__attribute__((visibility("default")))
VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    return GetVtbl(commandBuffer).ResetCommandBuffer(commandBuffer, flags);
}

__attribute__((visibility("default")))
void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    GetVtbl(commandBuffer).CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

__attribute__((visibility("default")))
void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    GetVtbl(commandBuffer).CmdSetViewport(commandBuffer, viewportCount, pViewports);
}

__attribute__((visibility("default")))
void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    GetVtbl(commandBuffer).CmdSetScissor(commandBuffer, scissorCount, pScissors);
}

__attribute__((visibility("default")))
void vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    GetVtbl(commandBuffer).CmdSetLineWidth(commandBuffer, lineWidth);
}

__attribute__((visibility("default")))
void vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    GetVtbl(commandBuffer).CmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

__attribute__((visibility("default")))
void vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
    GetVtbl(commandBuffer).CmdSetBlendConstants(commandBuffer, blendConstants);
}

__attribute__((visibility("default")))
void vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    GetVtbl(commandBuffer).CmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

__attribute__((visibility("default")))
void vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    GetVtbl(commandBuffer).CmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

__attribute__((visibility("default")))
void vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    GetVtbl(commandBuffer).CmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

__attribute__((visibility("default")))
void vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    GetVtbl(commandBuffer).CmdSetStencilReference(commandBuffer, faceMask, reference);
}

__attribute__((visibility("default")))
void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    GetVtbl(commandBuffer).CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

__attribute__((visibility("default")))
void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    GetVtbl(commandBuffer).CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

__attribute__((visibility("default")))
void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    GetVtbl(commandBuffer).CmdBindVertexBuffers(commandBuffer, startBinding, bindingCount, pBuffers, pOffsets);
}

__attribute__((visibility("default")))
void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    GetVtbl(commandBuffer).CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    GetVtbl(commandBuffer).CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

__attribute__((visibility("default")))
void vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetVtbl(commandBuffer).CmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetVtbl(commandBuffer).CmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
void vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z) {
    GetVtbl(commandBuffer).CmdDispatch(commandBuffer, x, y, z);
}

__attribute__((visibility("default")))
void vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    GetVtbl(commandBuffer).CmdDispatchIndirect(commandBuffer, buffer, offset);
}

__attribute__((visibility("default")))
void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    GetVtbl(commandBuffer).CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    GetVtbl(commandBuffer).CmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    GetVtbl(commandBuffer).CmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

__attribute__((visibility("default")))
void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetVtbl(commandBuffer).CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetVtbl(commandBuffer).CmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const uint32_t* pData) {
    GetVtbl(commandBuffer).CmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

__attribute__((visibility("default")))
void vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    GetVtbl(commandBuffer).CmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

__attribute__((visibility("default")))
void vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(commandBuffer).CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(commandBuffer).CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    GetVtbl(commandBuffer).CmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

__attribute__((visibility("default")))
void vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    GetVtbl(commandBuffer).CmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetVtbl(commandBuffer).CmdSetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
void vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetVtbl(commandBuffer).CmdResetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
void vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const void* const* ppMemoryBarriers) {
    GetVtbl(commandBuffer).CmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, ppMemoryBarriers);
}

__attribute__((visibility("default")))
void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const void* const* ppMemoryBarriers) {
    GetVtbl(commandBuffer).CmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, ppMemoryBarriers);
}

__attribute__((visibility("default")))
void vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t entry, VkQueryControlFlags flags) {
    GetVtbl(commandBuffer).CmdBeginQuery(commandBuffer, queryPool, entry, flags);
}

__attribute__((visibility("default")))
void vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t entry) {
    GetVtbl(commandBuffer).CmdEndQuery(commandBuffer, queryPool, entry);
}

__attribute__((visibility("default")))
void vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount) {
    GetVtbl(commandBuffer).CmdResetQueryPool(commandBuffer, queryPool, startQuery, queryCount);
}

__attribute__((visibility("default")))
void vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t entry) {
    GetVtbl(commandBuffer).CmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, entry);
}

__attribute__((visibility("default")))
void vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    GetVtbl(commandBuffer).CmdCopyQueryPoolResults(commandBuffer, queryPool, startQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

__attribute__((visibility("default")))
void vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* values) {
    GetVtbl(commandBuffer).CmdPushConstants(commandBuffer, layout, stageFlags, offset, size, values);
}

__attribute__((visibility("default")))
void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    GetVtbl(commandBuffer).CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

__attribute__((visibility("default")))
void vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    GetVtbl(commandBuffer).CmdNextSubpass(commandBuffer, contents);
}

__attribute__((visibility("default")))
void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    GetVtbl(commandBuffer).CmdEndRenderPass(commandBuffer);
}

__attribute__((visibility("default")))
void vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBuffersCount, const VkCommandBuffer* pCommandBuffers) {
    GetVtbl(commandBuffer).CmdExecuteCommands(commandBuffer, commandBuffersCount, pCommandBuffers);
}

__attribute__((visibility("default")))
void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface) {
    GetVtbl(instance).DestroySurfaceKHR(instance, surface);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, pSupported);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, pPresentModeCount, pPresentModes);
}

__attribute__((visibility("default")))
VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, VkSwapchainKHR* pSwapchain) {
    return GetVtbl(device).CreateSwapchainKHR(device, pCreateInfo, pSwapchain);
}

__attribute__((visibility("default")))
void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain) {
    GetVtbl(device).DestroySwapchainKHR(device, swapchain);
}

__attribute__((visibility("default")))
VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    return GetVtbl(device).GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

__attribute__((visibility("default")))
VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    return GetVtbl(device).AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

__attribute__((visibility("default")))
VkResult vkQueuePresentKHR(VkQueue queue, VkPresentInfoKHR* pPresentInfo) {
    return GetVtbl(queue).QueuePresentKHR(queue, pPresentInfo);
}

__attribute__((visibility("default")))
VkResult vkCreateAndroidSurfaceKHR(VkInstance instance, struct ANativeWindow* window, VkSurfaceKHR* pSurface) {
    return GetVtbl(instance).CreateAndroidSurfaceKHR(instance, window, pSurface);
}
