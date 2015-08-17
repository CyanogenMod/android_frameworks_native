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
// To regenerate: $ apic template ../api/vulkan.api null_driver_gen.cpp.tmpl
// Requires apic from https://android.googlesource.com/platform/tools/gpu/.

#include <algorithm>
#include "null_driver.h"

using namespace null_driver;

namespace {

struct NameProcEntry {
    const char* name;
    PFN_vkVoidFunction proc;
};

template <size_t N>
PFN_vkVoidFunction LookupProcAddr(const NameProcEntry(&table)[N],
                                  const char* name) {
    auto entry = std::lower_bound(table, table + N, name,
                                  [](const NameProcEntry& e, const char* n) {
                                      return strcmp(e.name, n) < 0;
                                  });
    if (entry != (table + N) && strcmp(entry->name, name) == 0)
        return entry->proc;
    return nullptr;
}

const NameProcEntry kInstanceProcTbl[] = {
    // clang-format off
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(CreateDevice)},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(DestroyInstance)},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(EnumeratePhysicalDevices)},
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(GetInstanceProcAddr)},
    {"vkGetPhysicalDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceExtensionProperties)},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceFeatures)},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceFormatProperties)},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceImageFormatProperties)},
    {"vkGetPhysicalDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceLayerProperties)},
    {"vkGetPhysicalDeviceLimits", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceLimits)},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceMemoryProperties)},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceProperties)},
    {"vkGetPhysicalDeviceQueueCount", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceQueueCount)},
    {"vkGetPhysicalDeviceQueueProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceQueueProperties)},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(GetPhysicalDeviceSparseImageFormatProperties)},
    // clang-format on
};

const NameProcEntry kDeviceProcTbl[] = {
    // clang-format off
    {"vkAllocDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(AllocDescriptorSets)},
    {"vkAllocMemory", reinterpret_cast<PFN_vkVoidFunction>(AllocMemory)},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(BeginCommandBuffer)},
    {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(BindBufferMemory)},
    {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(BindImageMemory)},
    {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(CmdBeginQuery)},
    {"vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(CmdBeginRenderPass)},
    {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(CmdBindDescriptorSets)},
    {"vkCmdBindDynamicColorBlendState", reinterpret_cast<PFN_vkVoidFunction>(CmdBindDynamicColorBlendState)},
    {"vkCmdBindDynamicDepthStencilState", reinterpret_cast<PFN_vkVoidFunction>(CmdBindDynamicDepthStencilState)},
    {"vkCmdBindDynamicRasterState", reinterpret_cast<PFN_vkVoidFunction>(CmdBindDynamicRasterState)},
    {"vkCmdBindDynamicViewportState", reinterpret_cast<PFN_vkVoidFunction>(CmdBindDynamicViewportState)},
    {"vkCmdBindIndexBuffer", reinterpret_cast<PFN_vkVoidFunction>(CmdBindIndexBuffer)},
    {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(CmdBindPipeline)},
    {"vkCmdBindVertexBuffers", reinterpret_cast<PFN_vkVoidFunction>(CmdBindVertexBuffers)},
    {"vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(CmdBlitImage)},
    {"vkCmdClearColorAttachment", reinterpret_cast<PFN_vkVoidFunction>(CmdClearColorAttachment)},
    {"vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(CmdClearColorImage)},
    {"vkCmdClearDepthStencilAttachment", reinterpret_cast<PFN_vkVoidFunction>(CmdClearDepthStencilAttachment)},
    {"vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(CmdClearDepthStencilImage)},
    {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(CmdCopyBuffer)},
    {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(CmdCopyBufferToImage)},
    {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(CmdCopyImage)},
    {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(CmdCopyImageToBuffer)},
    {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(CmdCopyQueryPoolResults)},
    {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(CmdDispatch)},
    {"vkCmdDispatchIndirect", reinterpret_cast<PFN_vkVoidFunction>(CmdDispatchIndirect)},
    {"vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(CmdDraw)},
    {"vkCmdDrawIndexed", reinterpret_cast<PFN_vkVoidFunction>(CmdDrawIndexed)},
    {"vkCmdDrawIndexedIndirect", reinterpret_cast<PFN_vkVoidFunction>(CmdDrawIndexedIndirect)},
    {"vkCmdDrawIndirect", reinterpret_cast<PFN_vkVoidFunction>(CmdDrawIndirect)},
    {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(CmdEndQuery)},
    {"vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(CmdEndRenderPass)},
    {"vkCmdExecuteCommands", reinterpret_cast<PFN_vkVoidFunction>(CmdExecuteCommands)},
    {"vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(CmdFillBuffer)},
    {"vkCmdNextSubpass", reinterpret_cast<PFN_vkVoidFunction>(CmdNextSubpass)},
    {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(CmdPipelineBarrier)},
    {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(CmdPushConstants)},
    {"vkCmdResetEvent", reinterpret_cast<PFN_vkVoidFunction>(CmdResetEvent)},
    {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(CmdResetQueryPool)},
    {"vkCmdResolveImage", reinterpret_cast<PFN_vkVoidFunction>(CmdResolveImage)},
    {"vkCmdSetEvent", reinterpret_cast<PFN_vkVoidFunction>(CmdSetEvent)},
    {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(CmdUpdateBuffer)},
    {"vkCmdWaitEvents", reinterpret_cast<PFN_vkVoidFunction>(CmdWaitEvents)},
    {"vkCmdWriteTimestamp", reinterpret_cast<PFN_vkVoidFunction>(CmdWriteTimestamp)},
    {"vkCreateAttachmentView", reinterpret_cast<PFN_vkVoidFunction>(CreateAttachmentView)},
    {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(CreateBuffer)},
    {"vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(CreateBufferView)},
    {"vkCreateCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(CreateCommandBuffer)},
    {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(CreateCommandPool)},
    {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(CreateComputePipelines)},
    {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(CreateDescriptorPool)},
    {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(CreateDescriptorSetLayout)},
    {"vkCreateDynamicColorBlendState", reinterpret_cast<PFN_vkVoidFunction>(CreateDynamicColorBlendState)},
    {"vkCreateDynamicDepthStencilState", reinterpret_cast<PFN_vkVoidFunction>(CreateDynamicDepthStencilState)},
    {"vkCreateDynamicRasterState", reinterpret_cast<PFN_vkVoidFunction>(CreateDynamicRasterState)},
    {"vkCreateDynamicViewportState", reinterpret_cast<PFN_vkVoidFunction>(CreateDynamicViewportState)},
    {"vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(CreateEvent)},
    {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(CreateFence)},
    {"vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(CreateFramebuffer)},
    {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(CreateGraphicsPipelines)},
    {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(CreateImage)},
    {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(CreateImageView)},
    {"vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(CreatePipelineCache)},
    {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(CreatePipelineLayout)},
    {"vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(CreateQueryPool)},
    {"vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(CreateRenderPass)},
    {"vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(CreateSampler)},
    {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(CreateSemaphore)},
    {"vkCreateShader", reinterpret_cast<PFN_vkVoidFunction>(CreateShader)},
    {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(CreateShaderModule)},
    {"vkDestroyAttachmentView", reinterpret_cast<PFN_vkVoidFunction>(DestroyAttachmentView)},
    {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(DestroyBuffer)},
    {"vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(DestroyBufferView)},
    {"vkDestroyCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(DestroyCommandBuffer)},
    {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(DestroyCommandPool)},
    {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(DestroyDescriptorPool)},
    {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(DestroyDescriptorSetLayout)},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(DestroyDevice)},
    {"vkDestroyDynamicColorBlendState", reinterpret_cast<PFN_vkVoidFunction>(DestroyDynamicColorBlendState)},
    {"vkDestroyDynamicDepthStencilState", reinterpret_cast<PFN_vkVoidFunction>(DestroyDynamicDepthStencilState)},
    {"vkDestroyDynamicRasterState", reinterpret_cast<PFN_vkVoidFunction>(DestroyDynamicRasterState)},
    {"vkDestroyDynamicViewportState", reinterpret_cast<PFN_vkVoidFunction>(DestroyDynamicViewportState)},
    {"vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(DestroyEvent)},
    {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(DestroyFence)},
    {"vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(DestroyFramebuffer)},
    {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(DestroyImage)},
    {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(DestroyImageView)},
    {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(DestroyPipeline)},
    {"vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(DestroyPipelineCache)},
    {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(DestroyPipelineLayout)},
    {"vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(DestroyQueryPool)},
    {"vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(DestroyRenderPass)},
    {"vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(DestroySampler)},
    {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(DestroySemaphore)},
    {"vkDestroyShader", reinterpret_cast<PFN_vkVoidFunction>(DestroyShader)},
    {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(DestroyShaderModule)},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(DeviceWaitIdle)},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(EndCommandBuffer)},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(FlushMappedMemoryRanges)},
    {"vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(FreeDescriptorSets)},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(FreeMemory)},
    {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(GetBufferMemoryRequirements)},
    {"vkGetDeviceMemoryCommitment", reinterpret_cast<PFN_vkVoidFunction>(GetDeviceMemoryCommitment)},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr)},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(GetDeviceQueue)},
    {"vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(GetEventStatus)},
    {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(GetFenceStatus)},
    {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(GetImageMemoryRequirements)},
    {"vkGetImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(GetImageSparseMemoryRequirements)},
    {"vkGetImageSubresourceLayout", reinterpret_cast<PFN_vkVoidFunction>(GetImageSubresourceLayout)},
    {"vkGetPipelineCacheData", reinterpret_cast<PFN_vkVoidFunction>(GetPipelineCacheData)},
    {"vkGetPipelineCacheSize", reinterpret_cast<PFN_vkVoidFunction>(GetPipelineCacheSize)},
    {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(GetQueryPoolResults)},
    {"vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(GetRenderAreaGranularity)},
    {"vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(InvalidateMappedMemoryRanges)},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(MapMemory)},
    {"vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(MergePipelineCaches)},
    {"vkQueueBindSparseBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(QueueBindSparseBufferMemory)},
    {"vkQueueBindSparseImageMemory", reinterpret_cast<PFN_vkVoidFunction>(QueueBindSparseImageMemory)},
    {"vkQueueBindSparseImageOpaqueMemory", reinterpret_cast<PFN_vkVoidFunction>(QueueBindSparseImageOpaqueMemory)},
    {"vkQueueSignalSemaphore", reinterpret_cast<PFN_vkVoidFunction>(QueueSignalSemaphore)},
    {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(QueueSubmit)},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(QueueWaitIdle)},
    {"vkQueueWaitSemaphore", reinterpret_cast<PFN_vkVoidFunction>(QueueWaitSemaphore)},
    {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(ResetCommandBuffer)},
    {"vkResetCommandPool", reinterpret_cast<PFN_vkVoidFunction>(ResetCommandPool)},
    {"vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(ResetDescriptorPool)},
    {"vkResetEvent", reinterpret_cast<PFN_vkVoidFunction>(ResetEvent)},
    {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(ResetFences)},
    {"vkSetEvent", reinterpret_cast<PFN_vkVoidFunction>(SetEvent)},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(UnmapMemory)},
    {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(UpdateDescriptorSets)},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(WaitForFences)},
    // clang-format on
};

}  // namespace

namespace null_driver {

PFN_vkVoidFunction LookupInstanceProcAddr(const char* name) {
    return LookupProcAddr(kInstanceProcTbl, name);
}

PFN_vkVoidFunction LookupDeviceProcAddr(const char* name) {
    return LookupProcAddr(kDeviceProcTbl, name);
}

}  // namespace null_driver
