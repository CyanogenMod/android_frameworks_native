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
#include <malloc.h>
#include <pthread.h>
#include <string.h>
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

// Define Handle typedef to be void* as returned from dlopen.
typedef void* SharedLibraryHandle;

// Custom versions of std classes that use the vulkan alloc callback.
template <class T>
class CallbackAllocator {
   public:
    typedef T value_type;

    CallbackAllocator(const VkAllocCallbacks* alloc_input)
        : alloc(alloc_input) {}

    template <class T2>
    CallbackAllocator(const CallbackAllocator<T2>& other)
        : alloc(other.alloc) {}

    T* allocate(std::size_t n) {
        void* mem = alloc->pfnAlloc(alloc->pUserData, n * sizeof(T), alignof(T),
                                    VK_SYSTEM_ALLOC_TYPE_INTERNAL);
        return static_cast<T*>(mem);
    }

    void deallocate(T* array, std::size_t /*n*/) {
        alloc->pfnFree(alloc->pUserData, array);
    }

    const VkAllocCallbacks* alloc;
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
          class Pred = std::equal_to<Key> >
using UnorderedMap =
    std::unordered_map<Key,
                       T,
                       Hash,
                       Pred,
                       CallbackAllocator<std::pair<const Key, T> > >;

template <class T>
using Vector = std::vector<T, CallbackAllocator<T> >;

typedef std::basic_string<char,
                          std::char_traits<char>,
                          CallbackAllocator<char> > String;

}  // namespace

// -----------------------------------------------------------------------------

struct VkInstance_T {
    VkInstance_T(const VkAllocCallbacks* alloc_callbacks)
        : vtbl(&vtbl_storage),
          alloc(alloc_callbacks),
          num_physical_devices(0),
          active_layers(
              CallbackAllocator<std::pair<String, SharedLibraryHandle> >(
                  alloc)) {
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

    Vector<std::pair<String, SharedLibraryHandle> > active_layers;

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
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
};

// -----------------------------------------------------------------------------
// Utility Code

inline const InstanceVtbl* GetVtbl(VkPhysicalDevice physicalDevice) {
    return *reinterpret_cast<InstanceVtbl**>(physicalDevice);
}

inline const DeviceVtbl* GetVtbl(VkDevice device) {
    return *reinterpret_cast<DeviceVtbl**>(device);
}
inline const DeviceVtbl* GetVtbl(VkQueue queue) {
    return *reinterpret_cast<DeviceVtbl**>(queue);
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

void FindLayersInDirectory(
    Instance& instance,
    UnorderedMap<String, SharedLibraryHandle>& layer_name_to_handle_map,
    const String& dir_name) {
    DIR* directory;
    struct dirent* entry;
    if ((directory = opendir(dir_name.c_str()))) {
        Vector<VkLayerProperties> properties(
            CallbackAllocator<VkLayerProperties>(instance.alloc));
        while ((entry = readdir(directory))) {
            size_t length = strlen(entry->d_name);
            if (strncmp(entry->d_name, "libVKLayer", 10) != 0 ||
                strncmp(entry->d_name + length - 3, ".so", 3) != 0)
                continue;
            // Open so
            SharedLibraryHandle layer_handle = dlopen(
                (dir_name + entry->d_name).c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!layer_handle) {
                ALOGE("%s failed to load with error %s; Skipping",
                      entry->d_name, dlerror());
                continue;
            }

            // Get Layers in so
            PFN_vkGetGlobalLayerProperties get_layer_properties =
                reinterpret_cast<PFN_vkGetGlobalLayerProperties>(
                    dlsym(layer_handle, "vkGetGlobalLayerProperties"));
            if (!get_layer_properties) {
                ALOGE(
                    "%s failed to find vkGetGlobalLayerProperties with "
                    "error %s; Skipping",
                    entry->d_name, dlerror());
                dlclose(layer_handle);
                continue;
            }
            uint32_t count;
            get_layer_properties(&count, nullptr);

            properties.resize(count);
            get_layer_properties(&count, &properties[0]);

            // Add Layers to potential list
            for (uint32_t i = 0; i < count; ++i) {
                layer_name_to_handle_map.insert(std::make_pair(
                    String(properties[i].layerName,
                           CallbackAllocator<char>(instance.alloc)),
                    layer_handle));
                ALOGV("Found layer %s", properties[i].layerName);
            }
        }
        closedir(directory);
    } else {
        ALOGE("Failed to Open Directory %s: %s (%d)", dir_name.c_str(),
              strerror(errno), errno);
    }
}

void LoadLayer(Instance* instance,
               const String& name,
               SharedLibraryHandle& layer_handle) {
    ALOGV("Loading layer %s", name.c_str());
    instance->active_layers.push_back(std::make_pair(name, layer_handle));
}

VkResult CreateDeviceNoop(VkPhysicalDevice,
                          const VkDeviceCreateInfo*,
                          VkDevice*) {
    return VK_SUCCESS;
}

PFN_vkVoidFunction GetLayerDeviceProcAddr(VkDevice device, const char* name) {
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(GetLayerDeviceProcAddr);
    }
    if (strcmp(name, "vkCreateDevice") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(CreateDeviceNoop);
    }
    if (!device)
        return GetGlobalDeviceProcAddr(name);
    Device* loader_device = reinterpret_cast<Device*>(GetVtbl(device)->device);
    return loader_device->GetDeviceProcAddr(device, name);
}

struct InstanceLayersPair {
    Instance* instance;
    UnorderedMap<String, SharedLibraryHandle>* layers;
};

void LoadLayerFromProperty(const char* name, const char* value, void* data) {
    auto instance_layers_pair = static_cast<InstanceLayersPair*>(data);
    const char prefix[] = "debug.vulkan.layer.";
    const size_t prefixlen = sizeof(prefix) - 1;
    if (value[0] == '\0' || strncmp(name, prefix, prefixlen) != 0)
        return;
    String layer_name_str(
        name + prefixlen,
        CallbackAllocator<char>(instance_layers_pair->instance->alloc));
    LoadLayer(instance_layers_pair->instance, layer_name_str,
              (*instance_layers_pair->layers)[layer_name_str]);
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
    for (auto& layer : instance->active_layers) {
        dlclose(layer.second);
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

    if (!LoadInstanceVtbl(
            instance->drv.vtbl.instance, instance->drv.vtbl.instance,
            g_hwdevice->GetInstanceProcAddr, instance->drv.vtbl)) {
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
    device->GetDeviceProcAddr = instance.drv.GetDeviceProcAddr;

    VkDevice drv_device;
    result = instance.drv.vtbl.CreateDevice(pdev, create_info, &drv_device);
    if (result != VK_SUCCESS) {
        DestroyDevice(device);
        return result;
    }

    hwvulkan_dispatch_t* dispatch =
        reinterpret_cast<hwvulkan_dispatch_t*>(drv_device);
    if (dispatch->magic != HWVULKAN_DISPATCH_MAGIC) {
        ALOGE("invalid VkDevice dispatch magic: 0x%" PRIxPTR, dispatch->magic);
        PFN_vkDestroyDevice destroy_device =
            reinterpret_cast<PFN_vkDestroyDevice>(
                instance.drv.GetDeviceProcAddr(drv_device, "vkDestroyDevice"));
        destroy_device(drv_device);
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    dispatch->vtbl = &device->vtbl_storage;

    device->vtbl_storage.GetSurfacePropertiesKHR = GetSurfacePropertiesKHR;
    device->vtbl_storage.GetSurfaceFormatsKHR = GetSurfaceFormatsKHR;
    device->vtbl_storage.GetSurfacePresentModesKHR = GetSurfacePresentModesKHR;
    device->vtbl_storage.CreateSwapchainKHR = CreateSwapchainKHR;
    device->vtbl_storage.DestroySwapchainKHR = DestroySwapchainKHR;
    device->vtbl_storage.GetSwapchainImagesKHR = GetSwapchainImagesKHR;
    device->vtbl_storage.AcquireNextImageKHR = AcquireNextImageKHR;
    device->vtbl_storage.QueuePresentKHR = QueuePresentKHR;

    void* base_object = static_cast<void*>(drv_device);
    void* next_object = base_object;
    VkLayerLinkedListElem* next_element;
    PFN_vkGetDeviceProcAddr next_get_proc_addr = GetLayerDeviceProcAddr;
    Vector<VkLayerLinkedListElem> elem_list(
        instance.active_layers.size(),
        CallbackAllocator<VkLayerLinkedListElem>(instance.alloc));

    for (size_t i = elem_list.size(); i > 0; i--) {
        size_t idx = i - 1;
        next_element = &elem_list[idx];
        next_element->get_proc_addr =
            reinterpret_cast<PFN_vkGetProcAddr>(next_get_proc_addr);
        next_element->base_object = base_object;
        next_element->next_element = next_object;
        next_object = static_cast<void*>(next_element);

        auto& name_libhandle_pair = instance.active_layers[idx];
        next_get_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
            dlsym(name_libhandle_pair.second,
                  (name_libhandle_pair.first + "GetDeviceProcAddr").c_str()));
        if (!next_get_proc_addr) {
            next_get_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                dlsym(name_libhandle_pair.second, "vkGetDeviceProcAddr"));
            if (!next_get_proc_addr) {
                ALOGE("Cannot find vkGetDeviceProcAddr for %s, error is %s",
                      name_libhandle_pair.first.c_str(), dlerror());
                next_object = next_element->next_element;
                next_get_proc_addr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    next_element->get_proc_addr);
            }
        }
    }

    if (!LoadDeviceVtbl(static_cast<VkDevice>(base_object),
                        static_cast<VkDevice>(next_object), next_get_proc_addr,
                        device->vtbl_storage)) {
        DestroyDevice(device);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkCreateDevice layer_createDevice =
        reinterpret_cast<PFN_vkCreateDevice>(
            device->vtbl_storage.GetDeviceProcAddr(drv_device,
                                                   "vkCreateDevice"));
    layer_createDevice(pdev, create_info, &drv_device);

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
    .GetPhysicalDeviceSurfaceSupportKHR = GetPhysicalDeviceSurfaceSupportKHR,
    // clang-format on
};

VkResult Noop(...) {
    return VK_SUCCESS;
}

PFN_vkVoidFunction GetInstanceProcAddrBottom(VkInstance, const char* name) {
    // TODO: Possibly move this into the instance table
    // TODO: Possibly register the callbacks in the loader
    if (strcmp(name, "vkDbgCreateMsgCallback") == 0 ||
        strcmp(name, "vkDbgDestroyMsgCallback") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(Noop);
    }
    if (strcmp(name, "vkCreateInstance") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(CreateInstanceBottom);
    }
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

    // Scan layers
    // TODO: Add more directories to scan
    UnorderedMap<String, SharedLibraryHandle> layers(
        CallbackAllocator<std::pair<String, SharedLibraryHandle> >(
            instance->alloc));
    CallbackAllocator<char> string_allocator(instance->alloc);
    String dir_name("/data/local/tmp/vulkan/", string_allocator);
    FindLayersInDirectory(*instance, layers, dir_name);

    // Load layers
    {
        char layer_prop[PROPERTY_VALUE_MAX];
        property_get("debug.vulkan.layers", layer_prop, "");
        String layer_name(string_allocator);
        String layer_prop_str(layer_prop, string_allocator);
        size_t end, start = 0;
        while ((end = layer_prop_str.find(':', start)) != std::string::npos) {
            layer_name = layer_prop_str.substr(start, end - start);
            auto element = layers.find(layer_name);
            if (element != layers.end()) {
                LoadLayer(instance, layer_name, element->second);
                layers.erase(element);
            }
            start = end + 1;
        }
        InstanceLayersPair instance_layers_pair = {.instance = instance,
                                                   .layers = &layers};
        property_list(LoadLayerFromProperty,
                      static_cast<void*>(&instance_layers_pair));
    }
    for (uint32_t i = 0; i < create_info->layerCount; ++i) {
        String layer_name(create_info->ppEnabledLayerNames[i],
                          string_allocator);
        auto element = layers.find(layer_name);
        if (element != layers.end()) {
            LoadLayer(instance, layer_name, element->second);
            layers.erase(element);
        }
    }
    for (auto& layer : layers) {
        dlclose(layer.second);
    }

    void* base_object = static_cast<void*>(instance);
    void* next_object = base_object;
    VkLayerLinkedListElem* next_element;
    PFN_vkGetInstanceProcAddr next_get_proc_addr =
        kBottomInstanceFunctions.GetInstanceProcAddr;
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

        auto& name_libhandle_pair = instance->active_layers[idx];
        next_get_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(name_libhandle_pair.second,
                  (name_libhandle_pair.first + "GetInstanceProcAddr").c_str()));
        if (!next_get_proc_addr) {
            next_get_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(name_libhandle_pair.second, "vkGetInstanceProcAddr"));
            if (!next_get_proc_addr) {
                ALOGE("Cannot find vkGetInstanceProcAddr for %s, error is %s",
                      name_libhandle_pair.first.c_str(), dlerror());
                next_object = next_element->next_element;
                next_get_proc_addr =
                    reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        next_element->get_proc_addr);
            }
        }
    }

    if (!LoadInstanceVtbl(static_cast<VkInstance>(base_object),
                          static_cast<VkInstance>(next_object),
                          next_get_proc_addr, instance->vtbl_storage)) {
        DestroyInstanceBottom(instance);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Force enable callback extension if required
    bool enable_callback =
        property_get_bool("debug.vulkan.enable_layer_callback", false);
    const char* callback_name = "DEBUG_REPORT";
    if (enable_callback) {
        for (uint32_t i = 0; i < create_info->extensionCount; ++i) {
            if (!strcmp(callback_name,
                        create_info->ppEnabledExtensionNames[i])) {
                enable_callback = false;
                break;
            }
        }
    }
    if (enable_callback) {
        uint32_t extension_count = local_create_info.extensionCount;
        local_create_info.extensionCount++;
        void* mem = instance->alloc->pfnAlloc(
            instance->alloc->pUserData,
            local_create_info.extensionCount * sizeof(char*), alignof(char*),
            VK_SYSTEM_ALLOC_TYPE_INTERNAL);
        if (mem) {
            const char** enabled_extensions = static_cast<const char**>(mem);
            for (uint32_t i = 0; i < extension_count; ++i) {
                enabled_extensions[i] =
                    local_create_info.ppEnabledExtensionNames[i];
            }
            enabled_extensions[extension_count] = callback_name;
            local_create_info.ppEnabledExtensionNames = enabled_extensions;
        } else {
            ALOGW("DEBUG_REPORT extension cannot be enabled!");
            enable_callback = false;
            local_create_info.extensionCount--;
        }
    }

    *out_instance = instance;
    result = instance->vtbl_storage.CreateInstance(create_info, out_instance);
    if (enable_callback) {
        const char* const* enabled_extensions =
            local_create_info.ppEnabledExtensionNames;
        instance->alloc->pfnFree(instance->alloc->pUserData,
                                 const_cast<char**>(enabled_extensions));
    }
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
    // TODO: Possibly move this into the instance table
    if (strcmp(name, "vkDbgCreateMsgCallback") == 0 ||
        strcmp(name, "vkDbgDestroyMsgCallback") == 0) {
        if (!instance->vtbl)
            return NULL;
        PFN_vkGetInstanceProcAddr gpa = instance->vtbl->GetInstanceProcAddr;
        return reinterpret_cast<PFN_vkVoidFunction>(gpa(instance, name));
    }
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
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr);
    }
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

void* AllocDeviceMem(VkDevice device,
                     size_t size,
                     size_t align,
                     VkSystemAllocType type) {
    const VkAllocCallbacks* alloc_cb =
        static_cast<Device*>(GetVtbl(device)->device)->alloc;
    return alloc_cb->pfnAlloc(alloc_cb->pUserData, size, align, type);
}

void FreeDeviceMem(VkDevice device, void* ptr) {
    const VkAllocCallbacks* alloc_cb =
        static_cast<Device*>(GetVtbl(device)->device)->alloc;
    alloc_cb->pfnFree(alloc_cb->pUserData, ptr);
}

const DeviceVtbl& GetDriverVtbl(VkDevice device) {
    // TODO(jessehall): This actually returns the API-level vtbl for the
    // device, not the driver entry points. Given the current use -- getting
    // the driver's private swapchain-related functions -- that works, but is
    // misleading and likely to cause bugs. Fix as part of separating the
    // loader->driver interface from the app->loader interface.
    return static_cast<Device*>(GetVtbl(device)->device)->vtbl_storage;
}

const DeviceVtbl& GetDriverVtbl(VkQueue queue) {
    // TODO(jessehall): This actually returns the API-level vtbl for the
    // device, not the driver entry points. Given the current use -- getting
    // the driver's private swapchain-related functions -- that works, but is
    // misleading and likely to cause bugs. Fix as part of separating the
    // loader->driver interface from the app->loader interface.
    return static_cast<Device*>(GetVtbl(queue)->device)->vtbl_storage;
}

}  // namespace vulkan
