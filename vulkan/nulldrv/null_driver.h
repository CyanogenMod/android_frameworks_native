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
#include <vulkan/vk_ext_android_native_buffer.h>

namespace null_driver {

PFN_vkVoidFunction LookupInstanceProcAddr(const char* name);
PFN_vkVoidFunction LookupDeviceProcAddr(const char* name);

// clang-format off
void DestroyInstance(VkInstance instance, const VkAllocCallbacks* allocator);
VkResult EnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
void GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
void GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties);
void GetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties);
void GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
void GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pQueueFamilyProperties);
void GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkDevice* pDevice);
void DestroyDevice(VkDevice device, const VkAllocCallbacks* allocator);
VkResult EnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateInstanceLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties);
VkResult EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties);
void GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
VkResult QueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmitInfo, VkFence fence);
VkResult QueueWaitIdle(VkQueue queue);
VkResult DeviceWaitIdle(VkDevice device);
VkResult AllocMemory(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, const VkAllocCallbacks* allocator, VkDeviceMemory* pMem);
void FreeMemory(VkDevice device, VkDeviceMemory mem, const VkAllocCallbacks* allocator);
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
VkResult CreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkFence* pFence);
void DestroyFence(VkDevice device, VkFence fence, const VkAllocCallbacks* allocator);
VkResult ResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
VkResult GetFenceStatus(VkDevice device, VkFence fence);
VkResult WaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
VkResult CreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkSemaphore* pSemaphore);
void DestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocCallbacks* allocator);
VkResult CreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkEvent* pEvent);
void DestroyEvent(VkDevice device, VkEvent event, const VkAllocCallbacks* allocator);
VkResult GetEventStatus(VkDevice device, VkEvent event);
VkResult SetEvent(VkDevice device, VkEvent event);
VkResult ResetEvent(VkDevice device, VkEvent event);
VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkQueryPool* pQueryPool);
void DestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocCallbacks* allocator);
VkResult GetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags);
VkResult CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkBuffer* pBuffer);
void DestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocCallbacks* allocator);
VkResult CreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkBufferView* pView);
void DestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocCallbacks* allocator);
VkResult CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkImage* pImage);
void DestroyImage(VkDevice device, VkImage image, const VkAllocCallbacks* allocator);
void GetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout);
VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkImageView* pView);
void DestroyImageView(VkDevice device, VkImageView imageView, const VkAllocCallbacks* allocator);
VkResult CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkShaderModule* pShaderModule);
void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocCallbacks* allocator);
VkResult CreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkShader* pShader);
void DestroyShader(VkDevice device, VkShader shader, const VkAllocCallbacks* allocator);
VkResult CreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkPipelineCache* pPipelineCache);
void DestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocCallbacks* allocator);
VkResult GetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData);
VkResult MergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches);
VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocCallbacks* allocator, VkPipeline* pPipelines);
VkResult CreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocCallbacks* allocator, VkPipeline* pPipelines);
void DestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocCallbacks* allocator);
VkResult CreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkPipelineLayout* pPipelineLayout);
void DestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocCallbacks* allocator);
VkResult CreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkSampler* pSampler);
void DestroySampler(VkDevice device, VkSampler sampler, const VkAllocCallbacks* allocator);
VkResult CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkDescriptorSetLayout* pSetLayout);
void DestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocCallbacks* allocator);
VkResult CreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkDescriptorPool* pDescriptorPool);
void DestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocCallbacks* allocator);
VkResult ResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags);
VkResult AllocDescriptorSets(VkDevice device, const VkDescriptorSetAllocInfo* pAllocInfo, VkDescriptorSet* pDescriptorSets);
VkResult FreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets);
void UpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies);
VkResult CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkFramebuffer* pFramebuffer);
void DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocCallbacks* allocator);
VkResult CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkRenderPass* pRenderPass);
void DestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocCallbacks* allocator);
void GetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity);
VkResult CreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, const VkAllocCallbacks* allocator, VkCmdPool* pCmdPool);
void DestroyCommandPool(VkDevice device, VkCmdPool cmdPool, const VkAllocCallbacks* allocator);
VkResult ResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags);
VkResult AllocCommandBuffers(VkDevice device, const VkCmdBufferAllocInfo* pAllocInfo, VkCmdBuffer* pCmdBuffers);
void FreeCommandBuffers(VkDevice device, VkCmdPool cmdPool, uint32_t count, const VkCmdBuffer* pCommandBuffers);
VkResult BeginCommandBuffer(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo);
VkResult EndCommandBuffer(VkCmdBuffer cmdBuffer);
VkResult ResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags);
void CmdBindPipeline(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
void CmdSetViewport(VkCmdBuffer cmdBuffer, uint32_t viewportCount, const VkViewport* pViewports);
void CmdSetScissor(VkCmdBuffer cmdBuffer, uint32_t scissorCount, const VkRect2D* pScissors);
void CmdSetLineWidth(VkCmdBuffer cmdBuffer, float lineWidth);
void CmdSetDepthBias(VkCmdBuffer cmdBuffer, float depthBias, float depthBiasClamp, float slopeScaledDepthBias);
void CmdSetBlendConstants(VkCmdBuffer cmdBuffer, const float blendConst[4]);
void CmdSetDepthBounds(VkCmdBuffer cmdBuffer, float minDepthBounds, float maxDepthBounds);
void CmdSetStencilCompareMask(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilCompareMask);
void CmdSetStencilWriteMask(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilWriteMask);
void CmdSetStencilReference(VkCmdBuffer cmdBuffer, VkStencilFaceFlags faceMask, uint32_t stencilReference);
void CmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
void CmdBindIndexBuffer(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);
void CmdBindVertexBuffers(VkCmdBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
void CmdDraw(VkCmdBuffer cmdBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void CmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
void CmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
void CmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride);
void CmdDispatch(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z);
void CmdDispatchIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset);
void CmdCopyBuffer(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions);
void CmdCopyImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
void CmdBlitImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter);
void CmdCopyBufferToImage(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdCopyImageToBuffer(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData);
void CmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data);
void CmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearDepthStencilImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearAttachments(VkCmdBuffer cmdBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects);
void CmdResolveImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
void CmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdWaitEvents(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags);
void CmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot);
void CmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount);
void CmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t slot);
void CmdCopyQueryPoolResults(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags);
void CmdPushConstants(VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t start, uint32_t length, const void* values);
void CmdBeginRenderPass(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents);
void CmdNextSubpass(VkCmdBuffer cmdBuffer, VkRenderPassContents contents);
void CmdEndRenderPass(VkCmdBuffer cmdBuffer);
void CmdExecuteCommands(VkCmdBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCmdBuffer* pCmdBuffers);

VkResult GetSwapchainGrallocUsageANDROID(VkDevice device, VkFormat format, VkImageUsageFlags imageUsage, int* grallocUsage);
VkResult AcquireImageANDROID(VkDevice device, VkImage image, int nativeFenceFd, VkSemaphore semaphore);
VkResult QueueSignalReleaseImageANDROID(VkQueue queue, VkImage image, int* pNativeFenceFd);
// clang-format on

}  // namespace null_driver

#endif  // NULLDRV_NULL_DRIVER_H
