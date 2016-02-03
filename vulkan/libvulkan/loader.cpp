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

using namespace vulkan;

static const uint32_t kMaxPhysicalDevices = 4;

namespace {

// These definitions are taken from the LunarG Vulkan Loader. They are used to
// enforce compatability between the Loader and Layers.
typedef void* (*PFN_vkGetProcAddr)(void* obj, const char* pName);

typedef struct VkLayerLinkedListElem_ {
    PFN_vkGetProcAddr get_proc_addr;
    void* next_element;
    void* base_object;
} VkLayerLinkedListElem;

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
    if (posix_memalign(&new_ptr, alignment, size) != 0)
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

const VkAllocationCallbacks kDefaultAllocCallbacks = {
    .pUserData = nullptr,
    .pfnAllocation = DefaultAllocate,
    .pfnReallocation = DefaultReallocate,
    .pfnFree = DefaultFree,
};

// ----------------------------------------------------------------------------
// Global Data and Initialization

hwvulkan_device_t* g_hwdevice = nullptr;
InstanceExtensionSet g_driver_instance_extensions;

void LoadVulkanHAL() {
    static const hwvulkan_module_t* module;
    int result =
        hw_get_module("vulkan", reinterpret_cast<const hw_module_t**>(&module));
    if (result != 0) {
        ALOGE("failed to load vulkan hal: %s (%d)", strerror(-result), result);
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

    VkResult vkresult;
    uint32_t count;
    if ((vkresult = g_hwdevice->EnumerateInstanceExtensionProperties(
             nullptr, &count, nullptr)) != VK_SUCCESS) {
        ALOGE("driver EnumerateInstanceExtensionProperties failed: %d",
              vkresult);
        g_hwdevice->common.close(&g_hwdevice->common);
        g_hwdevice = nullptr;
        module = nullptr;
        return;
    }
    VkExtensionProperties* extensions = static_cast<VkExtensionProperties*>(
        alloca(count * sizeof(VkExtensionProperties)));
    if ((vkresult = g_hwdevice->EnumerateInstanceExtensionProperties(
             nullptr, &count, extensions)) != VK_SUCCESS) {
        ALOGE("driver EnumerateInstanceExtensionProperties failed: %d",
              vkresult);
        g_hwdevice->common.close(&g_hwdevice->common);
        g_hwdevice = nullptr;
        module = nullptr;
        return;
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
}

bool EnsureInitialized() {
    static std::once_flag once_flag;
    std::call_once(once_flag, []() {
        LoadVulkanHAL();
        DiscoverLayers();
    });
    return g_hwdevice != nullptr;
}

// -----------------------------------------------------------------------------

struct Instance {
    Instance(const VkAllocationCallbacks* alloc_callbacks)
        : dispatch_ptr(&dispatch),
          handle(reinterpret_cast<VkInstance>(&dispatch_ptr)),
          alloc(alloc_callbacks),
          num_physical_devices(0),
          active_layers(CallbackAllocator<LayerRef>(alloc)),
          message(VK_NULL_HANDLE) {
        memset(&dispatch, 0, sizeof(dispatch));
        memset(physical_devices, 0, sizeof(physical_devices));
        drv.instance = VK_NULL_HANDLE;
        memset(&drv.dispatch, 0, sizeof(drv.dispatch));
        drv.num_physical_devices = 0;
    }

    ~Instance() {}

    const InstanceDispatchTable* dispatch_ptr;
    const VkInstance handle;
    InstanceDispatchTable dispatch;

    const VkAllocationCallbacks* alloc;
    uint32_t num_physical_devices;
    VkPhysicalDevice physical_devices[kMaxPhysicalDevices];
    DeviceExtensionSet physical_device_driver_extensions[kMaxPhysicalDevices];

    Vector<LayerRef> active_layers;
    VkDebugReportCallbackEXT message;
    DebugReportCallbackList debug_report_callbacks;

    struct {
        VkInstance instance;
        DriverDispatchTable dispatch;
        uint32_t num_physical_devices;
    } drv;  // may eventually be an array
};

struct Device {
    Device(Instance* instance_)
        : instance(instance_),
          active_layers(CallbackAllocator<LayerRef>(instance->alloc)) {
        memset(&dispatch, 0, sizeof(dispatch));
    }
    DeviceDispatchTable dispatch;
    Instance* instance;
    PFN_vkGetDeviceProcAddr get_device_proc_addr;
    Vector<LayerRef> active_layers;
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
    const size_t kDispatchOffset = offsetof(ObjectType, dispatch);
#pragma clang diagnostic pop

    const auto& dispatch = GetDispatchTable(handle);
    uintptr_t dispatch_addr = reinterpret_cast<uintptr_t>(&dispatch);
    uintptr_t object_addr = dispatch_addr - kDispatchOffset;
    return *reinterpret_cast<ObjectType*>(object_addr);
}

// -----------------------------------------------------------------------------

void DestroyDevice(Device* device) {
    const VkAllocationCallbacks* alloc = device->instance->alloc;
    device->~Device();
    alloc->pfnFree(alloc->pUserData, device);
}

template <class TObject>
LayerRef GetLayerRef(const char* name);
template <>
LayerRef GetLayerRef<Instance>(const char* name) {
    return GetInstanceLayerRef(name);
}
template <>
LayerRef GetLayerRef<Device>(const char* name) {
    return GetDeviceLayerRef(name);
}

template <class TObject>
bool ActivateLayer(TObject* object, const char* name) {
    LayerRef layer(GetLayerRef<TObject>(name));
    if (!layer)
        return false;
    if (std::find(object->active_layers.begin(), object->active_layers.end(),
                  layer) == object->active_layers.end()) {
        try {
            object->active_layers.push_back(std::move(layer));
        } catch (std::bad_alloc&) {
            // TODO(jessehall): We should fail with VK_ERROR_OUT_OF_MEMORY
            // if we can't enable a requested layer. Callers currently ignore
            // ActivateLayer's return value.
            ALOGW("failed to activate layer '%s': out of memory", name);
            return false;
        }
    }
    ALOGV("activated layer '%s'", name);
    return true;
}

struct InstanceNamesPair {
    Instance* instance;
    Vector<String>* layer_names;
};

void SetLayerNamesFromProperty(const char* name,
                               const char* value,
                               void* data) {
    try {
        const char prefix[] = "debug.vulkan.layer.";
        const size_t prefixlen = sizeof(prefix) - 1;
        if (value[0] == '\0' || strncmp(name, prefix, prefixlen) != 0)
            return;
        const char* number_str = name + prefixlen;
        long layer_number = strtol(number_str, nullptr, 10);
        if (layer_number <= 0 || layer_number == LONG_MAX) {
            ALOGW("Cannot use a layer at number %ld from string %s",
                  layer_number, number_str);
            return;
        }
        auto instance_names_pair = static_cast<InstanceNamesPair*>(data);
        Vector<String>* layer_names = instance_names_pair->layer_names;
        Instance* instance = instance_names_pair->instance;
        size_t layer_size = static_cast<size_t>(layer_number);
        if (layer_size > layer_names->size()) {
            layer_names->resize(
                layer_size, String(CallbackAllocator<char>(instance->alloc)));
        }
        (*layer_names)[layer_size - 1] = value;
    } catch (std::bad_alloc&) {
        ALOGW("failed to handle property '%s'='%s': out of memory", name,
              value);
        return;
    }
}

template <class TInfo, class TObject>
VkResult ActivateAllLayers(TInfo create_info,
                           Instance* instance,
                           TObject* object) {
    ALOG_ASSERT(create_info->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO ||
                    create_info->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                "Cannot activate layers for unknown object %p", object);
    CallbackAllocator<char> string_allocator(instance->alloc);
    // Load system layers
    if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
        char layer_prop[PROPERTY_VALUE_MAX];
        property_get("debug.vulkan.layers", layer_prop, "");
        char* strtok_state;
        char* layer_name = nullptr;
        while ((layer_name = strtok_r(layer_name ? nullptr : layer_prop, ":",
                                      &strtok_state))) {
            ActivateLayer(object, layer_name);
        }
        Vector<String> layer_names(CallbackAllocator<String>(instance->alloc));
        InstanceNamesPair instance_names_pair = {.instance = instance,
                                                 .layer_names = &layer_names};
        property_list(SetLayerNamesFromProperty,
                      static_cast<void*>(&instance_names_pair));
        for (auto layer_name_element : layer_names) {
            ActivateLayer(object, layer_name_element.c_str());
        }
    }
    // Load app layers
    for (uint32_t i = 0; i < create_info->enabledLayerCount; ++i) {
        if (!ActivateLayer(object, create_info->ppEnabledLayerNames[i])) {
            ALOGE("requested %s layer '%s' not present",
                  create_info->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
                      ? "instance"
                      : "device",
                  create_info->ppEnabledLayerNames[i]);
            return VK_ERROR_LAYER_NOT_PRESENT;
        }
    }
    return VK_SUCCESS;
}

template <class TCreateInfo>
bool AddExtensionToCreateInfo(TCreateInfo& local_create_info,
                              const char* extension_name,
                              const VkAllocationCallbacks* alloc) {
    for (uint32_t i = 0; i < local_create_info.enabledExtensionCount; ++i) {
        if (!strcmp(extension_name,
                    local_create_info.ppEnabledExtensionNames[i])) {
            return false;
        }
    }
    uint32_t extension_count = local_create_info.enabledExtensionCount;
    local_create_info.enabledExtensionCount++;
    void* mem = alloc->pfnAllocation(
        alloc->pUserData,
        local_create_info.enabledExtensionCount * sizeof(char*), alignof(char*),
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
    if (mem) {
        const char** enabled_extensions = static_cast<const char**>(mem);
        for (uint32_t i = 0; i < extension_count; ++i) {
            enabled_extensions[i] =
                local_create_info.ppEnabledExtensionNames[i];
        }
        enabled_extensions[extension_count] = extension_name;
        local_create_info.ppEnabledExtensionNames = enabled_extensions;
    } else {
        ALOGW("%s extension cannot be enabled: memory allocation failed",
              extension_name);
        local_create_info.enabledExtensionCount--;
        return false;
    }
    return true;
}

template <class T>
void FreeAllocatedCreateInfo(T& local_create_info,
                             const VkAllocationCallbacks* alloc) {
    alloc->pfnFree(
        alloc->pUserData,
        const_cast<char**>(local_create_info.ppEnabledExtensionNames));
}

VKAPI_ATTR
VkBool32 LogDebugMessageCallback(VkDebugReportFlagsEXT flags,
                                 VkDebugReportObjectTypeEXT /*objectType*/,
                                 uint64_t /*object*/,
                                 size_t /*location*/,
                                 int32_t message_code,
                                 const char* layer_prefix,
                                 const char* message,
                                 void* /*user_data*/) {
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        ALOGE("[%s] Code %d : %s", layer_prefix, message_code, message);
    } else if (flags & VK_DEBUG_REPORT_WARN_BIT_EXT) {
        ALOGW("[%s] Code %d : %s", layer_prefix, message_code, message);
    }
    return false;
}

VkResult Noop() {
    return VK_SUCCESS;
}

}  // anonymous namespace

namespace vulkan {

// -----------------------------------------------------------------------------
// "Bottom" functions. These are called at the end of the instance dispatch
// chain.

VkResult CreateInstance_Bottom(const VkInstanceCreateInfo* create_info,
                               const VkAllocationCallbacks* allocator,
                               VkInstance* vkinstance) {
    Instance& instance = GetDispatchParent(*vkinstance);
    VkResult result;

    // Check that all enabled extensions are supported
    InstanceExtensionSet enabled_extensions;
    uint32_t num_driver_extensions = 0;
    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
        const char* name = create_info->ppEnabledExtensionNames[i];
        InstanceExtension id = InstanceExtensionFromName(name);
        if (id != kInstanceExtensionCount) {
            if (g_driver_instance_extensions[id]) {
                num_driver_extensions++;
                enabled_extensions.set(id);
                continue;
            }
            if (id == kKHR_surface || id == kKHR_android_surface) {
                enabled_extensions.set(id);
                continue;
            }
            // The loader natively supports debug report.
            if (id == kEXT_debug_report) {
                continue;
            }
        }
        bool supported = false;
        for (const auto& layer : instance.active_layers) {
            if (layer.SupportsExtension(name))
                supported = true;
        }
        if (!supported) {
            ALOGE(
                "requested instance extension '%s' not supported by "
                "loader, driver, or any active layers",
                name);
            DestroyInstance_Bottom(instance.handle, allocator);
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    VkInstanceCreateInfo driver_create_info = *create_info;
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

    result = g_hwdevice->CreateInstance(&driver_create_info, instance.alloc,
                                        &instance.drv.instance);
    if (result != VK_SUCCESS) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return result;
    }

    hwvulkan_dispatch_t* drv_dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(instance.drv.instance);
    if (drv_dispatch->magic == HWVULKAN_DISPATCH_MAGIC) {
        // Skip setting drv_dispatch->vtbl, since we never call through it;
        // we go through instance.drv.dispatch instead.
    } else {
        ALOGE("invalid VkInstance dispatch magic: 0x%" PRIxPTR,
              drv_dispatch->magic);
        DestroyInstance_Bottom(instance.handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!LoadDriverDispatchTable(instance.drv.instance,
                                 g_hwdevice->GetInstanceProcAddr,
                                 enabled_extensions, instance.drv.dispatch)) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t num_physical_devices = 0;
    result = instance.drv.dispatch.EnumeratePhysicalDevices(
        instance.drv.instance, &num_physical_devices, nullptr);
    if (result != VK_SUCCESS) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    num_physical_devices = std::min(num_physical_devices, kMaxPhysicalDevices);
    result = instance.drv.dispatch.EnumeratePhysicalDevices(
        instance.drv.instance, &num_physical_devices,
        instance.physical_devices);
    if (result != VK_SUCCESS) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    Vector<VkExtensionProperties> extensions(
        Vector<VkExtensionProperties>::allocator_type(instance.alloc));
    for (uint32_t i = 0; i < num_physical_devices; i++) {
        hwvulkan_dispatch_t* pdev_dispatch =
            reinterpret_cast<hwvulkan_dispatch_t*>(
                instance.physical_devices[i]);
        if (pdev_dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
            ALOGE("invalid VkPhysicalDevice dispatch magic: 0x%" PRIxPTR,
                  pdev_dispatch->magic);
            DestroyInstance_Bottom(instance.handle, allocator);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        pdev_dispatch->vtbl = instance.dispatch_ptr;

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
            DestroyInstance_Bottom(instance.handle, allocator);
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
    instance.drv.num_physical_devices = num_physical_devices;
    instance.num_physical_devices = instance.drv.num_physical_devices;

    return VK_SUCCESS;
}

PFN_vkVoidFunction GetInstanceProcAddr_Bottom(VkInstance, const char* name) {
    PFN_vkVoidFunction pfn;
    if ((pfn = GetLoaderBottomProcAddr(name)))
        return pfn;
    return nullptr;
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

void GetPhysicalDeviceProperties_Bottom(
    VkPhysicalDevice pdev,
    VkPhysicalDeviceProperties* properties) {
    GetDispatchParent(pdev).drv.dispatch.GetPhysicalDeviceProperties(
        pdev, properties);
}

void GetPhysicalDeviceFeatures_Bottom(VkPhysicalDevice pdev,
                                      VkPhysicalDeviceFeatures* features) {
    GetDispatchParent(pdev).drv.dispatch.GetPhysicalDeviceFeatures(pdev,
                                                                   features);
}

void GetPhysicalDeviceMemoryProperties_Bottom(
    VkPhysicalDevice pdev,
    VkPhysicalDeviceMemoryProperties* properties) {
    GetDispatchParent(pdev).drv.dispatch.GetPhysicalDeviceMemoryProperties(
        pdev, properties);
}

void GetPhysicalDeviceQueueFamilyProperties_Bottom(
    VkPhysicalDevice pdev,
    uint32_t* pCount,
    VkQueueFamilyProperties* properties) {
    GetDispatchParent(pdev).drv.dispatch.GetPhysicalDeviceQueueFamilyProperties(
        pdev, pCount, properties);
}

void GetPhysicalDeviceFormatProperties_Bottom(VkPhysicalDevice pdev,
                                              VkFormat format,
                                              VkFormatProperties* properties) {
    GetDispatchParent(pdev).drv.dispatch.GetPhysicalDeviceFormatProperties(
        pdev, format, properties);
}

VkResult GetPhysicalDeviceImageFormatProperties_Bottom(
    VkPhysicalDevice pdev,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageFormatProperties* properties) {
    return GetDispatchParent(pdev)
        .drv.dispatch.GetPhysicalDeviceImageFormatProperties(
            pdev, format, type, tiling, usage, flags, properties);
}

void GetPhysicalDeviceSparseImageFormatProperties_Bottom(
    VkPhysicalDevice pdev,
    VkFormat format,
    VkImageType type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage,
    VkImageTiling tiling,
    uint32_t* properties_count,
    VkSparseImageFormatProperties* properties) {
    GetDispatchParent(pdev)
        .drv.dispatch.GetPhysicalDeviceSparseImageFormatProperties(
            pdev, format, type, samples, usage, tiling, properties_count,
            properties);
}

VKAPI_ATTR
VkResult EnumerateDeviceExtensionProperties_Bottom(
    VkPhysicalDevice gpu,
    const char* layer_name,
    uint32_t* properties_count,
    VkExtensionProperties* properties) {
    const VkExtensionProperties* extensions = nullptr;
    uint32_t num_extensions = 0;
    if (layer_name) {
        GetDeviceLayerExtensions(layer_name, &extensions, &num_extensions);
    } else {
        Instance& instance = GetDispatchParent(gpu);
        size_t gpu_idx = 0;
        while (instance.physical_devices[gpu_idx] != gpu)
            gpu_idx++;
        const DeviceExtensionSet driver_extensions =
            instance.physical_device_driver_extensions[gpu_idx];

        // We only support VK_KHR_swapchain if the GPU supports
        // VK_ANDROID_native_buffer
        VkExtensionProperties* available = static_cast<VkExtensionProperties*>(
            alloca(kDeviceExtensionCount * sizeof(VkExtensionProperties)));
        if (driver_extensions[kANDROID_native_buffer]) {
            available[num_extensions++] = VkExtensionProperties{
                VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION};
        }

        // TODO(jessehall): We need to also enumerate extensions supported by
        // implicitly-enabled layers. Currently we don't have that list of
        // layers until instance creation.
        extensions = available;
    }

    if (!properties || *properties_count > num_extensions)
        *properties_count = num_extensions;
    if (properties)
        std::copy(extensions, extensions + *properties_count, properties);
    return *properties_count < num_extensions ? VK_INCOMPLETE : VK_SUCCESS;
}

VKAPI_ATTR
VkResult EnumerateDeviceLayerProperties_Bottom(VkPhysicalDevice /*pdev*/,
                                               uint32_t* properties_count,
                                               VkLayerProperties* properties) {
    uint32_t layer_count =
        EnumerateDeviceLayers(properties ? *properties_count : 0, properties);
    if (!properties || *properties_count > layer_count)
        *properties_count = layer_count;
    return *properties_count < layer_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VKAPI_ATTR
VkResult CreateDevice_Bottom(VkPhysicalDevice gpu,
                             const VkDeviceCreateInfo* create_info,
                             const VkAllocationCallbacks* allocator,
                             VkDevice* device_out) {
    Instance& instance = GetDispatchParent(gpu);
    VkResult result;

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

    result = ActivateAllLayers(create_info, &instance, device);
    if (result != VK_SUCCESS) {
        DestroyDevice(device);
        return result;
    }

    size_t gpu_idx = 0;
    while (instance.physical_devices[gpu_idx] != gpu)
        gpu_idx++;

    uint32_t num_driver_extensions = 0;
    const char** driver_extensions = static_cast<const char**>(
        alloca(create_info->enabledExtensionCount * sizeof(const char*)));
    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
        const char* name = create_info->ppEnabledExtensionNames[i];
        DeviceExtension id = DeviceExtensionFromName(name);
        if (id != kDeviceExtensionCount) {
            if (instance.physical_device_driver_extensions[gpu_idx][id]) {
                driver_extensions[num_driver_extensions++] = name;
                continue;
            }
            if (id == kKHR_swapchain &&
                instance.physical_device_driver_extensions
                    [gpu_idx][kANDROID_native_buffer]) {
                driver_extensions[num_driver_extensions++] =
                    VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME;
                continue;
            }
        }
        bool supported = false;
        for (const auto& layer : device->active_layers) {
            if (layer.SupportsExtension(name))
                supported = true;
        }
        if (!supported) {
            ALOGE(
                "requested device extension '%s' not supported by loader, "
                "driver, or any active layers",
                name);
            DestroyDevice(device);
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    VkDeviceCreateInfo driver_create_info = *create_info;
    driver_create_info.enabledLayerCount = 0;
    driver_create_info.ppEnabledLayerNames = nullptr;
    // TODO(jessehall): As soon as we enumerate device extensions supported by
    // the driver, we need to filter the requested extension list to those
    // supported by the driver here. Also, add the VK_ANDROID_native_buffer
    // extension to the list iff the VK_KHR_swapchain extension was requested,
    // instead of adding it unconditionally like we do now.
    driver_create_info.enabledExtensionCount = num_driver_extensions;
    driver_create_info.ppEnabledExtensionNames = driver_extensions;

    VkDevice drv_device;
    result = instance.drv.dispatch.CreateDevice(gpu, &driver_create_info,
                                                allocator, &drv_device);
    if (result != VK_SUCCESS) {
        DestroyDevice(device);
        return result;
    }

    hwvulkan_dispatch_t* drv_dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(drv_device);
    if (drv_dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
        ALOGE("invalid VkDevice dispatch magic: 0x%" PRIxPTR,
              drv_dispatch->magic);
        PFN_vkDestroyDevice destroy_device =
            reinterpret_cast<PFN_vkDestroyDevice>(
                instance.drv.dispatch.GetDeviceProcAddr(drv_device,
                                                        "vkDestroyDevice"));
        destroy_device(drv_device, allocator);
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    drv_dispatch->vtbl = &device->dispatch;
    device->get_device_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        instance.drv.dispatch.GetDeviceProcAddr(drv_device,
                                                "vkGetDeviceProcAddr"));

    void* base_object = static_cast<void*>(drv_device);
    void* next_object = base_object;
    VkLayerLinkedListElem* next_element;
    PFN_vkGetDeviceProcAddr next_get_proc_addr = GetDeviceProcAddr_Bottom;
    Vector<VkLayerLinkedListElem> elem_list(
        CallbackAllocator<VkLayerLinkedListElem>(instance.alloc));
    try {
        elem_list.resize(device->active_layers.size());
    } catch (std::bad_alloc&) {
        ALOGE("device creation failed: out of memory");
        PFN_vkDestroyDevice destroy_device =
            reinterpret_cast<PFN_vkDestroyDevice>(
                instance.drv.dispatch.GetDeviceProcAddr(drv_device,
                                                        "vkDestroyDevice"));
        destroy_device(drv_device, allocator);
        DestroyDevice(device);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = elem_list.size(); i > 0; i--) {
        size_t idx = i - 1;
        next_element = &elem_list[idx];
        next_element->get_proc_addr =
            reinterpret_cast<PFN_vkGetProcAddr>(next_get_proc_addr);
        next_element->base_object = base_object;
        next_element->next_element = next_object;
        next_object = static_cast<void*>(next_element);

        next_get_proc_addr = device->active_layers[idx].GetGetDeviceProcAddr();
        if (!next_get_proc_addr) {
            next_object = next_element->next_element;
            next_get_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                next_element->get_proc_addr);
        }
    }

    // This is the magic call that initializes all the layer devices and
    // allows them to create their device_handle -> device_data mapping.
    next_get_proc_addr(static_cast<VkDevice>(next_object),
                       "vkGetDeviceProcAddr");

    // We must create all the layer devices *before* retrieving the device
    // procaddrs, so that the layers know which extensions are enabled and
    // therefore which functions to return procaddrs for.
    PFN_vkCreateDevice create_device = reinterpret_cast<PFN_vkCreateDevice>(
        next_get_proc_addr(drv_device, "vkCreateDevice"));
    create_device(gpu, create_info, allocator, &drv_device);

    if (!LoadDeviceDispatchTable(static_cast<VkDevice>(base_object),
                                 next_get_proc_addr, device->dispatch)) {
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *device_out = drv_device;
    return VK_SUCCESS;
}

void DestroyInstance_Bottom(VkInstance vkinstance,
                            const VkAllocationCallbacks* allocator) {
    Instance& instance = GetDispatchParent(vkinstance);

    // These checks allow us to call DestroyInstance_Bottom from any error
    // path in CreateInstance_Bottom, before the driver instance is fully
    // initialized.
    if (instance.drv.instance != VK_NULL_HANDLE &&
        instance.drv.dispatch.DestroyInstance) {
        instance.drv.dispatch.DestroyInstance(instance.drv.instance, allocator);
    }
    if (instance.message) {
        PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback;
        destroy_debug_report_callback =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
                vkGetInstanceProcAddr(vkinstance,
                                      "vkDestroyDebugReportCallbackEXT"));
        destroy_debug_report_callback(vkinstance, instance.message, allocator);
    }
    instance.active_layers.clear();
    const VkAllocationCallbacks* alloc = instance.alloc;
    instance.~Instance();
    alloc->pfnFree(alloc->pUserData, &instance);
}

PFN_vkVoidFunction GetDeviceProcAddr_Bottom(VkDevice vkdevice,
                                            const char* name) {
    if (strcmp(name, "vkCreateDevice") == 0) {
        // TODO(jessehall): Blegh, having this here is disgusting. The current
        // layer init process can't call through the instance dispatch table's
        // vkCreateDevice, because that goes through the instance layers rather
        // than through the device layers. So we need to be able to get the
        // vkCreateDevice pointer through the *device* layer chain.
        //
        // Because we've already created the driver device before calling
        // through the layer vkCreateDevice functions, the loader bottom proc
        // is a no-op.
        return reinterpret_cast<PFN_vkVoidFunction>(Noop);
    }

    // VK_ANDROID_native_buffer should be hidden from applications and layers.
    // TODO(jessehall): Generate this as part of GetLoaderBottomProcAddr.
    PFN_vkVoidFunction pfn;
    if (strcmp(name, "vkGetSwapchainGrallocUsageANDROID") == 0 ||
        strcmp(name, "vkAcquireImageANDROID") == 0 ||
        strcmp(name, "vkQueueSignalReleaseImageANDROID") == 0) {
        return nullptr;
    }
    if ((pfn = GetLoaderBottomProcAddr(name)))
        return pfn;
    return GetDispatchParent(vkdevice).get_device_proc_addr(vkdevice, name);
}

// -----------------------------------------------------------------------------
// Loader top functions. These are called directly from the loader entry
// points or from the application (via vkGetInstanceProcAddr) without going
// through a dispatch table.

VkResult EnumerateInstanceExtensionProperties_Top(
    const char* layer_name,
    uint32_t* properties_count,
    VkExtensionProperties* properties) {
    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkExtensionProperties* extensions = nullptr;
    uint32_t num_extensions = 0;
    if (layer_name) {
        GetInstanceLayerExtensions(layer_name, &extensions, &num_extensions);
    } else {
        VkExtensionProperties* available = static_cast<VkExtensionProperties*>(
            alloca(kInstanceExtensionCount * sizeof(VkExtensionProperties)));
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
        // TODO(jessehall): We need to also enumerate extensions supported by
        // implicitly-enabled layers. Currently we don't have that list of
        // layers until instance creation.
        extensions = available;
    }

    if (!properties || *properties_count > num_extensions)
        *properties_count = num_extensions;
    if (properties)
        std::copy(extensions, extensions + *properties_count, properties);
    return *properties_count < num_extensions ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult EnumerateInstanceLayerProperties_Top(uint32_t* properties_count,
                                              VkLayerProperties* properties) {
    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    uint32_t layer_count =
        EnumerateInstanceLayers(properties ? *properties_count : 0, properties);
    if (!properties || *properties_count > layer_count)
        *properties_count = layer_count;
    return *properties_count < layer_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult CreateInstance_Top(const VkInstanceCreateInfo* create_info,
                            const VkAllocationCallbacks* allocator,
                            VkInstance* instance_out) {
    VkResult result;

    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    if (!allocator)
        allocator = &kDefaultAllocCallbacks;

    VkInstanceCreateInfo local_create_info = *create_info;
    create_info = &local_create_info;

    void* instance_mem = allocator->pfnAllocation(
        allocator->pUserData, sizeof(Instance), alignof(Instance),
        VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
    if (!instance_mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    Instance* instance = new (instance_mem) Instance(allocator);

    result = ActivateAllLayers(create_info, instance, instance);
    if (result != VK_SUCCESS) {
        DestroyInstance_Bottom(instance->handle, allocator);
        return result;
    }

    void* base_object = static_cast<void*>(instance->handle);
    void* next_object = base_object;
    VkLayerLinkedListElem* next_element;
    PFN_vkGetInstanceProcAddr next_get_proc_addr = GetInstanceProcAddr_Bottom;
    Vector<VkLayerLinkedListElem> elem_list(
        CallbackAllocator<VkLayerLinkedListElem>(instance->alloc));
    try {
        elem_list.resize(instance->active_layers.size());
    } catch (std::bad_alloc&) {
        ALOGE("instance creation failed: out of memory");
        DestroyInstance_Bottom(instance->handle, allocator);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = elem_list.size(); i > 0; i--) {
        size_t idx = i - 1;
        next_element = &elem_list[idx];
        next_element->get_proc_addr =
            reinterpret_cast<PFN_vkGetProcAddr>(next_get_proc_addr);
        next_element->base_object = base_object;
        next_element->next_element = next_object;
        next_object = static_cast<void*>(next_element);

        next_get_proc_addr =
            instance->active_layers[idx].GetGetInstanceProcAddr();
        if (!next_get_proc_addr) {
            next_object = next_element->next_element;
            next_get_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                next_element->get_proc_addr);
        }
    }

    // This is the magic call that initializes all the layer instances and
    // allows them to create their instance_handle -> instance_data mapping.
    next_get_proc_addr(static_cast<VkInstance>(next_object),
                       "vkGetInstanceProcAddr");

    if (!LoadInstanceDispatchTable(static_cast<VkInstance>(base_object),
                                   next_get_proc_addr, instance->dispatch)) {
        DestroyInstance_Bottom(instance->handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Force enable callback extension if required
    bool enable_callback = false;
    bool enable_logging = false;
    if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)) {
        enable_callback =
            property_get_bool("debug.vulkan.enable_callback", false);
        enable_logging = enable_callback;
        if (enable_callback) {
            enable_callback = AddExtensionToCreateInfo(
                local_create_info, "VK_EXT_debug_report", instance->alloc);
        }
    }

    VkInstance handle = instance->handle;
    PFN_vkCreateInstance create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(
            next_get_proc_addr(instance->handle, "vkCreateInstance"));
    result = create_instance(create_info, allocator, &handle);
    if (enable_callback)
        FreeAllocatedCreateInfo(local_create_info, instance->alloc);
    if (result >= 0) {
        *instance_out = instance->handle;
    } else {
        // For every layer, including the loader top and bottom layers:
        // - If a call to the next CreateInstance fails, the layer must clean
        //   up anything it has successfully done so far, and propagate the
        //   error upwards.
        // - If a layer successfully calls the next layer's CreateInstance, and
        //   afterwards must fail for some reason, it must call the next layer's
        //   DestroyInstance before returning.
        // - The layer must not call the next layer's DestroyInstance if that
        //   layer's CreateInstance wasn't called, or returned failure.

        // On failure, CreateInstance_Bottom frees the instance struct, so it's
        // already gone at this point. Nothing to do.
    }

    if (enable_logging) {
        const VkDebugReportCallbackCreateInfoEXT callback_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
            .flags =
                VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARN_BIT_EXT,
            .pfnCallback = LogDebugMessageCallback,
        };
        PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback =
            reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
                GetInstanceProcAddr_Top(instance->handle,
                                        "vkCreateDebugReportCallbackEXT"));
        create_debug_report_callback(instance->handle, &callback_create_info,
                                     allocator, &instance->message);
    }

    return result;
}

PFN_vkVoidFunction GetInstanceProcAddr_Top(VkInstance vkinstance,
                                           const char* name) {
    // vkGetInstanceProcAddr(NULL_HANDLE, ..) only works for global commands
    if (!vkinstance)
        return GetLoaderGlobalProcAddr(name);

    const InstanceDispatchTable& dispatch = GetDispatchTable(vkinstance);
    PFN_vkVoidFunction pfn;
    // Always go through the loader-top function if there is one.
    if ((pfn = GetLoaderTopProcAddr(name)))
        return pfn;
    // Otherwise, look up the handler in the instance dispatch table
    if ((pfn = GetDispatchProcAddr(dispatch, name)))
        return pfn;
    // Anything not handled already must be a device-dispatched function
    // without a loader-top. We must return a function that will dispatch based
    // on the dispatchable object parameter -- which is exactly what the
    // exported functions do. So just return them here.
    return GetLoaderExportProcAddr(name);
}

void DestroyInstance_Top(VkInstance instance,
                         const VkAllocationCallbacks* allocator) {
    if (!instance)
        return;
    GetDispatchTable(instance).DestroyInstance(instance, allocator);
}

PFN_vkVoidFunction GetDeviceProcAddr_Top(VkDevice device, const char* name) {
    PFN_vkVoidFunction pfn;
    if (!device)
        return nullptr;
    if ((pfn = GetLoaderTopProcAddr(name)))
        return pfn;
    return GetDispatchProcAddr(GetDispatchTable(device), name);
}

void GetDeviceQueue_Top(VkDevice vkdevice,
                        uint32_t family,
                        uint32_t index,
                        VkQueue* queue_out) {
    const auto& table = GetDispatchTable(vkdevice);
    table.GetDeviceQueue(vkdevice, family, index, queue_out);
    hwvulkan_dispatch_t* queue_dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(*queue_out);
    if (queue_dispatch->magic != HWVULKAN_DISPATCH_MAGIC &&
        queue_dispatch->vtbl != &table)
        ALOGE("invalid VkQueue dispatch magic: 0x%" PRIxPTR,
              queue_dispatch->magic);
    queue_dispatch->vtbl = &table;
}

VkResult AllocateCommandBuffers_Top(
    VkDevice vkdevice,
    const VkCommandBufferAllocateInfo* alloc_info,
    VkCommandBuffer* cmdbufs) {
    const auto& table = GetDispatchTable(vkdevice);
    VkResult result =
        table.AllocateCommandBuffers(vkdevice, alloc_info, cmdbufs);
    if (result != VK_SUCCESS)
        return result;
    for (uint32_t i = 0; i < alloc_info->commandBufferCount; i++) {
        hwvulkan_dispatch_t* cmdbuf_dispatch =
            reinterpret_cast<hwvulkan_dispatch_t*>(cmdbufs[i]);
        ALOGE_IF(cmdbuf_dispatch->magic != HWVULKAN_DISPATCH_MAGIC,
                 "invalid VkCommandBuffer dispatch magic: 0x%" PRIxPTR,
                 cmdbuf_dispatch->magic);
        cmdbuf_dispatch->vtbl = &table;
    }
    return VK_SUCCESS;
}

void DestroyDevice_Top(VkDevice vkdevice,
                       const VkAllocationCallbacks* /*allocator*/) {
    if (!vkdevice)
        return;
    Device& device = GetDispatchParent(vkdevice);
    device.dispatch.DestroyDevice(vkdevice, device.instance->alloc);
    DestroyDevice(&device);
}

// -----------------------------------------------------------------------------

const VkAllocationCallbacks* GetAllocator(VkInstance vkinstance) {
    return GetDispatchParent(vkinstance).alloc;
}

const VkAllocationCallbacks* GetAllocator(VkDevice vkdevice) {
    return GetDispatchParent(vkdevice).instance->alloc;
}

VkInstance GetDriverInstance(VkInstance instance) {
    return GetDispatchParent(instance).drv.instance;
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

}  // namespace vulkan
