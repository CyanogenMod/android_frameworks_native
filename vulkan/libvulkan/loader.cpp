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
#include "driver.h"
// standard C headers
#include <dirent.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
// standard C++ headers
#include <algorithm>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
// platform/library headers
#include <cutils/properties.h>
#include <hardware/hwvulkan.h>
#include <log/log.h>
#include <vulkan/vulkan_loader_data.h>
#include <vulkan/vk_layer_interface.h>

using namespace vulkan;

static const uint32_t kMaxPhysicalDevices = 4;

namespace {

// ----------------------------------------------------------------------------

// Standard-library allocator that delegates to VkAllocationCallbacks.
//
// TODO(jessehall): This class currently always uses
// VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE. The scope to use could be a template
// parameter or a constructor parameter. The former would help catch bugs
// where we use the wrong scope, e.g. adding a command-scope string to an
// instance-scope vector. But that might also be pretty annoying to deal with.
template <class T>
class CallbackAllocator {
   public:
    typedef T value_type;

    CallbackAllocator(const VkAllocationCallbacks* alloc_input)
        : alloc(alloc_input) {}

    template <class T2>
    CallbackAllocator(const CallbackAllocator<T2>& other)
        : alloc(other.alloc) {}

    T* allocate(std::size_t n) {
        void* mem =
            alloc->pfnAllocation(alloc->pUserData, n * sizeof(T), alignof(T),
                                 VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
        if (!mem)
            throw std::bad_alloc();
        return static_cast<T*>(mem);
    }

    void deallocate(T* array, std::size_t /*n*/) noexcept {
        alloc->pfnFree(alloc->pUserData, array);
    }

    const VkAllocationCallbacks* alloc;
};
// These are needed in order to move Strings
template <class T>
bool operator==(const CallbackAllocator<T>& alloc1,
                const CallbackAllocator<T>& alloc2) {
    return alloc1.alloc == alloc2.alloc;
}
template <class T>
bool operator!=(const CallbackAllocator<T>& alloc1,
                const CallbackAllocator<T>& alloc2) {
    return !(alloc1 == alloc2);
}

template <class T>
using Vector = std::vector<T, CallbackAllocator<T>>;

typedef std::basic_string<char, std::char_traits<char>, CallbackAllocator<char>>
    String;

// ----------------------------------------------------------------------------
// Global Data and Initialization

hwvulkan_device_t* g_hwdevice = nullptr;
InstanceExtensionSet g_driver_instance_extensions;

bool LoadVulkanHAL() {
    VkResult vkresult;
    uint32_t count;
    if ((vkresult = g_hwdevice->EnumerateInstanceExtensionProperties(
             nullptr, &count, nullptr)) != VK_SUCCESS) {
        ALOGE("driver EnumerateInstanceExtensionProperties failed: %d",
              vkresult);
        return false;
    }
    VkExtensionProperties* extensions = static_cast<VkExtensionProperties*>(
        alloca(count * sizeof(VkExtensionProperties)));
    if ((vkresult = g_hwdevice->EnumerateInstanceExtensionProperties(
             nullptr, &count, extensions)) != VK_SUCCESS) {
        ALOGE("driver EnumerateInstanceExtensionProperties failed: %d",
              vkresult);
        return false;
    }
    ALOGV_IF(count > 0, "Driver-supported instance extensions:");
    for (uint32_t i = 0; i < count; i++) {
        ALOGV("  %s (v%u)", extensions[i].extensionName,
              extensions[i].specVersion);
        InstanceExtension id =
            InstanceExtensionFromName(extensions[i].extensionName);
        if (id != kInstanceExtensionCount)
            g_driver_instance_extensions.set(id);
    }
    // Ignore driver attempts to support loader extensions
    g_driver_instance_extensions.reset(kKHR_surface);
    g_driver_instance_extensions.reset(kKHR_android_surface);

    return true;
}

// -----------------------------------------------------------------------------

struct Instance {
    Instance(const VkAllocationCallbacks* alloc_callbacks)
        : base(*alloc_callbacks),
          alloc(&base.allocator),
          num_physical_devices(0) {
        memset(physical_devices, 0, sizeof(physical_devices));
        enabled_extensions.reset();
        memset(&drv.dispatch, 0, sizeof(drv.dispatch));
    }

    ~Instance() {}

    driver::InstanceData base;

    const VkAllocationCallbacks* alloc;
    uint32_t num_physical_devices;
    VkPhysicalDevice physical_devices_top[kMaxPhysicalDevices];
    VkPhysicalDevice physical_devices[kMaxPhysicalDevices];
    DeviceExtensionSet physical_device_driver_extensions[kMaxPhysicalDevices];

    DebugReportCallbackList debug_report_callbacks;
    InstanceExtensionSet enabled_extensions;

    struct {
        DriverDispatchTable dispatch;
    } drv;  // may eventually be an array
};

struct Device {
    Device(Instance* instance_) : base(*instance_->alloc), instance(instance_) {
        enabled_extensions.reset();
    }

    driver::DeviceData base;

    Instance* instance;
    DeviceExtensionSet enabled_extensions;
};

template <typename THandle>
struct HandleTraits {};
template <>
struct HandleTraits<VkInstance> {
    typedef Instance LoaderObjectType;
};
template <>
struct HandleTraits<VkPhysicalDevice> {
    typedef Instance LoaderObjectType;
};
template <>
struct HandleTraits<VkDevice> {
    typedef Device LoaderObjectType;
};
template <>
struct HandleTraits<VkQueue> {
    typedef Device LoaderObjectType;
};
template <>
struct HandleTraits<VkCommandBuffer> {
    typedef Device LoaderObjectType;
};

template <typename THandle>
typename HandleTraits<THandle>::LoaderObjectType& GetDispatchParent(
    THandle handle) {
    // TODO(jessehall): Make Instance and Device POD types (by removing the
    // non-default constructors), so that offsetof is actually legal to use.
    // The specific case we're using here is safe in gcc/clang (and probably
    // most other C++ compilers), but isn't guaranteed by C++.
    typedef typename HandleTraits<THandle>::LoaderObjectType ObjectType;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
    const size_t kBaseOffset = offsetof(ObjectType, base);
#pragma clang diagnostic pop

    const auto& base = driver::GetData(handle);
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(&base);
    uintptr_t object_addr = base_addr - kBaseOffset;
    return *reinterpret_cast<ObjectType*>(object_addr);
}

// -----------------------------------------------------------------------------

void DestroyDevice(Device* device, VkDevice vkdevice) {
    const auto& instance = *device->instance;

    if (vkdevice != VK_NULL_HANDLE)
        instance.drv.dispatch.DestroyDevice(vkdevice, instance.alloc);

    device->~Device();
    instance.alloc->pfnFree(instance.alloc->pUserData, device);
}

/*
 * This function will return the pNext pointer of any
 * CreateInfo extensions that are not loader extensions.
 * This is used to skip past the loader extensions prepended
 * to the list during CreateInstance and CreateDevice.
 */
void* StripCreateExtensions(const void* pNext) {
    VkLayerInstanceCreateInfo* create_info =
        const_cast<VkLayerInstanceCreateInfo*>(
            static_cast<const VkLayerInstanceCreateInfo*>(pNext));

    while (
        create_info &&
        (create_info->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
         create_info->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)) {
        create_info = const_cast<VkLayerInstanceCreateInfo*>(
            static_cast<const VkLayerInstanceCreateInfo*>(create_info->pNext));
    }

    return create_info;
}

// Clean up and deallocate an Instance; called from both the failure paths in
// CreateInstance_Top as well as from DestroyInstance_Top. This function does
// not call down the dispatch chain; that should be done before calling this
// function, iff the lower vkCreateInstance call has been made and returned
// successfully.
void DestroyInstance(Instance* instance,
                     const VkAllocationCallbacks* allocator,
                     VkInstance vkinstance) {
    if (vkinstance != VK_NULL_HANDLE && instance->drv.dispatch.DestroyInstance)
        instance->drv.dispatch.DestroyInstance(vkinstance, allocator);

    instance->~Instance();
    allocator->pfnFree(allocator->pUserData, instance);
}

driver::ProcHook::Extension InstanceExtensionToProcHookExtension(
    InstanceExtension id) {
    switch (id) {
        case kKHR_surface:
            return driver::ProcHook::KHR_surface;
        case kKHR_android_surface:
            return driver::ProcHook::KHR_android_surface;
        case kEXT_debug_report:
            return driver::ProcHook::EXT_debug_report;
        default:
            return driver::ProcHook::EXTENSION_UNKNOWN;
    }
}

driver::ProcHook::Extension DeviceExtensionToProcHookExtension(
    DeviceExtension id) {
    switch (id) {
        case kKHR_swapchain:
            return driver::ProcHook::KHR_swapchain;
        case kANDROID_native_buffer:
            return driver::ProcHook::ANDROID_native_buffer;
        default:
            return driver::ProcHook::EXTENSION_UNKNOWN;
    }
}

}  // anonymous namespace

namespace vulkan {

// -----------------------------------------------------------------------------
// "Bottom" functions. These are called at the end of the instance dispatch
// chain.

VkResult CreateInstance_Bottom(const VkInstanceCreateInfo* create_info,
                               const VkAllocationCallbacks* allocator,
                               VkInstance* vkinstance) {
    VkResult result;

    if (!allocator)
        allocator = &driver::GetDefaultAllocator();

    void* instance_mem = allocator->pfnAllocation(
        allocator->pUserData, sizeof(Instance), alignof(Instance),
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
    if (!instance_mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Instance& instance = *new (instance_mem) Instance(allocator);

    // Check that all enabled extensions are supported
    uint32_t num_driver_extensions = 0;
    bool enable_kEXT_debug_report = false;
    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
        const char* name = create_info->ppEnabledExtensionNames[i];
        InstanceExtension id = InstanceExtensionFromName(name);
        if (id != kInstanceExtensionCount) {
            if (g_driver_instance_extensions[id]) {
                num_driver_extensions++;
                instance.enabled_extensions.set(id);
                continue;
            }
            if (id == kKHR_surface || id == kKHR_android_surface) {
                instance.enabled_extensions.set(id);
                continue;
            }
            // The loader natively supports debug report.
            if (id == kEXT_debug_report) {
                enable_kEXT_debug_report = true;
                continue;
            }
        }
    }

    auto& hal_exts = instance.base.hal_extensions;
    for (size_t i = 0; i < instance.enabled_extensions.size(); i++) {
        if (instance.enabled_extensions[i]) {
            auto bit = InstanceExtensionToProcHookExtension(
                static_cast<InstanceExtension>(i));
            if (bit != driver::ProcHook::EXTENSION_UNKNOWN)
                hal_exts.set(bit);
        }
    }

    auto& hook_exts = instance.base.hook_extensions;
    hook_exts = hal_exts;
    if (enable_kEXT_debug_report)
        hook_exts.set(driver::ProcHook::EXT_debug_report);

    VkInstanceCreateInfo driver_create_info = *create_info;
    driver_create_info.pNext = StripCreateExtensions(create_info->pNext);
    driver_create_info.enabledLayerCount = 0;
    driver_create_info.ppEnabledLayerNames = nullptr;
    driver_create_info.enabledExtensionCount = 0;
    driver_create_info.ppEnabledExtensionNames = nullptr;
    if (num_driver_extensions > 0) {
        const char** names = static_cast<const char**>(
            alloca(num_driver_extensions * sizeof(char*)));
        for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
            const char* name = create_info->ppEnabledExtensionNames[i];
            InstanceExtension id = InstanceExtensionFromName(name);
            if (id != kInstanceExtensionCount) {
                if (g_driver_instance_extensions[id]) {
                    names[driver_create_info.enabledExtensionCount++] = name;
                    continue;
                }
            }
        }
        driver_create_info.ppEnabledExtensionNames = names;
        ALOG_ASSERT(
            driver_create_info.enabledExtensionCount == num_driver_extensions,
            "counted enabled driver instance extensions twice and got "
            "different answers!");
    }

    VkInstance drv_instance;
    result = g_hwdevice->CreateInstance(&driver_create_info, instance.alloc,
                                        &drv_instance);
    if (result != VK_SUCCESS) {
        DestroyInstance(&instance, allocator, VK_NULL_HANDLE);
        return result;
    }

    if (!driver::SetData(drv_instance, instance.base)) {
        DestroyInstance(&instance, allocator, drv_instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!LoadDriverDispatchTable(drv_instance, g_hwdevice->GetInstanceProcAddr,
                                 instance.enabled_extensions,
                                 instance.drv.dispatch)) {
        DestroyInstance(&instance, allocator, drv_instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t num_physical_devices = 0;
    result = instance.drv.dispatch.EnumeratePhysicalDevices(
        drv_instance, &num_physical_devices, nullptr);
    if (result != VK_SUCCESS) {
        DestroyInstance(&instance, allocator, drv_instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    num_physical_devices = std::min(num_physical_devices, kMaxPhysicalDevices);
    result = instance.drv.dispatch.EnumeratePhysicalDevices(
        drv_instance, &num_physical_devices, instance.physical_devices);
    if (result != VK_SUCCESS) {
        DestroyInstance(&instance, allocator, drv_instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    Vector<VkExtensionProperties> extensions(
        Vector<VkExtensionProperties>::allocator_type(instance.alloc));
    for (uint32_t i = 0; i < num_physical_devices; i++) {
        if (!driver::SetData(instance.physical_devices[i], instance.base)) {
            DestroyInstance(&instance, allocator, drv_instance);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        uint32_t count;
        if ((result = instance.drv.dispatch.EnumerateDeviceExtensionProperties(
                 instance.physical_devices[i], nullptr, &count, nullptr)) !=
            VK_SUCCESS) {
            ALOGW("driver EnumerateDeviceExtensionProperties(%u) failed: %d", i,
                  result);
            continue;
        }
        try {
            extensions.resize(count);
        } catch (std::bad_alloc&) {
            ALOGE("instance creation failed: out of memory");
            DestroyInstance(&instance, allocator, drv_instance);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        if ((result = instance.drv.dispatch.EnumerateDeviceExtensionProperties(
                 instance.physical_devices[i], nullptr, &count,
                 extensions.data())) != VK_SUCCESS) {
            ALOGW("driver EnumerateDeviceExtensionProperties(%u) failed: %d", i,
                  result);
            continue;
        }
        ALOGV_IF(count > 0, "driver gpu[%u] supports extensions:", i);
        for (const auto& extension : extensions) {
            ALOGV("  %s (v%u)", extension.extensionName, extension.specVersion);
            DeviceExtension id =
                DeviceExtensionFromName(extension.extensionName);
            if (id == kDeviceExtensionCount) {
                ALOGW("driver gpu[%u] extension '%s' unknown to loader", i,
                      extension.extensionName);
            } else {
                instance.physical_device_driver_extensions[i].set(id);
            }
        }
        // Ignore driver attempts to support loader extensions
        instance.physical_device_driver_extensions[i].reset(kKHR_swapchain);
    }
    instance.num_physical_devices = num_physical_devices;

    *vkinstance = drv_instance;

    return VK_SUCCESS;
}

VkResult EnumeratePhysicalDevices_Bottom(VkInstance vkinstance,
                                         uint32_t* pdev_count,
                                         VkPhysicalDevice* pdevs) {
    Instance& instance = GetDispatchParent(vkinstance);
    uint32_t count = instance.num_physical_devices;
    if (pdevs) {
        count = std::min(count, *pdev_count);
        std::copy(instance.physical_devices, instance.physical_devices + count,
                  pdevs);
    }
    *pdev_count = count;
    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult EnumerateDeviceExtensionProperties_Bottom(
    VkPhysicalDevice pdev,
    const char* layer_name,
    uint32_t* properties_count,
    VkExtensionProperties* properties) {
    (void)layer_name;

    Instance& instance = GetDispatchParent(pdev);

    size_t gpu_idx = 0;
    while (instance.physical_devices[gpu_idx] != pdev)
        gpu_idx++;
    const DeviceExtensionSet driver_extensions =
        instance.physical_device_driver_extensions[gpu_idx];

    // We only support VK_KHR_swapchain if the GPU supports
    // VK_ANDROID_native_buffer
    VkExtensionProperties* available = static_cast<VkExtensionProperties*>(
        alloca(kDeviceExtensionCount * sizeof(VkExtensionProperties)));
    uint32_t num_extensions = 0;
    if (driver_extensions[kANDROID_native_buffer]) {
        available[num_extensions++] = VkExtensionProperties{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION};
    }

    if (!properties || *properties_count > num_extensions)
        *properties_count = num_extensions;
    if (properties)
        std::copy(available, available + *properties_count, properties);

    return *properties_count < num_extensions ? VK_INCOMPLETE : VK_SUCCESS;
}

VKAPI_ATTR
VkResult CreateDevice_Bottom(VkPhysicalDevice gpu,
                             const VkDeviceCreateInfo* create_info,
                             const VkAllocationCallbacks* allocator,
                             VkDevice* device_out) {
    Instance& instance = GetDispatchParent(gpu);

    // FIXME(jessehall): We don't have good conventions or infrastructure yet to
    // do better than just using the instance allocator and scope for
    // everything. See b/26732122.
    if (true /*!allocator*/)
        allocator = instance.alloc;

    void* mem = allocator->pfnAllocation(allocator->pUserData, sizeof(Device),
                                         alignof(Device),
                                         VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
    if (!mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Device* device = new (mem) Device(&instance);

    size_t gpu_idx = 0;
    while (instance.physical_devices[gpu_idx] != gpu)
        gpu_idx++;

    VkDeviceCreateInfo driver_create_info = *create_info;
    driver_create_info.pNext = StripCreateExtensions(create_info->pNext);
    driver_create_info.enabledLayerCount = 0;
    driver_create_info.ppEnabledLayerNames = nullptr;

    uint32_t num_driver_extensions = 0;
    const char** driver_extensions = static_cast<const char**>(
        alloca(create_info->enabledExtensionCount * sizeof(const char*)));
    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
        const char* name = create_info->ppEnabledExtensionNames[i];
        DeviceExtension id = DeviceExtensionFromName(name);
        if (id != kDeviceExtensionCount) {
            if (instance.physical_device_driver_extensions[gpu_idx][id]) {
                driver_extensions[num_driver_extensions++] = name;
                device->enabled_extensions.set(id);
                continue;
            }
            // Add the VK_ANDROID_native_buffer extension to the list iff
            // the VK_KHR_swapchain extension was requested
            if (id == kKHR_swapchain &&
                instance.physical_device_driver_extensions
                    [gpu_idx][kANDROID_native_buffer]) {
                driver_extensions[num_driver_extensions++] =
                    VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME;
                device->enabled_extensions.set(id);
                continue;
            }
        }
    }

    // Unlike instance->enabled_extensions, device->enabled_extensions maps to
    // hook extensions.
    auto& hook_exts = device->base.hook_extensions;
    for (size_t i = 0; i < device->enabled_extensions.size(); i++) {
        if (device->enabled_extensions[i]) {
            auto bit = DeviceExtensionToProcHookExtension(
                static_cast<DeviceExtension>(i));
            if (bit != driver::ProcHook::EXTENSION_UNKNOWN)
                hook_exts.set(bit);
        }
    }

    auto& hal_exts = device->base.hal_extensions;
    hal_exts = hook_exts;
    // map VK_KHR_swapchain to VK_ANDROID_native_buffer
    if (hal_exts[driver::ProcHook::KHR_swapchain]) {
        hal_exts.reset(driver::ProcHook::KHR_swapchain);
        hal_exts.set(driver::ProcHook::ANDROID_native_buffer);
    }

    driver_create_info.enabledExtensionCount = num_driver_extensions;
    driver_create_info.ppEnabledExtensionNames = driver_extensions;
    VkDevice drv_device;
    VkResult result = instance.drv.dispatch.CreateDevice(
        gpu, &driver_create_info, allocator, &drv_device);
    if (result != VK_SUCCESS) {
        DestroyDevice(device, VK_NULL_HANDLE);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!driver::SetData(drv_device, device->base)) {
        DestroyDevice(device, drv_device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    device->base.get_device_proc_addr =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(
            instance.drv.dispatch.GetDeviceProcAddr(drv_device,
                                                    "vkGetDeviceProcAddr"));

    *device_out = drv_device;
    return VK_SUCCESS;
}

void DestroyInstance_Bottom(VkInstance vkinstance,
                            const VkAllocationCallbacks* allocator) {
    Instance& instance = GetDispatchParent(vkinstance);

    VkAllocationCallbacks local_allocator;
    if (!allocator) {
        local_allocator = *instance.alloc;
        allocator = &local_allocator;
    }

    DestroyInstance(&instance, allocator, vkinstance);
}

void DestroyDevice_Bottom(VkDevice vkdevice, const VkAllocationCallbacks*) {
    DestroyDevice(&GetDispatchParent(vkdevice), vkdevice);
}

void GetDeviceQueue_Bottom(VkDevice vkdevice,
                           uint32_t family,
                           uint32_t index,
                           VkQueue* queue_out) {
    const auto& device = GetDispatchParent(vkdevice);
    const auto& instance = *device.instance;

    instance.drv.dispatch.GetDeviceQueue(vkdevice, family, index, queue_out);
    driver::SetData(*queue_out, device.base);
}

VkResult AllocateCommandBuffers_Bottom(
    VkDevice vkdevice,
    const VkCommandBufferAllocateInfo* alloc_info,
    VkCommandBuffer* cmdbufs) {
    const auto& device = GetDispatchParent(vkdevice);
    const auto& instance = *device.instance;

    VkResult result = instance.drv.dispatch.AllocateCommandBuffers(
        vkdevice, alloc_info, cmdbufs);
    if (result == VK_SUCCESS) {
        for (uint32_t i = 0; i < alloc_info->commandBufferCount; i++)
            driver::SetData(cmdbufs[i], device.base);
    }

    return result;
}

// -----------------------------------------------------------------------------

const VkAllocationCallbacks* GetAllocator(VkInstance vkinstance) {
    return GetDispatchParent(vkinstance).alloc;
}

const VkAllocationCallbacks* GetAllocator(VkDevice vkdevice) {
    return GetDispatchParent(vkdevice).instance->alloc;
}

VkInstance GetDriverInstance(VkInstance instance) {
    return instance;
}

const DriverDispatchTable& GetDriverDispatch(VkInstance instance) {
    return GetDispatchParent(instance).drv.dispatch;
}

const DriverDispatchTable& GetDriverDispatch(VkDevice device) {
    return GetDispatchParent(device).instance->drv.dispatch;
}

const DriverDispatchTable& GetDriverDispatch(VkQueue queue) {
    return GetDispatchParent(queue).instance->drv.dispatch;
}

DebugReportCallbackList& GetDebugReportCallbacks(VkInstance instance) {
    return GetDispatchParent(instance).debug_report_callbacks;
}

bool InitLoader(hwvulkan_device_t* dev) {
    if (!g_hwdevice) {
        g_hwdevice = dev;
        if (!LoadVulkanHAL())
            g_hwdevice = nullptr;
    }

    return (g_hwdevice != nullptr);
}

namespace driver {

VkResult EnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
    (void)pLayerName;

    VkExtensionProperties* available = static_cast<VkExtensionProperties*>(
        alloca(kInstanceExtensionCount * sizeof(VkExtensionProperties)));
    uint32_t num_extensions = 0;

    available[num_extensions++] = VkExtensionProperties{
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION};
    available[num_extensions++] =
        VkExtensionProperties{VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
                              VK_KHR_ANDROID_SURFACE_SPEC_VERSION};
    if (g_driver_instance_extensions[kEXT_debug_report]) {
        available[num_extensions++] =
            VkExtensionProperties{VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                                  VK_EXT_DEBUG_REPORT_SPEC_VERSION};
    }

    if (!pProperties || *pPropertyCount > num_extensions)
        *pPropertyCount = num_extensions;
    if (pProperties)
        std::copy(available, available + *pPropertyCount, pProperties);

    return *pPropertyCount < num_extensions ? VK_INCOMPLETE : VK_SUCCESS;
}

}  // namespace driver

}  // namespace vulkan
