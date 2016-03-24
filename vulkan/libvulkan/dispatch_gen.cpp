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

// WARNING: This file is generated. See ../README.md for instructions.

#include <log/log.h>
#include <algorithm>
#include "loader.h"

#define UNLIKELY(expr) __builtin_expect((expr), 0)

using namespace vulkan;

namespace vulkan {

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
    dispatch.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(get_proc_addr(instance, "vkDestroyDevice"));
    if (UNLIKELY(!dispatch.DestroyDevice)) {
        ALOGE("missing driver proc: %s", "vkDestroyDevice");
        success = false;
    }
    dispatch.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(get_proc_addr(instance, "vkGetDeviceQueue"));
    if (UNLIKELY(!dispatch.GetDeviceQueue)) {
        ALOGE("missing driver proc: %s", "vkGetDeviceQueue");
        success = false;
    }
    dispatch.AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(get_proc_addr(instance, "vkAllocateCommandBuffers"));
    if (UNLIKELY(!dispatch.AllocateCommandBuffers)) {
        ALOGE("missing driver proc: %s", "vkAllocateCommandBuffers");
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
