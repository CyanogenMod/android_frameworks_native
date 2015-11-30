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
void DestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator);
VkResult EnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
void GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
void GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties);
void GetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties);
void GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
void GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pQueueFamilyProperties);
void GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDevice* pDevice);
void DestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator);
VkResult EnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateInstanceLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties);
VkResult EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties);
void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
VkResult QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmitInfo, VkFence fence);
VkResult QueueWaitIdle(VkQueue queue);
VkResult DeviceWaitIdle(VkDevice device);
VkResult AllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocInfo, const VkAllocationCallbacks* allocator, VkDeviceMemory* pMem);
void FreeMemory(VkDevice device, VkDeviceMemory mem, const VkAllocationCallbacks* allocator);
VkResult MapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
void UnmapMemory(VkDevice device, VkDeviceMemory mem);
VkResult FlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
VkResult InvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
void GetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes);
VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset);
VkResult BindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset);
void GetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements);
void GetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements);
void GetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements);
void GetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties);
VkResult QueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence);
VkResult CreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkFence* pFence);
void DestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator);
VkResult ResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
VkResult GetFenceStatus(VkDevice device, VkFence fence);
VkResult WaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
VkResult CreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkSemaphore* pSemaphore);
void DestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* allocator);
VkResult CreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkEvent* pEvent);
void DestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* allocator);
VkResult GetEventStatus(VkDevice device, VkEvent event);
VkResult SetEvent(VkDevice device, VkEvent event);
VkResult ResetEvent(VkDevice device, VkEvent event);
VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkQueryPool* pQueryPool);
void DestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* allocator);
VkResult GetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags);
VkResult CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkBuffer* pBuffer);
void DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator);
VkResult CreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkBufferView* pView);
void DestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* allocator);
VkResult CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkImage* pImage);
void DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator);
void GetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout);
VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkImageView* pView);
void DestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* allocator);
VkResult CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkShaderModule* pShaderModule);
void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* allocator);
VkResult CreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkPipelineCache* pPipelineCache);
void DestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* allocator);
VkResult GetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData);
VkResult MergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches);
VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* allocator, VkPipeline* pPipelines);
VkResult CreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* allocator, VkPipeline* pPipelines);
void DestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* allocator);
VkResult CreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkPipelineLayout* pPipelineLayout);
void DestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* allocator);
VkResult CreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkSampler* pSampler);
void DestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* allocator);
VkResult CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDescriptorSetLayout* pSetLayout);
void DestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* allocator);
VkResult CreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkDescriptorPool* pDescriptorPool);
void DestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* allocator);
VkResult ResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags);
VkResult AllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocInfo, VkDescriptorSet* pDescriptorSets);
VkResult FreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets);
void UpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies);
VkResult CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkFramebuffer* pFramebuffer);
void DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* allocator);
VkResult CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkRenderPass* pRenderPass);
void DestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* allocator);
void GetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity);
VkResult CreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* allocator, VkCommandPool* pCmdPool);
void DestroyCommandPool(VkDevice device, VkCommandPool cmdPool, const VkAllocationCallbacks* allocator);
VkResult ResetCommandPool(VkDevice device, VkCommandPool cmdPool, VkCommandPoolResetFlags flags);
VkResult AllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocInfo, VkCommandBuffer* pCmdBuffers);
void FreeCommandBuffers(VkDevice device, VkCommandPool cmdPool, uint32_t count, const VkCommandBuffer* pCommandBuffers);
VkResult BeginCommandBuffer(VkCommandBuffer cmdBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
VkResult EndCommandBuffer(VkCommandBuffer cmdBuffer);
VkResult ResetCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandBufferResetFlags flags);
void CmdBindPipeline(VkCommandBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
void CmdSetViewport(VkCommandBuffer cmdBuffer, uint32_t viewportCount, const VkViewport* pViewports);
void CmdSetScissor(VkCommandBuffer cmdBuffer, uint32_t scissorCount, const VkRect2D* pScissors);
void CmdSetLineWidth(VkCommandBuffer cmdBuffer, float lineWidth);
void CmdSetDepthBias(VkCommandBuffer cmdBuffer, float depthBias, float depthBiasClamp, float slopeScaledDepthBias);
void CmdSetBlendConstants(VkCommandBuffer cmdBuffer, const float blendConst[4]);
void CmdSetDepthBounds(VkCommandBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds);
void CmdSetStencilCompareMask(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilCompareMask);
void CmdSetStencilWriteMask(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilWriteMask);
void CmdSetStencilReference(VkCommandBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilReference);
void CmdBindDescriptorSets(VkCommandBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
void CmdBindIndexBuffer(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
void CmdBindVertexBuffers(VkCommandBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
void CmdDraw(VkCommandBuffer cmdBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void CmdDrawIndexed(VkCommandBuffer cmdBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
void CmdDrawIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
void CmdDrawIndexedIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
void CmdDispatch(VkCommandBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z);
void CmdDispatchIndirect(VkCommandBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset);
void CmdCopyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);
void CmdCopyImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
void CmdBlitImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter);
void CmdCopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdCopyImageToBuffer(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdUpdateBuffer(VkCommandBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData);
void CmdFillBuffer(VkCommandBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data);
void CmdClearColorImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearDepthStencilImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearAttachments(VkCommandBuffer cmdBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects);
void CmdResolveImage(VkCommandBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
void CmdSetEvent(VkCommandBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdResetEvent(VkCommandBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdWaitEvents(VkCommandBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdPipelineBarrier(VkCommandBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkDependencyFlags dependencyFlags, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdBeginQuery(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags);
void CmdEndQuery(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot);
void CmdResetQueryPool(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount);
void CmdWriteTimestamp(VkCommandBuffer cmdBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t slot);
void CmdCopyQueryPoolResults(VkCommandBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags);
void CmdPushConstants(VkCommandBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t start, uint32_t length, const void* values);
void CmdBeginRenderPass(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
void CmdNextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents);
void CmdEndRenderPass(VkCommandBuffer cmdBuffer);
void CmdExecuteCommands(VkCommandBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCommandBuffer* pCmdBuffers);

VkResult GetSwapchainGrallocUsageANDROID(VkDevice device, VkFormat format, VkImageUsageFlags imageUsage, int* grallocUsage);
VkResult AcquireImageANDROID(VkDevice device, VkImage image, int nativeFenceFd, VkSemaphore semaphore);
VkResult QueueSignalReleaseImageANDROID(VkQueue queue, VkImage image, int* pNativeFenceFd);
// clang-format on

}  // namespace null_driver

#endif  // NULLDRV_NULL_DRIVER_H
