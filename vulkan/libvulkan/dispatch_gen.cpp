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

#include <log/log.h>
#include <algorithm>
#include "loader.h"

#define UNLIKELY(expr) __builtin_expect((expr), 0)

using namespace vulkan;

namespace {

struct NameProc {
    const char* name;
    PFN_vkVoidFunction proc;
};

PFN_vkVoidFunction Lookup(const char* name,
                          const NameProc* begin,
                          const NameProc* end) {
    const auto& entry = std::lower_bound(
        begin, end, name,
        [](const NameProc& e, const char* n) { return strcmp(e.name, n) < 0; });
    if (entry == end || strcmp(entry->name, name) != 0)
        return nullptr;
    return entry->proc;
}

template <size_t N>
PFN_vkVoidFunction Lookup(const char* name, const NameProc (&procs)[N]) {
    return Lookup(name, procs, procs + N);
}

const NameProc kLoaderExportProcs[] = {
    // clang-format off
    {"vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(vkAcquireNextImageKHR)},
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateCommandBuffers)},
    {"vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateDescriptorSets)},
    {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(vkAllocateMemory)},
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
    {"vkCmdClearAttachments", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearAttachments)},
    {"vkCmdClearColorImage", reinterpret_cast<PFN_vkVoidFunction>(vkCmdClearColorImage)},
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
    {"vkCreateAndroidSurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(vkCreateAndroidSurfaceKHR)},
    {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBuffer)},
    {"vkCreateBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateBufferView)},
    {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateCommandPool)},
    {"vkCreateComputePipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateComputePipelines)},
    {"vkCreateDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorPool)},
    {"vkCreateDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDescriptorSetLayout)},
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(vkCreateDevice)},
    {"vkCreateEvent", reinterpret_cast<PFN_vkVoidFunction>(vkCreateEvent)},
    {"vkCreateFence", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFence)},
    {"vkCreateFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkCreateFramebuffer)},
    {"vkCreateGraphicsPipelines", reinterpret_cast<PFN_vkVoidFunction>(vkCreateGraphicsPipelines)},
    {"vkCreateImage", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImage)},
    {"vkCreateImageView", reinterpret_cast<PFN_vkVoidFunction>(vkCreateImageView)},
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(vkCreateInstance)},
    {"vkCreatePipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineCache)},
    {"vkCreatePipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkCreatePipelineLayout)},
    {"vkCreateQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkCreateQueryPool)},
    {"vkCreateRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkCreateRenderPass)},
    {"vkCreateSampler", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSampler)},
    {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSemaphore)},
    {"vkCreateShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule)},
    {"vkCreateSwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(vkCreateSwapchainKHR)},
    {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBuffer)},
    {"vkDestroyBufferView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyBufferView)},
    {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyCommandPool)},
    {"vkDestroyDescriptorPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorPool)},
    {"vkDestroyDescriptorSetLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDescriptorSetLayout)},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyDevice)},
    {"vkDestroyEvent", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyEvent)},
    {"vkDestroyFence", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFence)},
    {"vkDestroyFramebuffer", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyFramebuffer)},
    {"vkDestroyImage", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImage)},
    {"vkDestroyImageView", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyImageView)},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyInstance)},
    {"vkDestroyPipeline", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipeline)},
    {"vkDestroyPipelineCache", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineCache)},
    {"vkDestroyPipelineLayout", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyPipelineLayout)},
    {"vkDestroyQueryPool", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyQueryPool)},
    {"vkDestroyRenderPass", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyRenderPass)},
    {"vkDestroySampler", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySampler)},
    {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySemaphore)},
    {"vkDestroyShaderModule", reinterpret_cast<PFN_vkVoidFunction>(vkDestroyShaderModule)},
    {"vkDestroySurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySurfaceKHR)},
    {"vkDestroySwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(vkDestroySwapchainKHR)},
    {"vkDeviceWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkDeviceWaitIdle)},
    {"vkEndCommandBuffer", reinterpret_cast<PFN_vkVoidFunction>(vkEndCommandBuffer)},
    {"vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateDeviceExtensionProperties)},
    {"vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateDeviceLayerProperties)},
    {"vkEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateInstanceExtensionProperties)},
    {"vkEnumerateInstanceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(vkEnumerateInstanceLayerProperties)},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(vkEnumeratePhysicalDevices)},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkFlushMappedMemoryRanges)},
    {"vkFreeCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(vkFreeCommandBuffers)},
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
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr)},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceFeatures)},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceFormatProperties)},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceImageFormatProperties)},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceMemoryProperties)},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceProperties)},
    {"vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceQueueFamilyProperties)},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSparseImageFormatProperties)},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSurfaceFormatsKHR)},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSurfacePresentModesKHR)},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetPhysicalDeviceSurfaceSupportKHR)},
    {"vkGetPipelineCacheData", reinterpret_cast<PFN_vkVoidFunction>(vkGetPipelineCacheData)},
    {"vkGetQueryPoolResults", reinterpret_cast<PFN_vkVoidFunction>(vkGetQueryPoolResults)},
    {"vkGetRenderAreaGranularity", reinterpret_cast<PFN_vkVoidFunction>(vkGetRenderAreaGranularity)},
    {"vkGetSwapchainImagesKHR", reinterpret_cast<PFN_vkVoidFunction>(vkGetSwapchainImagesKHR)},
    {"vkInvalidateMappedMemoryRanges", reinterpret_cast<PFN_vkVoidFunction>(vkInvalidateMappedMemoryRanges)},
    {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(vkMapMemory)},
    {"vkMergePipelineCaches", reinterpret_cast<PFN_vkVoidFunction>(vkMergePipelineCaches)},
    {"vkQueueBindSparse", reinterpret_cast<PFN_vkVoidFunction>(vkQueueBindSparse)},
    {"vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(vkQueuePresentKHR)},
    {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(vkQueueSubmit)},
    {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(vkQueueWaitIdle)},
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

const NameProc kLoaderGlobalProcs[] = {
    // clang-format off
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateInstance>(CreateInstance_Top))},
    {"vkEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateInstanceExtensionProperties>(EnumerateInstanceExtensionProperties_Top))},
    {"vkEnumerateInstanceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateInstanceLayerProperties>(EnumerateInstanceLayerProperties_Top))},
    // clang-format on
};

const NameProc kLoaderTopProcs[] = {
    // clang-format off
    {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkAllocateCommandBuffers>(AllocateCommandBuffers_Top))},
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateInstance>(CreateInstance_Top))},
    {"vkDestroyDevice", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyDevice>(DestroyDevice_Top))},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyInstance>(DestroyInstance_Top))},
    {"vkEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateInstanceExtensionProperties>(EnumerateInstanceExtensionProperties_Top))},
    {"vkEnumerateInstanceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateInstanceLayerProperties>(EnumerateInstanceLayerProperties_Top))},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceProcAddr>(GetDeviceProcAddr_Top))},
    {"vkGetDeviceQueue", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceQueue>(GetDeviceQueue_Top))},
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetInstanceProcAddr>(GetInstanceProcAddr_Top))},
    // clang-format on
};

const NameProc kLoaderBottomProcs[] = {
    // clang-format off
    {"vkAcquireNextImageKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkAcquireNextImageKHR>(AcquireNextImageKHR_Bottom))},
    {"vkCreateAndroidSurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateAndroidSurfaceKHR>(CreateAndroidSurfaceKHR_Bottom))},
    {"vkCreateDebugReportCallbackEXT", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateDebugReportCallbackEXT>(CreateDebugReportCallbackEXT_Bottom))},
    {"vkCreateDevice", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateDevice>(CreateDevice_Bottom))},
    {"vkCreateInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateInstance>(CreateInstance_Bottom))},
    {"vkCreateSwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkCreateSwapchainKHR>(CreateSwapchainKHR_Bottom))},
    {"vkDebugReportMessageEXT", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDebugReportMessageEXT>(DebugReportMessageEXT_Bottom))},
    {"vkDestroyDebugReportCallbackEXT", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyDebugReportCallbackEXT>(DestroyDebugReportCallbackEXT_Bottom))},
    {"vkDestroyInstance", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroyInstance>(DestroyInstance_Bottom))},
    {"vkDestroySurfaceKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroySurfaceKHR>(DestroySurfaceKHR_Bottom))},
    {"vkDestroySwapchainKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkDestroySwapchainKHR>(DestroySwapchainKHR_Bottom))},
    {"vkEnumerateDeviceExtensionProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateDeviceExtensionProperties>(EnumerateDeviceExtensionProperties_Bottom))},
    {"vkEnumerateDeviceLayerProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumerateDeviceLayerProperties>(EnumerateDeviceLayerProperties_Bottom))},
    {"vkEnumeratePhysicalDevices", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkEnumeratePhysicalDevices>(EnumeratePhysicalDevices_Bottom))},
    {"vkGetDeviceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetDeviceProcAddr>(GetDeviceProcAddr_Bottom))},
    {"vkGetInstanceProcAddr", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetInstanceProcAddr>(GetInstanceProcAddr_Bottom))},
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceFeatures>(GetPhysicalDeviceFeatures_Bottom))},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceFormatProperties>(GetPhysicalDeviceFormatProperties_Bottom))},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(GetPhysicalDeviceImageFormatProperties_Bottom))},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(GetPhysicalDeviceMemoryProperties_Bottom))},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceProperties>(GetPhysicalDeviceProperties_Bottom))},
    {"vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(GetPhysicalDeviceQueueFamilyProperties_Bottom))},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(GetPhysicalDeviceSparseImageFormatProperties_Bottom))},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(GetPhysicalDeviceSurfaceCapabilitiesKHR_Bottom))},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(GetPhysicalDeviceSurfaceFormatsKHR_Bottom))},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(GetPhysicalDeviceSurfacePresentModesKHR_Bottom))},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(GetPhysicalDeviceSurfaceSupportKHR_Bottom))},
    {"vkGetSwapchainImagesKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkGetSwapchainImagesKHR>(GetSwapchainImagesKHR_Bottom))},
    {"vkQueuePresentKHR", reinterpret_cast<PFN_vkVoidFunction>(static_cast<PFN_vkQueuePresentKHR>(QueuePresentKHR_Bottom))},
    // clang-format on
};

struct NameOffset {
    const char* name;
    size_t offset;
};

ssize_t Lookup(const char* name,
               const NameOffset* begin,
               const NameOffset* end) {
    const auto& entry = std::lower_bound(
        begin, end, name, [](const NameOffset& e, const char* n) {
            return strcmp(e.name, n) < 0;
        });
    if (entry == end || strcmp(entry->name, name) != 0)
        return -1;
    return static_cast<ssize_t>(entry->offset);
}

template <size_t N, class Table>
PFN_vkVoidFunction Lookup(const char* name,
                          const NameOffset (&offsets)[N],
                          const Table& table) {
    ssize_t offset = Lookup(name, offsets, offsets + N);
    if (offset < 0)
        return nullptr;
    uintptr_t base = reinterpret_cast<uintptr_t>(&table);
    return *reinterpret_cast<PFN_vkVoidFunction*>(base +
                                                  static_cast<size_t>(offset));
}

const NameOffset kInstanceDispatchOffsets[] = {
    // clang-format off
    {"vkCreateAndroidSurfaceKHR", offsetof(InstanceDispatchTable, CreateAndroidSurfaceKHR)},
    {"vkCreateDebugReportCallbackEXT", offsetof(InstanceDispatchTable, CreateDebugReportCallbackEXT)},
    {"vkCreateDevice", offsetof(InstanceDispatchTable, CreateDevice)},
    {"vkDebugReportMessageEXT", offsetof(InstanceDispatchTable, DebugReportMessageEXT)},
    {"vkDestroyDebugReportCallbackEXT", offsetof(InstanceDispatchTable, DestroyDebugReportCallbackEXT)},
    {"vkDestroyInstance", offsetof(InstanceDispatchTable, DestroyInstance)},
    {"vkDestroySurfaceKHR", offsetof(InstanceDispatchTable, DestroySurfaceKHR)},
    {"vkEnumerateDeviceExtensionProperties", offsetof(InstanceDispatchTable, EnumerateDeviceExtensionProperties)},
    {"vkEnumerateDeviceLayerProperties", offsetof(InstanceDispatchTable, EnumerateDeviceLayerProperties)},
    {"vkEnumeratePhysicalDevices", offsetof(InstanceDispatchTable, EnumeratePhysicalDevices)},
    {"vkGetPhysicalDeviceFeatures", offsetof(InstanceDispatchTable, GetPhysicalDeviceFeatures)},
    {"vkGetPhysicalDeviceFormatProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceFormatProperties)},
    {"vkGetPhysicalDeviceImageFormatProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceImageFormatProperties)},
    {"vkGetPhysicalDeviceMemoryProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceMemoryProperties)},
    {"vkGetPhysicalDeviceProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceProperties)},
    {"vkGetPhysicalDeviceQueueFamilyProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceQueueFamilyProperties)},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", offsetof(InstanceDispatchTable, GetPhysicalDeviceSparseImageFormatProperties)},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", offsetof(InstanceDispatchTable, GetPhysicalDeviceSurfaceCapabilitiesKHR)},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", offsetof(InstanceDispatchTable, GetPhysicalDeviceSurfaceFormatsKHR)},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", offsetof(InstanceDispatchTable, GetPhysicalDeviceSurfacePresentModesKHR)},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", offsetof(InstanceDispatchTable, GetPhysicalDeviceSurfaceSupportKHR)},
    // clang-format on
};

const NameOffset kDeviceDispatchOffsets[] = {
    // clang-format off
    {"vkAcquireNextImageKHR", offsetof(DeviceDispatchTable, AcquireNextImageKHR)},
    {"vkAllocateCommandBuffers", offsetof(DeviceDispatchTable, AllocateCommandBuffers)},
    {"vkAllocateDescriptorSets", offsetof(DeviceDispatchTable, AllocateDescriptorSets)},
    {"vkAllocateMemory", offsetof(DeviceDispatchTable, AllocateMemory)},
    {"vkBeginCommandBuffer", offsetof(DeviceDispatchTable, BeginCommandBuffer)},
    {"vkBindBufferMemory", offsetof(DeviceDispatchTable, BindBufferMemory)},
    {"vkBindImageMemory", offsetof(DeviceDispatchTable, BindImageMemory)},
    {"vkCmdBeginQuery", offsetof(DeviceDispatchTable, CmdBeginQuery)},
    {"vkCmdBeginRenderPass", offsetof(DeviceDispatchTable, CmdBeginRenderPass)},
    {"vkCmdBindDescriptorSets", offsetof(DeviceDispatchTable, CmdBindDescriptorSets)},
    {"vkCmdBindIndexBuffer", offsetof(DeviceDispatchTable, CmdBindIndexBuffer)},
    {"vkCmdBindPipeline", offsetof(DeviceDispatchTable, CmdBindPipeline)},
    {"vkCmdBindVertexBuffers", offsetof(DeviceDispatchTable, CmdBindVertexBuffers)},
    {"vkCmdBlitImage", offsetof(DeviceDispatchTable, CmdBlitImage)},
    {"vkCmdClearAttachments", offsetof(DeviceDispatchTable, CmdClearAttachments)},
    {"vkCmdClearColorImage", offsetof(DeviceDispatchTable, CmdClearColorImage)},
    {"vkCmdClearDepthStencilImage", offsetof(DeviceDispatchTable, CmdClearDepthStencilImage)},
    {"vkCmdCopyBuffer", offsetof(DeviceDispatchTable, CmdCopyBuffer)},
    {"vkCmdCopyBufferToImage", offsetof(DeviceDispatchTable, CmdCopyBufferToImage)},
    {"vkCmdCopyImage", offsetof(DeviceDispatchTable, CmdCopyImage)},
    {"vkCmdCopyImageToBuffer", offsetof(DeviceDispatchTable, CmdCopyImageToBuffer)},
    {"vkCmdCopyQueryPoolResults", offsetof(DeviceDispatchTable, CmdCopyQueryPoolResults)},
    {"vkCmdDispatch", offsetof(DeviceDispatchTable, CmdDispatch)},
    {"vkCmdDispatchIndirect", offsetof(DeviceDispatchTable, CmdDispatchIndirect)},
    {"vkCmdDraw", offsetof(DeviceDispatchTable, CmdDraw)},
    {"vkCmdDrawIndexed", offsetof(DeviceDispatchTable, CmdDrawIndexed)},
    {"vkCmdDrawIndexedIndirect", offsetof(DeviceDispatchTable, CmdDrawIndexedIndirect)},
    {"vkCmdDrawIndirect", offsetof(DeviceDispatchTable, CmdDrawIndirect)},
    {"vkCmdEndQuery", offsetof(DeviceDispatchTable, CmdEndQuery)},
    {"vkCmdEndRenderPass", offsetof(DeviceDispatchTable, CmdEndRenderPass)},
    {"vkCmdExecuteCommands", offsetof(DeviceDispatchTable, CmdExecuteCommands)},
    {"vkCmdFillBuffer", offsetof(DeviceDispatchTable, CmdFillBuffer)},
    {"vkCmdNextSubpass", offsetof(DeviceDispatchTable, CmdNextSubpass)},
    {"vkCmdPipelineBarrier", offsetof(DeviceDispatchTable, CmdPipelineBarrier)},
    {"vkCmdPushConstants", offsetof(DeviceDispatchTable, CmdPushConstants)},
    {"vkCmdResetEvent", offsetof(DeviceDispatchTable, CmdResetEvent)},
    {"vkCmdResetQueryPool", offsetof(DeviceDispatchTable, CmdResetQueryPool)},
    {"vkCmdResolveImage", offsetof(DeviceDispatchTable, CmdResolveImage)},
    {"vkCmdSetBlendConstants", offsetof(DeviceDispatchTable, CmdSetBlendConstants)},
    {"vkCmdSetDepthBias", offsetof(DeviceDispatchTable, CmdSetDepthBias)},
    {"vkCmdSetDepthBounds", offsetof(DeviceDispatchTable, CmdSetDepthBounds)},
    {"vkCmdSetEvent", offsetof(DeviceDispatchTable, CmdSetEvent)},
    {"vkCmdSetLineWidth", offsetof(DeviceDispatchTable, CmdSetLineWidth)},
    {"vkCmdSetScissor", offsetof(DeviceDispatchTable, CmdSetScissor)},
    {"vkCmdSetStencilCompareMask", offsetof(DeviceDispatchTable, CmdSetStencilCompareMask)},
    {"vkCmdSetStencilReference", offsetof(DeviceDispatchTable, CmdSetStencilReference)},
    {"vkCmdSetStencilWriteMask", offsetof(DeviceDispatchTable, CmdSetStencilWriteMask)},
    {"vkCmdSetViewport", offsetof(DeviceDispatchTable, CmdSetViewport)},
    {"vkCmdUpdateBuffer", offsetof(DeviceDispatchTable, CmdUpdateBuffer)},
    {"vkCmdWaitEvents", offsetof(DeviceDispatchTable, CmdWaitEvents)},
    {"vkCmdWriteTimestamp", offsetof(DeviceDispatchTable, CmdWriteTimestamp)},
    {"vkCreateBuffer", offsetof(DeviceDispatchTable, CreateBuffer)},
    {"vkCreateBufferView", offsetof(DeviceDispatchTable, CreateBufferView)},
    {"vkCreateCommandPool", offsetof(DeviceDispatchTable, CreateCommandPool)},
    {"vkCreateComputePipelines", offsetof(DeviceDispatchTable, CreateComputePipelines)},
    {"vkCreateDescriptorPool", offsetof(DeviceDispatchTable, CreateDescriptorPool)},
    {"vkCreateDescriptorSetLayout", offsetof(DeviceDispatchTable, CreateDescriptorSetLayout)},
    {"vkCreateEvent", offsetof(DeviceDispatchTable, CreateEvent)},
    {"vkCreateFence", offsetof(DeviceDispatchTable, CreateFence)},
    {"vkCreateFramebuffer", offsetof(DeviceDispatchTable, CreateFramebuffer)},
    {"vkCreateGraphicsPipelines", offsetof(DeviceDispatchTable, CreateGraphicsPipelines)},
    {"vkCreateImage", offsetof(DeviceDispatchTable, CreateImage)},
    {"vkCreateImageView", offsetof(DeviceDispatchTable, CreateImageView)},
    {"vkCreatePipelineCache", offsetof(DeviceDispatchTable, CreatePipelineCache)},
    {"vkCreatePipelineLayout", offsetof(DeviceDispatchTable, CreatePipelineLayout)},
    {"vkCreateQueryPool", offsetof(DeviceDispatchTable, CreateQueryPool)},
    {"vkCreateRenderPass", offsetof(DeviceDispatchTable, CreateRenderPass)},
    {"vkCreateSampler", offsetof(DeviceDispatchTable, CreateSampler)},
    {"vkCreateSemaphore", offsetof(DeviceDispatchTable, CreateSemaphore)},
    {"vkCreateShaderModule", offsetof(DeviceDispatchTable, CreateShaderModule)},
    {"vkCreateSwapchainKHR", offsetof(DeviceDispatchTable, CreateSwapchainKHR)},
    {"vkDestroyBuffer", offsetof(DeviceDispatchTable, DestroyBuffer)},
    {"vkDestroyBufferView", offsetof(DeviceDispatchTable, DestroyBufferView)},
    {"vkDestroyCommandPool", offsetof(DeviceDispatchTable, DestroyCommandPool)},
    {"vkDestroyDescriptorPool", offsetof(DeviceDispatchTable, DestroyDescriptorPool)},
    {"vkDestroyDescriptorSetLayout", offsetof(DeviceDispatchTable, DestroyDescriptorSetLayout)},
    {"vkDestroyDevice", offsetof(DeviceDispatchTable, DestroyDevice)},
    {"vkDestroyEvent", offsetof(DeviceDispatchTable, DestroyEvent)},
    {"vkDestroyFence", offsetof(DeviceDispatchTable, DestroyFence)},
    {"vkDestroyFramebuffer", offsetof(DeviceDispatchTable, DestroyFramebuffer)},
    {"vkDestroyImage", offsetof(DeviceDispatchTable, DestroyImage)},
    {"vkDestroyImageView", offsetof(DeviceDispatchTable, DestroyImageView)},
    {"vkDestroyPipeline", offsetof(DeviceDispatchTable, DestroyPipeline)},
    {"vkDestroyPipelineCache", offsetof(DeviceDispatchTable, DestroyPipelineCache)},
    {"vkDestroyPipelineLayout", offsetof(DeviceDispatchTable, DestroyPipelineLayout)},
    {"vkDestroyQueryPool", offsetof(DeviceDispatchTable, DestroyQueryPool)},
    {"vkDestroyRenderPass", offsetof(DeviceDispatchTable, DestroyRenderPass)},
    {"vkDestroySampler", offsetof(DeviceDispatchTable, DestroySampler)},
    {"vkDestroySemaphore", offsetof(DeviceDispatchTable, DestroySemaphore)},
    {"vkDestroyShaderModule", offsetof(DeviceDispatchTable, DestroyShaderModule)},
    {"vkDestroySwapchainKHR", offsetof(DeviceDispatchTable, DestroySwapchainKHR)},
    {"vkDeviceWaitIdle", offsetof(DeviceDispatchTable, DeviceWaitIdle)},
    {"vkEndCommandBuffer", offsetof(DeviceDispatchTable, EndCommandBuffer)},
    {"vkFlushMappedMemoryRanges", offsetof(DeviceDispatchTable, FlushMappedMemoryRanges)},
    {"vkFreeCommandBuffers", offsetof(DeviceDispatchTable, FreeCommandBuffers)},
    {"vkFreeDescriptorSets", offsetof(DeviceDispatchTable, FreeDescriptorSets)},
    {"vkFreeMemory", offsetof(DeviceDispatchTable, FreeMemory)},
    {"vkGetBufferMemoryRequirements", offsetof(DeviceDispatchTable, GetBufferMemoryRequirements)},
    {"vkGetDeviceMemoryCommitment", offsetof(DeviceDispatchTable, GetDeviceMemoryCommitment)},
    {"vkGetDeviceQueue", offsetof(DeviceDispatchTable, GetDeviceQueue)},
    {"vkGetEventStatus", offsetof(DeviceDispatchTable, GetEventStatus)},
    {"vkGetFenceStatus", offsetof(DeviceDispatchTable, GetFenceStatus)},
    {"vkGetImageMemoryRequirements", offsetof(DeviceDispatchTable, GetImageMemoryRequirements)},
    {"vkGetImageSparseMemoryRequirements", offsetof(DeviceDispatchTable, GetImageSparseMemoryRequirements)},
    {"vkGetImageSubresourceLayout", offsetof(DeviceDispatchTable, GetImageSubresourceLayout)},
    {"vkGetPipelineCacheData", offsetof(DeviceDispatchTable, GetPipelineCacheData)},
    {"vkGetQueryPoolResults", offsetof(DeviceDispatchTable, GetQueryPoolResults)},
    {"vkGetRenderAreaGranularity", offsetof(DeviceDispatchTable, GetRenderAreaGranularity)},
    {"vkGetSwapchainImagesKHR", offsetof(DeviceDispatchTable, GetSwapchainImagesKHR)},
    {"vkInvalidateMappedMemoryRanges", offsetof(DeviceDispatchTable, InvalidateMappedMemoryRanges)},
    {"vkMapMemory", offsetof(DeviceDispatchTable, MapMemory)},
    {"vkMergePipelineCaches", offsetof(DeviceDispatchTable, MergePipelineCaches)},
    {"vkQueueBindSparse", offsetof(DeviceDispatchTable, QueueBindSparse)},
    {"vkQueuePresentKHR", offsetof(DeviceDispatchTable, QueuePresentKHR)},
    {"vkQueueSubmit", offsetof(DeviceDispatchTable, QueueSubmit)},
    {"vkQueueWaitIdle", offsetof(DeviceDispatchTable, QueueWaitIdle)},
    {"vkResetCommandBuffer", offsetof(DeviceDispatchTable, ResetCommandBuffer)},
    {"vkResetCommandPool", offsetof(DeviceDispatchTable, ResetCommandPool)},
    {"vkResetDescriptorPool", offsetof(DeviceDispatchTable, ResetDescriptorPool)},
    {"vkResetEvent", offsetof(DeviceDispatchTable, ResetEvent)},
    {"vkResetFences", offsetof(DeviceDispatchTable, ResetFences)},
    {"vkSetEvent", offsetof(DeviceDispatchTable, SetEvent)},
    {"vkUnmapMemory", offsetof(DeviceDispatchTable, UnmapMemory)},
    {"vkUpdateDescriptorSets", offsetof(DeviceDispatchTable, UpdateDescriptorSets)},
    {"vkWaitForFences", offsetof(DeviceDispatchTable, WaitForFences)},
    // clang-format on
};

}  // anonymous namespace

namespace vulkan {

PFN_vkVoidFunction GetLoaderExportProcAddr(const char* name) {
    return Lookup(name, kLoaderExportProcs);
}

PFN_vkVoidFunction GetLoaderGlobalProcAddr(const char* name) {
    return Lookup(name, kLoaderGlobalProcs);
}

PFN_vkVoidFunction GetLoaderTopProcAddr(const char* name) {
    return Lookup(name, kLoaderTopProcs);
}

PFN_vkVoidFunction GetLoaderBottomProcAddr(const char* name) {
    return Lookup(name, kLoaderBottomProcs);
}

PFN_vkVoidFunction GetDispatchProcAddr(const InstanceDispatchTable& dispatch,
                                       const char* name) {
    return Lookup(name, kInstanceDispatchOffsets, dispatch);
}

PFN_vkVoidFunction GetDispatchProcAddr(const DeviceDispatchTable& dispatch,
                                       const char* name) {
    return Lookup(name, kDeviceDispatchOffsets, dispatch);
}

bool LoadInstanceDispatchTable(VkInstance instance,
                               PFN_vkGetInstanceProcAddr get_proc_addr,
                               InstanceDispatchTable& dispatch) {
    bool success = true;
    // clang-format off
    dispatch.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(get_proc_addr(instance, "vkDestroyInstance"));
    if (UNLIKELY(!dispatch.DestroyInstance)) {
        ALOGE("missing instance proc: %s", "vkDestroyInstance");
        success = false;
    }
    dispatch.EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    if (UNLIKELY(!dispatch.EnumeratePhysicalDevices)) {
        ALOGE("missing instance proc: %s", "vkEnumeratePhysicalDevices");
        success = false;
    }
    dispatch.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceQueueFamilyProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceQueueFamilyProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceMemoryProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceMemoryProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(get_proc_addr(instance, "vkGetPhysicalDeviceFeatures"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceFeatures)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceFeatures");
        success = false;
    }
    dispatch.GetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceFormatProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceImageFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceImageFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceImageFormatProperties");
        success = false;
    }
    dispatch.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(get_proc_addr(instance, "vkCreateDevice"));
    if (UNLIKELY(!dispatch.CreateDevice)) {
        ALOGE("missing instance proc: %s", "vkCreateDevice");
        success = false;
    }
    dispatch.EnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(get_proc_addr(instance, "vkEnumerateDeviceLayerProperties"));
    if (UNLIKELY(!dispatch.EnumerateDeviceLayerProperties)) {
        ALOGE("missing instance proc: %s", "vkEnumerateDeviceLayerProperties");
        success = false;
    }
    dispatch.EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(get_proc_addr(instance, "vkEnumerateDeviceExtensionProperties"));
    if (UNLIKELY(!dispatch.EnumerateDeviceExtensionProperties)) {
        ALOGE("missing instance proc: %s", "vkEnumerateDeviceExtensionProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSparseImageFormatProperties)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSparseImageFormatProperties");
        success = false;
    }
    dispatch.DestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(get_proc_addr(instance, "vkDestroySurfaceKHR"));
    if (UNLIKELY(!dispatch.DestroySurfaceKHR)) {
        ALOGE("missing instance proc: %s", "vkDestroySurfaceKHR");
        success = false;
    }
    dispatch.GetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(get_proc_addr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSurfaceSupportKHR)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSurfaceSupportKHR");
        success = false;
    }
    dispatch.GetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(get_proc_addr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSurfaceCapabilitiesKHR)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        success = false;
    }
    dispatch.GetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(get_proc_addr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSurfaceFormatsKHR)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSurfaceFormatsKHR");
        success = false;
    }
    dispatch.GetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(get_proc_addr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSurfacePresentModesKHR)) {
        ALOGE("missing instance proc: %s", "vkGetPhysicalDeviceSurfacePresentModesKHR");
        success = false;
    }
    dispatch.CreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(get_proc_addr(instance, "vkCreateAndroidSurfaceKHR"));
    if (UNLIKELY(!dispatch.CreateAndroidSurfaceKHR)) {
        ALOGE("missing instance proc: %s", "vkCreateAndroidSurfaceKHR");
        success = false;
    }
    dispatch.CreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(get_proc_addr(instance, "vkCreateDebugReportCallbackEXT"));
    if (UNLIKELY(!dispatch.CreateDebugReportCallbackEXT)) {
        ALOGE("missing instance proc: %s", "vkCreateDebugReportCallbackEXT");
        success = false;
    }
    dispatch.DestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(get_proc_addr(instance, "vkDestroyDebugReportCallbackEXT"));
    if (UNLIKELY(!dispatch.DestroyDebugReportCallbackEXT)) {
        ALOGE("missing instance proc: %s", "vkDestroyDebugReportCallbackEXT");
        success = false;
    }
    dispatch.DebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(get_proc_addr(instance, "vkDebugReportMessageEXT"));
    if (UNLIKELY(!dispatch.DebugReportMessageEXT)) {
        ALOGE("missing instance proc: %s", "vkDebugReportMessageEXT");
        success = false;
    }
    // clang-format on
    return success;
}

bool LoadDeviceDispatchTable(VkDevice device,
                             PFN_vkGetDeviceProcAddr get_proc_addr,
                             DeviceDispatchTable& dispatch) {
    bool success = true;
    // clang-format off
    dispatch.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(get_proc_addr(device, "vkDestroyDevice"));
    if (UNLIKELY(!dispatch.DestroyDevice)) {
        ALOGE("missing device proc: %s", "vkDestroyDevice");
        success = false;
    }
    dispatch.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(get_proc_addr(device, "vkGetDeviceQueue"));
    if (UNLIKELY(!dispatch.GetDeviceQueue)) {
        ALOGE("missing device proc: %s", "vkGetDeviceQueue");
        success = false;
    }
    dispatch.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(get_proc_addr(device, "vkQueueSubmit"));
    if (UNLIKELY(!dispatch.QueueSubmit)) {
        ALOGE("missing device proc: %s", "vkQueueSubmit");
        success = false;
    }
    dispatch.QueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(get_proc_addr(device, "vkQueueWaitIdle"));
    if (UNLIKELY(!dispatch.QueueWaitIdle)) {
        ALOGE("missing device proc: %s", "vkQueueWaitIdle");
        success = false;
    }
    dispatch.DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(get_proc_addr(device, "vkDeviceWaitIdle"));
    if (UNLIKELY(!dispatch.DeviceWaitIdle)) {
        ALOGE("missing device proc: %s", "vkDeviceWaitIdle");
        success = false;
    }
    dispatch.AllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(get_proc_addr(device, "vkAllocateMemory"));
    if (UNLIKELY(!dispatch.AllocateMemory)) {
        ALOGE("missing device proc: %s", "vkAllocateMemory");
        success = false;
    }
    dispatch.FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(get_proc_addr(device, "vkFreeMemory"));
    if (UNLIKELY(!dispatch.FreeMemory)) {
        ALOGE("missing device proc: %s", "vkFreeMemory");
        success = false;
    }
    dispatch.MapMemory = reinterpret_cast<PFN_vkMapMemory>(get_proc_addr(device, "vkMapMemory"));
    if (UNLIKELY(!dispatch.MapMemory)) {
        ALOGE("missing device proc: %s", "vkMapMemory");
        success = false;
    }
    dispatch.UnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(get_proc_addr(device, "vkUnmapMemory"));
    if (UNLIKELY(!dispatch.UnmapMemory)) {
        ALOGE("missing device proc: %s", "vkUnmapMemory");
        success = false;
    }
    dispatch.FlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(get_proc_addr(device, "vkFlushMappedMemoryRanges"));
    if (UNLIKELY(!dispatch.FlushMappedMemoryRanges)) {
        ALOGE("missing device proc: %s", "vkFlushMappedMemoryRanges");
        success = false;
    }
    dispatch.InvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(get_proc_addr(device, "vkInvalidateMappedMemoryRanges"));
    if (UNLIKELY(!dispatch.InvalidateMappedMemoryRanges)) {
        ALOGE("missing device proc: %s", "vkInvalidateMappedMemoryRanges");
        success = false;
    }
    dispatch.GetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(get_proc_addr(device, "vkGetDeviceMemoryCommitment"));
    if (UNLIKELY(!dispatch.GetDeviceMemoryCommitment)) {
        ALOGE("missing device proc: %s", "vkGetDeviceMemoryCommitment");
        success = false;
    }
    dispatch.GetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(get_proc_addr(device, "vkGetBufferMemoryRequirements"));
    if (UNLIKELY(!dispatch.GetBufferMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetBufferMemoryRequirements");
        success = false;
    }
    dispatch.BindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(get_proc_addr(device, "vkBindBufferMemory"));
    if (UNLIKELY(!dispatch.BindBufferMemory)) {
        ALOGE("missing device proc: %s", "vkBindBufferMemory");
        success = false;
    }
    dispatch.GetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get_proc_addr(device, "vkGetImageMemoryRequirements"));
    if (UNLIKELY(!dispatch.GetImageMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetImageMemoryRequirements");
        success = false;
    }
    dispatch.BindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(get_proc_addr(device, "vkBindImageMemory"));
    if (UNLIKELY(!dispatch.BindImageMemory)) {
        ALOGE("missing device proc: %s", "vkBindImageMemory");
        success = false;
    }
    dispatch.GetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(get_proc_addr(device, "vkGetImageSparseMemoryRequirements"));
    if (UNLIKELY(!dispatch.GetImageSparseMemoryRequirements)) {
        ALOGE("missing device proc: %s", "vkGetImageSparseMemoryRequirements");
        success = false;
    }
    dispatch.QueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(get_proc_addr(device, "vkQueueBindSparse"));
    if (UNLIKELY(!dispatch.QueueBindSparse)) {
        ALOGE("missing device proc: %s", "vkQueueBindSparse");
        success = false;
    }
    dispatch.CreateFence = reinterpret_cast<PFN_vkCreateFence>(get_proc_addr(device, "vkCreateFence"));
    if (UNLIKELY(!dispatch.CreateFence)) {
        ALOGE("missing device proc: %s", "vkCreateFence");
        success = false;
    }
    dispatch.DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(get_proc_addr(device, "vkDestroyFence"));
    if (UNLIKELY(!dispatch.DestroyFence)) {
        ALOGE("missing device proc: %s", "vkDestroyFence");
        success = false;
    }
    dispatch.ResetFences = reinterpret_cast<PFN_vkResetFences>(get_proc_addr(device, "vkResetFences"));
    if (UNLIKELY(!dispatch.ResetFences)) {
        ALOGE("missing device proc: %s", "vkResetFences");
        success = false;
    }
    dispatch.GetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(get_proc_addr(device, "vkGetFenceStatus"));
    if (UNLIKELY(!dispatch.GetFenceStatus)) {
        ALOGE("missing device proc: %s", "vkGetFenceStatus");
        success = false;
    }
    dispatch.WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(get_proc_addr(device, "vkWaitForFences"));
    if (UNLIKELY(!dispatch.WaitForFences)) {
        ALOGE("missing device proc: %s", "vkWaitForFences");
        success = false;
    }
    dispatch.CreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(get_proc_addr(device, "vkCreateSemaphore"));
    if (UNLIKELY(!dispatch.CreateSemaphore)) {
        ALOGE("missing device proc: %s", "vkCreateSemaphore");
        success = false;
    }
    dispatch.DestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(get_proc_addr(device, "vkDestroySemaphore"));
    if (UNLIKELY(!dispatch.DestroySemaphore)) {
        ALOGE("missing device proc: %s", "vkDestroySemaphore");
        success = false;
    }
    dispatch.CreateEvent = reinterpret_cast<PFN_vkCreateEvent>(get_proc_addr(device, "vkCreateEvent"));
    if (UNLIKELY(!dispatch.CreateEvent)) {
        ALOGE("missing device proc: %s", "vkCreateEvent");
        success = false;
    }
    dispatch.DestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(get_proc_addr(device, "vkDestroyEvent"));
    if (UNLIKELY(!dispatch.DestroyEvent)) {
        ALOGE("missing device proc: %s", "vkDestroyEvent");
        success = false;
    }
    dispatch.GetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(get_proc_addr(device, "vkGetEventStatus"));
    if (UNLIKELY(!dispatch.GetEventStatus)) {
        ALOGE("missing device proc: %s", "vkGetEventStatus");
        success = false;
    }
    dispatch.SetEvent = reinterpret_cast<PFN_vkSetEvent>(get_proc_addr(device, "vkSetEvent"));
    if (UNLIKELY(!dispatch.SetEvent)) {
        ALOGE("missing device proc: %s", "vkSetEvent");
        success = false;
    }
    dispatch.ResetEvent = reinterpret_cast<PFN_vkResetEvent>(get_proc_addr(device, "vkResetEvent"));
    if (UNLIKELY(!dispatch.ResetEvent)) {
        ALOGE("missing device proc: %s", "vkResetEvent");
        success = false;
    }
    dispatch.CreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(get_proc_addr(device, "vkCreateQueryPool"));
    if (UNLIKELY(!dispatch.CreateQueryPool)) {
        ALOGE("missing device proc: %s", "vkCreateQueryPool");
        success = false;
    }
    dispatch.DestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(get_proc_addr(device, "vkDestroyQueryPool"));
    if (UNLIKELY(!dispatch.DestroyQueryPool)) {
        ALOGE("missing device proc: %s", "vkDestroyQueryPool");
        success = false;
    }
    dispatch.GetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(get_proc_addr(device, "vkGetQueryPoolResults"));
    if (UNLIKELY(!dispatch.GetQueryPoolResults)) {
        ALOGE("missing device proc: %s", "vkGetQueryPoolResults");
        success = false;
    }
    dispatch.CreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(get_proc_addr(device, "vkCreateBuffer"));
    if (UNLIKELY(!dispatch.CreateBuffer)) {
        ALOGE("missing device proc: %s", "vkCreateBuffer");
        success = false;
    }
    dispatch.DestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(get_proc_addr(device, "vkDestroyBuffer"));
    if (UNLIKELY(!dispatch.DestroyBuffer)) {
        ALOGE("missing device proc: %s", "vkDestroyBuffer");
        success = false;
    }
    dispatch.CreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(get_proc_addr(device, "vkCreateBufferView"));
    if (UNLIKELY(!dispatch.CreateBufferView)) {
        ALOGE("missing device proc: %s", "vkCreateBufferView");
        success = false;
    }
    dispatch.DestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(get_proc_addr(device, "vkDestroyBufferView"));
    if (UNLIKELY(!dispatch.DestroyBufferView)) {
        ALOGE("missing device proc: %s", "vkDestroyBufferView");
        success = false;
    }
    dispatch.CreateImage = reinterpret_cast<PFN_vkCreateImage>(get_proc_addr(device, "vkCreateImage"));
    if (UNLIKELY(!dispatch.CreateImage)) {
        ALOGE("missing device proc: %s", "vkCreateImage");
        success = false;
    }
    dispatch.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(get_proc_addr(device, "vkDestroyImage"));
    if (UNLIKELY(!dispatch.DestroyImage)) {
        ALOGE("missing device proc: %s", "vkDestroyImage");
        success = false;
    }
    dispatch.GetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(get_proc_addr(device, "vkGetImageSubresourceLayout"));
    if (UNLIKELY(!dispatch.GetImageSubresourceLayout)) {
        ALOGE("missing device proc: %s", "vkGetImageSubresourceLayout");
        success = false;
    }
    dispatch.CreateImageView = reinterpret_cast<PFN_vkCreateImageView>(get_proc_addr(device, "vkCreateImageView"));
    if (UNLIKELY(!dispatch.CreateImageView)) {
        ALOGE("missing device proc: %s", "vkCreateImageView");
        success = false;
    }
    dispatch.DestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(get_proc_addr(device, "vkDestroyImageView"));
    if (UNLIKELY(!dispatch.DestroyImageView)) {
        ALOGE("missing device proc: %s", "vkDestroyImageView");
        success = false;
    }
    dispatch.CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(get_proc_addr(device, "vkCreateShaderModule"));
    if (UNLIKELY(!dispatch.CreateShaderModule)) {
        ALOGE("missing device proc: %s", "vkCreateShaderModule");
        success = false;
    }
    dispatch.DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(get_proc_addr(device, "vkDestroyShaderModule"));
    if (UNLIKELY(!dispatch.DestroyShaderModule)) {
        ALOGE("missing device proc: %s", "vkDestroyShaderModule");
        success = false;
    }
    dispatch.CreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(get_proc_addr(device, "vkCreatePipelineCache"));
    if (UNLIKELY(!dispatch.CreatePipelineCache)) {
        ALOGE("missing device proc: %s", "vkCreatePipelineCache");
        success = false;
    }
    dispatch.DestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(get_proc_addr(device, "vkDestroyPipelineCache"));
    if (UNLIKELY(!dispatch.DestroyPipelineCache)) {
        ALOGE("missing device proc: %s", "vkDestroyPipelineCache");
        success = false;
    }
    dispatch.GetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(get_proc_addr(device, "vkGetPipelineCacheData"));
    if (UNLIKELY(!dispatch.GetPipelineCacheData)) {
        ALOGE("missing device proc: %s", "vkGetPipelineCacheData");
        success = false;
    }
    dispatch.MergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(get_proc_addr(device, "vkMergePipelineCaches"));
    if (UNLIKELY(!dispatch.MergePipelineCaches)) {
        ALOGE("missing device proc: %s", "vkMergePipelineCaches");
        success = false;
    }
    dispatch.CreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(get_proc_addr(device, "vkCreateGraphicsPipelines"));
    if (UNLIKELY(!dispatch.CreateGraphicsPipelines)) {
        ALOGE("missing device proc: %s", "vkCreateGraphicsPipelines");
        success = false;
    }
    dispatch.CreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(get_proc_addr(device, "vkCreateComputePipelines"));
    if (UNLIKELY(!dispatch.CreateComputePipelines)) {
        ALOGE("missing device proc: %s", "vkCreateComputePipelines");
        success = false;
    }
    dispatch.DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(get_proc_addr(device, "vkDestroyPipeline"));
    if (UNLIKELY(!dispatch.DestroyPipeline)) {
        ALOGE("missing device proc: %s", "vkDestroyPipeline");
        success = false;
    }
    dispatch.CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(get_proc_addr(device, "vkCreatePipelineLayout"));
    if (UNLIKELY(!dispatch.CreatePipelineLayout)) {
        ALOGE("missing device proc: %s", "vkCreatePipelineLayout");
        success = false;
    }
    dispatch.DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(get_proc_addr(device, "vkDestroyPipelineLayout"));
    if (UNLIKELY(!dispatch.DestroyPipelineLayout)) {
        ALOGE("missing device proc: %s", "vkDestroyPipelineLayout");
        success = false;
    }
    dispatch.CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(get_proc_addr(device, "vkCreateSampler"));
    if (UNLIKELY(!dispatch.CreateSampler)) {
        ALOGE("missing device proc: %s", "vkCreateSampler");
        success = false;
    }
    dispatch.DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(get_proc_addr(device, "vkDestroySampler"));
    if (UNLIKELY(!dispatch.DestroySampler)) {
        ALOGE("missing device proc: %s", "vkDestroySampler");
        success = false;
    }
    dispatch.CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(get_proc_addr(device, "vkCreateDescriptorSetLayout"));
    if (UNLIKELY(!dispatch.CreateDescriptorSetLayout)) {
        ALOGE("missing device proc: %s", "vkCreateDescriptorSetLayout");
        success = false;
    }
    dispatch.DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(get_proc_addr(device, "vkDestroyDescriptorSetLayout"));
    if (UNLIKELY(!dispatch.DestroyDescriptorSetLayout)) {
        ALOGE("missing device proc: %s", "vkDestroyDescriptorSetLayout");
        success = false;
    }
    dispatch.CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(get_proc_addr(device, "vkCreateDescriptorPool"));
    if (UNLIKELY(!dispatch.CreateDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkCreateDescriptorPool");
        success = false;
    }
    dispatch.DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_proc_addr(device, "vkDestroyDescriptorPool"));
    if (UNLIKELY(!dispatch.DestroyDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkDestroyDescriptorPool");
        success = false;
    }
    dispatch.ResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(get_proc_addr(device, "vkResetDescriptorPool"));
    if (UNLIKELY(!dispatch.ResetDescriptorPool)) {
        ALOGE("missing device proc: %s", "vkResetDescriptorPool");
        success = false;
    }
    dispatch.AllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(get_proc_addr(device, "vkAllocateDescriptorSets"));
    if (UNLIKELY(!dispatch.AllocateDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkAllocateDescriptorSets");
        success = false;
    }
    dispatch.FreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(get_proc_addr(device, "vkFreeDescriptorSets"));
    if (UNLIKELY(!dispatch.FreeDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkFreeDescriptorSets");
        success = false;
    }
    dispatch.UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(get_proc_addr(device, "vkUpdateDescriptorSets"));
    if (UNLIKELY(!dispatch.UpdateDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkUpdateDescriptorSets");
        success = false;
    }
    dispatch.CreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(get_proc_addr(device, "vkCreateFramebuffer"));
    if (UNLIKELY(!dispatch.CreateFramebuffer)) {
        ALOGE("missing device proc: %s", "vkCreateFramebuffer");
        success = false;
    }
    dispatch.DestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(get_proc_addr(device, "vkDestroyFramebuffer"));
    if (UNLIKELY(!dispatch.DestroyFramebuffer)) {
        ALOGE("missing device proc: %s", "vkDestroyFramebuffer");
        success = false;
    }
    dispatch.CreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(get_proc_addr(device, "vkCreateRenderPass"));
    if (UNLIKELY(!dispatch.CreateRenderPass)) {
        ALOGE("missing device proc: %s", "vkCreateRenderPass");
        success = false;
    }
    dispatch.DestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(get_proc_addr(device, "vkDestroyRenderPass"));
    if (UNLIKELY(!dispatch.DestroyRenderPass)) {
        ALOGE("missing device proc: %s", "vkDestroyRenderPass");
        success = false;
    }
    dispatch.GetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(get_proc_addr(device, "vkGetRenderAreaGranularity"));
    if (UNLIKELY(!dispatch.GetRenderAreaGranularity)) {
        ALOGE("missing device proc: %s", "vkGetRenderAreaGranularity");
        success = false;
    }
    dispatch.CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(get_proc_addr(device, "vkCreateCommandPool"));
    if (UNLIKELY(!dispatch.CreateCommandPool)) {
        ALOGE("missing device proc: %s", "vkCreateCommandPool");
        success = false;
    }
    dispatch.DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(get_proc_addr(device, "vkDestroyCommandPool"));
    if (UNLIKELY(!dispatch.DestroyCommandPool)) {
        ALOGE("missing device proc: %s", "vkDestroyCommandPool");
        success = false;
    }
    dispatch.ResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(get_proc_addr(device, "vkResetCommandPool"));
    if (UNLIKELY(!dispatch.ResetCommandPool)) {
        ALOGE("missing device proc: %s", "vkResetCommandPool");
        success = false;
    }
    dispatch.AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(get_proc_addr(device, "vkAllocateCommandBuffers"));
    if (UNLIKELY(!dispatch.AllocateCommandBuffers)) {
        ALOGE("missing device proc: %s", "vkAllocateCommandBuffers");
        success = false;
    }
    dispatch.FreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(get_proc_addr(device, "vkFreeCommandBuffers"));
    if (UNLIKELY(!dispatch.FreeCommandBuffers)) {
        ALOGE("missing device proc: %s", "vkFreeCommandBuffers");
        success = false;
    }
    dispatch.BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(get_proc_addr(device, "vkBeginCommandBuffer"));
    if (UNLIKELY(!dispatch.BeginCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkBeginCommandBuffer");
        success = false;
    }
    dispatch.EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(get_proc_addr(device, "vkEndCommandBuffer"));
    if (UNLIKELY(!dispatch.EndCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkEndCommandBuffer");
        success = false;
    }
    dispatch.ResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(get_proc_addr(device, "vkResetCommandBuffer"));
    if (UNLIKELY(!dispatch.ResetCommandBuffer)) {
        ALOGE("missing device proc: %s", "vkResetCommandBuffer");
        success = false;
    }
    dispatch.CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(get_proc_addr(device, "vkCmdBindPipeline"));
    if (UNLIKELY(!dispatch.CmdBindPipeline)) {
        ALOGE("missing device proc: %s", "vkCmdBindPipeline");
        success = false;
    }
    dispatch.CmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(get_proc_addr(device, "vkCmdSetViewport"));
    if (UNLIKELY(!dispatch.CmdSetViewport)) {
        ALOGE("missing device proc: %s", "vkCmdSetViewport");
        success = false;
    }
    dispatch.CmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(get_proc_addr(device, "vkCmdSetScissor"));
    if (UNLIKELY(!dispatch.CmdSetScissor)) {
        ALOGE("missing device proc: %s", "vkCmdSetScissor");
        success = false;
    }
    dispatch.CmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(get_proc_addr(device, "vkCmdSetLineWidth"));
    if (UNLIKELY(!dispatch.CmdSetLineWidth)) {
        ALOGE("missing device proc: %s", "vkCmdSetLineWidth");
        success = false;
    }
    dispatch.CmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(get_proc_addr(device, "vkCmdSetDepthBias"));
    if (UNLIKELY(!dispatch.CmdSetDepthBias)) {
        ALOGE("missing device proc: %s", "vkCmdSetDepthBias");
        success = false;
    }
    dispatch.CmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(get_proc_addr(device, "vkCmdSetBlendConstants"));
    if (UNLIKELY(!dispatch.CmdSetBlendConstants)) {
        ALOGE("missing device proc: %s", "vkCmdSetBlendConstants");
        success = false;
    }
    dispatch.CmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(get_proc_addr(device, "vkCmdSetDepthBounds"));
    if (UNLIKELY(!dispatch.CmdSetDepthBounds)) {
        ALOGE("missing device proc: %s", "vkCmdSetDepthBounds");
        success = false;
    }
    dispatch.CmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(get_proc_addr(device, "vkCmdSetStencilCompareMask"));
    if (UNLIKELY(!dispatch.CmdSetStencilCompareMask)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilCompareMask");
        success = false;
    }
    dispatch.CmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(get_proc_addr(device, "vkCmdSetStencilWriteMask"));
    if (UNLIKELY(!dispatch.CmdSetStencilWriteMask)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilWriteMask");
        success = false;
    }
    dispatch.CmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(get_proc_addr(device, "vkCmdSetStencilReference"));
    if (UNLIKELY(!dispatch.CmdSetStencilReference)) {
        ALOGE("missing device proc: %s", "vkCmdSetStencilReference");
        success = false;
    }
    dispatch.CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(get_proc_addr(device, "vkCmdBindDescriptorSets"));
    if (UNLIKELY(!dispatch.CmdBindDescriptorSets)) {
        ALOGE("missing device proc: %s", "vkCmdBindDescriptorSets");
        success = false;
    }
    dispatch.CmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(get_proc_addr(device, "vkCmdBindIndexBuffer"));
    if (UNLIKELY(!dispatch.CmdBindIndexBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdBindIndexBuffer");
        success = false;
    }
    dispatch.CmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(get_proc_addr(device, "vkCmdBindVertexBuffers"));
    if (UNLIKELY(!dispatch.CmdBindVertexBuffers)) {
        ALOGE("missing device proc: %s", "vkCmdBindVertexBuffers");
        success = false;
    }
    dispatch.CmdDraw = reinterpret_cast<PFN_vkCmdDraw>(get_proc_addr(device, "vkCmdDraw"));
    if (UNLIKELY(!dispatch.CmdDraw)) {
        ALOGE("missing device proc: %s", "vkCmdDraw");
        success = false;
    }
    dispatch.CmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(get_proc_addr(device, "vkCmdDrawIndexed"));
    if (UNLIKELY(!dispatch.CmdDrawIndexed)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndexed");
        success = false;
    }
    dispatch.CmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(get_proc_addr(device, "vkCmdDrawIndirect"));
    if (UNLIKELY(!dispatch.CmdDrawIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndirect");
        success = false;
    }
    dispatch.CmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(get_proc_addr(device, "vkCmdDrawIndexedIndirect"));
    if (UNLIKELY(!dispatch.CmdDrawIndexedIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDrawIndexedIndirect");
        success = false;
    }
    dispatch.CmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(get_proc_addr(device, "vkCmdDispatch"));
    if (UNLIKELY(!dispatch.CmdDispatch)) {
        ALOGE("missing device proc: %s", "vkCmdDispatch");
        success = false;
    }
    dispatch.CmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(get_proc_addr(device, "vkCmdDispatchIndirect"));
    if (UNLIKELY(!dispatch.CmdDispatchIndirect)) {
        ALOGE("missing device proc: %s", "vkCmdDispatchIndirect");
        success = false;
    }
    dispatch.CmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(get_proc_addr(device, "vkCmdCopyBuffer"));
    if (UNLIKELY(!dispatch.CmdCopyBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdCopyBuffer");
        success = false;
    }
    dispatch.CmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(get_proc_addr(device, "vkCmdCopyImage"));
    if (UNLIKELY(!dispatch.CmdCopyImage)) {
        ALOGE("missing device proc: %s", "vkCmdCopyImage");
        success = false;
    }
    dispatch.CmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(get_proc_addr(device, "vkCmdBlitImage"));
    if (UNLIKELY(!dispatch.CmdBlitImage)) {
        ALOGE("missing device proc: %s", "vkCmdBlitImage");
        success = false;
    }
    dispatch.CmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(get_proc_addr(device, "vkCmdCopyBufferToImage"));
    if (UNLIKELY(!dispatch.CmdCopyBufferToImage)) {
        ALOGE("missing device proc: %s", "vkCmdCopyBufferToImage");
        success = false;
    }
    dispatch.CmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(get_proc_addr(device, "vkCmdCopyImageToBuffer"));
    if (UNLIKELY(!dispatch.CmdCopyImageToBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdCopyImageToBuffer");
        success = false;
    }
    dispatch.CmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(get_proc_addr(device, "vkCmdUpdateBuffer"));
    if (UNLIKELY(!dispatch.CmdUpdateBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdUpdateBuffer");
        success = false;
    }
    dispatch.CmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(get_proc_addr(device, "vkCmdFillBuffer"));
    if (UNLIKELY(!dispatch.CmdFillBuffer)) {
        ALOGE("missing device proc: %s", "vkCmdFillBuffer");
        success = false;
    }
    dispatch.CmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(get_proc_addr(device, "vkCmdClearColorImage"));
    if (UNLIKELY(!dispatch.CmdClearColorImage)) {
        ALOGE("missing device proc: %s", "vkCmdClearColorImage");
        success = false;
    }
    dispatch.CmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(get_proc_addr(device, "vkCmdClearDepthStencilImage"));
    if (UNLIKELY(!dispatch.CmdClearDepthStencilImage)) {
        ALOGE("missing device proc: %s", "vkCmdClearDepthStencilImage");
        success = false;
    }
    dispatch.CmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(get_proc_addr(device, "vkCmdClearAttachments"));
    if (UNLIKELY(!dispatch.CmdClearAttachments)) {
        ALOGE("missing device proc: %s", "vkCmdClearAttachments");
        success = false;
    }
    dispatch.CmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(get_proc_addr(device, "vkCmdResolveImage"));
    if (UNLIKELY(!dispatch.CmdResolveImage)) {
        ALOGE("missing device proc: %s", "vkCmdResolveImage");
        success = false;
    }
    dispatch.CmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(get_proc_addr(device, "vkCmdSetEvent"));
    if (UNLIKELY(!dispatch.CmdSetEvent)) {
        ALOGE("missing device proc: %s", "vkCmdSetEvent");
        success = false;
    }
    dispatch.CmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(get_proc_addr(device, "vkCmdResetEvent"));
    if (UNLIKELY(!dispatch.CmdResetEvent)) {
        ALOGE("missing device proc: %s", "vkCmdResetEvent");
        success = false;
    }
    dispatch.CmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(get_proc_addr(device, "vkCmdWaitEvents"));
    if (UNLIKELY(!dispatch.CmdWaitEvents)) {
        ALOGE("missing device proc: %s", "vkCmdWaitEvents");
        success = false;
    }
    dispatch.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(get_proc_addr(device, "vkCmdPipelineBarrier"));
    if (UNLIKELY(!dispatch.CmdPipelineBarrier)) {
        ALOGE("missing device proc: %s", "vkCmdPipelineBarrier");
        success = false;
    }
    dispatch.CmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(get_proc_addr(device, "vkCmdBeginQuery"));
    if (UNLIKELY(!dispatch.CmdBeginQuery)) {
        ALOGE("missing device proc: %s", "vkCmdBeginQuery");
        success = false;
    }
    dispatch.CmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(get_proc_addr(device, "vkCmdEndQuery"));
    if (UNLIKELY(!dispatch.CmdEndQuery)) {
        ALOGE("missing device proc: %s", "vkCmdEndQuery");
        success = false;
    }
    dispatch.CmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(get_proc_addr(device, "vkCmdResetQueryPool"));
    if (UNLIKELY(!dispatch.CmdResetQueryPool)) {
        ALOGE("missing device proc: %s", "vkCmdResetQueryPool");
        success = false;
    }
    dispatch.CmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(get_proc_addr(device, "vkCmdWriteTimestamp"));
    if (UNLIKELY(!dispatch.CmdWriteTimestamp)) {
        ALOGE("missing device proc: %s", "vkCmdWriteTimestamp");
        success = false;
    }
    dispatch.CmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(get_proc_addr(device, "vkCmdCopyQueryPoolResults"));
    if (UNLIKELY(!dispatch.CmdCopyQueryPoolResults)) {
        ALOGE("missing device proc: %s", "vkCmdCopyQueryPoolResults");
        success = false;
    }
    dispatch.CmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(get_proc_addr(device, "vkCmdPushConstants"));
    if (UNLIKELY(!dispatch.CmdPushConstants)) {
        ALOGE("missing device proc: %s", "vkCmdPushConstants");
        success = false;
    }
    dispatch.CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(get_proc_addr(device, "vkCmdBeginRenderPass"));
    if (UNLIKELY(!dispatch.CmdBeginRenderPass)) {
        ALOGE("missing device proc: %s", "vkCmdBeginRenderPass");
        success = false;
    }
    dispatch.CmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(get_proc_addr(device, "vkCmdNextSubpass"));
    if (UNLIKELY(!dispatch.CmdNextSubpass)) {
        ALOGE("missing device proc: %s", "vkCmdNextSubpass");
        success = false;
    }
    dispatch.CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(get_proc_addr(device, "vkCmdEndRenderPass"));
    if (UNLIKELY(!dispatch.CmdEndRenderPass)) {
        ALOGE("missing device proc: %s", "vkCmdEndRenderPass");
        success = false;
    }
    dispatch.CmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(get_proc_addr(device, "vkCmdExecuteCommands"));
    if (UNLIKELY(!dispatch.CmdExecuteCommands)) {
        ALOGE("missing device proc: %s", "vkCmdExecuteCommands");
        success = false;
    }
    dispatch.CreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(get_proc_addr(device, "vkCreateSwapchainKHR"));
    if (UNLIKELY(!dispatch.CreateSwapchainKHR)) {
        ALOGE("missing device proc: %s", "vkCreateSwapchainKHR");
        success = false;
    }
    dispatch.DestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(get_proc_addr(device, "vkDestroySwapchainKHR"));
    if (UNLIKELY(!dispatch.DestroySwapchainKHR)) {
        ALOGE("missing device proc: %s", "vkDestroySwapchainKHR");
        success = false;
    }
    dispatch.GetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(get_proc_addr(device, "vkGetSwapchainImagesKHR"));
    if (UNLIKELY(!dispatch.GetSwapchainImagesKHR)) {
        ALOGE("missing device proc: %s", "vkGetSwapchainImagesKHR");
        success = false;
    }
    dispatch.AcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(get_proc_addr(device, "vkAcquireNextImageKHR"));
    if (UNLIKELY(!dispatch.AcquireNextImageKHR)) {
        ALOGE("missing device proc: %s", "vkAcquireNextImageKHR");
        success = false;
    }
    dispatch.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(get_proc_addr(device, "vkQueuePresentKHR"));
    if (UNLIKELY(!dispatch.QueuePresentKHR)) {
        ALOGE("missing device proc: %s", "vkQueuePresentKHR");
        success = false;
    }
    // clang-format on
    return success;
}

bool LoadDriverDispatchTable(VkInstance instance,
                             PFN_vkGetInstanceProcAddr get_proc_addr,
                             const InstanceExtensionSet& extensions,
                             DriverDispatchTable& dispatch) {
    bool success = true;
    // clang-format off
    dispatch.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(get_proc_addr(instance, "vkDestroyInstance"));
    if (UNLIKELY(!dispatch.DestroyInstance)) {
        ALOGE("missing driver proc: %s", "vkDestroyInstance");
        success = false;
    }
    dispatch.EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get_proc_addr(instance, "vkEnumeratePhysicalDevices"));
    if (UNLIKELY(!dispatch.EnumeratePhysicalDevices)) {
        ALOGE("missing driver proc: %s", "vkEnumeratePhysicalDevices");
        success = false;
    }
    dispatch.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceQueueFamilyProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceQueueFamilyProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceMemoryProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceMemoryProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(get_proc_addr(instance, "vkGetPhysicalDeviceFeatures"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceFeatures)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceFeatures");
        success = false;
    }
    dispatch.GetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceFormatProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceFormatProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceImageFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceImageFormatProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceImageFormatProperties");
        success = false;
    }
    dispatch.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(get_proc_addr(instance, "vkCreateDevice"));
    if (UNLIKELY(!dispatch.CreateDevice)) {
        ALOGE("missing driver proc: %s", "vkCreateDevice");
        success = false;
    }
    dispatch.EnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(get_proc_addr(instance, "vkEnumerateDeviceLayerProperties"));
    if (UNLIKELY(!dispatch.EnumerateDeviceLayerProperties)) {
        ALOGE("missing driver proc: %s", "vkEnumerateDeviceLayerProperties");
        success = false;
    }
    dispatch.EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(get_proc_addr(instance, "vkEnumerateDeviceExtensionProperties"));
    if (UNLIKELY(!dispatch.EnumerateDeviceExtensionProperties)) {
        ALOGE("missing driver proc: %s", "vkEnumerateDeviceExtensionProperties");
        success = false;
    }
    dispatch.GetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(get_proc_addr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    if (UNLIKELY(!dispatch.GetPhysicalDeviceSparseImageFormatProperties)) {
        ALOGE("missing driver proc: %s", "vkGetPhysicalDeviceSparseImageFormatProperties");
        success = false;
    }
    if (extensions[kEXT_debug_report]) {
        dispatch.CreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(get_proc_addr(instance, "vkCreateDebugReportCallbackEXT"));
        if (UNLIKELY(!dispatch.CreateDebugReportCallbackEXT)) {
            ALOGE("missing driver proc: %s", "vkCreateDebugReportCallbackEXT");
            success = false;
        }
    }
    if (extensions[kEXT_debug_report]) {
        dispatch.DestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(get_proc_addr(instance, "vkDestroyDebugReportCallbackEXT"));
        if (UNLIKELY(!dispatch.DestroyDebugReportCallbackEXT)) {
            ALOGE("missing driver proc: %s", "vkDestroyDebugReportCallbackEXT");
            success = false;
        }
    }
    if (extensions[kEXT_debug_report]) {
        dispatch.DebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(get_proc_addr(instance, "vkDebugReportMessageEXT"));
        if (UNLIKELY(!dispatch.DebugReportMessageEXT)) {
            ALOGE("missing driver proc: %s", "vkDebugReportMessageEXT");
            success = false;
        }
    }
    dispatch.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(get_proc_addr(instance, "vkGetDeviceProcAddr"));
    if (UNLIKELY(!dispatch.GetDeviceProcAddr)) {
        ALOGE("missing driver proc: %s", "vkGetDeviceProcAddr");
        success = false;
    }
    dispatch.CreateImage = reinterpret_cast<PFN_vkCreateImage>(get_proc_addr(instance, "vkCreateImage"));
    if (UNLIKELY(!dispatch.CreateImage)) {
        ALOGE("missing driver proc: %s", "vkCreateImage");
        success = false;
    }
    dispatch.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(get_proc_addr(instance, "vkDestroyImage"));
    if (UNLIKELY(!dispatch.DestroyImage)) {
        ALOGE("missing driver proc: %s", "vkDestroyImage");
        success = false;
    }
    dispatch.GetSwapchainGrallocUsageANDROID = reinterpret_cast<PFN_vkGetSwapchainGrallocUsageANDROID>(get_proc_addr(instance, "vkGetSwapchainGrallocUsageANDROID"));
    if (UNLIKELY(!dispatch.GetSwapchainGrallocUsageANDROID)) {
        ALOGE("missing driver proc: %s", "vkGetSwapchainGrallocUsageANDROID");
        success = false;
    }
    dispatch.AcquireImageANDROID = reinterpret_cast<PFN_vkAcquireImageANDROID>(get_proc_addr(instance, "vkAcquireImageANDROID"));
    if (UNLIKELY(!dispatch.AcquireImageANDROID)) {
        ALOGE("missing driver proc: %s", "vkAcquireImageANDROID");
        success = false;
    }
    dispatch.QueueSignalReleaseImageANDROID = reinterpret_cast<PFN_vkQueueSignalReleaseImageANDROID>(get_proc_addr(instance, "vkQueueSignalReleaseImageANDROID"));
    if (UNLIKELY(!dispatch.QueueSignalReleaseImageANDROID)) {
        ALOGE("missing driver proc: %s", "vkQueueSignalReleaseImageANDROID");
        success = false;
    }
    // clang-format on
    return success;
}

}  // namespace vulkan

// clang-format off

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    return CreateInstance_Top(pCreateInfo, pAllocator, pInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    DestroyInstance_Top(instance, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    return GetDispatchTable(instance).EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

__attribute__((visibility("default")))
VKAPI_ATTR PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    return GetDeviceProcAddr_Top(device, pName);
}

__attribute__((visibility("default")))
VKAPI_ATTR PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    return GetInstanceProcAddr_Top(instance, pName);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceProperties(physicalDevice, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    return GetDispatchTable(physicalDevice).GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    return GetDispatchTable(physicalDevice).CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    DestroyDevice_Top(device, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    return EnumerateInstanceLayerProperties_Top(pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    return EnumerateInstanceExtensionProperties_Top(pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    return GetDispatchTable(physicalDevice).EnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    return GetDispatchTable(physicalDevice).EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    GetDeviceQueue_Top(device, queueFamilyIndex, queueIndex, pQueue);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    return GetDispatchTable(queue).QueueSubmit(queue, submitCount, pSubmits, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueWaitIdle(VkQueue queue) {
    return GetDispatchTable(queue).QueueWaitIdle(queue);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkDeviceWaitIdle(VkDevice device) {
    return GetDispatchTable(device).DeviceWaitIdle(device);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    return GetDispatchTable(device).AllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).FreeMemory(device, memory, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    return GetDispatchTable(device).MapMemory(device, memory, offset, size, flags, ppData);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    GetDispatchTable(device).UnmapMemory(device, memory);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return GetDispatchTable(device).FlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    return GetDispatchTable(device).InvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    GetDispatchTable(device).GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    GetDispatchTable(device).GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return GetDispatchTable(device).BindBufferMemory(device, buffer, memory, memoryOffset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    GetDispatchTable(device).GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    return GetDispatchTable(device).BindImageMemory(device, image, memory, memoryOffset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    GetDispatchTable(device).GetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    GetDispatchTable(physicalDevice).GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    return GetDispatchTable(queue).QueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    return GetDispatchTable(device).CreateFence(device, pCreateInfo, pAllocator, pFence);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyFence(device, fence, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    return GetDispatchTable(device).ResetFences(device, fenceCount, pFences);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetFenceStatus(VkDevice device, VkFence fence) {
    return GetDispatchTable(device).GetFenceStatus(device, fence);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    return GetDispatchTable(device).WaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    return GetDispatchTable(device).CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroySemaphore(device, semaphore, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    return GetDispatchTable(device).CreateEvent(device, pCreateInfo, pAllocator, pEvent);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyEvent(device, event, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetEventStatus(VkDevice device, VkEvent event) {
    return GetDispatchTable(device).GetEventStatus(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkSetEvent(VkDevice device, VkEvent event) {
    return GetDispatchTable(device).SetEvent(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetEvent(VkDevice device, VkEvent event) {
    return GetDispatchTable(device).ResetEvent(device, event);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    return GetDispatchTable(device).CreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyQueryPool(device, queryPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    return GetDispatchTable(device).GetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    return GetDispatchTable(device).CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyBuffer(device, buffer, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    return GetDispatchTable(device).CreateBufferView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyBufferView(device, bufferView, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    return GetDispatchTable(device).CreateImage(device, pCreateInfo, pAllocator, pImage);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyImage(device, image, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    GetDispatchTable(device).GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    return GetDispatchTable(device).CreateImageView(device, pCreateInfo, pAllocator, pView);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyImageView(device, imageView, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    return GetDispatchTable(device).CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyShaderModule(device, shaderModule, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    return GetDispatchTable(device).CreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyPipelineCache(device, pipelineCache, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    return GetDispatchTable(device).GetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    return GetDispatchTable(device).MergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetDispatchTable(device).CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    return GetDispatchTable(device).CreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyPipeline(device, pipeline, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    return GetDispatchTable(device).CreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    return GetDispatchTable(device).CreateSampler(device, pCreateInfo, pAllocator, pSampler);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroySampler(device, sampler, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    return GetDispatchTable(device).CreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    return GetDispatchTable(device).CreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyDescriptorPool(device, descriptorPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    return GetDispatchTable(device).ResetDescriptorPool(device, descriptorPool, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    return GetDispatchTable(device).AllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    return GetDispatchTable(device).FreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    GetDispatchTable(device).UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    return GetDispatchTable(device).CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyFramebuffer(device, framebuffer, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    return GetDispatchTable(device).CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyRenderPass(device, renderPass, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    GetDispatchTable(device).GetRenderAreaGranularity(device, renderPass, pGranularity);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    return GetDispatchTable(device).CreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroyCommandPool(device, commandPool, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    return GetDispatchTable(device).ResetCommandPool(device, commandPool, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    return AllocateCommandBuffers_Top(device, pAllocateInfo, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    GetDispatchTable(device).FreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    return GetDispatchTable(commandBuffer).BeginCommandBuffer(commandBuffer, pBeginInfo);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return GetDispatchTable(commandBuffer).EndCommandBuffer(commandBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    return GetDispatchTable(commandBuffer).ResetCommandBuffer(commandBuffer, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    GetDispatchTable(commandBuffer).CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    GetDispatchTable(commandBuffer).CmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    GetDispatchTable(commandBuffer).CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    GetDispatchTable(commandBuffer).CmdSetLineWidth(commandBuffer, lineWidth);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    GetDispatchTable(commandBuffer).CmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
    GetDispatchTable(commandBuffer).CmdSetBlendConstants(commandBuffer, blendConstants);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    GetDispatchTable(commandBuffer).CmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    GetDispatchTable(commandBuffer).CmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    GetDispatchTable(commandBuffer).CmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    GetDispatchTable(commandBuffer).CmdSetStencilReference(commandBuffer, faceMask, reference);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    GetDispatchTable(commandBuffer).CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    GetDispatchTable(commandBuffer).CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    GetDispatchTable(commandBuffer).CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    GetDispatchTable(commandBuffer).CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    GetDispatchTable(commandBuffer).CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetDispatchTable(commandBuffer).CmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    GetDispatchTable(commandBuffer).CmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z) {
    GetDispatchTable(commandBuffer).CmdDispatch(commandBuffer, x, y, z);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    GetDispatchTable(commandBuffer).CmdDispatchIndirect(commandBuffer, buffer, offset);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    GetDispatchTable(commandBuffer).CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    GetDispatchTable(commandBuffer).CmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    GetDispatchTable(commandBuffer).CmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetDispatchTable(commandBuffer).CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    GetDispatchTable(commandBuffer).CmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const uint32_t* pData) {
    GetDispatchTable(commandBuffer).CmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    GetDispatchTable(commandBuffer).CmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetDispatchTable(commandBuffer).CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    GetDispatchTable(commandBuffer).CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    GetDispatchTable(commandBuffer).CmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    GetDispatchTable(commandBuffer).CmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetDispatchTable(commandBuffer).CmdSetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    GetDispatchTable(commandBuffer).CmdResetEvent(commandBuffer, event, stageMask);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    GetDispatchTable(commandBuffer).CmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    GetDispatchTable(commandBuffer).CmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    GetDispatchTable(commandBuffer).CmdBeginQuery(commandBuffer, queryPool, query, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    GetDispatchTable(commandBuffer).CmdEndQuery(commandBuffer, queryPool, query);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    GetDispatchTable(commandBuffer).CmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    GetDispatchTable(commandBuffer).CmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    GetDispatchTable(commandBuffer).CmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    GetDispatchTable(commandBuffer).CmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    GetDispatchTable(commandBuffer).CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    GetDispatchTable(commandBuffer).CmdNextSubpass(commandBuffer, contents);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    GetDispatchTable(commandBuffer).CmdEndRenderPass(commandBuffer);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    GetDispatchTable(commandBuffer).CmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(instance).DestroySurfaceKHR(instance, surface, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
    return GetDispatchTable(physicalDevice).GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, pSupported);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    return GetDispatchTable(physicalDevice).GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    return GetDispatchTable(physicalDevice).GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    return GetDispatchTable(physicalDevice).GetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, pPresentModeCount, pPresentModes);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    return GetDispatchTable(device).CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

__attribute__((visibility("default")))
VKAPI_ATTR void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    GetDispatchTable(device).DestroySwapchainKHR(device, swapchain, pAllocator);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    return GetDispatchTable(device).GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    return GetDispatchTable(device).AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    return GetDispatchTable(queue).QueuePresentKHR(queue, pPresentInfo);
}

__attribute__((visibility("default")))
VKAPI_ATTR VkResult vkCreateAndroidSurfaceKHR(VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    return GetDispatchTable(instance).CreateAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
}

// clang-format on
