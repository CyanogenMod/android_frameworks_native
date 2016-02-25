/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef LIBVULKAN_DEBUG_REPORT_H
#define LIBVULKAN_DEBUG_REPORT_H 1

#include <shared_mutex>

namespace vulkan {

// clang-format off
VKAPI_ATTR VkResult CreateDebugReportCallbackEXT_Bottom(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
VKAPI_ATTR void DestroyDebugReportCallbackEXT_Bottom(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
VKAPI_ATTR void DebugReportMessageEXT_Bottom(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage);
// clang-format on

class DebugReportCallbackList {
   public:
    DebugReportCallbackList()
        : head_{nullptr, 0, nullptr, nullptr, VK_NULL_HANDLE} {}
    DebugReportCallbackList(const DebugReportCallbackList&) = delete;
    DebugReportCallbackList& operator=(const DebugReportCallbackList&) = delete;
    ~DebugReportCallbackList() = default;

    VkResult CreateCallback(
        VkInstance instance,
        const VkDebugReportCallbackCreateInfoEXT* create_info,
        const VkAllocationCallbacks* allocator,
        VkDebugReportCallbackEXT* callback);
    void DestroyCallback(VkInstance instance,
                         VkDebugReportCallbackEXT callback,
                         const VkAllocationCallbacks* allocator);
    void Message(VkDebugReportFlagsEXT flags,
                 VkDebugReportObjectTypeEXT object_type,
                 uint64_t object,
                 size_t location,
                 int32_t message_code,
                 const char* layer_prefix,
                 const char* message);

   private:
    struct Node {
        Node* next;
        VkDebugReportFlagsEXT flags;
        PFN_vkDebugReportCallbackEXT callback;
        void* data;
        VkDebugReportCallbackEXT driver_callback;
    };

    // TODO(jessehall): replace with std::shared_mutex when available in libc++
    std::shared_timed_mutex rwmutex_;
    Node head_;
};

}  // namespace vulkan

#endif  // LIBVULKAN_DEBUG_REPORT_H
