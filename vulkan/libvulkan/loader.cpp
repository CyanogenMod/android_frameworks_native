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

#include "loader.h"

namespace vulkan {

const VkAllocationCallbacks* GetAllocator(VkInstance vkinstance) {
    return &driver::GetData(vkinstance).allocator;
}

const VkAllocationCallbacks* GetAllocator(VkDevice vkdevice) {
    return &driver::GetData(vkdevice).allocator;
}

VkInstance GetDriverInstance(VkInstance instance) {
    return instance;
}

const driver::InstanceDriverTable& GetDriverDispatch(VkInstance instance) {
    return driver::GetData(instance).driver;
}

const driver::DeviceDriverTable& GetDriverDispatch(VkDevice device) {
    return driver::GetData(device).driver;
}

const driver::DeviceDriverTable& GetDriverDispatch(VkQueue queue) {
    return driver::GetData(queue).driver;
}

DebugReportCallbackList& GetDebugReportCallbacks(VkInstance instance) {
    return driver::GetData(instance).debug_report_callbacks;
}

}  // namespace vulkan
