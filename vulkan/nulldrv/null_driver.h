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
void DestroyInstance(VkInstance instance);
VkResult EnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
VkResult GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
VkResult GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties);
VkResult GetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties);
VkResult GetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
VkResult GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkQueueFamilyProperties* pQueueFamilyProperties);
VkResult GetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties);
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName);
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName);
VkResult CreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice);
void DestroyDevice(VkDevice device);
VkResult EnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties);
VkResult EnumerateInstanceLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties);
VkResult EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties);
VkResult GetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
VkResult QueueSubmit(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence);
VkResult QueueWaitIdle(VkQueue queue);
VkResult DeviceWaitIdle(VkDevice device);
VkResult AllocMemory(VkDevice device, const VkMemoryAllocInfo* pAllocInfo, VkDeviceMemory* pMem);
void FreeMemory(VkDevice device, VkDeviceMemory mem);
VkResult MapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
void UnmapMemory(VkDevice device, VkDeviceMemory mem);
VkResult FlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
VkResult InvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges);
VkResult GetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes);
VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset);
VkResult BindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset);
VkResult GetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements);
VkResult GetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements);
VkResult GetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements);
VkResult GetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties);
VkResult QueueBindSparseBufferMemory(VkQueue queue, VkBuffer buffer, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo);
VkResult QueueBindSparseImageOpaqueMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo);
VkResult QueueBindSparseImageMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseImageMemoryBindInfo* pBindInfo);
VkResult CreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence);
void DestroyFence(VkDevice device, VkFence fence);
VkResult ResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
VkResult GetFenceStatus(VkDevice device, VkFence fence);
VkResult WaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
VkResult CreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore);
void DestroySemaphore(VkDevice device, VkSemaphore semaphore);
VkResult QueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore);
VkResult QueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore);
VkResult CreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent);
void DestroyEvent(VkDevice device, VkEvent event);
VkResult GetEventStatus(VkDevice device, VkEvent event);
VkResult SetEvent(VkDevice device, VkEvent event);
VkResult ResetEvent(VkDevice device, VkEvent event);
VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool);
void DestroyQueryPool(VkDevice device, VkQueryPool queryPool);
VkResult GetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t* pDataSize, void* pData, VkQueryResultFlags flags);
VkResult CreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, VkBuffer* pBuffer);
void DestroyBuffer(VkDevice device, VkBuffer buffer);
VkResult CreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView);
void DestroyBufferView(VkDevice device, VkBufferView bufferView);
VkResult CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage);
void DestroyImage(VkDevice device, VkImage image);
VkResult GetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout);
VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView);
void DestroyImageView(VkDevice device, VkImageView imageView);
VkResult CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule);
void DestroyShaderModule(VkDevice device, VkShaderModule shaderModule);
VkResult CreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader);
void DestroyShader(VkDevice device, VkShader shader);
VkResult CreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache);
void DestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache);
size_t GetPipelineCacheSize(VkDevice device, VkPipelineCache pipelineCache);
VkResult GetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, void* pData);
VkResult MergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches);
VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines);
VkResult CreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines);
void DestroyPipeline(VkDevice device, VkPipeline pipeline);
VkResult CreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout);
void DestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout);
VkResult CreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler);
void DestroySampler(VkDevice device, VkSampler sampler);
VkResult CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout);
void DestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
VkResult CreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool);
void DestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool);
VkResult ResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool);
VkResult AllocDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets);
VkResult FreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets);
void UpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies);
VkResult CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer);
void DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer);
VkResult CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass);
void DestroyRenderPass(VkDevice device, VkRenderPass renderPass);
VkResult GetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity);
VkResult CreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool);
void DestroyCommandPool(VkDevice device, VkCmdPool cmdPool);
VkResult ResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags);
VkResult CreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer);
void DestroyCommandBuffer(VkDevice device, VkCmdBuffer commandBuffer);
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
void CmdBlitImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkTexFilter filter);
void CmdCopyBufferToImage(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdCopyImageToBuffer(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions);
void CmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData);
void CmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data);
void CmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearDepthStencilImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
void CmdClearAttachments(VkCmdBuffer cmdBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkRect3D* pRects);
void CmdResolveImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions);
void CmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask);
void CmdWaitEvents(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void* const* ppMemBarriers);
void CmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags);
void CmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot);
void CmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount);
void CmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset);
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
