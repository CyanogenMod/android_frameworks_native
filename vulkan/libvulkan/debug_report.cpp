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

#include "loader.h"

namespace vulkan {
namespace driver {

VkResult DebugReportCallbackList::CreateCallback(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugReportCallbackEXT* callback) {
    VkDebugReportCallbackEXT driver_callback = VK_NULL_HANDLE;

    if (GetDriverDispatch(instance).CreateDebugReportCallbackEXT) {
        VkResult result =
            GetDriverDispatch(instance).CreateDebugReportCallbackEXT(
                GetDriverInstance(instance), create_info, allocator,
                &driver_callback);
        if (result != VK_SUCCESS)
            return result;
    }

    const VkAllocationCallbacks* alloc =
        allocator ? allocator : GetAllocator(instance);
    void* mem =
        alloc->pfnAllocation(alloc->pUserData, sizeof(Node), alignof(Node),
                             VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!mem) {
        if (GetDriverDispatch(instance).DestroyDebugReportCallbackEXT) {
            GetDriverDispatch(instance).DestroyDebugReportCallbackEXT(
                GetDriverInstance(instance), driver_callback, allocator);
        }
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    std::lock_guard<decltype(rwmutex_)> lock(rwmutex_);
    head_.next =
        new (mem) Node{head_.next, create_info->flags, create_info->pfnCallback,
                       create_info->pUserData, driver_callback};
    *callback =
        VkDebugReportCallbackEXT(reinterpret_cast<uintptr_t>(head_.next));
    return VK_SUCCESS;
}

void DebugReportCallbackList::DestroyCallback(
    VkInstance instance,
    VkDebugReportCallbackEXT callback,
    const VkAllocationCallbacks* allocator) {
    Node* node = reinterpret_cast<Node*>(uintptr_t(callback));
    std::unique_lock<decltype(rwmutex_)> lock(rwmutex_);
    Node* prev = &head_;
    while (prev && prev->next != node)
        prev = prev->next;
    prev->next = node->next;
    lock.unlock();

    if (GetDriverDispatch(instance).DestroyDebugReportCallbackEXT) {
        GetDriverDispatch(instance).DestroyDebugReportCallbackEXT(
            GetDriverInstance(instance), node->driver_callback, allocator);
    }

    const VkAllocationCallbacks* alloc =
        allocator ? allocator : GetAllocator(instance);
    alloc->pfnFree(alloc->pUserData, node);
}

void DebugReportCallbackList::Message(VkDebugReportFlagsEXT flags,
                                      VkDebugReportObjectTypeEXT object_type,
                                      uint64_t object,
                                      size_t location,
                                      int32_t message_code,
                                      const char* layer_prefix,
                                      const char* message) {
    std::shared_lock<decltype(rwmutex_)> lock(rwmutex_);
    Node* node = &head_;
    while ((node = node->next)) {
        if ((node->flags & flags) != 0) {
            node->callback(flags, object_type, object, location, message_code,
                           layer_prefix, message, node->data);
        }
    }
}

VkResult CreateDebugReportCallbackEXT(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugReportCallbackEXT* callback) {
    return GetDebugReportCallbacks(instance).CreateCallback(
        instance, create_info, allocator, callback);
}

void DestroyDebugReportCallbackEXT(VkInstance instance,
                                   VkDebugReportCallbackEXT callback,
                                   const VkAllocationCallbacks* allocator) {
    if (callback)
        GetDebugReportCallbacks(instance).DestroyCallback(instance, callback,
                                                          allocator);
}

void DebugReportMessageEXT(VkInstance instance,
                           VkDebugReportFlagsEXT flags,
                           VkDebugReportObjectTypeEXT object_type,
                           uint64_t object,
                           size_t location,
                           int32_t message_code,
                           const char* layer_prefix,
                           const char* message) {
    if (GetDriverDispatch(instance).DebugReportMessageEXT) {
        GetDriverDispatch(instance).DebugReportMessageEXT(
            GetDriverInstance(instance), flags, object_type, object, location,
            message_code, layer_prefix, message);
    }
    GetDebugReportCallbacks(instance).Message(flags, object_type, object,
                                              location, message_code,
                                              layer_prefix, message);
}

}  // namespace driver
}  // namespace vulkan
