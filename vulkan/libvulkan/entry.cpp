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
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkInstance* pInstance) {
    return vulkan::CreateInstance(pCreateInfo, pAllocator, pInstance);
}

__attribute__((visibility("default")))
void vkDestroyInstance(VkInstance instance, const VkAllocCallbacks* pAllocator) {
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
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkDevice* pDevice) {
    return GetVtbl(physicalDevice).CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

__attribute__((visibility("default")))
void vkDestroyDevice(VkDevice device, const VkAllocCallbacks* pAllocator) {
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
VkResult vkAllocMemory(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, const VkAllocCallbacks* pAllocator, VkDeviceMemory* pMem) {
    return GetVtbl(device).AllocMemory(device, pAllocInfo, pAllocator, pMem);
}

__attribute__((visibility("default")))
void vkFreeMemory(VkDevice device, VkDeviceMemory mem, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).FreeMemory(device, mem, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkMapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    return GetVtbl(device).MapMemory(device, mem, offset, size, flags, ppData);
}

__attribute__((visibility("default")))
void vkUnmapMemory(VkDevice device, VkDeviceMemory mem) {
    GetVtbl(device).UnmapMemory(device, mem);
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
void vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    GetVtbl(device).GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

__attribute__((visibility("default")))
void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    GetVtbl(device).GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset) {
    return GetVtbl(device).BindBufferMemory(device, buffer, mem, memOffset);
}

__attribute__((visibility("default")))
void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    GetVtbl(device).GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

__attribute__((visibility("default")))
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset) {
    return GetVtbl(device).BindImageMemory(device, image, mem, memOffset);
}

__attribute__((visibility("default")))
void vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    GetVtbl(device).GetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

__attribute__((visibility("default")))
void vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    GetVtbl(physicalDevice).GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    return GetVtbl(queue).QueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

__attribute__((visibility("default")))
VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkFence* pFence) {
    return GetVtbl(device).CreateFence(device, pCreateInfo, pAllocator, pFence);
}

__attribute__((visibility("default")))
void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocCallbacks* pAllocator) {
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
VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    return GetVtbl(device).CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

__attribute__((visibility("default")))
void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroySemaphore(device, semaphore, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkEvent* pEvent) {
    return GetVtbl(device).CreateEvent(device, pCreateInfo, pAllocator, pEvent);
}

__attribute__((visibility("default")))
void vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocCallbacks* pAllocator) {
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
VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    return GetVtbl(device).CreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

__attribute__((visibility("default")))
void vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyQueryPool(device, queryPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    return GetVtbl(device).GetQueryPoolResults(device, queryPool, startQuery, queryCount, dataSize, pData, stride, flags);
}

__attribute__((visibility("default")))
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkBuffer* pBuffer) {
    return GetVtbl(device).CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

__attribute__((visibility("default")))
void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyBuffer(device, buffer, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkBufferView* pView) {
    return GetVtbl(device).CreateBufferView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
void vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyBufferView(device, bufferView, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkImage* pImage) {
    return GetVtbl(device).CreateImage(device, pCreateInfo, pAllocator, pImage);
}

__attribute__((visibility("default")))
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyImage(device, image, pAllocator);
}

__attribute__((visibility("default")))
void vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    GetVtbl(device).GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

__attribute__((visibility("default")))
VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkImageView* pView) {
    return GetVtbl(device).CreateImageView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyImageView(device, imageView, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    return GetVtbl(device).CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

__attribute__((visibility("default")))
void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyShaderModule(device, shaderModule, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkShader* pShader) {
    return GetVtbl(device).CreateShader(device, pCreateInfo, pAllocator, pShader);
}

__attribute__((visibility("default")))
void vkDestroyShader(VkDevice device, VkShader shader, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyShader(device, shader, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    return GetVtbl(device).CreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

__attribute__((visibility("default")))
void vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipelineCache(device, pipelineCache, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    return GetVtbl(device).GetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

__attribute__((visibility("default")))
VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    return GetVtbl(device).MergePipelineCaches(device, destCache, srcCacheCount, pSrcCaches);
}

__attribute__((visibility("default")))
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetVtbl(device).CreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipeline(device, pipeline, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    return GetVtbl(device).CreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

__attribute__((visibility("default")))
void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkSampler* pSampler) {
    return GetVtbl(device).CreateSampler(device, pCreateInfo, pAllocator, pSampler);
}

__attribute__((visibility("default")))
void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroySampler(device, sampler, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    return GetVtbl(device).CreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

__attribute__((visibility("default")))
void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    return GetVtbl(device).CreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

__attribute__((visibility("default")))
void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyDescriptorPool(device, descriptorPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    return GetVtbl(device).ResetDescriptorPool(device, descriptorPool, flags);
}

__attribute__((visibility("default")))
VkResult vkAllocDescriptorSets(VkDevice device, const VkDescriptorSetAllocInfo* pAllocInfo, VkDescriptorSet* pDescriptorSets) {
    return GetVtbl(device).AllocDescriptorSets(device, pAllocInfo, pDescriptorSets);
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
VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    return GetVtbl(device).CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

__attribute__((visibility("default")))
void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyFramebuffer(device, framebuffer, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    return GetVtbl(device).CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

__attribute__((visibility("default")))
void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyRenderPass(device, renderPass, pAllocator);
}

__attribute__((visibility("default")))
void vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    GetVtbl(device).GetRenderAreaGranularity(device, renderPass, pGranularity);
}

__attribute__((visibility("default")))
VkResult vkCreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* pAllocator, VkCmdPool* pCmdPool) {
    return GetVtbl(device).CreateCommandPool(device, pCreateInfo, pAllocator, pCmdPool);
}

__attribute__((visibility("default")))
void vkDestroyCommandPool(VkDevice device, VkCmdPool cmdPool, const VkAllocCallbacks* pAllocator) {
    GetVtbl(device).DestroyCommandPool(device, cmdPool, pAllocator);
}

__attribute__((visibility("default")))
VkResult vkResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags) {
    return GetVtbl(device).ResetCommandPool(device, cmdPool, flags);
}

__attribute__((visibility("default")))
VkResult vkAllocCommandBuffers(VkDevice device, const VkCmdBufferAllocInfo* pAllocInfo, VkCmdBuffer* pCmdBuffers) {
    return GetVtbl(device).AllocCommandBuffers(device, pAllocInfo, pCmdBuffers);
}

__attribute__((visibility("default")))
void vkFreeCommandBuffers(VkDevice device, VkCmdPool cmdPool, uint32_t commandBufferCount, const VkCmdBuffer* pCommandBuffers) {
    GetVtbl(device).FreeCommandBuffers(device, cmdPool, commandBufferCount, pCommandBuffers);
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
void vkCmdSetViewport(VkCmdBuffer cmdBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    GetVtbl(cmdBuffer).CmdSetViewport(cmdBuffer, viewportCount, pViewports);
}

__attribute__((visibility("default")))
void vkCmdSetScissor(VkCmdBuffer cmdBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    GetVtbl(cmdBuffer).CmdSetScissor(cmdBuffer, scissorCount, pScissors);
}

__attribute__((visibility("default")))
void vkCmdSetLineWidth(VkCmdBuffer cmdBuffer, float lineWidth) {
    GetVtbl(cmdBuffer).CmdSetLineWidth(cmdBuffer, lineWidth);
}

__attribute__((visibility("default")))
void vkCmdSetDepthBias(VkCmdBuffer cmdBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    GetVtbl(cmdBuffer).CmdSetDepthBias(cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

__attribute__((visibility("default")))
void vkCmdSetBlendConstants(VkCmdBuffer cmdBuffer, const float blendConstants[4]) {
    GetVtbl(cmdBuffer).CmdSetBlendConstants(cmdBuffer, blendConstants);
}

__attribute__((visibility("default")))
void vkCmdSetDepthBounds(VkCmdBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds) {
    GetVtbl(cmdBuffer).CmdSetDepthBounds(cmdBuffer, minDepthBounds, maxDepthBounds);
}

__attribute__((visibility("default")))
void vkCmdSetStencilCompareMask(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilCompareMask) {
    GetVtbl(cmdBuffer).CmdSetStencilCompareMask(cmdBuffer, faceMask, stencilCompareMask);
}

__attribute__((visibility("default")))
void vkCmdSetStencilWriteMask(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilWriteMask) {
    GetVtbl(cmdBuffer).CmdSetStencilWriteMask(cmdBuffer, faceMask, stencilWriteMask);
}

__attribute__((visibility("default")))
void vkCmdSetStencilReference(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilReference) {
    GetVtbl(cmdBuffer).CmdSetStencilReference(cmdBuffer, faceMask, stencilReference);
}

__attribute__((visibility("default")))
void vkCmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    GetVtbl(cmdBuffer).CmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
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
void vkCmdDraw(VkCmdBuffer cmdBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    GetVtbl(cmdBuffer).CmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    GetVtbl(cmdBuffer).CmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

__attribute__((visibility("default")))
void vkCmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetVtbl(cmdBuffer).CmdDrawIndirect(cmdBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
void vkCmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetVtbl(cmdBuffer).CmdDrawIndexedIndirect(cmdBuffer, buffer, offset, drawCount, stride);
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
void vkCmdBlitImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
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
void vkCmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize size, uint32_t data) {
    GetVtbl(cmdBuffer).CmdFillBuffer(cmdBuffer, destBuffer, destOffset, size, data);
}

__attribute__((visibility("default")))
void vkCmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(cmdBuffer).CmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearDepthStencilImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetVtbl(cmdBuffer).CmdClearDepthStencilImage(cmdBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

__attribute__((visibility("default")))
void vkCmdClearAttachments(VkCmdBuffer cmdBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    GetVtbl(cmdBuffer).CmdClearAttachments(cmdBuffer, attachmentCount, pAttachments, rectCount, pRects);
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
void vkCmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkDependencyFlags dependencyFlags, uint32_t memBarrierCount, const void* const* ppMemBarriers) {
    GetVtbl(cmdBuffer).CmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, dependencyFlags, memBarrierCount, ppMemBarriers);
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
void vkCmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t slot) {
    GetVtbl(cmdBuffer).CmdWriteTimestamp(cmdBuffer, pipelineStage, queryPool, slot);
}

__attribute__((visibility("default")))
void vkCmdCopyQueryPoolResults(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    GetVtbl(cmdBuffer).CmdCopyQueryPoolResults(cmdBuffer, queryPool, startQuery, queryCount, destBuffer, destOffset, stride, flags);
}

__attribute__((visibility("default")))
void vkCmdPushConstants(VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* values) {
    GetVtbl(cmdBuffer).CmdPushConstants(cmdBuffer, layout, stageFlags, offset, size, values);
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
