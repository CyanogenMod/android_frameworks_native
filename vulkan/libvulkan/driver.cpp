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

#include <sys/prctl.h>

#include "driver.h"
#include "loader.h"

namespace vulkan {
namespace driver {

namespace {

hwvulkan_device_t* g_hwdevice = nullptr;

}  // anonymous namespace

bool Debuggable() {
    return (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) >= 0);
}

bool OpenHAL() {
    if (g_hwdevice)
        return true;

    const hwvulkan_module_t* module;
    int result =
        hw_get_module("vulkan", reinterpret_cast<const hw_module_t**>(&module));
    if (result != 0) {
        ALOGE("failed to load vulkan hal: %s (%d)", strerror(-result), result);
        return false;
    }

    hwvulkan_device_t* device;
    result =
        module->common.methods->open(&module->common, HWVULKAN_DEVICE_0,
                                     reinterpret_cast<hw_device_t**>(&device));
    if (result != 0) {
        ALOGE("failed to open vulkan driver: %s (%d)", strerror(-result),
              result);
        return false;
    }

    if (!InitLoader(device)) {
        device->common.close(&device->common);
        return false;
    }

    g_hwdevice = device;

    return true;
}

}  // namespace driver
}  // namespace vulkan
