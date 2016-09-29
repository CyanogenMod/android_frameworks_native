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

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <array>
#include <new>
#include <malloc.h>
#include <sys/prctl.h>
#include <cutils/properties.h>

#include "driver.h"
#include "stubhal.h"

// #define ENABLE_ALLOC_CALLSTACKS 1
#if ENABLE_ALLOC_CALLSTACKS
#include <utils/CallStack.h>
#define ALOGD_CALLSTACK(...)                             \
    do {                                                 \
        ALOGD(__VA_ARGS__);                              \
        android::CallStack callstack;                    \
        callstack.update();                              \
        callstack.log(LOG_TAG, ANDROID_LOG_DEBUG, "  "); \
    } while (false)
#else
#define ALOGD_CALLSTACK(...) \
    do {                     \
    } while (false)
#endif

namespace vulkan {
namespace driver {

namespace {

class Hal {
   public:
    static bool Open();

    static const Hal& Get() { return hal_; }
    static const hwvulkan_device_t& Device() { return *Get().dev_; }

    int GetDebugReportIndex() const { return debug_report_index_; }

   private:
    Hal() : dev_(nullptr), debug_report_index_(-1) {}
    Hal(const Hal&) = delete;
    Hal& operator=(const Hal&) = delete;

    bool InitDebugReportIndex();

    static Hal hal_;

    const hwvulkan_device_t* dev_;
    int debug_report_index_;
};

class CreateInfoWrapper {
   public:
    CreateInfoWrapper(const VkInstanceCreateInfo& create_info,
                      const VkAllocationCallbacks& allocator);
    CreateInfoWrapper(VkPhysicalDevice physical_dev,
                      const VkDeviceCreateInfo& create_info,
                      const VkAllocationCallbacks& allocator);
    ~CreateInfoWrapper();

    VkResult Validate();

    const std::bitset<ProcHook::EXTENSION_COUNT>& GetHookExtensions() const;
    const std::bitset<ProcHook::EXTENSION_COUNT>& GetHalExtensions() const;

    explicit operator const VkInstanceCreateInfo*() const;
    explicit operator const VkDeviceCreateInfo*() const;

   private:
    struct ExtensionFilter {
        VkExtensionProperties* exts;
        uint32_t ext_count;

        const char** names;
        uint32_t name_count;
    };

    VkResult SanitizePNext();

    VkResult SanitizeLayers();
    VkResult SanitizeExtensions();

    VkResult QueryExtensionCount(uint32_t& count) const;
    VkResult EnumerateExtensions(uint32_t& count,
                                 VkExtensionProperties* props) const;
    VkResult InitExtensionFilter();
    void FilterExtension(const char* name);

    const bool is_instance_;
    const VkAllocationCallbacks& allocator_;

    VkPhysicalDevice physical_dev_;

    union {
        VkInstanceCreateInfo instance_info_;
        VkDeviceCreateInfo dev_info_;
    };

    ExtensionFilter extension_filter_;

    std::bitset<ProcHook::EXTENSION_COUNT> hook_extensions_;
    std::bitset<ProcHook::EXTENSION_COUNT> hal_extensions_;
};

Hal Hal::hal_;

bool Hal::Open() {
    ALOG_ASSERT(!hal_.dev_, "OpenHAL called more than once");

    // Use a stub device unless we successfully open a real HAL device.
    hal_.dev_ = &stubhal::kDevice;

    const hwvulkan_module_t* module;

    // Use stub HAL if vulkan is disabled
    bool disableVulkan = property_get_bool("persist.graphics.vulkan.disable", false);
    if (disableVulkan == true) {
        ALOGI("no Vulkan HAL present, using stub HAL");
        return true;
    }

    int result =
        hw_get_module("vulkan", reinterpret_cast<const hw_module_t**>(&module));
    if (result != 0) {
        ALOGI("no Vulkan HAL present, using stub HAL");
        return true;
    }

    hwvulkan_device_t* device;
    result =
        module->common.methods->open(&module->common, HWVULKAN_DEVICE_0,
                                     reinterpret_cast<hw_device_t**>(&device));
    if (result != 0) {
        // Any device with a Vulkan HAL should be able to open the device.
        ALOGE("failed to open Vulkan HAL device: %s (%d)", strerror(-result),
              result);
        return false;
    }

    hal_.dev_ = device;

    hal_.InitDebugReportIndex();

    return true;
}

bool Hal::InitDebugReportIndex() {
    uint32_t count;
    if (dev_->EnumerateInstanceExtensionProperties(nullptr, &count, nullptr) !=
        VK_SUCCESS) {
        ALOGE("failed to get HAL instance extension count");
        return false;
    }

    VkExtensionProperties* exts = reinterpret_cast<VkExtensionProperties*>(
        malloc(sizeof(VkExtensionProperties) * count));
    if (!exts) {
        ALOGE("failed to allocate HAL instance extension array");
        return false;
    }

    if (dev_->EnumerateInstanceExtensionProperties(nullptr, &count, exts) !=
        VK_SUCCESS) {
        ALOGE("failed to enumerate HAL instance extensions");
        free(exts);
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(exts[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) ==
            0) {
            debug_report_index_ = static_cast<int>(i);
            break;
        }
    }

    free(exts);

    return true;
}

CreateInfoWrapper::CreateInfoWrapper(const VkInstanceCreateInfo& create_info,
                                     const VkAllocationCallbacks& allocator)
    : is_instance_(true),
      allocator_(allocator),
      physical_dev_(VK_NULL_HANDLE),
      instance_info_(create_info),
      extension_filter_() {
    hook_extensions_.set(ProcHook::EXTENSION_CORE);
    hal_extensions_.set(ProcHook::EXTENSION_CORE);
}

CreateInfoWrapper::CreateInfoWrapper(VkPhysicalDevice physical_dev,
                                     const VkDeviceCreateInfo& create_info,
                                     const VkAllocationCallbacks& allocator)
    : is_instance_(false),
      allocator_(allocator),
      physical_dev_(physical_dev),
      dev_info_(create_info),
      extension_filter_() {
    hook_extensions_.set(ProcHook::EXTENSION_CORE);
    hal_extensions_.set(ProcHook::EXTENSION_CORE);
}

CreateInfoWrapper::~CreateInfoWrapper() {
    allocator_.pfnFree(allocator_.pUserData, extension_filter_.exts);
    allocator_.pfnFree(allocator_.pUserData, extension_filter_.names);
}

VkResult CreateInfoWrapper::Validate() {
    VkResult result = SanitizePNext();
    if (result == VK_SUCCESS)
        result = SanitizeLayers();
    if (result == VK_SUCCESS)
        result = SanitizeExtensions();

    return result;
}

const std::bitset<ProcHook::EXTENSION_COUNT>&
CreateInfoWrapper::GetHookExtensions() const {
    return hook_extensions_;
}

const std::bitset<ProcHook::EXTENSION_COUNT>&
CreateInfoWrapper::GetHalExtensions() const {
    return hal_extensions_;
}

CreateInfoWrapper::operator const VkInstanceCreateInfo*() const {
    return &instance_info_;
}

CreateInfoWrapper::operator const VkDeviceCreateInfo*() const {
    return &dev_info_;
}

VkResult CreateInfoWrapper::SanitizePNext() {
    const struct StructHeader {
        VkStructureType type;
        const void* next;
    } * header;

    if (is_instance_) {
        header = reinterpret_cast<const StructHeader*>(instance_info_.pNext);

        // skip leading VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFOs
        while (header &&
               header->type == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO)
            header = reinterpret_cast<const StructHeader*>(header->next);

        instance_info_.pNext = header;
    } else {
        header = reinterpret_cast<const StructHeader*>(dev_info_.pNext);

        // skip leading VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFOs
        while (header &&
               header->type == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
            header = reinterpret_cast<const StructHeader*>(header->next);

        dev_info_.pNext = header;
    }

    return VK_SUCCESS;
}

VkResult CreateInfoWrapper::SanitizeLayers() {
    auto& layer_names = (is_instance_) ? instance_info_.ppEnabledLayerNames
                                       : dev_info_.ppEnabledLayerNames;
    auto& layer_count = (is_instance_) ? instance_info_.enabledLayerCount
                                       : dev_info_.enabledLayerCount;

    // remove all layers
    layer_names = nullptr;
    layer_count = 0;

    return VK_SUCCESS;
}

VkResult CreateInfoWrapper::SanitizeExtensions() {
    auto& ext_names = (is_instance_) ? instance_info_.ppEnabledExtensionNames
                                     : dev_info_.ppEnabledExtensionNames;
    auto& ext_count = (is_instance_) ? instance_info_.enabledExtensionCount
                                     : dev_info_.enabledExtensionCount;
    if (!ext_count)
        return VK_SUCCESS;

    VkResult result = InitExtensionFilter();
    if (result != VK_SUCCESS)
        return result;

    for (uint32_t i = 0; i < ext_count; i++)
        FilterExtension(ext_names[i]);

    ext_names = extension_filter_.names;
    ext_count = extension_filter_.name_count;

    return VK_SUCCESS;
}

VkResult CreateInfoWrapper::QueryExtensionCount(uint32_t& count) const {
    if (is_instance_) {
        return Hal::Device().EnumerateInstanceExtensionProperties(
            nullptr, &count, nullptr);
    } else {
        const auto& driver = GetData(physical_dev_).driver;
        return driver.EnumerateDeviceExtensionProperties(physical_dev_, nullptr,
                                                         &count, nullptr);
    }
}

VkResult CreateInfoWrapper::EnumerateExtensions(
    uint32_t& count,
    VkExtensionProperties* props) const {
    if (is_instance_) {
        return Hal::Device().EnumerateInstanceExtensionProperties(
            nullptr, &count, props);
    } else {
        const auto& driver = GetData(physical_dev_).driver;
        return driver.EnumerateDeviceExtensionProperties(physical_dev_, nullptr,
                                                         &count, props);
    }
}

VkResult CreateInfoWrapper::InitExtensionFilter() {
    // query extension count
    uint32_t count;
    VkResult result = QueryExtensionCount(count);
    if (result != VK_SUCCESS || count == 0)
        return result;

    auto& filter = extension_filter_;
    filter.exts =
        reinterpret_cast<VkExtensionProperties*>(allocator_.pfnAllocation(
            allocator_.pUserData, sizeof(VkExtensionProperties) * count,
            alignof(VkExtensionProperties),
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));
    if (!filter.exts)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    // enumerate extensions
    result = EnumerateExtensions(count, filter.exts);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        return result;

    if (!count)
        return VK_SUCCESS;

    filter.ext_count = count;

    // allocate name array
    uint32_t enabled_ext_count = (is_instance_)
                                     ? instance_info_.enabledExtensionCount
                                     : dev_info_.enabledExtensionCount;
    count = std::min(filter.ext_count, enabled_ext_count);
    filter.names = reinterpret_cast<const char**>(allocator_.pfnAllocation(
        allocator_.pUserData, sizeof(const char*) * count, alignof(const char*),
        VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));
    if (!filter.names)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    return VK_SUCCESS;
}

void CreateInfoWrapper::FilterExtension(const char* name) {
    auto& filter = extension_filter_;

    ProcHook::Extension ext_bit = GetProcHookExtension(name);
    if (is_instance_) {
        switch (ext_bit) {
            case ProcHook::KHR_android_surface:
            case ProcHook::KHR_surface:
                hook_extensions_.set(ext_bit);
                // return now as these extensions do not require HAL support
                return;
            case ProcHook::EXT_debug_report:
                // both we and HAL can take part in
                hook_extensions_.set(ext_bit);
                break;
            case ProcHook::EXTENSION_UNKNOWN:
                // HAL's extensions
                break;
            default:
                ALOGW("Ignored invalid instance extension %s", name);
                return;
        }
    } else {
        switch (ext_bit) {
            case ProcHook::KHR_swapchain:
                // map VK_KHR_swapchain to VK_ANDROID_native_buffer
                name = VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME;
                ext_bit = ProcHook::ANDROID_native_buffer;
                break;
            case ProcHook::EXTENSION_UNKNOWN:
                // HAL's extensions
                break;
            default:
                ALOGW("Ignored invalid device extension %s", name);
                return;
        }
    }

    for (uint32_t i = 0; i < filter.ext_count; i++) {
        const VkExtensionProperties& props = filter.exts[i];
        // ignore unknown extensions
        if (strcmp(name, props.extensionName) != 0)
            continue;

        filter.names[filter.name_count++] = name;
        if (ext_bit != ProcHook::EXTENSION_UNKNOWN) {
            if (ext_bit == ProcHook::ANDROID_native_buffer)
                hook_extensions_.set(ProcHook::KHR_swapchain);

            hal_extensions_.set(ext_bit);
        }

        break;
    }
}

VKAPI_ATTR void* DefaultAllocate(void*,
                                 size_t size,
                                 size_t alignment,
                                 VkSystemAllocationScope) {
    void* ptr = nullptr;
    // Vulkan requires 'alignment' to be a power of two, but posix_memalign
    // additionally requires that it be at least sizeof(void*).
    int ret = posix_memalign(&ptr, std::max(alignment, sizeof(void*)), size);
    ALOGD_CALLSTACK("Allocate: size=%zu align=%zu => (%d) %p", size, alignment,
                    ret, ptr);
    return ret == 0 ? ptr : nullptr;
}

VKAPI_ATTR void* DefaultReallocate(void*,
                                   void* ptr,
                                   size_t size,
                                   size_t alignment,
                                   VkSystemAllocationScope) {
    if (size == 0) {
        free(ptr);
        return nullptr;
    }

    // TODO(jessehall): Right now we never shrink allocations; if the new
    // request is smaller than the existing chunk, we just continue using it.
    // Right now the loader never reallocs, so this doesn't matter. If that
    // changes, or if this code is copied into some other project, this should
    // probably have a heuristic to allocate-copy-free when doing so will save
    // "enough" space.
    size_t old_size = ptr ? malloc_usable_size(ptr) : 0;
    if (size <= old_size)
        return ptr;

    void* new_ptr = nullptr;
    if (posix_memalign(&new_ptr, std::max(alignment, sizeof(void*)), size) != 0)
        return nullptr;
    if (ptr) {
        memcpy(new_ptr, ptr, std::min(old_size, size));
        free(ptr);
    }
    return new_ptr;
}

VKAPI_ATTR void DefaultFree(void*, void* ptr) {
    ALOGD_CALLSTACK("Free: %p", ptr);
    free(ptr);
}

InstanceData* AllocateInstanceData(const VkAllocationCallbacks& allocator) {
    void* data_mem = allocator.pfnAllocation(
        allocator.pUserData, sizeof(InstanceData), alignof(InstanceData),
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
    if (!data_mem)
        return nullptr;

    return new (data_mem) InstanceData(allocator);
}

void FreeInstanceData(InstanceData* data,
                      const VkAllocationCallbacks& allocator) {
    data->~InstanceData();
    allocator.pfnFree(allocator.pUserData, data);
}

DeviceData* AllocateDeviceData(
    const VkAllocationCallbacks& allocator,
    const DebugReportCallbackList& debug_report_callbacks) {
    void* data_mem = allocator.pfnAllocation(
        allocator.pUserData, sizeof(DeviceData), alignof(DeviceData),
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
    if (!data_mem)
        return nullptr;

    return new (data_mem) DeviceData(allocator, debug_report_callbacks);
}

void FreeDeviceData(DeviceData* data, const VkAllocationCallbacks& allocator) {
    data->~DeviceData();
    allocator.pfnFree(allocator.pUserData, data);
}

}  // anonymous namespace

bool Debuggable() {
    return (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) >= 0);
}

bool OpenHAL() {
    return Hal::Open();
}

const VkAllocationCallbacks& GetDefaultAllocator() {
    static const VkAllocationCallbacks kDefaultAllocCallbacks = {
        .pUserData = nullptr,
        .pfnAllocation = DefaultAllocate,
        .pfnReallocation = DefaultReallocate,
        .pfnFree = DefaultFree,
    };

    return kDefaultAllocCallbacks;
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* pName) {
    const ProcHook* hook = GetProcHook(pName);
    if (!hook)
        return Hal::Device().GetInstanceProcAddr(instance, pName);

    if (!instance) {
        if (hook->type == ProcHook::GLOBAL)
            return hook->proc;

        // v0 layers expect
        //
        //   vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice");
        //
        // to work.
        if (strcmp(pName, "vkCreateDevice") == 0)
            return hook->proc;

        ALOGE(
            "internal vkGetInstanceProcAddr called for %s without an instance",
            pName);

        return nullptr;
    }

    PFN_vkVoidFunction proc;

    switch (hook->type) {
        case ProcHook::INSTANCE:
            proc = (GetData(instance).hook_extensions[hook->extension])
                       ? hook->proc
                       : nullptr;
            break;
        case ProcHook::DEVICE:
            proc = (hook->extension == ProcHook::EXTENSION_CORE)
                       ? hook->proc
                       : hook->checked_proc;
            break;
        default:
            ALOGE(
                "internal vkGetInstanceProcAddr called for %s with an instance",
                pName);
            proc = nullptr;
            break;
    }

    return proc;
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char* pName) {
    const ProcHook* hook = GetProcHook(pName);
    if (!hook)
        return GetData(device).driver.GetDeviceProcAddr(device, pName);

    if (hook->type != ProcHook::DEVICE) {
        ALOGE("internal vkGetDeviceProcAddr called for %s", pName);
        return nullptr;
    }

    return (GetData(device).hook_extensions[hook->extension]) ? hook->proc
                                                              : nullptr;
}

VkResult EnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
    static const std::array<VkExtensionProperties, 2> loader_extensions = {{
        // WSI extensions
        {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION},
        {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
         VK_KHR_ANDROID_SURFACE_SPEC_VERSION},
    }};
    static const VkExtensionProperties loader_debug_report_extension = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_SPEC_VERSION,
    };

    // enumerate our extensions first
    if (!pLayerName && pProperties) {
        uint32_t count = std::min(
            *pPropertyCount, static_cast<uint32_t>(loader_extensions.size()));

        std::copy_n(loader_extensions.begin(), count, pProperties);

        if (count < loader_extensions.size()) {
            *pPropertyCount = count;
            return VK_INCOMPLETE;
        }

        pProperties += count;
        *pPropertyCount -= count;

        if (Hal::Get().GetDebugReportIndex() < 0) {
            if (!*pPropertyCount) {
                *pPropertyCount = count;
                return VK_INCOMPLETE;
            }

            pProperties[0] = loader_debug_report_extension;
            pProperties += 1;
            *pPropertyCount -= 1;
        }
    }

    VkResult result = Hal::Device().EnumerateInstanceExtensionProperties(
        pLayerName, pPropertyCount, pProperties);

    if (!pLayerName && (result == VK_SUCCESS || result == VK_INCOMPLETE)) {
        int idx = Hal::Get().GetDebugReportIndex();
        if (idx < 0) {
            *pPropertyCount += 1;
        } else if (pProperties &&
                   static_cast<uint32_t>(idx) < *pPropertyCount) {
            pProperties[idx].specVersion =
                std::min(pProperties[idx].specVersion,
                         loader_debug_report_extension.specVersion);
        }

        *pPropertyCount += loader_extensions.size();
    }

    return result;
}

VkResult EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
    const InstanceData& data = GetData(physicalDevice);

    VkResult result = data.driver.EnumerateDeviceExtensionProperties(
        physicalDevice, pLayerName, pPropertyCount, pProperties);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        return result;

    if (!pProperties)
        return result;

    // map VK_ANDROID_native_buffer to VK_KHR_swapchain
    for (uint32_t i = 0; i < *pPropertyCount; i++) {
        auto& prop = pProperties[i];

        if (strcmp(prop.extensionName,
                   VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME) != 0)
            continue;

        memcpy(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME,
               sizeof(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
        prop.specVersion = VK_KHR_SWAPCHAIN_SPEC_VERSION;
    }

    return result;
}

VkResult CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                        const VkAllocationCallbacks* pAllocator,
                        VkInstance* pInstance) {
    const VkAllocationCallbacks& data_allocator =
        (pAllocator) ? *pAllocator : GetDefaultAllocator();

    CreateInfoWrapper wrapper(*pCreateInfo, data_allocator);
    VkResult result = wrapper.Validate();
    if (result != VK_SUCCESS)
        return result;

    InstanceData* data = AllocateInstanceData(data_allocator);
    if (!data)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    data->hook_extensions |= wrapper.GetHookExtensions();

    // call into the driver
    VkInstance instance;
    result = Hal::Device().CreateInstance(
        static_cast<const VkInstanceCreateInfo*>(wrapper), pAllocator,
        &instance);
    if (result != VK_SUCCESS) {
        FreeInstanceData(data, data_allocator);
        return result;
    }

    // initialize InstanceDriverTable
    if (!SetData(instance, *data) ||
        !InitDriverTable(instance, Hal::Device().GetInstanceProcAddr,
                         wrapper.GetHalExtensions())) {
        data->driver.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
            Hal::Device().GetInstanceProcAddr(instance, "vkDestroyInstance"));
        if (data->driver.DestroyInstance)
            data->driver.DestroyInstance(instance, pAllocator);

        FreeInstanceData(data, data_allocator);

        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    data->get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        Hal::Device().GetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    if (!data->get_device_proc_addr) {
        data->driver.DestroyInstance(instance, pAllocator);
        FreeInstanceData(data, data_allocator);

        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    *pInstance = instance;

    return VK_SUCCESS;
}

void DestroyInstance(VkInstance instance,
                     const VkAllocationCallbacks* pAllocator) {
    InstanceData& data = GetData(instance);
    data.driver.DestroyInstance(instance, pAllocator);

    VkAllocationCallbacks local_allocator;
    if (!pAllocator) {
        local_allocator = data.allocator;
        pAllocator = &local_allocator;
    }

    FreeInstanceData(&data, *pAllocator);
}

VkResult CreateDevice(VkPhysicalDevice physicalDevice,
                      const VkDeviceCreateInfo* pCreateInfo,
                      const VkAllocationCallbacks* pAllocator,
                      VkDevice* pDevice) {
    const InstanceData& instance_data = GetData(physicalDevice);
    const VkAllocationCallbacks& data_allocator =
        (pAllocator) ? *pAllocator : instance_data.allocator;

    CreateInfoWrapper wrapper(physicalDevice, *pCreateInfo, data_allocator);
    VkResult result = wrapper.Validate();
    if (result != VK_SUCCESS)
        return result;

    DeviceData* data = AllocateDeviceData(data_allocator,
                                          instance_data.debug_report_callbacks);
    if (!data)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    data->hook_extensions |= wrapper.GetHookExtensions();

    // call into the driver
    VkDevice dev;
    result = instance_data.driver.CreateDevice(
        physicalDevice, static_cast<const VkDeviceCreateInfo*>(wrapper),
        pAllocator, &dev);
    if (result != VK_SUCCESS) {
        FreeDeviceData(data, data_allocator);
        return result;
    }

    // initialize DeviceDriverTable
    if (!SetData(dev, *data) ||
        !InitDriverTable(dev, instance_data.get_device_proc_addr,
                         wrapper.GetHalExtensions())) {
        data->driver.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
            instance_data.get_device_proc_addr(dev, "vkDestroyDevice"));
        if (data->driver.DestroyDevice)
            data->driver.DestroyDevice(dev, pAllocator);

        FreeDeviceData(data, data_allocator);

        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    data->driver_device = dev;

    *pDevice = dev;

    return VK_SUCCESS;
}

void DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    DeviceData& data = GetData(device);
    data.driver.DestroyDevice(device, pAllocator);

    VkAllocationCallbacks local_allocator;
    if (!pAllocator) {
        local_allocator = data.allocator;
        pAllocator = &local_allocator;
    }

    FreeDeviceData(&data, *pAllocator);
}

VkResult EnumeratePhysicalDevices(VkInstance instance,
                                  uint32_t* pPhysicalDeviceCount,
                                  VkPhysicalDevice* pPhysicalDevices) {
    const auto& data = GetData(instance);

    VkResult result = data.driver.EnumeratePhysicalDevices(
        instance, pPhysicalDeviceCount, pPhysicalDevices);
    if ((result == VK_SUCCESS || result == VK_INCOMPLETE) && pPhysicalDevices) {
        for (uint32_t i = 0; i < *pPhysicalDeviceCount; i++)
            SetData(pPhysicalDevices[i], data);
    }

    return result;
}

void GetDeviceQueue(VkDevice device,
                    uint32_t queueFamilyIndex,
                    uint32_t queueIndex,
                    VkQueue* pQueue) {
    const auto& data = GetData(device);

    data.driver.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    SetData(*pQueue, data);
}

VKAPI_ATTR VkResult
AllocateCommandBuffers(VkDevice device,
                       const VkCommandBufferAllocateInfo* pAllocateInfo,
                       VkCommandBuffer* pCommandBuffers) {
    const auto& data = GetData(device);

    VkResult result = data.driver.AllocateCommandBuffers(device, pAllocateInfo,
                                                         pCommandBuffers);
    if (result == VK_SUCCESS) {
        for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++)
            SetData(pCommandBuffers[i], data);
    }

    return result;
}

}  // namespace driver
}  // namespace vulkan
