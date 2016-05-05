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

#include <vulkan/vulkan.h>
#include <shared_mutex>

namespace vulkan {
namespace driver {

// clang-format off
VKAPI_ATTR VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
VKAPI_ATTR void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
VKAPI_ATTR void DebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage);
// clang-format on

class DebugReportCallbackList {
   private:
    // forward declaration
    struct Node;

   public:
    DebugReportCallbackList()
        : head_{nullptr, 0, nullptr, nullptr, VK_NULL_HANDLE} {}
    DebugReportCallbackList(const DebugReportCallbackList&) = delete;
    DebugReportCallbackList& operator=(const DebugReportCallbackList&) = delete;
    ~DebugReportCallbackList() = default;

    Node* AddCallback(const VkDebugReportCallbackCreateInfoEXT& info,
                      VkDebugReportCallbackEXT driver_handle,
                      const VkAllocationCallbacks& allocator);
    void RemoveCallback(Node* node, const VkAllocationCallbacks& allocator);

    void Message(VkDebugReportFlagsEXT flags,
                 VkDebugReportObjectTypeEXT object_type,
                 uint64_t object,
                 size_t location,
                 int32_t message_code,
                 const char* layer_prefix,
                 const char* message);

    static Node* FromHandle(VkDebugReportCallbackEXT handle) {
        return reinterpret_cast<Node*>(uintptr_t(handle));
    }

    static VkDebugReportCallbackEXT GetHandle(const Node* node) {
        return VkDebugReportCallbackEXT(reinterpret_cast<uintptr_t>(node));
    }

    static VkDebugReportCallbackEXT GetDriverHandle(const Node* node) {
        return node->driver_handle;
    }

   private:
    struct Node {
        Node* next;

        VkDebugReportFlagsEXT flags;
        PFN_vkDebugReportCallbackEXT callback;
        void* user_data;

        VkDebugReportCallbackEXT driver_handle;
    };

    // TODO(jessehall): replace with std::shared_mutex when available in libc++
    std::shared_timed_mutex rwmutex_;
    Node head_;
};

}  // namespace driver
}  // namespace vulkan

#endif  // LIBVULKAN_DEBUG_REPORT_H
