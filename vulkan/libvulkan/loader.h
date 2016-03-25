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

#ifndef LIBVULKAN_LOADER_H
#define LIBVULKAN_LOADER_H 1

#include <bitset>
#include <vulkan/vulkan.h>
#include "debug_report.h"
#include "driver.h"
#include "swapchain.h"

struct hwvulkan_device_t;

namespace vulkan {

// -----------------------------------------------------------------------------
// loader.cpp

const VkAllocationCallbacks* GetAllocator(VkInstance instance);
const VkAllocationCallbacks* GetAllocator(VkDevice device);
VkInstance GetDriverInstance(VkInstance instance);
const driver::InstanceDriverTable& GetDriverDispatch(VkInstance instance);
const driver::DeviceDriverTable& GetDriverDispatch(VkDevice device);
const driver::DeviceDriverTable& GetDriverDispatch(VkQueue queue);
driver::DebugReportCallbackList& GetDebugReportCallbacks(VkInstance instance);

}  // namespace vulkan

#endif  // LIBVULKAN_LOADER_H
