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

#ifndef NULLDRV_NULL_DRIVER_H
#define NULLDRV_NULL_DRIVER_H 1

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_android_native_buffer.h>

namespace null_driver {

PFN_vkVoidFunction LookupInstanceProcAddr(const char* name);
PFN_vkVoidFunction LookupDeviceProcAddr(const char* name);

// clang-format off
VKAPI_ATTR void DestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult EnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
VKAPI_ATTR void GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
VKAPI_ATTR void GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties);
VKAPI_ATTR VkResult GetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties);
VKAPI_ATTR void GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
VKAPI_ATTR void GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pQueueFamilyProperties);
VKAPI_ATTR void GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
VKAPI_ATTR PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
VKAPI_ATTR VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDevice* pDevice);
VKAPI_ATTR void DestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult EnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VKAPI_ATTR VkResult EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VKAPI_ATTR VkResult EnumerateInstanceLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties);
VKAPI_ATTR VkResult EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties);
VKAPI_ATTR void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
VKAPI_ATTR VkResult QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmitInfo, VkFence fence);
VKAPI_ATTR VkResult QueueWaitIdle(VkQueue queue);
VKAPI_ATTR VkResult DeviceWaitIdle(VkDevice device);
VKAPI_ATTR VkResult AllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocInfo, const VkAllocationCallbacks* allocator, VkDeviceMemory* pMem);
VKAPI_ATTR void FreeMemory(VkDevice device, VkDeviceMemory mem, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult MapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
VKAPI_ATTR void UnmapMemory(VkDevice device, VkDeviceMemory mem);
VKAPI_ATTR VkResult FlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
VKAPI_ATTR VkResult InvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
VKAPI_ATTR void GetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes);
VKAPI_ATTR VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset);
VKAPI_ATTR VkResult BindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset);
VKAPI_ATTR void GetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements);
VKAPI_ATTR void GetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements);
VKAPI_ATTR void GetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements);
VKAPI_ATTR void GetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties);
VKAPI_ATTR VkResult QueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence);
VKAPI_ATTR VkResult CreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkFence* pFence);
VKAPI_ATTR void DestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult ResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
VKAPI_ATTR VkResult GetFenceStatus(VkDevice device, VkFence fence);
VKAPI_ATTR VkResult WaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
VKAPI_ATTR VkResult CreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkSemaphore* pSemaphore);
VKAPI_ATTR void DestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkEvent* pEvent);
VKAPI_ATTR void DestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult GetEventStatus(VkDevice device, VkEvent event);
VKAPI_ATTR VkResult SetEvent(VkDevice device, VkEvent event);
VKAPI_ATTR VkResult ResetEvent(VkDevice device, VkEvent event);
VKAPI_ATTR VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkQueryPool* pQueryPool);
VKAPI_ATTR void DestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult GetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags);
VKAPI_ATTR VkResult CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkBuffer* pBuffer);
VKAPI_ATTR void DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkBufferView* pView);
VKAPI_ATTR void DestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkImage* pImage);
VKAPI_ATTR void DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator);
VKAPI_ATTR void GetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout);
VKAPI_ATTR VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkImageView* pView);
VKAPI_ATTR void DestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkShaderModule* pShaderModule);
VKAPI_ATTR void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkPipelineCache* pPipelineCache);
VKAPI_ATTR void DestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult GetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData);
VKAPI_ATTR VkResult MergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches);
VKAPI_ATTR VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* allocator, VkPipeline* pPipelines);
VKAPI_ATTR VkResult CreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* allocator, VkPipeline* pPipelines);
VKAPI_ATTR void DestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkPipelineLayout* pPipelineLayout);
VKAPI_ATTR void DestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkSampler* pSampler);
VKAPI_ATTR void DestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDescriptorSetLayout* pSetLayout);
VKAPI_ATTR void DestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDescriptorPool* pDescriptorPool);
VKAPI_ATTR void DestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult ResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags);
VKAPI_ATTR VkResult AllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocInfo, VkDescriptorSet* pDescriptorSets);
VKAPI_ATTR VkResult FreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets);
VKAPI_ATTR void UpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies);
VKAPI_ATTR VkResult CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkFramebuffer* pFramebuffer);
VKAPI_ATTR void DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkRenderPass* pRenderPass);
VKAPI_ATTR void DestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* allocator);
VKAPI_ATTR void GetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity);
VKAPI_ATTR VkResult CreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkCommandPool* pCmdPool);
VKAPI_ATTR void DestroyCommandPool(VkDevice device, VkCommandPool cmdPool, const VkAllocationCallbacks* allocator);
VKAPI_ATTR VkResult ResetCommandPool(VkDevice device, VkCommandPool cmdPool, VkCommandPoolResetFlags flags);
VKAPI_ATTR VkResult AllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocInfo, VkCommandBuffer* pCmdBuffers);
VKAPI_ATTR void FreeCommandBuffers(VkDevice device, VkCommandPool cmdPool, uint32_t count, const VkCommandBuffer* pCommandBuffers);
VKAPI_ATTR VkResult BeginCommandBuffer(VkCommandBuffer cmdBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
VKAPI_ATTR VkResult EndCommandBuffer(VkCommandBuffer cmdBuffer);
VKAPI_ATTR VkResult ResetCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandBufferResetFlags flags);
VKAPI_ATTR void CmdBindPipeline(VkCommandBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
VKAPI_ATTR void CmdSetViewport(VkCommandBuffer cmdBuffer, uint32_t viewportCount, const VkViewport* pViewports);
VKAPI_ATTR void CmdSetScissor(VkCommandBuffer cmdBuffer, uint32_t scissorCount, const VkRect2D* pScissors);
VKAPI_ATTR void CmdSetLineWidth(VkCommandBuffer cmdBuffer, float lineWidth);
VKAPI_ATTR void CmdSetDepthBias(VkCommandBuffer cmdBuffer, float depthBias, float depthBiasClamp, float slopeScaledDepthBias);
VKAPI_ATTR void CmdSetBlendConstants(VkCommandBuffer cmdBuffer, const float blendConst[4]);
VKAPI_ATTR void CmdSetDepthBounds(VkCommandBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds);
VKAPI_ATTR void CmdSetStencilCompareMask(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilCompareMask);
VKAPI_ATTR void CmdSetStencilWriteMask(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilWriteMask);
VKAPI_ATTR void CmdSetStencilReference(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilReference);
VKAPI_ATTR void CmdBindDescriptorSets(VkCommandBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
VKAPI_ATTR void CmdBindIndexBuffer(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
VKAPI_ATTR void CmdBindVertexBuffers(VkCommandBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
VKAPI_ATTR void CmdDraw(VkCommandBuffer cmdBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
VKAPI_ATTR void CmdDrawIndexed(VkCommandBuffer cmdBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
VKAPI_ATTR void CmdDrawIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
VKAPI_ATTR void CmdDrawIndexedIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
VKAPI_ATTR void CmdDispatch(VkCommandBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z);
VKAPI_ATTR void CmdDispatchIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset);
VKAPI_ATTR void CmdCopyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);
VKAPI_ATTR void CmdCopyImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
VKAPI_ATTR void CmdBlitImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter);
VKAPI_ATTR void CmdCopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
VKAPI_ATTR void CmdCopyImageToBuffer(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
VKAPI_ATTR void CmdUpdateBuffer(VkCommandBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData);
VKAPI_ATTR void CmdFillBuffer(VkCommandBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data);
VKAPI_ATTR void CmdClearColorImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
VKAPI_ATTR void CmdClearDepthStencilImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
VKAPI_ATTR void CmdClearAttachments(VkCommandBuffer cmdBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects);
VKAPI_ATTR void CmdResolveImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
VKAPI_ATTR void CmdSetEvent(VkCommandBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
VKAPI_ATTR void CmdResetEvent(VkCommandBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
VKAPI_ATTR void CmdWaitEvents(VkCommandBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers);
VKAPI_ATTR void CmdPipelineBarrier(VkCommandBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkDependencyFlags dependencyFlags, uint32_t memBarrierCount, const void* const* ppMemBarriers);
VKAPI_ATTR void CmdBeginQuery(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags);
VKAPI_ATTR void CmdEndQuery(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot);
VKAPI_ATTR void CmdResetQueryPool(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount);
VKAPI_ATTR void CmdWriteTimestamp(VkCommandBuffer cmdBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t slot);
VKAPI_ATTR void CmdCopyQueryPoolResults(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags);
VKAPI_ATTR void CmdPushConstants(VkCommandBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t start, uint32_t length, const void* values);
VKAPI_ATTR void CmdBeginRenderPass(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
VKAPI_ATTR void CmdNextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents);
VKAPI_ATTR void CmdEndRenderPass(VkCommandBuffer cmdBuffer);
VKAPI_ATTR void CmdExecuteCommands(VkCommandBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCommandBuffer* pCmdBuffers);

VKAPI_ATTR VkResult GetSwapchainGrallocUsageANDROID(VkDevice device, VkFormat format, VkImageUsageFlags imageUsage, int* grallocUsage);
VKAPI_ATTR VkResult AcquireImageANDROID(VkDevice device, VkImage image, int nativeFenceFd, VkSemaphore semaphore);
VKAPI_ATTR VkResult QueueSignalReleaseImageANDROID(VkQueue queue, VkImage image, int* pNativeFenceFd);
// clang-format on

}  // namespace null_driver

#endif  // NULLDRV_NULL_DRIVER_H
