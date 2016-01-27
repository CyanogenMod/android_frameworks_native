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
    inline const DeviceVtbl& GetVtbl(VkCmdBuffer cmdBuffer) {
        return **reinterpret_cast<DeviceVtbl**>(cmdBuffer);
    }
} // namespace

__attribute__((visibility("default")))
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance) {
    return vulkan::CreateInstance(pCreateInfo, pInstance);
}

__attribute__((visibility("default")))
VkResult vkDestroyInstance(VkInstance instance) {
    return GetVtbl(instance).DestroyInstance(instance);
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
VkResult vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceProperties(physicalDevice, pProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceQueueCount(VkPhysicalDevice physicalDevice, uint32_t* pCount) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceQueueCount(physicalDevice, pCount);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceQueueProperties(VkPhysicalDevice physicalDevice, uint32_t count, VkPhysicalDeviceQueueProperties* pQueueProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceQueueProperties(physicalDevice, count, pQueueProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageFormatProperties* pImageFormatProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, pImageFormatProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceLimits(VkPhysicalDevice physicalDevice, VkPhysicalDeviceLimits* pLimits) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceLimits(physicalDevice, pLimits);
}

__attribute__((visibility("default")))
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice) {
    return GetVtbl(physicalDevice).CreateDevice(physicalDevice, pCreateInfo, pDevice);
}

__attribute__((visibility("default")))
VkResult vkDestroyDevice(VkDevice device) {
    return vulkan::DestroyDevice(device);
}

__attribute__((visibility("default")))
VkResult vkGetGlobalLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties) {
    return vulkan::GetGlobalLayerProperties(pCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkGetGlobalExtensionProperties(const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties) {
    return vulkan::GetGlobalExtensionProperties(pLayerName, pCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceLayerProperties(physicalDevice, pCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceExtensionProperties(physicalDevice, pLayerName, pCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    return vulkan::GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

__attribute__((visibility("default")))
VkResult vkQueueSubmit(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence) {
    return GetVtbl(queue).QueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);
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
VkResult vkAllocMemory(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem) {
    return GetVtbl(device).AllocMemory(device, pAllocInfo, pMem);
}

__attribute__((visibility("default")))
VkResult vkFreeMemory(VkDevice device, VkDeviceMemory mem) {
    return GetVtbl(device).FreeMemory(device, mem);
}

__attribute__((visibility("default")))
VkResult vkMapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    return GetVtbl(device).MapMemory(device, mem, offset, size, flags, ppData);
}

__attribute__((visibility("default")))
VkResult vkUnmapMemory(VkDevice device, VkDeviceMemory mem) {
    return GetVtbl(device).UnmapMemory(device, mem);
}

__attribute__((visibility("default")))
VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges) {
    return GetVtbl(device).FlushMappedMemoryRanges(device, memRangeCount, pMemRanges);
}

__attribute__((visibility("default")))
VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges) {
    return GetVtbl(device).InvalidateMappedMemoryRanges(device, memRangeCount, pMemRanges);
}

__attribute__((visibility("default")))
VkResult vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    return GetVtbl(device).GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

__attribute__((visibility("default")))
VkResult vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    return GetVtbl(device).GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset) {
    return GetVtbl(device).BindBufferMemory(device, buffer, mem, memOffset);
}

__attribute__((visibility("default")))
VkResult vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    return GetVtbl(device).GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset) {
    return GetVtbl(device).BindImageMemory(device, image, mem, memOffset);
}

__attribute__((visibility("default")))
VkResult vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    return GetVtbl(device).GetImageSparseMemoryRequirements(device, image, pNumRequirements, pSparseMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties) {
    return GetVtbl(physicalDevice).GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pNumProperties, pProperties);
}

__attribute__((visibility("default")))
VkResult vkQueueBindSparseBufferMemory(VkQueue queue, VkBuffer buffer, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo) {
    return GetVtbl(queue).QueueBindSparseBufferMemory(queue, buffer, numBindings, pBindInfo);
}

__attribute__((visibility("default")))
VkResult vkQueueBindSparseImageOpaqueMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo) {
    return GetVtbl(queue).QueueBindSparseImageOpaqueMemory(queue, image, numBindings, pBindInfo);
}

__attribute__((visibility("default")))
VkResult vkQueueBindSparseImageMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseImageMemoryBindInfo* pBindInfo) {
    return GetVtbl(queue).QueueBindSparseImageMemory(queue, image, numBindings, pBindInfo);
}

__attribute__((visibility("default")))
VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence) {
    return GetVtbl(device).CreateFence(device, pCreateInfo, pFence);
}

__attribute__((visibility("default")))
VkResult vkDestroyFence(VkDevice device, VkFence fence) {
    return GetVtbl(device).DestroyFence(device, fence);
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
VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore) {
    return GetVtbl(device).CreateSemaphore(device, pCreateInfo, pSemaphore);
}

__attribute__((visibility("default")))
VkResult vkDestroySemaphore(VkDevice device, VkSemaphore semaphore) {
    return GetVtbl(device).DestroySemaphore(device, semaphore);
}

__attribute__((visibility("default")))
VkResult vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore) {
    return GetVtbl(queue).QueueSignalSemaphore(queue, semaphore);
}

__attribute__((visibility("default")))
VkResult vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore) {
    return GetVtbl(queue).QueueWaitSemaphore(queue, semaphore);
}

__attribute__((visibility("default")))
VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent) {
    return GetVtbl(device).CreateEvent(device, pCreateInfo, pEvent);
}

__attribute__((visibility("default")))
VkResult vkDestroyEvent(VkDevice device, VkEvent event) {
    return GetVtbl(device).DestroyEvent(device, event);
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
VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool) {
    return GetVtbl(device).CreateQueryPool(device, pCreateInfo, pQueryPool);
}

__attribute__((visibility("default")))
VkResult vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool) {
    return GetVtbl(device).DestroyQueryPool(device, queryPool);
}

__attribute__((visibility("default")))
VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t* pDataSize, void* pData, VkQueryResultFlags flags) {
    return GetVtbl(device).GetQueryPoolResults(device, queryPool, startQuery, queryCount, pDataSize, pData, flags);
}

__attribute__((visibility("default")))
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer) {
    return GetVtbl(device).CreateBuffer(device, pCreateInfo, pBuffer);
}

__attribute__((visibility("default")))
VkResult vkDestroyBuffer(VkDevice device, VkBuffer buffer) {
    return GetVtbl(device).DestroyBuffer(device, buffer);
}

__attribute__((visibility("default")))
VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView) {
    return GetVtbl(device).CreateBufferView(device, pCreateInfo, pView);
}

__attribute__((visibility("default")))
VkResult vkDestroyBufferView(VkDevice device, VkBufferView bufferView) {
    return GetVtbl(device).DestroyBufferView(device, bufferView);
}

__attribute__((visibility("default")))
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage) {
    return GetVtbl(device).CreateImage(device, pCreateInfo, pImage);
}

__attribute__((visibility("default")))
VkResult vkDestroyImage(VkDevice device, VkImage image) {
    return GetVtbl(device).DestroyImage(device, image);
}

__attribute__((visibility("default")))
VkResult vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    return GetVtbl(device).GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

__attribute__((visibility("default")))
VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView) {
    return GetVtbl(device).CreateImageView(device, pCreateInfo, pView);
}

__attribute__((visibility("default")))
VkResult vkDestroyImageView(VkDevice device, VkImageView imageView) {
    return GetVtbl(device).DestroyImageView(device, imageView);
}

__attribute__((visibility("default")))
VkResult vkCreateAttachmentView(VkDevice device, const VkAttachmentViewCreateInfo* pCreateInfo, VkAttachmentView* pView) {
    return GetVtbl(device).CreateAttachmentView(device, pCreateInfo, pView);
}

__attribute__((visibility("default")))
VkResult vkDestroyAttachmentView(VkDevice device, VkAttachmentView attachmentView) {
    return GetVtbl(device).DestroyAttachmentView(device, attachmentView);
}

__attribute__((visibility("default")))
VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule) {
    return GetVtbl(device).CreateShaderModule(device, pCreateInfo, pShaderModule);
}

__attribute__((visibility("default")))
VkResult vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule) {
    return GetVtbl(device).DestroyShaderModule(device, shaderModule);
}

__attribute__((visibility("default")))
VkResult vkCreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader) {
    return GetVtbl(device).CreateShader(device, pCreateInfo, pShader);
}

__attribute__((visibility("default")))
VkResult vkDestroyShader(VkDevice device, VkShader shader) {
    return GetVtbl(device).DestroyShader(device, shader);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache) {
    return GetVtbl(device).CreatePipelineCache(device, pCreateInfo, pPipelineCache);
}

__attribute__((visibility("default")))
VkResult vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache) {
    return GetVtbl(device).DestroyPipelineCache(device, pipelineCache);
}

__attribute__((visibility("default")))
size_t vkGetPipelineCacheSize(VkDevice device, VkPipelineCache pipelineCache) {
    return GetVtbl(device).GetPipelineCacheSize(device, pipelineCache);
}

__attribute__((visibility("default")))
VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, void* pData) {
    return GetVtbl(device).GetPipelineCacheData(device, pipelineCache, pData);
}

__attribute__((visibility("default")))
VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    return GetVtbl(device).MergePipelineCaches(device, destCache, srcCacheCount, pSrcCaches);
}

__attribute__((visibility("default")))
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateGraphicsPipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
}

__attribute__((visibility("default")))
VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateComputePipelines(device, pipelineCache, count, pCreateInfos, pPipelines);
}

__attribute__((visibility("default")))
VkResult vkDestroyPipeline(VkDevice device, VkPipeline pipeline) {
    return GetVtbl(device).DestroyPipeline(device, pipeline);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) {
    return GetVtbl(device).CreatePipelineLayout(device, pCreateInfo, pPipelineLayout);
}

__attribute__((visibility("default")))
VkResult vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout) {
    return GetVtbl(device).DestroyPipelineLayout(device, pipelineLayout);
}

__attribute__((visibility("default")))
VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler) {
    return GetVtbl(device).CreateSampler(device, pCreateInfo, pSampler);
}

__attribute__((visibility("default")))
VkResult vkDestroySampler(VkDevice device, VkSampler sampler) {
    return GetVtbl(device).DestroySampler(device, sampler);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout) {
    return GetVtbl(device).CreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);
}

__attribute__((visibility("default")))
VkResult vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
    return GetVtbl(device).DestroyDescriptorSetLayout(device, descriptorSetLayout);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorPool(VkDevice device, VkDescriptorPoolUsage poolUsage, uint32_t maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool) {
    return GetVtbl(device).CreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);
}

__attribute__((visibility("default")))
VkResult vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool) {
    return GetVtbl(device).DestroyDescriptorPool(device, descriptorPool);
}

__attribute__((visibility("default")))
VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool) {
    return GetVtbl(device).ResetDescriptorPool(device, descriptorPool);
}

__attribute__((visibility("default")))
VkResult vkAllocDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, uint32_t* pCount) {
    return GetVtbl(device).AllocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
}

__attribute__((visibility("default")))
VkResult vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets) {
    return GetVtbl(device).FreeDescriptorSets(device, descriptorPool, count, pDescriptorSets);
}

__attribute__((visibility("default")))
VkResult vkUpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    return GetVtbl(device).UpdateDescriptorSets(device, writeCount, pDescriptorWrites, copyCount, pDescriptorCopies);
}

__attribute__((visibility("default")))
VkResult vkCreateDynamicViewportState(VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo, VkDynamicViewportState* pState) {
    return GetVtbl(device).CreateDynamicViewportState(device, pCreateInfo, pState);
}

__attribute__((visibility("default")))
VkResult vkDestroyDynamicViewportState(VkDevice device, VkDynamicViewportState dynamicViewportState) {
    return GetVtbl(device).DestroyDynamicViewportState(device, dynamicViewportState);
}

__attribute__((visibility("default")))
VkResult vkCreateDynamicRasterState(VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo, VkDynamicRasterState* pState) {
    return GetVtbl(device).CreateDynamicRasterState(device, pCreateInfo, pState);
}

__attribute__((visibility("default")))
VkResult vkDestroyDynamicRasterState(VkDevice device, VkDynamicRasterState dynamicRasterState) {
    return GetVtbl(device).DestroyDynamicRasterState(device, dynamicRasterState);
}

__attribute__((visibility("default")))
VkResult vkCreateDynamicColorBlendState(VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo, VkDynamicColorBlendState* pState) {
    return GetVtbl(device).CreateDynamicColorBlendState(device, pCreateInfo, pState);
}

__attribute__((visibility("default")))
VkResult vkDestroyDynamicColorBlendState(VkDevice device, VkDynamicColorBlendState dynamicColorBlendState) {
    return GetVtbl(device).DestroyDynamicColorBlendState(device, dynamicColorBlendState);
}

__attribute__((visibility("default")))
VkResult vkCreateDynamicDepthStencilState(VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo, VkDynamicDepthStencilState* pState) {
    return GetVtbl(device).CreateDynamicDepthStencilState(device, pCreateInfo, pState);
}

__attribute__((visibility("default")))
VkResult vkDestroyDynamicDepthStencilState(VkDevice device, VkDynamicDepthStencilState dynamicDepthStencilState) {
    return GetVtbl(device).DestroyDynamicDepthStencilState(device, dynamicDepthStencilState);
}

__attribute__((visibility("default")))
VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer) {
    return GetVtbl(device).CreateFramebuffer(device, pCreateInfo, pFramebuffer);
}

__attribute__((visibility("default")))
VkResult vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer) {
    return GetVtbl(device).DestroyFramebuffer(device, framebuffer);
}

__attribute__((visibility("default")))
VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) {
    return GetVtbl(device).CreateRenderPass(device, pCreateInfo, pRenderPass);
}

__attribute__((visibility("default")))
VkResult vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass) {
    return GetVtbl(device).DestroyRenderPass(device, renderPass);
}

__attribute__((visibility("default")))
VkResult vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    return GetVtbl(device).GetRenderAreaGranularity(device, renderPass, pGranularity);
}

__attribute__((visibility("default")))
VkResult vkCreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool) {
    return GetVtbl(device).CreateCommandPool(device, pCreateInfo, pCmdPool);
}

__attribute__((visibility("default")))
VkResult vkDestroyCommandPool(VkDevice device, VkCmdPool cmdPool) {
    return GetVtbl(device).DestroyCommandPool(device, cmdPool);
}

__attribute__((visibility("default")))
VkResult vkResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags) {
    return GetVtbl(device).ResetCommandPool(device, cmdPool, flags);
}

__attribute__((visibility("default")))
VkResult vkCreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer) {
    return vulkan::CreateCommandBuffer(device, pCreateInfo, pCmdBuffer);
}

__attribute__((visibility("default")))
VkResult vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer commandBuffer) {
    return GetVtbl(device).DestroyCommandBuffer(device, commandBuffer);
}

__attribute__((visibility("default")))
VkResult vkBeginCommandBuffer(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo) {
    return GetVtbl(cmdBuffer).BeginCommandBuffer(cmdBuffer, pBeginInfo);
}

__attribute__((visibility("default")))
VkResult vkEndCommandBuffer(VkCmdBuffer cmdBuffer) {
    return GetVtbl(cmdBuffer).EndCommandBuffer(cmdBuffer);
}

__attribute__((visibility("default")))
VkResult vkResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags) {
    return GetVtbl(cmdBuffer).ResetCommandBuffer(cmdBuffer, flags);
}

__attribute__((visibility("default")))
void vkCmdBindPipeline(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    GetVtbl(cmdBuffer).CmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
}

__attribute__((visibility("default")))
void vkCmdBindDynamicViewportState(VkCmdBuffer cmdBuffer, VkDynamicViewportState dynamicViewportState) {
    GetVtbl(cmdBuffer).CmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);
}

__attribute__((visibility("default")))
void vkCmdBindDynamicRasterState(VkCmdBuffer cmdBuffer, VkDynamicRasterState dynamicRasterState) {
    GetVtbl(cmdBuffer).CmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);
}

__attribute__((visibility("default")))
void vkCmdBindDynamicColorBlendState(VkCmdBuffer cmdBuffer, VkDynamicColorBlendState dynamicColorBlendState) {
    GetVtbl(cmdBuffer).CmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);
}

__attribute__((visibility("default")))
void vkCmdBindDynamicDepthStencilState(VkCmdBuffer cmdBuffer, VkDynamicDepthStencilState dynamicDepthStencilState) {
    GetVtbl(cmdBuffer).CmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);
}

__attribute__((visibility("default")))
void vkCmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    GetVtbl(cmdBuffer).CmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

__attribute__((visibility("default")))
void vkCmdBindIndexBuffer(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    GetVtbl(cmdBuffer).CmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);
}

__attribute__((visibility("default")))
void vkCmdBindVertexBuffers(VkCmdBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    GetVtbl(cmdBuffer).CmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);
}

__attribute__((visibility("default")))
void vkCmdDraw(VkCmdBuffer cmdBuffer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount) {
    GetVtbl(cmdBuffer).CmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t firstIndex, uint32_t indexCount, int32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount) {
    GetVtbl(cmdBuffer).CmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
}

__attribute__((visibility("default")))
void vkCmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride) {
    GetVtbl(cmdBuffer).CmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride) {
    GetVtbl(cmdBuffer).CmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);
}

__attribute__((visibility("default")))
void vkCmdDispatch(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z) {
    GetVtbl(cmdBuffer).CmdDispatch(cmdBuffer, x, y, z);
}

__attribute__((visibility("default")))
void vkCmdDispatchIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset) {
    GetVtbl(cmdBuffer).CmdDispatchIndirect(cmdBuffer, buffer, offset);
}

__attribute__((visibility("default")))
void vkCmdCopyBuffer(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    GetVtbl(cmdBuffer).CmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdCopyImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    GetVtbl(cmdBuffer).CmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdBlitImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkTexFilter filter) {
    GetVtbl(cmdBuffer).CmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);
}

__attribute__((visibility("default")))
void vkCmdCopyBufferToImage(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetVtbl(cmdBuffer).CmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdCopyImageToBuffer(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetVtbl(cmdBuffer).CmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData) {
    GetVtbl(cmdBuffer).CmdUpdateBuffer(cmdBuffer, destBuffer, destOffset, dataSize, pData);
}

__attribute__((visibility("default")))
void vkCmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data) {
    GetVtbl(cmdBuffer).CmdFillBuffer(cmdBuffer, destBuffer, destOffset, fillSize, data);
}

__attribute__((visibility("default")))
void vkCmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(cmdBuffer).CmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearDepthStencilImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(cmdBuffer).CmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearColorAttachment(VkCmdBuffer cmdBuffer, uint32_t colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rectCount, const VkRect3D* pRects) {
    GetVtbl(cmdBuffer).CmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);
}

__attribute__((visibility("default")))
void vkCmdClearDepthStencilAttachment(VkCmdBuffer cmdBuffer, VkImageAspectFlags imageAspectMask, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rectCount, const VkRect3D* pRects) {
    GetVtbl(cmdBuffer).CmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);
}

__attribute__((visibility("default")))
void vkCmdResolveImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    GetVtbl(cmdBuffer).CmdResolveImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
void vkCmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetVtbl(cmdBuffer).CmdSetEvent(cmdBuffer, event, stageMask);
}

__attribute__((visibility("default")))
void vkCmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetVtbl(cmdBuffer).CmdResetEvent(cmdBuffer, event, stageMask);
}

__attribute__((visibility("default")))
void vkCmdWaitEvents(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers) {
    GetVtbl(cmdBuffer).CmdWaitEvents(cmdBuffer, eventCount, pEvents, srcStageMask, destStageMask, memBarrierCount, ppMemBarriers);
}

__attribute__((visibility("default")))
void vkCmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void* const* ppMemBarriers) {
    GetVtbl(cmdBuffer).CmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);
}

__attribute__((visibility("default")))
void vkCmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags) {
    GetVtbl(cmdBuffer).CmdBeginQuery(cmdBuffer, queryPool, slot, flags);
}

__attribute__((visibility("default")))
void vkCmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot) {
    GetVtbl(cmdBuffer).CmdEndQuery(cmdBuffer, queryPool, slot);
}

__attribute__((visibility("default")))
void vkCmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount) {
    GetVtbl(cmdBuffer).CmdResetQueryPool(cmdBuffer, queryPool, startQuery, queryCount);
}

__attribute__((visibility("default")))
void vkCmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset) {
    GetVtbl(cmdBuffer).CmdWriteTimestamp(cmdBuffer, timestampType, destBuffer, destOffset);
}

__attribute__((visibility("default")))
void vkCmdCopyQueryPoolResults(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags) {
    GetVtbl(cmdBuffer).CmdCopyQueryPoolResults(cmdBuffer, queryPool, startQuery, queryCount, destBuffer, destOffset, destStride, flags);
}

__attribute__((visibility("default")))
void vkCmdPushConstants(VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t start, uint32_t length, const void* values) {
    GetVtbl(cmdBuffer).CmdPushConstants(cmdBuffer, layout, stageFlags, start, length, values);
}

__attribute__((visibility("default")))
void vkCmdBeginRenderPass(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents) {
    GetVtbl(cmdBuffer).CmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);
}

__attribute__((visibility("default")))
void vkCmdNextSubpass(VkCmdBuffer cmdBuffer, VkRenderPassContents contents) {
    GetVtbl(cmdBuffer).CmdNextSubpass(cmdBuffer, contents);
}

__attribute__((visibility("default")))
void vkCmdEndRenderPass(VkCmdBuffer cmdBuffer) {
    GetVtbl(cmdBuffer).CmdEndRenderPass(cmdBuffer);
}

__attribute__((visibility("default")))
void vkCmdExecuteCommands(VkCmdBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCmdBuffer* pCmdBuffers) {
    GetVtbl(cmdBuffer).CmdExecuteCommands(cmdBuffer, cmdBuffersCount, pCmdBuffers);
}
