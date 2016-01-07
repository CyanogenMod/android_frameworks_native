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

#define LOG_NDEBUG 0

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
#include <vulkan/vk_debug_report_lunarg.h>
#include <vulkan/vulkan_loader_data.h>

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
        return static_cast<T*>(mem);
    }

    void deallocate(T* array, std::size_t /*n*/) {
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

template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class Pred = std::equal_to<Key>>
using UnorderedMap =
    std::unordered_map<Key,
                       T,
                       Hash,
                       Pred,
                       CallbackAllocator<std::pair<const Key, T>>>;

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
    return posix_memalign(&ptr, std::max(alignment, sizeof(void*)), size) == 0
               ? ptr
               : nullptr;
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

VKAPI_ATTR void DefaultFree(void*, void* pMem) {
    free(pMem);
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
          get_instance_proc_addr(nullptr),
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

    // TODO(jessehall): Only needed by GetInstanceProcAddr_Top for
    // vkDbg*MessageCallback. Points to the outermost layer's function. Remove
    // once the DEBUG_CALLBACK is integrated into the API file.
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;

    const VkAllocationCallbacks* alloc;
    uint32_t num_physical_devices;
    VkPhysicalDevice physical_devices[kMaxPhysicalDevices];

    Vector<LayerRef> active_layers;
    VkDbgMsgCallback message;

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
bool ActivateLayer(TObject* object, const char* name) {
    LayerRef layer(GetLayerRef(name));
    if (!layer)
        return false;
    if (std::find(object->active_layers.begin(), object->active_layers.end(),
                  layer) == object->active_layers.end())
        object->active_layers.push_back(std::move(layer));
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
    const char prefix[] = "debug.vulkan.layer.";
    const size_t prefixlen = sizeof(prefix) - 1;
    if (value[0] == '\0' || strncmp(name, prefix, prefixlen) != 0)
        return;
    const char* number_str = name + prefixlen;
    long layer_number = strtol(number_str, nullptr, 10);
    if (layer_number <= 0 || layer_number == LONG_MAX) {
        ALOGW("Cannot use a layer at number %ld from string %s", layer_number,
              number_str);
        return;
    }
    auto instance_names_pair = static_cast<InstanceNamesPair*>(data);
    Vector<String>* layer_names = instance_names_pair->layer_names;
    Instance* instance = instance_names_pair->instance;
    size_t layer_size = static_cast<size_t>(layer_number);
    if (layer_size > layer_names->size()) {
        layer_names->resize(layer_size,
                            String(CallbackAllocator<char>(instance->alloc)));
    }
    (*layer_names)[layer_size - 1] = value;
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
        String layer_name(string_allocator);
        String layer_prop_str(layer_prop, string_allocator);
        size_t end, start = 0;
        while ((end = layer_prop_str.find(':', start)) != std::string::npos) {
            layer_name = layer_prop_str.substr(start, end - start);
            ActivateLayer(object, layer_name.c_str());
            start = end + 1;
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
    for (uint32_t i = 0; i < create_info->enabledLayerNameCount; ++i) {
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
    for (uint32_t i = 0; i < local_create_info.enabledExtensionNameCount; ++i) {
        if (!strcmp(extension_name,
                    local_create_info.ppEnabledExtensionNames[i])) {
            return false;
        }
    }
    uint32_t extension_count = local_create_info.enabledExtensionNameCount;
    local_create_info.enabledExtensionNameCount++;
    void* mem = alloc->pfnAllocation(
        alloc->pUserData,
        local_create_info.enabledExtensionNameCount * sizeof(char*),
        alignof(char*), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
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
        local_create_info.enabledExtensionNameCount--;
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
VkBool32 LogDebugMessageCallback(VkFlags message_flags,
                                 VkDbgObjectType /*obj_type*/,
                                 uint64_t /*src_object*/,
                                 size_t /*location*/,
                                 int32_t message_code,
                                 const char* layer_prefix,
                                 const char* message,
                                 void* /*user_data*/) {
    if (message_flags & VK_DBG_REPORT_ERROR_BIT) {
        ALOGE("[%s] Code %d : %s", layer_prefix, message_code, message);
    } else if (message_flags & VK_DBG_REPORT_WARN_BIT) {
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

    result = g_hwdevice->CreateInstance(create_info, instance.alloc,
                                        &instance.drv.instance);
    if (result != VK_SUCCESS) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return result;
    }

    if (!LoadDriverDispatchTable(instance.drv.instance,
                                 g_hwdevice->GetInstanceProcAddr,
                                 instance.drv.dispatch)) {
        DestroyInstance_Bottom(instance.handle, allocator);
        return VK_ERROR_INITIALIZATION_FAILED;
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
    }
    instance.drv.num_physical_devices = num_physical_devices;

    instance.num_physical_devices = instance.drv.num_physical_devices;
    return VK_SUCCESS;
}

PFN_vkVoidFunction GetInstanceProcAddr_Bottom(VkInstance, const char* name) {
    PFN_vkVoidFunction pfn;
    if ((pfn = GetLoaderBottomProcAddr(name)))
        return pfn;
    // TODO: Possibly move this into the instance table
    // TODO: Possibly register the callbacks in the loader
    if (strcmp(name, "vkDbgCreateMsgCallback") == 0 ||
        strcmp(name, "vkDbgDestroyMsgCallback") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(Noop);
    }
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
    VkPhysicalDevice /*pdev*/,
    const char* /*layer_name*/,
    uint32_t* properties_count,
    VkExtensionProperties* /*properties*/) {
    // TODO(jessehall): Implement me...
    *properties_count = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult EnumerateDeviceLayerProperties_Bottom(VkPhysicalDevice /*pdev*/,
                                               uint32_t* properties_count,
                                               VkLayerProperties* /*properties*/) {
    // TODO(jessehall): Implement me...
    *properties_count = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR
VkResult CreateDevice_Bottom(VkPhysicalDevice pdev,
                             const VkDeviceCreateInfo* create_info,
                             const VkAllocationCallbacks* allocator,
                             VkDevice* device_out) {
    Instance& instance = GetDispatchParent(pdev);
    VkResult result;

    if (!allocator) {
        if (instance.alloc)
            allocator = instance.alloc;
        else
            allocator = &kDefaultAllocCallbacks;
    }

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

    VkDevice drv_device;
    result = instance.drv.dispatch.CreateDevice(pdev, create_info, allocator,
                                                &drv_device);
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
        device->active_layers.size(),
        CallbackAllocator<VkLayerLinkedListElem>(instance.alloc));

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
    create_device(pdev, create_info, allocator, &drv_device);

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
        PFN_vkDbgDestroyMsgCallback DebugDestroyMessageCallback;
        DebugDestroyMessageCallback =
            reinterpret_cast<PFN_vkDbgDestroyMsgCallback>(
                vkGetInstanceProcAddr(vkinstance, "vkDbgDestroyMsgCallback"));
        DebugDestroyMessageCallback(vkinstance, instance.message);
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
        GetLayerExtensions(layer_name, &extensions, &num_extensions);
    } else {
        static const VkExtensionProperties kInstanceExtensions[] =
            {{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_REVISION},
             {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_REVISION}};
        extensions = kInstanceExtensions;
        num_extensions = sizeof(kInstanceExtensions) / sizeof(kInstanceExtensions[0]);
        // TODO(jessehall): We need to also enumerate extensions supported by
        // implicitly-enabled layers. Currently we don't have that list of
        // layers until instance creation.
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
        EnumerateLayers(properties ? *properties_count : 0, properties);
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
        instance->active_layers.size(),
        CallbackAllocator<VkLayerLinkedListElem>(instance->alloc));

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
    instance->get_instance_proc_addr = next_get_proc_addr;

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
                local_create_info, "DEBUG_REPORT", instance->alloc);
        }
    }

    *instance_out = instance->handle;
    PFN_vkCreateInstance create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(
            next_get_proc_addr(instance->handle, "vkCreateInstance"));
    result = create_instance(create_info, allocator, instance_out);
    if (enable_callback)
        FreeAllocatedCreateInfo(local_create_info, instance->alloc);
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

        // On failure, CreateInstance_Bottom frees the instance struct, so it's
        // already gone at this point. Nothing to do.
    }

    if (enable_logging) {
        PFN_vkDbgCreateMsgCallback dbg_create_msg_callback;
        dbg_create_msg_callback = reinterpret_cast<PFN_vkDbgCreateMsgCallback>(
            GetInstanceProcAddr_Top(instance->handle,
                                    "vkDbgCreateMsgCallback"));
        dbg_create_msg_callback(
            instance->handle, VK_DBG_REPORT_ERROR_BIT | VK_DBG_REPORT_WARN_BIT,
            LogDebugMessageCallback, nullptr, &instance->message);
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
    // TODO(jessehall): Generate these into the instance dispatch table, and
    // add loader-bottom procs for them.
    if (strcmp(name, "vkDbgCreateMsgCallback") == 0 ||
        strcmp(name, "vkDbgDestroyMsgCallback") == 0) {
        return GetDispatchParent(vkinstance)
            .get_instance_proc_addr(vkinstance, name);
    }
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
    for (uint32_t i = 0; i < alloc_info->bufferCount; i++) {
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

const DriverDispatchTable& GetDriverDispatch(VkDevice device) {
    return GetDispatchParent(device).instance->drv.dispatch;
}

const DriverDispatchTable& GetDriverDispatch(VkQueue queue) {
    return GetDispatchParent(queue).instance->drv.dispatch;
}

}  // namespace vulkan
