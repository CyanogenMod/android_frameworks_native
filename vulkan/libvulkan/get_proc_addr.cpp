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
// To regenerate: $ apic template ../api/vulkan.api get_proc_addr.cpp.tmpl
// Requires apic from https://android.googlesource.com/platform/tools/gpu/.

#include <algorithm>
#include <log/log.h>
#include "loader.h"
using namespace vulkan;

#define UNLIKELY(expr) __builtin_expect((expr), 0)

namespace {

struct NameProcEntry {
    const char* name;
    PFN_vkVoidFunction proc;
};

struct NameOffsetEntry {
    const char* name;
    size_t offset;
};

template <typename TEntry, size_t N>
const TEntry* FindProcEntry(const TEntry(&table)[N], const char* name) {
    auto entry = std::lower_bound(
        table, table + N, name,
        [](const TEntry& e, const char* n) { return strcmp(e.name, n) < 0; });
    if (entry != (table + N) && strcmp(entry->name, name) == 0)
        return entry;
    return nullptr;
}

const NameProcEntry kInstanceProcTbl[] = {
    // clang-format off
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice)},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance)},
    {"vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateDeviceExtensionProperties)},
    {"vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateDeviceLayerProperties)},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(vkEnumeratePhysicalDevices)},
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr)},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceFeatures)},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceFormatProperties)},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceImageFormatProperties)},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceMemoryProperties)},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceProperties)},
    {"vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceQueueFamilyProperties)},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSparseImageFormatProperties)},
    // clang-format on
};

const NameProcEntry kDeviceProcTbl[] = {
    // clang-format off
    {"vkAllocDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkAllocDescriptorSets)},
    {"vkAllocMemory", reinterpret_cast<PFN_vkVoidFunction>(vkAllocMemory)},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkBeginCommandBuffer)},
    {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(vkBindBufferMemory)},
    {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(vkBindImageMemory)},
    {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginQuery)},
    {"vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRenderPass)},
    {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindDescriptorSets)},
    {"vkCmdBindIndexBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindIndexBuffer)},
    {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindPipeline)},
    {"vkCmdBindVertexBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBindVertexBuffers)},
    {"vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdBlitImage)},
    {"vkCmdClearColorAttachment", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearColorAttachment)},
    {"vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearColorImage)},
    {"vkCmdClearDepthStencilAttachment", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearDepthStencilAttachment)},
    {"vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearDepthStencilImage)},
    {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyBuffer)},
    {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyBufferToImage)},
    {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyImage)},
    {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyImageToBuffer)},
    {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(vkCmdCopyQueryPoolResults)},
    {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDispatch)},
    {"vkCmdDispatchIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDispatchIndirect)},
    {"vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDraw)},
    {"vkCmdDrawIndexed", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexed)},
    {"vkCmdDrawIndexedIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndexedIndirect)},
    {"vkCmdDrawIndirect", reinterpret_cast<PFN_vkVoidFunction>(vkCmdDrawIndirect)},
    {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndQuery)},
    {"vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdEndRenderPass)},
    {"vkCmdExecuteCommands", reinterpret_cast<PFN_vkVoidFunction>(vkCmdExecuteCommands)},
    {"vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdFillBuffer)},
    {"vkCmdNextSubpass", reinterpret_cast<PFN_vkVoidFunction>(vkCmdNextSubpass)},
    {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(vkCmdPipelineBarrier)},
    {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(vkCmdPushConstants)},
    {"vkCmdResetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResetEvent)},
    {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResetQueryPool)},
    {"vkCmdResolveImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdResolveImage)},
    {"vkCmdSetBlendConstants", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetBlendConstants)},
    {"vkCmdSetDepthBias", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetDepthBias)},
    {"vkCmdSetDepthBounds", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetDepthBounds)},
    {"vkCmdSetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetEvent)},
    {"vkCmdSetLineWidth", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetLineWidth)},
    {"vkCmdSetScissor", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetScissor)},
    {"vkCmdSetStencilCompareMask", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilCompareMask)},
    {"vkCmdSetStencilReference", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilReference)},
    {"vkCmdSetStencilWriteMask", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetStencilWriteMask)},
    {"vkCmdSetViewport", reinterpret_cast<PFN_vkVoidFunction>(vkCmdSetViewport)},
    {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCmdUpdateBuffer)},
    {"vkCmdWaitEvents", reinterpret_cast<PFN_vkVoidFunction>(vkCmdWaitEvents)},
    {"vkCmdWriteTimestamp", reinterpret_cast<PFN_vkVoidFunction>(vkCmdWriteTimestamp)},
    {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBuffer)},
    {"vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBufferView)},
    {"vkCreateCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateCommandBuffer)},
    {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateCommandPool)},
    {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateComputePipelines)},
    {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorPool)},
    {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorSetLayout)},
    {"vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCreateEvent)},
    {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFence)},
    {"vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFramebuffer)},
    {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateGraphicsPipelines)},
    {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImage)},
    {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImageView)},
    {"vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineCache)},
    {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineLayout)},
    {"vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateQueryPool)},
    {"vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass)},
    {"vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSampler)},
    {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSemaphore)},
    {"vkCreateShader", reinterpret_cast<PFN_vkVoidFunction>(vkCreateShader)},
    {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule)},
    {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBuffer)},
    {"vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBufferView)},
    {"vkDestroyCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyCommandBuffer)},
    {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyCommandPool)},
    {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorPool)},
    {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorSetLayout)},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice)},
    {"vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyEvent)},
    {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFence)},
    {"vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFramebuffer)},
    {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImage)},
    {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImageView)},
    {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipeline)},
    {"vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineCache)},
    {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineLayout)},
    {"vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyQueryPool)},
    {"vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyRenderPass)},
    {"vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySampler)},
    {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySemaphore)},
    {"vkDestroyShader", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyShader)},
    {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyShaderModule)},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkDeviceWaitIdle)},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkEndCommandBuffer)},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkFlushMappedMemoryRanges)},
    {"vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkFreeDescriptorSets)},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(vkFreeMemory)},
    {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetBufferMemoryRequirements)},
    {"vkGetDeviceMemoryCommitment", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceMemoryCommitment)},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr)},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceQueue)},
    {"vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(vkGetEventStatus)},
    {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(vkGetFenceStatus)},
    {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageMemoryRequirements)},
    {"vkGetImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageSparseMemoryRequirements)},
    {"vkGetImageSubresourceLayout", reinterpret_cast<PFN_vkVoidFunction>(vkGetImageSubresourceLayout)},
    {"vkGetPipelineCacheData", reinterpret_cast<PFN_vkVoidFunction>(vkGetPipelineCacheData)},
    {"vkGetPipelineCacheSize", reinterpret_cast<PFN_vkVoidFunction>(vkGetPipelineCacheSize)},
    {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(vkGetQueryPoolResults)},
    {"vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(vkGetRenderAreaGranularity)},
    {"vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkInvalidateMappedMemoryRanges)},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(vkMapMemory)},
    {"vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(vkMergePipelineCaches)},
    {"vkQueueBindSparseBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(vkQueueBindSparseBufferMemory)},
    {"vkQueueBindSparseImageMemory", reinterpret_cast<PFN_vkVoidFunction>(vkQueueBindSparseImageMemory)},
    {"vkQueueBindSparseImageOpaqueMemory", reinterpret_cast<PFN_vkVoidFunction>(vkQueueBindSparseImageOpaqueMemory)},
    {"vkQueueSignalSemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkQueueSignalSemaphore)},
    {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(vkQueueSubmit)},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkQueueWaitIdle)},
    {"vkQueueWaitSemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkQueueWaitSemaphore)},
    {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkResetCommandBuffer)},
    {"vkResetCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkResetCommandPool)},
    {"vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkResetDescriptorPool)},
    {"vkResetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkResetEvent)},
    {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(vkResetFences)},
    {"vkSetEvent", reinterpret_cast<PFN_vkVoidFunction>(vkSetEvent)},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(vkUnmapMemory)},
    {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkUpdateDescriptorSets)},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(vkWaitForFences)},
    // clang-format on
};

const NameOffsetEntry kInstanceOffsetTbl[] = {
    // clang-format off
    {"vkCreateDevice", offsetof(InstanceVtbl, CreateDevice)},
    {"vkDestroyInstance", offsetof(InstanceVtbl, DestroyInstance)},
    {"vkEnumerateDeviceExtensionProperties", offsetof(InstanceVtbl, EnumerateDeviceExtensionProperties)},
    {"vkEnumerateDeviceLayerProperties", offsetof(InstanceVtbl, EnumerateDeviceLayerProperties)},
    {"vkEnumeratePhysicalDevices", offsetof(InstanceVtbl, EnumeratePhysicalDevices)},
    {"vkGetInstanceProcAddr", offsetof(InstanceVtbl, GetInstanceProcAddr)},
    {"vkGetPhysicalDeviceFeatures", offsetof(InstanceVtbl, GetPhysicalDeviceFeatures)},
    {"vkGetPhysicalDeviceFormatProperties", offsetof(InstanceVtbl, GetPhysicalDeviceFormatProperties)},
    {"vkGetPhysicalDeviceImageFormatProperties", offsetof(InstanceVtbl, GetPhysicalDeviceImageFormatProperties)},
    {"vkGetPhysicalDeviceMemoryProperties", offsetof(InstanceVtbl, GetPhysicalDeviceMemoryProperties)},
    {"vkGetPhysicalDeviceProperties", offsetof(InstanceVtbl, GetPhysicalDeviceProperties)},
    {"vkGetPhysicalDeviceQueueFamilyProperties", offsetof(InstanceVtbl, GetPhysicalDeviceQueueFamilyProperties)},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", offsetof(InstanceVtbl, GetPhysicalDeviceSparseImageFormatProperties)},
    // clang-format on
};

const NameOffsetEntry kDeviceOffsetTbl[] = {
    // clang-format off
    {"vkAllocDescriptorSets", offsetof(DeviceVtbl, AllocDescriptorSets)},
    {"vkAllocMemory", offsetof(DeviceVtbl, AllocMemory)},
    {"vkBeginCommandBuffer", offsetof(DeviceVtbl, BeginCommandBuffer)},
    {"vkBindBufferMemory", offsetof(DeviceVtbl, BindBufferMemory)},
    {"vkBindImageMemory", offsetof(DeviceVtbl, BindImageMemory)},
    {"vkCmdBeginQuery", offsetof(DeviceVtbl, CmdBeginQuery)},
    {"vkCmdBeginRenderPass", offsetof(DeviceVtbl, CmdBeginRenderPass)},
    {"vkCmdBindDescriptorSets", offsetof(DeviceVtbl, CmdBindDescriptorSets)},
    {"vkCmdBindIndexBuffer", offsetof(DeviceVtbl, CmdBindIndexBuffer)},
    {"vkCmdBindPipeline", offsetof(DeviceVtbl, CmdBindPipeline)},
    {"vkCmdBindVertexBuffers", offsetof(DeviceVtbl, CmdBindVertexBuffers)},
    {"vkCmdBlitImage", offsetof(DeviceVtbl, CmdBlitImage)},
    {"vkCmdClearColorAttachment", offsetof(DeviceVtbl, CmdClearColorAttachment)},
    {"vkCmdClearColorImage", offsetof(DeviceVtbl, CmdClearColorImage)},
    {"vkCmdClearDepthStencilAttachment", offsetof(DeviceVtbl, CmdClearDepthStencilAttachment)},
    {"vkCmdClearDepthStencilImage", offsetof(DeviceVtbl, CmdClearDepthStencilImage)},
    {"vkCmdCopyBuffer", offsetof(DeviceVtbl, CmdCopyBuffer)},
    {"vkCmdCopyBufferToImage", offsetof(DeviceVtbl, CmdCopyBufferToImage)},
    {"vkCmdCopyImage", offsetof(DeviceVtbl, CmdCopyImage)},
    {"vkCmdCopyImageToBuffer", offsetof(DeviceVtbl, CmdCopyImageToBuffer)},
    {"vkCmdCopyQueryPoolResults", offsetof(DeviceVtbl, CmdCopyQueryPoolResults)},
    {"vkCmdDispatch", offsetof(DeviceVtbl, CmdDispatch)},
    {"vkCmdDispatchIndirect", offsetof(DeviceVtbl, CmdDispatchIndirect)},
    {"vkCmdDraw", offsetof(DeviceVtbl, CmdDraw)},
    {"vkCmdDrawIndexed", offsetof(DeviceVtbl, CmdDrawIndexed)},
    {"vkCmdDrawIndexedIndirect", offsetof(DeviceVtbl, CmdDrawIndexedIndirect)},
    {"vkCmdDrawIndirect", offsetof(DeviceVtbl, CmdDrawIndirect)},
    {"vkCmdEndQuery", offsetof(DeviceVtbl, CmdEndQuery)},
    {"vkCmdEndRenderPass", offsetof(DeviceVtbl, CmdEndRenderPass)},
    {"vkCmdExecuteCommands", offsetof(DeviceVtbl, CmdExecuteCommands)},
    {"vkCmdFillBuffer", offsetof(DeviceVtbl, CmdFillBuffer)},
    {"vkCmdNextSubpass", offsetof(DeviceVtbl, CmdNextSubpass)},
    {"vkCmdPipelineBarrier", offsetof(DeviceVtbl, CmdPipelineBarrier)},
    {"vkCmdPushConstants", offsetof(DeviceVtbl, CmdPushConstants)},
    {"vkCmdResetEvent", offsetof(DeviceVtbl, CmdResetEvent)},
    {"vkCmdResetQueryPool", offsetof(DeviceVtbl, CmdResetQueryPool)},
    {"vkCmdResolveImage", offsetof(DeviceVtbl, CmdResolveImage)},
    {"vkCmdSetBlendConstants", offsetof(DeviceVtbl, CmdSetBlendConstants)},
    {"vkCmdSetDepthBias", offsetof(DeviceVtbl, CmdSetDepthBias)},
    {"vkCmdSetDepthBounds", offsetof(DeviceVtbl, CmdSetDepthBounds)},
    {"vkCmdSetEvent", offsetof(DeviceVtbl, CmdSetEvent)},
    {"vkCmdSetLineWidth", offsetof(DeviceVtbl, CmdSetLineWidth)},
    {"vkCmdSetScissor", offsetof(DeviceVtbl, CmdSetScissor)},
    {"vkCmdSetStencilCompareMask", offsetof(DeviceVtbl, CmdSetStencilCompareMask)},
    {"vkCmdSetStencilReference", offsetof(DeviceVtbl, CmdSetStencilReference)},
    {"vkCmdSetStencilWriteMask", offsetof(DeviceVtbl, CmdSetStencilWriteMask)},
    {"vkCmdSetViewport", offsetof(DeviceVtbl, CmdSetViewport)},
    {"vkCmdUpdateBuffer", offsetof(DeviceVtbl, CmdUpdateBuffer)},
    {"vkCmdWaitEvents", offsetof(DeviceVtbl, CmdWaitEvents)},
    {"vkCmdWriteTimestamp", offsetof(DeviceVtbl, CmdWriteTimestamp)},
    {"vkCreateBuffer", offsetof(DeviceVtbl, CreateBuffer)},
    {"vkCreateBufferView", offsetof(DeviceVtbl, CreateBufferView)},
    {"vkCreateCommandBuffer", offsetof(DeviceVtbl, CreateCommandBuffer)},
    {"vkCreateCommandPool", offsetof(DeviceVtbl, CreateCommandPool)},
    {"vkCreateComputePipelines", offsetof(DeviceVtbl, CreateComputePipelines)},
    {"vkCreateDescriptorPool", offsetof(DeviceVtbl, CreateDescriptorPool)},
    {"vkCreateDescriptorSetLayout", offsetof(DeviceVtbl, CreateDescriptorSetLayout)},
    {"vkCreateEvent", offsetof(DeviceVtbl, CreateEvent)},
    {"vkCreateFence", offsetof(DeviceVtbl, CreateFence)},
    {"vkCreateFramebuffer", offsetof(DeviceVtbl, CreateFramebuffer)},
    {"vkCreateGraphicsPipelines", offsetof(DeviceVtbl, CreateGraphicsPipelines)},
    {"vkCreateImage", offsetof(DeviceVtbl, CreateImage)},
    {"vkCreateImageView", offsetof(DeviceVtbl, CreateImageView)},
    {"vkCreatePipelineCache", offsetof(DeviceVtbl, CreatePipelineCache)},
    {"vkCreatePipelineLayout", offsetof(DeviceVtbl, CreatePipelineLayout)},
    {"vkCreateQueryPool", offsetof(DeviceVtbl, CreateQueryPool)},
    {"vkCreateRenderPass", offsetof(DeviceVtbl, CreateRenderPass)},
    {"vkCreateSampler", offsetof(DeviceVtbl, CreateSampler)},
    {"vkCreateSemaphore", offsetof(DeviceVtbl, CreateSemaphore)},
    {"vkCreateShader", offsetof(DeviceVtbl, CreateShader)},
    {"vkCreateShaderModule", offsetof(DeviceVtbl, CreateShaderModule)},
    {"vkDestroyBuffer", offsetof(DeviceVtbl, DestroyBuffer)},
    {"vkDestroyBufferView", offsetof(DeviceVtbl, DestroyBufferView)},
    {"vkDestroyCommandBuffer", offsetof(DeviceVtbl, DestroyCommandBuffer)},
    {"vkDestroyCommandPool", offsetof(DeviceVtbl, DestroyCommandPool)},
    {"vkDestroyDescriptorPool", offsetof(DeviceVtbl, DestroyDescriptorPool)},
    {"vkDestroyDescriptorSetLayout", offsetof(DeviceVtbl, DestroyDescriptorSetLayout)},
    {"vkDestroyDevice", offsetof(DeviceVtbl, DestroyDevice)},
    {"vkDestroyEvent", offsetof(DeviceVtbl, DestroyEvent)},
    {"vkDestroyFence", offsetof(DeviceVtbl, DestroyFence)},
    {"vkDestroyFramebuffer", offsetof(DeviceVtbl, DestroyFramebuffer)},
    {"vkDestroyImage", offsetof(DeviceVtbl, DestroyImage)},
    {"vkDestroyImageView", offsetof(DeviceVtbl, DestroyImageView)},
    {"vkDestroyPipeline", offsetof(DeviceVtbl, DestroyPipeline)},
    {"vkDestroyPipelineCache", offsetof(DeviceVtbl, DestroyPipelineCache)},
    {"vkDestroyPipelineLayout", offsetof(DeviceVtbl, DestroyPipelineLayout)},
    {"vkDestroyQueryPool", offsetof(DeviceVtbl, DestroyQueryPool)},
    {"vkDestroyRenderPass", offsetof(DeviceVtbl, DestroyRenderPass)},
    {"vkDestroySampler", offsetof(DeviceVtbl, DestroySampler)},
    {"vkDestroySemaphore", offsetof(DeviceVtbl, DestroySemaphore)},
    {"vkDestroyShader", offsetof(DeviceVtbl, DestroyShader)},
    {"vkDestroyShaderModule", offsetof(DeviceVtbl, DestroyShaderModule)},
    {"vkDeviceWaitIdle", offsetof(DeviceVtbl, DeviceWaitIdle)},
    {"vkEndCommandBuffer", offsetof(DeviceVtbl, EndCommandBuffer)},
    {"vkFlushMappedMemoryRanges", offsetof(DeviceVtbl, FlushMappedMemoryRanges)},
    {"vkFreeDescriptorSets", offsetof(DeviceVtbl, FreeDescriptorSets)},
    {"vkFreeMemory", offsetof(DeviceVtbl, FreeMemory)},
    {"vkGetBufferMemoryRequirements", offsetof(DeviceVtbl, GetBufferMemoryRequirements)},
    {"vkGetDeviceMemoryCommitment", offsetof(DeviceVtbl, GetDeviceMemoryCommitment)},
    {"vkGetDeviceProcAddr", offsetof(DeviceVtbl, GetDeviceProcAddr)},
    {"vkGetDeviceQueue", offsetof(DeviceVtbl, GetDeviceQueue)},
    {"vkGetEventStatus", offsetof(DeviceVtbl, GetEventStatus)},
    {"vkGetFenceStatus", offsetof(DeviceVtbl, GetFenceStatus)},
    {"vkGetImageMemoryRequirements", offsetof(DeviceVtbl, GetImageMemoryRequirements)},
    {"vkGetImageSparseMemoryRequirements", offsetof(DeviceVtbl, GetImageSparseMemoryRequirements)},
    {"vkGetImageSubresourceLayout", offsetof(DeviceVtbl, GetImageSubresourceLayout)},
    {"vkGetPipelineCacheData", offsetof(DeviceVtbl, GetPipelineCacheData)},
    {"vkGetPipelineCacheSize", offsetof(DeviceVtbl, GetPipelineCacheSize)},
    {"vkGetQueryPoolResults", offsetof(DeviceVtbl, GetQueryPoolResults)},
    {"vkGetRenderAreaGranularity", offsetof(DeviceVtbl, GetRenderAreaGranularity)},
    {"vkInvalidateMappedMemoryRanges", offsetof(DeviceVtbl, InvalidateMappedMemoryRanges)},
    {"vkMapMemory", offsetof(DeviceVtbl, MapMemory)},
    {"vkMergePipelineCaches", offsetof(DeviceVtbl, MergePipelineCaches)},
    {"vkQueueBindSparseBufferMemory", offsetof(DeviceVtbl, QueueBindSparseBufferMemory)},
    {"vkQueueBindSparseImageMemory", offsetof(DeviceVtbl, QueueBindSparseImageMemory)},
    {"vkQueueBindSparseImageOpaqueMemory", offsetof(DeviceVtbl, QueueBindSparseImageOpaqueMemory)},
    {"vkQueueSignalSemaphore", offsetof(DeviceVtbl, QueueSignalSemaphore)},
    {"vkQueueSubmit", offsetof(DeviceVtbl, QueueSubmit)},
    {"vkQueueWaitIdle", offsetof(DeviceVtbl, QueueWaitIdle)},
    {"vkQueueWaitSemaphore", offsetof(DeviceVtbl, QueueWaitSemaphore)},
    {"vkResetCommandBuffer", offsetof(DeviceVtbl, ResetCommandBuffer)},
    {"vkResetCommandPool", offsetof(DeviceVtbl, ResetCommandPool)},
    {"vkResetDescriptorPool", offsetof(DeviceVtbl, ResetDescriptorPool)},
    {"vkResetEvent", offsetof(DeviceVtbl, ResetEvent)},
    {"vkResetFences", offsetof(DeviceVtbl, ResetFences)},
    {"vkSetEvent", offsetof(DeviceVtbl, SetEvent)},
    {"vkUnmapMemory", offsetof(DeviceVtbl, UnmapMemory)},
    {"vkUpdateDescriptorSets", offsetof(DeviceVtbl, UpdateDescriptorSets)},
    {"vkWaitForFences", offsetof(DeviceVtbl, WaitForFences)},
    // clang-format on
};

}  // namespace

namespace vulkan {

PFN_vkVoidFunction GetGlobalInstanceProcAddr(const char* name) {
    const NameProcEntry* entry = FindProcEntry(kInstanceProcTbl, name);
    if (entry)
        return entry->proc;
    // vkGetDeviceProcAddr must be available at the global/instance level for
    // bootstrapping
    if (strcmp(name, "vkGetDeviceProcAddr") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    // special-case extension functions until they can be auto-generated
    if (strcmp(name, "vkGetPhysicalDeviceSurfaceSupportKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(
            vkGetPhysicalDeviceSurfaceSupportKHR);
    return nullptr;
}

PFN_vkVoidFunction GetGlobalDeviceProcAddr(const char* name) {
    const NameProcEntry* entry = FindProcEntry(kDeviceProcTbl, name);
    if (entry)
        return entry->proc;
    // special-case extension functions until they can be auto-generated
    if (strcmp(name, "vkGetSurfacePropertiesKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetSurfacePropertiesKHR);
    if (strcmp(name, "vkGetSurfaceFormatsKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetSurfaceFormatsKHR);
    if (strcmp(name, "vkGetSurfacePresentModesKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(
            vkGetSurfacePresentModesKHR);
    if (strcmp(name, "vkCreateSwapchainKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR);
    if (strcmp(name, "vkDestroySwapchainKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR);
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetSwapchainImagesKHR);
    if (strcmp(name, "vkAcquireNextImageKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkAcquireNextImageKHR);
    if (strcmp(name, "vkQueuePresentKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR);
    return nullptr;
}

PFN_vkVoidFunction GetSpecificInstanceProcAddr(const InstanceVtbl* vtbl,
                                               const char* name) {
    size_t offset;
    const NameOffsetEntry* entry = FindProcEntry(kInstanceOffsetTbl, name);
    if (entry)
        offset = entry->offset;
    else if (strcmp(name, "vkGetPhysicalDeviceSurfaceSupportKHR") == 0)
        offset = offsetof(InstanceVtbl, GetPhysicalDeviceSurfaceSupportKHR);
    else
        return nullptr;
    const unsigned char* base = reinterpret_cast<const unsigned char*>(vtbl);
    return *reinterpret_cast<PFN_vkVoidFunction*>(
        const_cast<unsigned char*>(base) + entry->offset);
}

PFN_vkVoidFunction GetSpecificDeviceProcAddr(const DeviceVtbl* vtbl,
                                             const char* name) {
    size_t offset;
    const NameOffsetEntry* entry = FindProcEntry(kDeviceOffsetTbl, name);
    if (entry)
        offset = entry->offset;
    else if (strcmp(name, "vkGetSurfacePropertiesKHR") == 0)
        offset = offsetof(DeviceVtbl, GetSurfacePropertiesKHR);
    else if (strcmp(name, "vkGetSurfaceFormatsKHR") == 0)
        offset = offsetof(DeviceVtbl, GetSurfaceFormatsKHR);
    else if (strcmp(name, "vkGetSurfacePresentModesKHR") == 0)
        offset = offsetof(DeviceVtbl, GetSurfacePresentModesKHR);
    else if (strcmp(name, "vkCreateSwapchainKHR") == 0)
        offset = offsetof(DeviceVtbl, CreateSwapchainKHR);
    else if (strcmp(name, "vkDestroySwapchainKHR") == 0)
        offset = offsetof(DeviceVtbl, DestroySwapchainKHR);
    else if (strcmp(name, "vkGetSwapchainImagesKHR") == 0)
        offset = offsetof(DeviceVtbl, GetSwapchainImagesKHR);
    else if (strcmp(name, "vkAcquireNextImageKHR") == 0)
        offset = offsetof(DeviceVtbl, AcquireNextImageKHR);
    else if (strcmp(name, "vkQueuePresentKHR") == 0)
        offset = offsetof(DeviceVtbl, QueuePresentKHR);
    else
        return nullptr;
    const unsigned char* base = reinterpret_cast<const unsigned char*>(vtbl);
    return *reinterpret_cast<PFN_vkVoidFunction*>(
        const_cast<unsigned char*>(base) + entry->offset);
}

bool LoadInstanceVtbl(VkInstance instance,
                      VkInstance instance_next,
                      PFN_vkGetInstanceProcAddr get_proc_addr,
                      InstanceVtbl& vtbl) {
    bool success = true;
    // clang-format off
    vtbl.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_proc_addr(instance_next, "vkGetInstanceProcAddr"));
    if (UNLIKELY(!vtbl.GetInstanceProcAddr)) {
        ALOGE("missing instance proc: %s", "vkGetInstanceProcAddr");
        success = false;
    }
    vtbl.CreateInstance = reinterpret_cast<PFN_vkCreateInstance>(get_proc_addr(instance, "vkCreateInstance"));
    if (UNLIKELY(!vtbl.CreateInstance)) {
        // This is allowed to fail as the driver doesn't have to return vkCreateInstance when given an instance
    }
    vtbl.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(get_proc_addr(instance, "vkDestroyInstance"));
    if (UNLIKELY(!vtbl.DestroyInstance)) {
        ALOGE("missing instance proc: %s", "vkDestroyInstance");
        success = false;
    }
    vtbl.EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    if (UNLIKELY(!vtbl.EnumeratePhysicalDevices)) {
        ALOGE("missing instance proc: %s", "vkEnumeratePhysicalDevices");
        success = false;
    }
    vtbl.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceProperties");
        success = false;
    }
    vtbl.GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceQueueFamilyProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceQueueFamilyProperties");
        success = false;
    }
    vtbl.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceMemoryProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceMemoryProperties");
        success = false;
    }
    vtbl.GetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(get_proc_addr(instance, "vkGetPhysicalDeviceFeatures"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceFeatures)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceFeatures");
        success = false;
    }
    vtbl.GetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceFormatProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceFormatProperties");
        success = false;
    }
    vtbl.GetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceImageFormatProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceImageFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceImageFormatProperties");
        success = false;
    }
    vtbl.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(get_proc_addr(instance, "vkCreateDevice"));
    if (UNLIKELY(!vtbl.CreateDevice)) {
        ALOGE("missing instance proc: %s", "vkCreateDevice");
        success = false;
    }
    vtbl.EnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(get_proc_addr(instance, "vkEnumerateDeviceLayerProperties"));
    if (UNLIKELY(!vtbl.EnumerateDeviceLayerProperties)) {
        ALOGE("missing instance proc: %s", "vkEnumerateDeviceLayerProperties");
        success = false;
    }
    vtbl.EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(get_proc_addr(instance, "vkEnumerateDeviceExtensionProperties"));
    if (UNLIKELY(!vtbl.EnumerateDeviceExtensionProperties)) {
        ALOGE("missing instance proc: %s", "vkEnumerateDeviceExtensionProperties");
        success = false;
    }
    vtbl.GetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    if (UNLIKELY(!vtbl.GetPhysicalDeviceSparseImageFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSparseImageFormatProperties");
        success = false;
    }
    // clang-format on
    return success;
}

bool LoadDeviceVtbl(VkDevice device,
                    VkDevice device_next,
                    PFN_vkGetDeviceProcAddr get_proc_addr,
                    DeviceVtbl& vtbl) {
    bool success = true;
    // clang-format off
    vtbl.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(get_proc_addr(device_next, "vkGetDeviceProcAddr"));
    if (UNLIKELY(!vtbl.GetDeviceProcAddr)) {
        ALOGE("missing device proc: %s", "vkGetDeviceProcAddr");
        success = false;
    }
    vtbl.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(get_proc_addr(device, "vkDestroyDevice"));
    if (UNLIKELY(!vtbl.DestroyDevice)) {
        ALOGE("missing device proc: %s", "vkDestroyDevice");
        success = false;
    }
    vtbl.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(get_proc_addr(device, "vkGetDeviceQueue"));
    if (UNLIKELY(!vtbl.GetDeviceQueue)) {
        ALOGE("missing device proc: %s", "vkGetDeviceQueue");
        success = false;
    }
    vtbl.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(get_proc_addr(device, "vkQueueSubmit"));
    if (UNLIKELY(!vtbl.QueueSubmit)) {
        ALOGE("missing device proc: %s", "vkQueueSubmit");
        success = false;
    }
    vtbl.QueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(get_proc_addr(device, "vkQueueWaitIdle"));
    if (UNLIKELY(!vtbl.QueueWaitIdle)) {
        ALOGE("missing device proc: %s", "vkQueueWaitIdle");
        success = false;
    }
    vtbl.DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(get_proc_addr(device, "vkDeviceWaitIdle"));
    if (UNLIKELY(!vtbl.DeviceWaitIdle)) {
        ALOGE("missing device proc: %s", "vkDeviceWaitIdle");
        success = false;
    }
    vtbl.AllocMemory = reinterpret_cast<PFN_vkAllocMemory>(get_proc_addr(device, "vkAllocMemory"));
    if (UNLIKELY(!vtbl.AllocMemory)) {
        ALOGE("missing device proc: %s", "vkAllocMemory");
        success = false;
    }
    vtbl.FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(get_proc_addr(device, "vkFreeMemory"));
    if (UNLIKELY(!vtbl.FreeMemory)) {
        ALOGE("missing device proc: %s", "vkFreeMemory");
        success = false;
    }
    vtbl.MapMemory = reinterpret_cast<PFN_vkMapMemory>(get_proc_addr(device, "vkMapMemory"));
    if (UNLIKELY(!vtbl.MapMemory)) {
        ALOGE("missing device proc: %s", "vkMapMemory");
        success = false;
    }
    vtbl.UnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(get_proc_addr(device, "vkUnmapMemory"));
    if (UNLIKELY(!vtbl.UnmapMemory)) {
        ALOGE("missing device proc: %s", "vkUnmapMemory");
        success = false;
    }
    vtbl.FlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(get_proc_addr(device, "vkFlushMappedMemoryRanges"));
    if (UNLIKELY(!vtbl.FlushMappedMemoryRanges)) {
        ALOGE("missing device proc: %s", "vkFlushMappedMemoryRanges");
        success = false;
    }
    vtbl.InvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(get_proc_addr(device, "vkInvalidateMappedMemoryRanges"));
    if (UNLIKELY(!vtbl.InvalidateMappedMemoryRanges)) {
        ALOGE("missing device proc: %s", "vkInvalidateMappedMemoryRanges");
        success = false;
    }
    vtbl.GetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(get_proc_addr(device, "vkGetDeviceMemoryCommitment"));
    if (UNLIKELY(!vtbl.GetDeviceMemoryCommitment)) {
        ALOGE("missing device proc: %s", "vkGetDeviceMemoryCommitment");
        success = false;
    }
    vtbl.GetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(get_proc_addr(device, "vkGetBufferMemoryRequirements"));
    if (UNLIKELY(!vtbl.GetBufferMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetBufferMemoryRequirements");
        success = false;
    }
    vtbl.BindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(get_proc_addr(device, "vkBindBufferMemory"));
    if (UNLIKELY(!vtbl.BindBufferMemory)) {
        ALOGE("missing device proc: %s", "vkBindBufferMemory");
        success = false;
    }
    vtbl.GetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get_proc_addr(device, "vkGetImageMemoryRequirements"));
    if (UNLIKELY(!vtbl.GetImageMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetImageMemoryRequirements");
        success = false;
    }
    vtbl.BindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(get_proc_addr(device, "vkBindImageMemory"));
    if (UNLIKELY(!vtbl.BindImageMemory)) {
        ALOGE("missing device proc: %s", "vkBindImageMemory");
        success = false;
    }
    vtbl.GetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(get_proc_addr(device, "vkGetImageSparseMemoryRequirements"));
    if (UNLIKELY(!vtbl.GetImageSparseMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetImageSparseMemoryRequirements");
        success = false;
    }
    vtbl.QueueBindSparseBufferMemory = reinterpret_cast<PFN_vkQueueBindSparseBufferMemory>(get_proc_addr(device, "vkQueueBindSparseBufferMemory"));
    if (UNLIKELY(!vtbl.QueueBindSparseBufferMemory)) {
        ALOGE("missing device proc: %s", "vkQueueBindSparseBufferMemory");
        success = false;
    }
    vtbl.QueueBindSparseImageOpaqueMemory = reinterpret_cast<PFN_vkQueueBindSparseImageOpaqueMemory>(get_proc_addr(device, "vkQueueBindSparseImageOpaqueMemory"));
    if (UNLIKELY(!vtbl.QueueBindSparseImageOpaqueMemory)) {
        ALOGE("missing device proc: %s", "vkQueueBindSparseImageOpaqueMemory");
        success = false;
    }
    vtbl.QueueBindSparseImageMemory = reinterpret_cast<PFN_vkQueueBindSparseImageMemory>(get_proc_addr(device, "vkQueueBindSparseImageMemory"));
    if (UNLIKELY(!vtbl.QueueBindSparseImageMemory)) {
        ALOGE("missing device proc: %s", "vkQueueBindSparseImageMemory");
        success = false;
    }
    vtbl.CreateFence = reinterpret_cast<PFN_vkCreateFence>(get_proc_addr(device, "vkCreateFence"));
    if (UNLIKELY(!vtbl.CreateFence)) {
        ALOGE("missing device proc: %s", "vkCreateFence");
        success = false;
    }
    vtbl.DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(get_proc_addr(device, "vkDestroyFence"));
    if (UNLIKELY(!vtbl.DestroyFence)) {
        ALOGE("missing device proc: %s", "vkDestroyFence");
        success = false;
    }
    vtbl.ResetFences = reinterpret_cast<PFN_vkResetFences>(get_proc_addr(device, "vkResetFences"));
    if (UNLIKELY(!vtbl.ResetFences)) {
        ALOGE("missing device proc: %s", "vkResetFences");
        success = false;
    }
    vtbl.GetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(get_proc_addr(device, "vkGetFenceStatus"));
    if (UNLIKELY(!vtbl.GetFenceStatus)) {
        ALOGE("missing device proc: %s", "vkGetFenceStatus");
        success = false;
    }
    vtbl.WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(get_proc_addr(device, "vkWaitForFences"));
    if (UNLIKELY(!vtbl.WaitForFences)) {
        ALOGE("missing device proc: %s", "vkWaitForFences");
        success = false;
    }
    vtbl.CreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(get_proc_addr(device, "vkCreateSemaphore"));
    if (UNLIKELY(!vtbl.CreateSemaphore)) {
        ALOGE("missing device proc: %s", "vkCreateSemaphore");
        success = false;
    }
    vtbl.DestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(get_proc_addr(device, "vkDestroySemaphore"));
    if (UNLIKELY(!vtbl.DestroySemaphore)) {
        ALOGE("missing device proc: %s", "vkDestroySemaphore");
        success = false;
    }
    vtbl.QueueSignalSemaphore = reinterpret_cast<PFN_vkQueueSignalSemaphore>(get_proc_addr(device, "vkQueueSignalSemaphore"));
    if (UNLIKELY(!vtbl.QueueSignalSemaphore)) {
        ALOGE("missing device proc: %s", "vkQueueSignalSemaphore");
        success = false;
    }
    vtbl.QueueWaitSemaphore = reinterpret_cast<PFN_vkQueueWaitSemaphore>(get_proc_addr(device, "vkQueueWaitSemaphore"));
    if (UNLIKELY(!vtbl.QueueWaitSemaphore)) {
        ALOGE("missing device proc: %s", "vkQueueWaitSemaphore");
        success = false;
    }
    vtbl.CreateEvent = reinterpret_cast<PFN_vkCreateEvent>(get_proc_addr(device, "vkCreateEvent"));
    if (UNLIKELY(!vtbl.CreateEvent)) {
        ALOGE("missing device proc: %s", "vkCreateEvent");
        success = false;
    }
    vtbl.DestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(get_proc_addr(device, "vkDestroyEvent"));
    if (UNLIKELY(!vtbl.DestroyEvent)) {
        ALOGE("missing device proc: %s", "vkDestroyEvent");
        success = false;
    }
    vtbl.GetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(get_proc_addr(device, "vkGetEventStatus"));
    if (UNLIKELY(!vtbl.GetEventStatus)) {
        ALOGE("missing device proc: %s", "vkGetEventStatus");
        success = false;
    }
    vtbl.SetEvent = reinterpret_cast<PFN_vkSetEvent>(get_proc_addr(device, "vkSetEvent"));
    if (UNLIKELY(!vtbl.SetEvent)) {
        ALOGE("missing device proc: %s", "vkSetEvent");
        success = false;
    }
    vtbl.ResetEvent = reinterpret_cast<PFN_vkResetEvent>(get_proc_addr(device, "vkResetEvent"));
    if (UNLIKELY(!vtbl.ResetEvent)) {
        ALOGE("missing device proc: %s", "vkResetEvent");
        success = false;
    }
    vtbl.CreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(get_proc_addr(device, "vkCreateQueryPool"));
    if (UNLIKELY(!vtbl.CreateQueryPool)) {
        ALOGE("missing device proc: %s", "vkCreateQueryPool");
        success = false;
    }
    vtbl.DestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(get_proc_addr(device, "vkDestroyQueryPool"));
    if (UNLIKELY(!vtbl.DestroyQueryPool)) {
        ALOGE("missing device proc: %s", "vkDestroyQueryPool");
        success = false;
    }
    vtbl.GetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(get_proc_addr(device, "vkGetQueryPoolResults"));
    if (UNLIKELY(!vtbl.GetQueryPoolResults)) {
        ALOGE("missing device proc: %s", "vkGetQueryPoolResults");
        success = false;
    }
    vtbl.CreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(get_proc_addr(device, "vkCreateBuffer"));
    if (UNLIKELY(!vtbl.CreateBuffer)) {
        ALOGE("missing device proc: %s", "vkCreateBuffer");
        success = false;
    }
    vtbl.DestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(get_proc_addr(device, "vkDestroyBuffer"));
    if (UNLIKELY(!vtbl.DestroyBuffer)) {
        ALOGE("missing device proc: %s", "vkDestroyBuffer");
        success = false;
    }
    vtbl.CreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(get_proc_addr(device, "vkCreateBufferView"));
    if (UNLIKELY(!vtbl.CreateBufferView)) {
        ALOGE("missing device proc: %s", "vkCreateBufferView");
        success = false;
    }
    vtbl.DestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(get_proc_addr(device, "vkDestroyBufferView"));
    if (UNLIKELY(!vtbl.DestroyBufferView)) {
        ALOGE("missing device proc: %s", "vkDestroyBufferView");
        success = false;
    }
    vtbl.CreateImage = reinterpret_cast<PFN_vkCreateImage>(get_proc_addr(device, "vkCreateImage"));
    if (UNLIKELY(!vtbl.CreateImage)) {
        ALOGE("missing device proc: %s", "vkCreateImage");
        success = false;
    }
    vtbl.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(get_proc_addr(device, "vkDestroyImage"));
    if (UNLIKELY(!vtbl.DestroyImage)) {
        ALOGE("missing device proc: %s", "vkDestroyImage");
        success = false;
    }
    vtbl.GetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(get_proc_addr(device, "vkGetImageSubresourceLayout"));
    if (UNLIKELY(!vtbl.GetImageSubresourceLayout)) {
        ALOGE("missing device proc: %s", "vkGetImageSubresourceLayout");
        success = false;
    }
    vtbl.CreateImageView = reinterpret_cast<PFN_vkCreateImageView>(get_proc_addr(device, "vkCreateImageView"));
    if (UNLIKELY(!vtbl.CreateImageView)) {
        ALOGE("missing device proc: %s", "vkCreateImageView");
        success = false;
    }
    vtbl.DestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(get_proc_addr(device, "vkDestroyImageView"));
    if (UNLIKELY(!vtbl.DestroyImageView)) {
        ALOGE("missing device proc: %s", "vkDestroyImageView");
        success = false;
    }
    vtbl.CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(get_proc_addr(device, "vkCreateShaderModule"));
    if (UNLIKELY(!vtbl.CreateShaderModule)) {
        ALOGE("missing device proc: %s", "vkCreateShaderModule");
        success = false;
    }
    vtbl.DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(get_proc_addr(device, "vkDestroyShaderModule"));
    if (UNLIKELY(!vtbl.DestroyShaderModule)) {
        ALOGE("missing device proc: %s", "vkDestroyShaderModule");
        success = false;
    }
    vtbl.CreateShader = reinterpret_cast<PFN_vkCreateShader>(get_proc_addr(device, "vkCreateShader"));
    if (UNLIKELY(!vtbl.CreateShader)) {
        ALOGE("missing device proc: %s", "vkCreateShader");
        success = false;
    }
    vtbl.DestroyShader = reinterpret_cast<PFN_vkDestroyShader>(get_proc_addr(device, "vkDestroyShader"));
    if (UNLIKELY(!vtbl.DestroyShader)) {
        ALOGE("missing device proc: %s", "vkDestroyShader");
        success = false;
    }
    vtbl.CreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(get_proc_addr(device, "vkCreatePipelineCache"));
    if (UNLIKELY(!vtbl.CreatePipelineCache)) {
        ALOGE("missing device proc: %s", "vkCreatePipelineCache");
        success = false;
    }
    vtbl.DestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(get_proc_addr(device, "vkDestroyPipelineCache"));
    if (UNLIKELY(!vtbl.DestroyPipelineCache)) {
        ALOGE("missing device proc: %s", "vkDestroyPipelineCache");
        success = false;
    }
    vtbl.GetPipelineCacheSize = reinterpret_cast<PFN_vkGetPipelineCacheSize>(get_proc_addr(device, "vkGetPipelineCacheSize"));
    if (UNLIKELY(!vtbl.GetPipelineCacheSize)) {
        ALOGE("missing device proc: %s", "vkGetPipelineCacheSize");
        success = false;
    }
    vtbl.GetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(get_proc_addr(device, "vkGetPipelineCacheData"));
    if (UNLIKELY(!vtbl.GetPipelineCacheData)) {
        ALOGE("missing device proc: %s", "vkGetPipelineCacheData");
        success = false;
    }
    vtbl.MergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(get_proc_addr(device, "vkMergePipelineCaches"));
    if (UNLIKELY(!vtbl.MergePipelineCaches)) {
        ALOGE("missing device proc: %s", "vkMergePipelineCaches");
        success = false;
    }
    vtbl.CreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get_proc_addr(device, "vkCreateGraphicsPipelines"));
    if (UNLIKELY(!vtbl.CreateGraphicsPipelines)) {
        ALOGE("missing device proc: %s", "vkCreateGraphicsPipelines");
        success = false;
    }
    vtbl.CreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(get_proc_addr(device, "vkCreateComputePipelines"));
    if (UNLIKELY(!vtbl.CreateComputePipelines)) {
        ALOGE("missing device proc: %s", "vkCreateComputePipelines");
        success = false;
    }
    vtbl.DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(get_proc_addr(device, "vkDestroyPipeline"));
    if (UNLIKELY(!vtbl.DestroyPipeline)) {
        ALOGE("missing device proc: %s", "vkDestroyPipeline");
        success = false;
    }
    vtbl.CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(get_proc_addr(device, "vkCreatePipelineLayout"));
    if (UNLIKELY(!vtbl.CreatePipelineLayout)) {
        ALOGE("missing device proc: %s", "vkCreatePipelineLayout");
        success = false;
    }
    vtbl.DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(get_proc_addr(device, "vkDestroyPipelineLayout"));
    if (UNLIKELY(!vtbl.DestroyPipelineLayout)) {
        ALOGE("missing device proc: %s", "vkDestroyPipelineLayout");
        success = false;
    }
    vtbl.CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(get_proc_addr(device, "vkCreateSampler"));
    if (UNLIKELY(!vtbl.CreateSampler)) {
        ALOGE("missing device proc: %s", "vkCreateSampler");
        success = false;
    }
    vtbl.DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(get_proc_addr(device, "vkDestroySampler"));
    if (UNLIKELY(!vtbl.DestroySampler)) {
        ALOGE("missing device proc: %s", "vkDestroySampler");
        success = false;
    }
    vtbl.CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(get_proc_addr(device, "vkCreateDescriptorSetLayout"));
    if (UNLIKELY(!vtbl.CreateDescriptorSetLayout)) {
        ALOGE("missing device proc: %s", "vkCreateDescriptorSetLayout");
        success = false;
    }
    vtbl.DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(get_proc_addr(device, "vkDestroyDescriptorSetLayout"));
    if (UNLIKELY(!vtbl.DestroyDescriptorSetLayout)) {
        ALOGE("missing device proc: %s", "vkDestroyDescriptorSetLayout");
        success = false;
    }
    vtbl.CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(get_proc_addr(device, "vkCreateDescriptorPool"));
    if (UNLIKELY(!vtbl.CreateDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkCreateDescriptorPool");
        success = false;
    }
    vtbl.DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_proc_addr(device, "vkDestroyDescriptorPool"));
    if (UNLIKELY(!vtbl.DestroyDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkDestroyDescriptorPool");
        success = false;
    }
    vtbl.ResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(get_proc_addr(device, "vkResetDescriptorPool"));
    if (UNLIKELY(!vtbl.ResetDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkResetDescriptorPool");
        success = false;
    }
    vtbl.AllocDescriptorSets = reinterpret_cast<PFN_vkAllocDescriptorSets>(get_proc_addr(device, "vkAllocDescriptorSets"));
    if (UNLIKELY(!vtbl.AllocDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkAllocDescriptorSets");
        success = false;
    }
    vtbl.FreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(get_proc_addr(device, "vkFreeDescriptorSets"));
    if (UNLIKELY(!vtbl.FreeDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkFreeDescriptorSets");
        success = false;
    }
    vtbl.UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(get_proc_addr(device, "vkUpdateDescriptorSets"));
    if (UNLIKELY(!vtbl.UpdateDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkUpdateDescriptorSets");
        success = false;
    }
    vtbl.CreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(get_proc_addr(device, "vkCreateFramebuffer"));
    if (UNLIKELY(!vtbl.CreateFramebuffer)) {
        ALOGE("missing device proc: %s", "vkCreateFramebuffer");
        success = false;
    }
    vtbl.DestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(get_proc_addr(device, "vkDestroyFramebuffer"));
    if (UNLIKELY(!vtbl.DestroyFramebuffer)) {
        ALOGE("missing device proc: %s", "vkDestroyFramebuffer");
        success = false;
    }
    vtbl.CreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(get_proc_addr(device, "vkCreateRenderPass"));
    if (UNLIKELY(!vtbl.CreateRenderPass)) {
        ALOGE("missing device proc: %s", "vkCreateRenderPass");
        success = false;
    }
    vtbl.DestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(get_proc_addr(device, "vkDestroyRenderPass"));
    if (UNLIKELY(!vtbl.DestroyRenderPass)) {
        ALOGE("missing device proc: %s", "vkDestroyRenderPass");
        success = false;
    }
    vtbl.GetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(get_proc_addr(device, "vkGetRenderAreaGranularity"));
    if (UNLIKELY(!vtbl.GetRenderAreaGranularity)) {
        ALOGE("missing device proc: %s", "vkGetRenderAreaGranularity");
        success = false;
    }
    vtbl.CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(get_proc_addr(device, "vkCreateCommandPool"));
    if (UNLIKELY(!vtbl.CreateCommandPool)) {
        ALOGE("missing device proc: %s", "vkCreateCommandPool");
        success = false;
    }
    vtbl.DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(get_proc_addr(device, "vkDestroyCommandPool"));
    if (UNLIKELY(!vtbl.DestroyCommandPool)) {
        ALOGE("missing device proc: %s", "vkDestroyCommandPool");
        success = false;
    }
    vtbl.ResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(get_proc_addr(device, "vkResetCommandPool"));
    if (UNLIKELY(!vtbl.ResetCommandPool)) {
        ALOGE("missing device proc: %s", "vkResetCommandPool");
        success = false;
    }
    vtbl.CreateCommandBuffer = reinterpret_cast<PFN_vkCreateCommandBuffer>(get_proc_addr(device, "vkCreateCommandBuffer"));
    if (UNLIKELY(!vtbl.CreateCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkCreateCommandBuffer");
        success = false;
    }
    vtbl.DestroyCommandBuffer = reinterpret_cast<PFN_vkDestroyCommandBuffer>(get_proc_addr(device, "vkDestroyCommandBuffer"));
    if (UNLIKELY(!vtbl.DestroyCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkDestroyCommandBuffer");
        success = false;
    }
    vtbl.BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(get_proc_addr(device, "vkBeginCommandBuffer"));
    if (UNLIKELY(!vtbl.BeginCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkBeginCommandBuffer");
        success = false;
    }
    vtbl.EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(get_proc_addr(device, "vkEndCommandBuffer"));
    if (UNLIKELY(!vtbl.EndCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkEndCommandBuffer");
        success = false;
    }
    vtbl.ResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(get_proc_addr(device, "vkResetCommandBuffer"));
    if (UNLIKELY(!vtbl.ResetCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkResetCommandBuffer");
        success = false;
    }
    vtbl.CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(get_proc_addr(device, "vkCmdBindPipeline"));
    if (UNLIKELY(!vtbl.CmdBindPipeline)) {
        ALOGE("missing device proc: %s", "vkCmdBindPipeline");
        success = false;
    }
    vtbl.CmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(get_proc_addr(device, "vkCmdSetViewport"));
    if (UNLIKELY(!vtbl.CmdSetViewport)) {
        ALOGE("missing device proc: %s", "vkCmdSetViewport");
        success = false;
    }
    vtbl.CmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(get_proc_addr(device, "vkCmdSetScissor"));
    if (UNLIKELY(!vtbl.CmdSetScissor)) {
        ALOGE("missing device proc: %s", "vkCmdSetScissor");
        success = false;
    }
    vtbl.CmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(get_proc_addr(device, "vkCmdSetLineWidth"));
    if (UNLIKELY(!vtbl.CmdSetLineWidth)) {
        ALOGE("missing device proc: %s", "vkCmdSetLineWidth");
        success = false;
    }
    vtbl.CmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(get_proc_addr(device, "vkCmdSetDepthBias"));
    if (UNLIKELY(!vtbl.CmdSetDepthBias)) {
        ALOGE("missing device proc: %s", "vkCmdSetDepthBias");
        success = false;
    }
    vtbl.CmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(get_proc_addr(device, "vkCmdSetBlendConstants"));
    if (UNLIKELY(!vtbl.CmdSetBlendConstants)) {
        ALOGE("missing device proc: %s", "vkCmdSetBlendConstants");
        success = false;
    }
    vtbl.CmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(get_proc_addr(device, "vkCmdSetDepthBounds"));
    if (UNLIKELY(!vtbl.CmdSetDepthBounds)) {
        ALOGE("missing device proc: %s", "vkCmdSetDepthBounds");
        success = false;
    }
    vtbl.CmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(get_proc_addr(device, "vkCmdSetStencilCompareMask"));
    if (UNLIKELY(!vtbl.CmdSetStencilCompareMask)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilCompareMask");
        success = false;
    }
    vtbl.CmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(get_proc_addr(device, "vkCmdSetStencilWriteMask"));
    if (UNLIKELY(!vtbl.CmdSetStencilWriteMask)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilWriteMask");
        success = false;
    }
    vtbl.CmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(get_proc_addr(device, "vkCmdSetStencilReference"));
    if (UNLIKELY(!vtbl.CmdSetStencilReference)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilReference");
        success = false;
    }
    vtbl.CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(get_proc_addr(device, "vkCmdBindDescriptorSets"));
    if (UNLIKELY(!vtbl.CmdBindDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkCmdBindDescriptorSets");
        success = false;
    }
    vtbl.CmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(get_proc_addr(device, "vkCmdBindIndexBuffer"));
    if (UNLIKELY(!vtbl.CmdBindIndexBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdBindIndexBuffer");
        success = false;
    }
    vtbl.CmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(get_proc_addr(device, "vkCmdBindVertexBuffers"));
    if (UNLIKELY(!vtbl.CmdBindVertexBuffers)) {
        ALOGE("missing device proc: %s", "vkCmdBindVertexBuffers");
        success = false;
    }
    vtbl.CmdDraw = reinterpret_cast<PFN_vkCmdDraw>(get_proc_addr(device, "vkCmdDraw"));
    if (UNLIKELY(!vtbl.CmdDraw)) {
        ALOGE("missing device proc: %s", "vkCmdDraw");
        success = false;
    }
    vtbl.CmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(get_proc_addr(device, "vkCmdDrawIndexed"));
    if (UNLIKELY(!vtbl.CmdDrawIndexed)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndexed");
        success = false;
    }
    vtbl.CmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(get_proc_addr(device, "vkCmdDrawIndirect"));
    if (UNLIKELY(!vtbl.CmdDrawIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndirect");
        success = false;
    }
    vtbl.CmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(get_proc_addr(device, "vkCmdDrawIndexedIndirect"));
    if (UNLIKELY(!vtbl.CmdDrawIndexedIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndexedIndirect");
        success = false;
    }
    vtbl.CmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(get_proc_addr(device, "vkCmdDispatch"));
    if (UNLIKELY(!vtbl.CmdDispatch)) {
        ALOGE("missing device proc: %s", "vkCmdDispatch");
        success = false;
    }
    vtbl.CmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(get_proc_addr(device, "vkCmdDispatchIndirect"));
    if (UNLIKELY(!vtbl.CmdDispatchIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDispatchIndirect");
        success = false;
    }
    vtbl.CmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(get_proc_addr(device, "vkCmdCopyBuffer"));
    if (UNLIKELY(!vtbl.CmdCopyBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdCopyBuffer");
        success = false;
    }
    vtbl.CmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(get_proc_addr(device, "vkCmdCopyImage"));
    if (UNLIKELY(!vtbl.CmdCopyImage)) {
        ALOGE("missing device proc: %s", "vkCmdCopyImage");
        success = false;
    }
    vtbl.CmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(get_proc_addr(device, "vkCmdBlitImage"));
    if (UNLIKELY(!vtbl.CmdBlitImage)) {
        ALOGE("missing device proc: %s", "vkCmdBlitImage");
        success = false;
    }
    vtbl.CmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(get_proc_addr(device, "vkCmdCopyBufferToImage"));
    if (UNLIKELY(!vtbl.CmdCopyBufferToImage)) {
        ALOGE("missing device proc: %s", "vkCmdCopyBufferToImage");
        success = false;
    }
    vtbl.CmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(get_proc_addr(device, "vkCmdCopyImageToBuffer"));
    if (UNLIKELY(!vtbl.CmdCopyImageToBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdCopyImageToBuffer");
        success = false;
    }
    vtbl.CmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(get_proc_addr(device, "vkCmdUpdateBuffer"));
    if (UNLIKELY(!vtbl.CmdUpdateBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdUpdateBuffer");
        success = false;
    }
    vtbl.CmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(get_proc_addr(device, "vkCmdFillBuffer"));
    if (UNLIKELY(!vtbl.CmdFillBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdFillBuffer");
        success = false;
    }
    vtbl.CmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(get_proc_addr(device, "vkCmdClearColorImage"));
    if (UNLIKELY(!vtbl.CmdClearColorImage)) {
        ALOGE("missing device proc: %s", "vkCmdClearColorImage");
        success = false;
    }
    vtbl.CmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(get_proc_addr(device, "vkCmdClearDepthStencilImage"));
    if (UNLIKELY(!vtbl.CmdClearDepthStencilImage)) {
        ALOGE("missing device proc: %s", "vkCmdClearDepthStencilImage");
        success = false;
    }
    vtbl.CmdClearColorAttachment = reinterpret_cast<PFN_vkCmdClearColorAttachment>(get_proc_addr(device, "vkCmdClearColorAttachment"));
    if (UNLIKELY(!vtbl.CmdClearColorAttachment)) {
        ALOGE("missing device proc: %s", "vkCmdClearColorAttachment");
        success = false;
    }
    vtbl.CmdClearDepthStencilAttachment = reinterpret_cast<PFN_vkCmdClearDepthStencilAttachment>(get_proc_addr(device, "vkCmdClearDepthStencilAttachment"));
    if (UNLIKELY(!vtbl.CmdClearDepthStencilAttachment)) {
        ALOGE("missing device proc: %s", "vkCmdClearDepthStencilAttachment");
        success = false;
    }
    vtbl.CmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(get_proc_addr(device, "vkCmdResolveImage"));
    if (UNLIKELY(!vtbl.CmdResolveImage)) {
        ALOGE("missing device proc: %s", "vkCmdResolveImage");
        success = false;
    }
    vtbl.CmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(get_proc_addr(device, "vkCmdSetEvent"));
    if (UNLIKELY(!vtbl.CmdSetEvent)) {
        ALOGE("missing device proc: %s", "vkCmdSetEvent");
        success = false;
    }
    vtbl.CmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(get_proc_addr(device, "vkCmdResetEvent"));
    if (UNLIKELY(!vtbl.CmdResetEvent)) {
        ALOGE("missing device proc: %s", "vkCmdResetEvent");
        success = false;
    }
    vtbl.CmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(get_proc_addr(device, "vkCmdWaitEvents"));
    if (UNLIKELY(!vtbl.CmdWaitEvents)) {
        ALOGE("missing device proc: %s", "vkCmdWaitEvents");
        success = false;
    }
    vtbl.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(get_proc_addr(device, "vkCmdPipelineBarrier"));
    if (UNLIKELY(!vtbl.CmdPipelineBarrier)) {
        ALOGE("missing device proc: %s", "vkCmdPipelineBarrier");
        success = false;
    }
    vtbl.CmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(get_proc_addr(device, "vkCmdBeginQuery"));
    if (UNLIKELY(!vtbl.CmdBeginQuery)) {
        ALOGE("missing device proc: %s", "vkCmdBeginQuery");
        success = false;
    }
    vtbl.CmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(get_proc_addr(device, "vkCmdEndQuery"));
    if (UNLIKELY(!vtbl.CmdEndQuery)) {
        ALOGE("missing device proc: %s", "vkCmdEndQuery");
        success = false;
    }
    vtbl.CmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(get_proc_addr(device, "vkCmdResetQueryPool"));
    if (UNLIKELY(!vtbl.CmdResetQueryPool)) {
        ALOGE("missing device proc: %s", "vkCmdResetQueryPool");
        success = false;
    }
    vtbl.CmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(get_proc_addr(device, "vkCmdWriteTimestamp"));
    if (UNLIKELY(!vtbl.CmdWriteTimestamp)) {
        ALOGE("missing device proc: %s", "vkCmdWriteTimestamp");
        success = false;
    }
    vtbl.CmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(get_proc_addr(device, "vkCmdCopyQueryPoolResults"));
    if (UNLIKELY(!vtbl.CmdCopyQueryPoolResults)) {
        ALOGE("missing device proc: %s", "vkCmdCopyQueryPoolResults");
        success = false;
    }
    vtbl.CmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(get_proc_addr(device, "vkCmdPushConstants"));
    if (UNLIKELY(!vtbl.CmdPushConstants)) {
        ALOGE("missing device proc: %s", "vkCmdPushConstants");
        success = false;
    }
    vtbl.CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(get_proc_addr(device, "vkCmdBeginRenderPass"));
    if (UNLIKELY(!vtbl.CmdBeginRenderPass)) {
        ALOGE("missing device proc: %s", "vkCmdBeginRenderPass");
        success = false;
    }
    vtbl.CmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(get_proc_addr(device, "vkCmdNextSubpass"));
    if (UNLIKELY(!vtbl.CmdNextSubpass)) {
        ALOGE("missing device proc: %s", "vkCmdNextSubpass");
        success = false;
    }
    vtbl.CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(get_proc_addr(device, "vkCmdEndRenderPass"));
    if (UNLIKELY(!vtbl.CmdEndRenderPass)) {
        ALOGE("missing device proc: %s", "vkCmdEndRenderPass");
        success = false;
    }
    vtbl.CmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(get_proc_addr(device, "vkCmdExecuteCommands"));
    if (UNLIKELY(!vtbl.CmdExecuteCommands)) {
        ALOGE("missing device proc: %s", "vkCmdExecuteCommands");
        success = false;
    }
    vtbl.ImportNativeFenceANDROID = reinterpret_cast<PFN_vkImportNativeFenceANDROID>(get_proc_addr(device, "vkImportNativeFenceANDROID"));
    if (UNLIKELY(!vtbl.ImportNativeFenceANDROID)) {
        ALOGE("missing device proc: %s", "vkImportNativeFenceANDROID");
        success = false;
    }
    vtbl.QueueSignalNativeFenceANDROID = reinterpret_cast<PFN_vkQueueSignalNativeFenceANDROID>(get_proc_addr(device, "vkQueueSignalNativeFenceANDROID"));
    if (UNLIKELY(!vtbl.QueueSignalNativeFenceANDROID)) {
        ALOGE("missing device proc: %s", "vkQueueSignalNativeFenceANDROID");
        success = false;
    }
    // clang-format on
    return success;
}

}  // namespace vulkan
