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

#ifndef __VK_EXT_ANDROID_NATIVE_BUFFER_H__
#define __VK_EXT_ANDROID_NATIVE_BUFFER_H__

#include <vulkan/vulkan.h>
#include <system/window.h>

// TODO(jessehall): Get a real extension number officially assigned.
#define VK_EXT_ANDROID_NATIVE_BUFFER_EXTENSION_NUMBER 1024
#define VK_EXT_ANDROID_NATIVE_BUFFER_REVISION         1
#define VK_EXT_ANDROID_NATIVE_BUFFER_EXTENSION_NAME   "VK_EXT_ANDROID_gralloc"

#ifdef __cplusplus
extern "C" {
#endif

// See https://gitlab.khronos.org/vulkan/vulkan/blob/master/doc/proposals/proposed/NVIDIA/VulkanRegistryProposal.txt
// and Khronos bug 14154 for explanation of these magic numbers.
#define VK_EXT_ANDROID_NATIVE_BUFFER_ENUM(type,id)    ((type)((int)0xc0000000 - VK_EXT_ANDROID_NATIVE_BUFFER_EXTENSION_NUMBER * -1024 + (id)))
#define VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID       VK_EXT_ANDROID_NATIVE_BUFFER_ENUM(VkStructureType, 0)

typedef struct {
    VkStructureType             sType; // must be VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID
    const void*                 pNext;

    // Buffer handle and stride returned from gralloc alloc()
    buffer_handle_t             handle;
    int                         stride;

    // Gralloc format and usage requested when the buffer was allocated.
    int                         format;
    int                         usage;
} VkNativeBufferANDROID;

typedef VkResult (VKAPI *PFN_vkGetSwapchainGrallocUsageANDROID)(VkDevice device, VkFormat format, VkImageUsageFlags imageUsage, int* grallocUsage);
typedef VkResult (VKAPI *PFN_vkImportNativeFenceANDROID)(VkDevice device, VkSemaphore semaphore, int nativeFenceFd);
typedef VkResult (VKAPI *PFN_vkQueueSignalNativeFenceANDROID)(VkQueue queue, int* pNativeFenceFd);

#ifdef VK_PROTOTYPES
VkResult VKAPI vkGetSwapchainGrallocUsageANDROID(
    VkDevice            device,
    VkFormat            format,
    VkImageUsageFlags   imageUsage,
    int*                grallocUsage
);
VkResult VKAPI vkImportNativeFenceANDROID(
    VkDevice            device,
    VkSemaphore         semaphore,
    int                 nativeFenceFd
);
VkResult VKAPI vkQueueSignalNativeFenceANDROID(
    VkQueue             queue,
    int*                pNativeFenceFd
);
#endif

#ifdef __cplusplus
}
#endif

#endif // __VK_EXT_ANDROID_NATIVE_BUFFER_H__
