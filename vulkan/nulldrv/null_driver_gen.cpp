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

// The reinterpret_cast<..>(static_cast<..>(..)) business is there to ensure
// that the function declaration in null_driver.h matches the function pointer
// type in vulkan.h. If we just used reinterpret_cast<>, the compiler wouldn't
// tell us if there's a mistake in null_driver.h. A better solution would be to
// generate the declarations in null_driver.h.
const NameProcEntry kInstanceProcTbl[] = {
    // clang-format off
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateDevice>(CreateDevice))},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyInstance>(DestroyInstance))},
    {"vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateDeviceExtensionProperties>(EnumerateDeviceExtensionProperties))},
    {"vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateDeviceLayerProperties>(EnumerateDeviceLayerProperties))},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumeratePhysicalDevices>(EnumeratePhysicalDevices))},
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetInstanceProcAddr>(GetInstanceProcAddr))},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceFeatures>(GetPhysicalDeviceFeatures))},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceFormatProperties>(GetPhysicalDeviceFormatProperties))},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(GetPhysicalDeviceImageFormatProperties))},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(GetPhysicalDeviceMemoryProperties))},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceProperties>(GetPhysicalDeviceProperties))},
    {"vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(GetPhysicalDeviceQueueFamilyProperties))},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(GetPhysicalDeviceSparseImageFormatProperties))},
    // clang-format on
};

const NameProcEntry kDeviceProcTbl[] = {
    // clang-format off
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkAllocateCommandBuffers>(AllocateCommandBuffers))},
    {"vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkAllocateDescriptorSets>(AllocateDescriptorSets))},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkAllocateMemory>(AllocateMemory))},
    {"vkBeginCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkBeginCommandBuffer>(BeginCommandBuffer))},
    {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkBindBufferMemory>(BindBufferMemory))},
    {"vkBindImageMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkBindImageMemory>(BindImageMemory))},
    {"vkCmdBeginQuery", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBeginQuery>(CmdBeginQuery))},
    {"vkCmdBeginRenderPass", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBeginRenderPass>(CmdBeginRenderPass))},
    {"vkCmdBindDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBindDescriptorSets>(CmdBindDescriptorSets))},
    {"vkCmdBindIndexBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBindIndexBuffer>(CmdBindIndexBuffer))},
    {"vkCmdBindPipeline", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBindPipeline>(CmdBindPipeline))},
    {"vkCmdBindVertexBuffers", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBindVertexBuffers>(CmdBindVertexBuffers))},
    {"vkCmdBlitImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdBlitImage>(CmdBlitImage))},
    {"vkCmdClearAttachments", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdClearAttachments>(CmdClearAttachments))},
    {"vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdClearColorImage>(CmdClearColorImage))},
    {"vkCmdClearDepthStencilImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdClearDepthStencilImage>(CmdClearDepthStencilImage))},
    {"vkCmdCopyBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdCopyBuffer>(CmdCopyBuffer))},
    {"vkCmdCopyBufferToImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdCopyBufferToImage>(CmdCopyBufferToImage))},
    {"vkCmdCopyImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdCopyImage>(CmdCopyImage))},
    {"vkCmdCopyImageToBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdCopyImageToBuffer>(CmdCopyImageToBuffer))},
    {"vkCmdCopyQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdCopyQueryPoolResults>(CmdCopyQueryPoolResults))},
    {"vkCmdDispatch", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDispatch>(CmdDispatch))},
    {"vkCmdDispatchIndirect", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDispatchIndirect>(CmdDispatchIndirect))},
    {"vkCmdDraw", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDraw>(CmdDraw))},
    {"vkCmdDrawIndexed", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDrawIndexed>(CmdDrawIndexed))},
    {"vkCmdDrawIndexedIndirect", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDrawIndexedIndirect>(CmdDrawIndexedIndirect))},
    {"vkCmdDrawIndirect", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdDrawIndirect>(CmdDrawIndirect))},
    {"vkCmdEndQuery", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdEndQuery>(CmdEndQuery))},
    {"vkCmdEndRenderPass", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdEndRenderPass>(CmdEndRenderPass))},
    {"vkCmdExecuteCommands", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdExecuteCommands>(CmdExecuteCommands))},
    {"vkCmdFillBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdFillBuffer>(CmdFillBuffer))},
    {"vkCmdNextSubpass", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdNextSubpass>(CmdNextSubpass))},
    {"vkCmdPipelineBarrier", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdPipelineBarrier>(CmdPipelineBarrier))},
    {"vkCmdPushConstants", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdPushConstants>(CmdPushConstants))},
    {"vkCmdResetEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdResetEvent>(CmdResetEvent))},
    {"vkCmdResetQueryPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdResetQueryPool>(CmdResetQueryPool))},
    {"vkCmdResolveImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdResolveImage>(CmdResolveImage))},
    {"vkCmdSetBlendConstants", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetBlendConstants>(CmdSetBlendConstants))},
    {"vkCmdSetDepthBias", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetDepthBias>(CmdSetDepthBias))},
    {"vkCmdSetDepthBounds", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetDepthBounds>(CmdSetDepthBounds))},
    {"vkCmdSetEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetEvent>(CmdSetEvent))},
    {"vkCmdSetLineWidth", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetLineWidth>(CmdSetLineWidth))},
    {"vkCmdSetScissor", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetScissor>(CmdSetScissor))},
    {"vkCmdSetStencilCompareMask", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetStencilCompareMask>(CmdSetStencilCompareMask))},
    {"vkCmdSetStencilReference", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetStencilReference>(CmdSetStencilReference))},
    {"vkCmdSetStencilWriteMask", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetStencilWriteMask>(CmdSetStencilWriteMask))},
    {"vkCmdSetViewport", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdSetViewport>(CmdSetViewport))},
    {"vkCmdUpdateBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdUpdateBuffer>(CmdUpdateBuffer))},
    {"vkCmdWaitEvents", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdWaitEvents>(CmdWaitEvents))},
    {"vkCmdWriteTimestamp", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCmdWriteTimestamp>(CmdWriteTimestamp))},
    {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateBuffer>(CreateBuffer))},
    {"vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateBufferView>(CreateBufferView))},
    {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateCommandPool>(CreateCommandPool))},
    {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateComputePipelines>(CreateComputePipelines))},
    {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateDescriptorPool>(CreateDescriptorPool))},
    {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateDescriptorSetLayout>(CreateDescriptorSetLayout))},
    {"vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateEvent>(CreateEvent))},
    {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateFence>(CreateFence))},
    {"vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateFramebuffer>(CreateFramebuffer))},
    {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateGraphicsPipelines>(CreateGraphicsPipelines))},
    {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateImage>(CreateImage))},
    {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateImageView>(CreateImageView))},
    {"vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreatePipelineCache>(CreatePipelineCache))},
    {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreatePipelineLayout>(CreatePipelineLayout))},
    {"vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateQueryPool>(CreateQueryPool))},
    {"vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateRenderPass>(CreateRenderPass))},
    {"vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateSampler>(CreateSampler))},
    {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateSemaphore>(CreateSemaphore))},
    {"vkCreateShader", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateShader>(CreateShader))},
    {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateShaderModule>(CreateShaderModule))},
    {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyBuffer>(DestroyBuffer))},
    {"vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyBufferView>(DestroyBufferView))},
    {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyCommandPool>(DestroyCommandPool))},
    {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyDescriptorPool>(DestroyDescriptorPool))},
    {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyDescriptorSetLayout>(DestroyDescriptorSetLayout))},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyDevice>(DestroyDevice))},
    {"vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyEvent>(DestroyEvent))},
    {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyFence>(DestroyFence))},
    {"vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyFramebuffer>(DestroyFramebuffer))},
    {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyImage>(DestroyImage))},
    {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyImageView>(DestroyImageView))},
    {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyPipeline>(DestroyPipeline))},
    {"vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyPipelineCache>(DestroyPipelineCache))},
    {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyPipelineLayout>(DestroyPipelineLayout))},
    {"vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyQueryPool>(DestroyQueryPool))},
    {"vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyRenderPass>(DestroyRenderPass))},
    {"vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroySampler>(DestroySampler))},
    {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroySemaphore>(DestroySemaphore))},
    {"vkDestroyShader", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyShader>(DestroyShader))},
    {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyShaderModule>(DestroyShaderModule))},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDeviceWaitIdle>(DeviceWaitIdle))},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEndCommandBuffer>(EndCommandBuffer))},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkFlushMappedMemoryRanges>(FlushMappedMemoryRanges))},
    {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkFreeCommandBuffers>(FreeCommandBuffers))},
    {"vkFreeDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkFreeDescriptorSets>(FreeDescriptorSets))},
    {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkFreeMemory>(FreeMemory))},
    {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetBufferMemoryRequirements>(GetBufferMemoryRequirements))},
    {"vkGetDeviceMemoryCommitment", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceMemoryCommitment>(GetDeviceMemoryCommitment))},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceProcAddr>(GetDeviceProcAddr))},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceQueue>(GetDeviceQueue))},
    {"vkGetEventStatus", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetEventStatus>(GetEventStatus))},
    {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetFenceStatus>(GetFenceStatus))},
    {"vkGetImageMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetImageMemoryRequirements>(GetImageMemoryRequirements))},
    {"vkGetImageSparseMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetImageSparseMemoryRequirements>(GetImageSparseMemoryRequirements))},
    {"vkGetImageSubresourceLayout", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetImageSubresourceLayout>(GetImageSubresourceLayout))},
    {"vkGetPipelineCacheData", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPipelineCacheData>(GetPipelineCacheData))},
    {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetQueryPoolResults>(GetQueryPoolResults))},
    {"vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetRenderAreaGranularity>(GetRenderAreaGranularity))},
    {"vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkInvalidateMappedMemoryRanges>(InvalidateMappedMemoryRanges))},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkMapMemory>(MapMemory))},
    {"vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkMergePipelineCaches>(MergePipelineCaches))},
    {"vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkQueueBindSparse>(QueueBindSparse))},
    {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkQueueSubmit>(QueueSubmit))},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkQueueWaitIdle>(QueueWaitIdle))},
    {"vkResetCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkResetCommandBuffer>(ResetCommandBuffer))},
    {"vkResetCommandPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkResetCommandPool>(ResetCommandPool))},
    {"vkResetDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkResetDescriptorPool>(ResetDescriptorPool))},
    {"vkResetEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkResetEvent>(ResetEvent))},
    {"vkResetFences", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkResetFences>(ResetFences))},
    {"vkSetEvent", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkSetEvent>(SetEvent))},
    {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkUnmapMemory>(UnmapMemory))},
    {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkUpdateDescriptorSets>(UpdateDescriptorSets))},
    {"vkWaitForFences", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkWaitForFences>(WaitForFences))},
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
