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

// module header
#include "loader.h"
// standard C headers
#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
// standard C++ headers
#include <algorithm>
#include <mutex>
// platform/library headers
#include <hardware/hwvulkan.h>
#include <log/log.h>

using namespace vulkan;

static const uint32_t kMaxPhysicalDevices = 4;

struct VkInstance_T {
    VkInstance_T(const VkAllocCallbacks* alloc_callbacks)
        : vtbl(&vtbl_storage), alloc(alloc_callbacks), num_physical_devices(0) {
        memset(&vtbl_storage, 0, sizeof(vtbl_storage));
        memset(physical_devices, 0, sizeof(physical_devices));
        memset(&drv.vtbl, 0, sizeof(drv.vtbl));
        drv.GetDeviceProcAddr = nullptr;
        drv.num_physical_devices = 0;
    }

    InstanceVtbl* vtbl;
    InstanceVtbl vtbl_storage;

    const VkAllocCallbacks* alloc;
    uint32_t num_physical_devices;
    VkPhysicalDevice physical_devices[kMaxPhysicalDevices];

    struct Driver {
        // Pointers to driver entry points. Used explicitly by the loader; not
        // set as the dispatch table for any objects.
        InstanceVtbl vtbl;

        // Pointer to the driver's get_device_proc_addr, must be valid for any
        // of the driver's physical devices. Not part of the InstanceVtbl since
        // it's not an Instance/PhysicalDevice function.
        PFN_vkGetDeviceProcAddr GetDeviceProcAddr;

        // Number of physical devices owned by this driver.
        uint32_t num_physical_devices;
    } drv;  // may eventually be an array
};

// -----------------------------------------------------------------------------

namespace {

typedef VkInstance_T Instance;

struct Device {
    Device(const VkAllocCallbacks* alloc_callbacks) : alloc(alloc_callbacks) {
        memset(&vtbl_storage, 0, sizeof(vtbl_storage));
        vtbl_storage.device = this;
    }
    DeviceVtbl vtbl_storage;
    const VkAllocCallbacks* alloc;
};

// -----------------------------------------------------------------------------
// Utility Code

inline const InstanceVtbl* GetVtbl(VkPhysicalDevice physicalDevice) {
    return *reinterpret_cast<InstanceVtbl**>(physicalDevice);
}

inline const DeviceVtbl* GetVtbl(VkDevice device) {
    return *reinterpret_cast<DeviceVtbl**>(device);
}

void* DefaultAlloc(void*, size_t size, size_t alignment, VkSystemAllocType) {
    return memalign(alignment, size);
}

void DefaultFree(void*, void* pMem) {
    free(pMem);
}

const VkAllocCallbacks kDefaultAllocCallbacks = {
    .pUserData = nullptr,
    .pfnAlloc = DefaultAlloc,
    .pfnFree = DefaultFree,
};

hwvulkan_device_t* g_hwdevice;
bool EnsureInitialized() {
    static std::once_flag once_flag;
    static const hwvulkan_module_t* module;

    std::call_once(once_flag, []() {
        int result;
        result = hw_get_module("vulkan",
                               reinterpret_cast<const hw_module_t**>(&module));
        if (result != 0) {
            ALOGE("failed to load vulkan hal: %s (%d)", strerror(-result),
                  result);
            return;
        }
        result = module->common.methods->open(
            &module->common, HWVULKAN_DEVICE_0,
            reinterpret_cast<hw_device_t**>(&g_hwdevice));
        if (result != 0) {
            ALOGE("failed to open vulkan driver: %s (%d)", strerror(-result),
                  result);
            module = nullptr;
            return;
        }
    });

    return module != nullptr && g_hwdevice != nullptr;
}

void DestroyDevice(Device* device) {
    const VkAllocCallbacks* alloc = device->alloc;
    device->~Device();
    alloc->pfnFree(alloc->pUserData, device);
}

// -----------------------------------------------------------------------------
// "Bottom" functions. These are called at the end of the instance dispatch
// chain.

VkResult DestroyInstanceBottom(VkInstance instance) {
    // These checks allow us to call DestroyInstanceBottom from any error path
    // in CreateInstanceBottom, before the driver instance is fully initialized.
    if (instance->drv.vtbl.instance != VK_NULL_HANDLE &&
        instance->drv.vtbl.DestroyInstance) {
        instance->drv.vtbl.DestroyInstance(instance->drv.vtbl.instance);
    }
    const VkAllocCallbacks* alloc = instance->alloc;
    instance->~VkInstance_T();
    alloc->pfnFree(alloc->pUserData, instance);
    return VK_SUCCESS;
}

VkResult CreateInstanceBottom(const VkInstanceCreateInfo* create_info,
                              VkInstance* instance_ptr) {
    Instance* instance = *instance_ptr;
    VkResult result;

    result =
        g_hwdevice->CreateInstance(create_info, &instance->drv.vtbl.instance);
    if (result != VK_SUCCESS) {
        DestroyInstanceBottom(instance);
        return result;
    }

    if (!LoadInstanceVtbl(instance->drv.vtbl.instance,
                          g_hwdevice->GetInstanceProcAddr,
                          instance->drv.vtbl)) {
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // vkGetDeviceProcAddr has a bootstrapping problem. We require that it be
    // queryable from the Instance, and that the resulting function work for any
    // VkDevice created from the instance.
    instance->drv.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        g_hwdevice->GetInstanceProcAddr(instance->drv.vtbl.instance,
                                        "vkGetDeviceProcAddr"));
    if (!instance->drv.GetDeviceProcAddr) {
        ALOGE("missing instance proc: \"%s\"", "vkGetDeviceProcAddr");
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    hwvulkan_dispatch_t* dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(instance->drv.vtbl.instance);
    if (dispatch->magic == HWVULKAN_DISPATCH_MAGIC) {
        // Skip setting dispatch->vtbl on the driver instance handle, since we
        // never intentionally call through it; we go through Instance::drv.vtbl
        // instead.
    } else {
        ALOGE("invalid VkInstance dispatch magic: 0x%" PRIxPTR,
              dispatch->magic);
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t num_physical_devices = 0;
    result = instance->drv.vtbl.EnumeratePhysicalDevices(
        instance->drv.vtbl.instance, &num_physical_devices, nullptr);
    if (result != VK_SUCCESS) {
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    num_physical_devices = std::min(num_physical_devices, kMaxPhysicalDevices);
    result = instance->drv.vtbl.EnumeratePhysicalDevices(
        instance->drv.vtbl.instance, &num_physical_devices,
        instance->physical_devices);
    if (result != VK_SUCCESS) {
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    for (uint32_t i = 0; i < num_physical_devices; i++) {
        dispatch = reinterpret_cast<hwvulkan_dispatch_t*>(
            instance->physical_devices[i]);
        if (dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
            ALOGE("invalid VkPhysicalDevice dispatch magic: 0x%" PRIxPTR,
                  dispatch->magic);
            DestroyInstanceBottom(instance);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        dispatch->vtbl = instance->vtbl;
    }
    instance->drv.num_physical_devices = num_physical_devices;

    instance->num_physical_devices = instance->drv.num_physical_devices;
    return VK_SUCCESS;
}

VkResult EnumeratePhysicalDevicesBottom(VkInstance instance,
                                        uint32_t* pdev_count,
                                        VkPhysicalDevice* pdevs) {
    uint32_t count = instance->num_physical_devices;
    if (pdevs) {
        count = std::min(count, *pdev_count);
        std::copy(instance->physical_devices,
                  instance->physical_devices + count, pdevs);
    }
    *pdev_count = count;
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceFeaturesBottom(VkPhysicalDevice pdev,
                                         VkPhysicalDeviceFeatures* features) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceFeatures(pdev, features);
}

VkResult GetPhysicalDeviceFormatPropertiesBottom(
    VkPhysicalDevice pdev,
    VkFormat format,
    VkFormatProperties* properties) {
    return GetVtbl(pdev)->instance->drv.vtbl.GetPhysicalDeviceFormatProperties(
        pdev, format, properties);
}

VkResult GetPhysicalDeviceImageFormatPropertiesBottom(
    VkPhysicalDevice pdev,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageFormatProperties* properties) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceImageFormatProperties(
            pdev, format, type, tiling, usage, properties);
}

VkResult GetPhysicalDeviceLimitsBottom(VkPhysicalDevice pdev,
                                       VkPhysicalDeviceLimits* limits) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceLimits(pdev, limits);
}

VkResult GetPhysicalDevicePropertiesBottom(
    VkPhysicalDevice pdev,
    VkPhysicalDeviceProperties* properties) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceProperties(pdev, properties);
}

VkResult GetPhysicalDeviceQueueCountBottom(VkPhysicalDevice pdev,
                                           uint32_t* count) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceQueueCount(pdev, count);
}

VkResult GetPhysicalDeviceQueuePropertiesBottom(
    VkPhysicalDevice pdev,
    uint32_t count,
    VkPhysicalDeviceQueueProperties* properties) {
    return GetVtbl(pdev)->instance->drv.vtbl.GetPhysicalDeviceQueueProperties(
        pdev, count, properties);
}

VkResult GetPhysicalDeviceMemoryPropertiesBottom(
    VkPhysicalDevice pdev,
    VkPhysicalDeviceMemoryProperties* properties) {
    return GetVtbl(pdev)->instance->drv.vtbl.GetPhysicalDeviceMemoryProperties(
        pdev, properties);
}

VkResult CreateDeviceBottom(VkPhysicalDevice pdev,
                            const VkDeviceCreateInfo* create_info,
                            VkDevice* out_device) {
    const Instance& instance = *static_cast<Instance*>(GetVtbl(pdev)->instance);
    VkResult result;

    void* mem = instance.alloc->pfnAlloc(instance.alloc->pUserData,
                                         sizeof(Device), alignof(Device),
                                         VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
    if (!mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Device* device = new (mem) Device(instance.alloc);

    VkDevice drv_device;
    result = instance.drv.vtbl.CreateDevice(pdev, create_info, &drv_device);
    if (result != VK_SUCCESS) {
        DestroyDevice(device);
        return result;
    }

    if (!LoadDeviceVtbl(drv_device, instance.drv.GetDeviceProcAddr,
                        device->vtbl_storage)) {
        if (device->vtbl_storage.DestroyDevice)
            device->vtbl_storage.DestroyDevice(drv_device);
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    hwvulkan_dispatch_t* dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(drv_device);
    if (dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
        ALOGE("invalid VkDevice dispatch magic: 0x%" PRIxPTR, dispatch->magic);
        device->vtbl_storage.DestroyDevice(drv_device);
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    dispatch->vtbl = &device->vtbl_storage;

    // TODO: insert device layer entry points into device->vtbl_storage here?

    *out_device = drv_device;
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceExtensionPropertiesBottom(
    VkPhysicalDevice pdev,
    const char* layer_name,
    uint32_t* properties_count,
    VkExtensionProperties* properties) {
    // TODO: what are we supposed to do with layer_name here?
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceExtensionProperties(
            pdev, layer_name, properties_count, properties);
}

VkResult GetPhysicalDeviceLayerPropertiesBottom(VkPhysicalDevice pdev,
                                                uint32_t* properties_count,
                                                VkLayerProperties* properties) {
    return GetVtbl(pdev)->instance->drv.vtbl.GetPhysicalDeviceLayerProperties(
        pdev, properties_count, properties);
}

VkResult GetPhysicalDeviceSparseImageFormatPropertiesBottom(
    VkPhysicalDevice pdev,
    VkFormat format,
    VkImageType type,
    uint32_t samples,
    VkImageUsageFlags usage,
    VkImageTiling tiling,
    uint32_t* properties_count,
    VkSparseImageFormatProperties* properties) {
    return GetVtbl(pdev)
        ->instance->drv.vtbl.GetPhysicalDeviceSparseImageFormatProperties(
            pdev, format, type, samples, usage, tiling, properties_count,
            properties);
}

PFN_vkVoidFunction GetInstanceProcAddrBottom(VkInstance, const char*);

const InstanceVtbl kBottomInstanceFunctions = {
    // clang-format off
    .instance = nullptr,
    .CreateInstance = CreateInstanceBottom,
    .DestroyInstance = DestroyInstanceBottom,
    .GetInstanceProcAddr = GetInstanceProcAddrBottom,
    .EnumeratePhysicalDevices = EnumeratePhysicalDevicesBottom,
    .GetPhysicalDeviceFeatures = GetPhysicalDeviceFeaturesBottom,
    .GetPhysicalDeviceFormatProperties = GetPhysicalDeviceFormatPropertiesBottom,
    .GetPhysicalDeviceImageFormatProperties = GetPhysicalDeviceImageFormatPropertiesBottom,
    .GetPhysicalDeviceLimits = GetPhysicalDeviceLimitsBottom,
    .GetPhysicalDeviceProperties = GetPhysicalDevicePropertiesBottom,
    .GetPhysicalDeviceQueueCount = GetPhysicalDeviceQueueCountBottom,
    .GetPhysicalDeviceQueueProperties = GetPhysicalDeviceQueuePropertiesBottom,
    .GetPhysicalDeviceMemoryProperties = GetPhysicalDeviceMemoryPropertiesBottom,
    .CreateDevice = CreateDeviceBottom,
    .GetPhysicalDeviceExtensionProperties = GetPhysicalDeviceExtensionPropertiesBottom,
    .GetPhysicalDeviceLayerProperties = GetPhysicalDeviceLayerPropertiesBottom,
    .GetPhysicalDeviceSparseImageFormatProperties = GetPhysicalDeviceSparseImageFormatPropertiesBottom,
    // clang-format on
};

PFN_vkVoidFunction GetInstanceProcAddrBottom(VkInstance, const char* name) {
    // The bottom GetInstanceProcAddr is only called by the innermost layer,
    // when there is one, when it initializes its own dispatch table.
    return GetSpecificInstanceProcAddr(&kBottomInstanceFunctions, name);
}

}  // namespace

// -----------------------------------------------------------------------------
// Global functions. These are called directly from the loader entry points,
// without going through a dispatch table.

namespace vulkan {

VkResult GetGlobalExtensionProperties(const char* /*layer_name*/,
                                      uint32_t* count,
                                      VkExtensionProperties* /*properties*/) {
    if (!count)
        return VK_ERROR_INVALID_POINTER;
    if (!EnsureInitialized())
        return VK_ERROR_UNAVAILABLE;

    // TODO: not yet implemented
    ALOGW("vkGetGlobalExtensionProperties not implemented");

    *count = 0;
    return VK_SUCCESS;
}

VkResult GetGlobalLayerProperties(uint32_t* count,
                                  VkLayerProperties* /*properties*/) {
    if (!count)
        return VK_ERROR_INVALID_POINTER;
    if (!EnsureInitialized())
        return VK_ERROR_UNAVAILABLE;

    // TODO: not yet implemented
    ALOGW("vkGetGlobalLayerProperties not implemented");

    *count = 0;
    return VK_SUCCESS;
}

VkResult CreateInstance(const VkInstanceCreateInfo* create_info,
                        VkInstance* out_instance) {
    VkResult result;

    if (!EnsureInitialized())
        return VK_ERROR_UNAVAILABLE;

    VkInstanceCreateInfo local_create_info = *create_info;
    if (!local_create_info.pAllocCb)
        local_create_info.pAllocCb = &kDefaultAllocCallbacks;
    create_info = &local_create_info;

    void* instance_mem = create_info->pAllocCb->pfnAlloc(
        create_info->pAllocCb->pUserData, sizeof(Instance), alignof(Instance),
        VK_SYSTEM_ALLOC_TYPE_API_OBJECT);
    if (!instance_mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Instance* instance = new (instance_mem) Instance(create_info->pAllocCb);

    instance->vtbl_storage = kBottomInstanceFunctions;
    instance->vtbl_storage.instance = instance;

    // TODO: Insert enabled layers into instance->dispatch_vtbl here.

    // TODO: We'll want to call CreateInstance through the dispatch table
    // instead of calling the loader's terminator
    *out_instance = instance;
    result = CreateInstanceBottom(create_info, out_instance);
    if (result <= 0) {
        // For every layer, including the loader top and bottom layers:
        // - If a call to the next CreateInstance fails, the layer must clean
        //   up anything it has successfully done so far, and propagate the
        //   error upwards.
        // - If a layer successfully calls the next layer's CreateInstance, and
        //   afterwards must fail for some reason, it must call the next layer's
        //   DestroyInstance before returning.
        // - The layer must not call the next layer's DestroyInstance if that
        //   layer's CreateInstance wasn't called, or returned failure.

        // On failure, CreateInstanceBottom frees the instance struct, so it's
        // already gone at this point. Nothing to do.
    }

    return result;
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* name) {
    if (!instance)
        return GetGlobalInstanceProcAddr(name);
    // For special-case functions we always return the loader entry
    if (strcmp(name, "vkGetInstanceProcAddr") == 0 ||
        strcmp(name, "vkGetDeviceProcAddr") == 0) {
        return GetGlobalInstanceProcAddr(name);
    }
    return GetSpecificInstanceProcAddr(instance->vtbl, name);
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* name) {
    if (!device)
        return GetGlobalDeviceProcAddr(name);
    // For special-case functions we always return the loader entry
    if (strcmp(name, "vkGetDeviceQueue") == 0 ||
        strcmp(name, "vkCreateCommandBuffer") == 0 ||
        strcmp(name, "vkDestroyDevice") == 0) {
        return GetGlobalDeviceProcAddr(name);
    }
    return GetSpecificDeviceProcAddr(GetVtbl(device), name);
}

VkResult GetDeviceQueue(VkDevice drv_device,
                        uint32_t family,
                        uint32_t index,
                        VkQueue* out_queue) {
    VkResult result;
    VkQueue queue;
    const DeviceVtbl* vtbl = GetVtbl(drv_device);
    result = vtbl->GetDeviceQueue(drv_device, family, index, &queue);
    if (result != VK_SUCCESS)
        return result;
    hwvulkan_dispatch_t* dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(queue);
    if (dispatch->magic != HWVULKAN_DISPATCH_MAGIC && dispatch->vtbl != &vtbl) {
        ALOGE("invalid VkQueue dispatch magic: 0x%" PRIxPTR, dispatch->magic);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    dispatch->vtbl = vtbl;
    *out_queue = queue;
    return VK_SUCCESS;
}

VkResult CreateCommandBuffer(VkDevice drv_device,
                             const VkCmdBufferCreateInfo* create_info,
                             VkCmdBuffer* out_cmdbuf) {
    const DeviceVtbl* vtbl = GetVtbl(drv_device);
    VkCmdBuffer cmdbuf;
    VkResult result =
        vtbl->CreateCommandBuffer(drv_device, create_info, &cmdbuf);
    if (result != VK_SUCCESS)
        return result;
    hwvulkan_dispatch_t* dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(cmdbuf);
    if (dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
        ALOGE("invalid VkCmdBuffer dispatch magic: 0x%" PRIxPTR,
              dispatch->magic);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    dispatch->vtbl = vtbl;
    *out_cmdbuf = cmdbuf;
    return VK_SUCCESS;
}

VkResult DestroyDevice(VkDevice drv_device) {
    const DeviceVtbl* vtbl = GetVtbl(drv_device);
    Device* device = static_cast<Device*>(vtbl->device);
    vtbl->DestroyDevice(drv_device);
    DestroyDevice(device);
    return VK_SUCCESS;
}

}  // namespace vulkan
