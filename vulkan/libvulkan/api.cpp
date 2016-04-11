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

// The API layer of the loader defines Vulkan API and manages layers.  The
// entrypoints are generated and defined in api_dispatch.cpp.  Most of them
// simply find the dispatch table and jump.
//
// There are a few of them requiring manual code for things such as layer
// discovery or chaining.  They call into functions defined in this file.

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <mutex>
#include <new>
#include <utility>
#include <cutils/properties.h>
#include <log/log.h>

#include <vulkan/vk_layer_interface.h>
#include "api.h"
#include "driver.h"
#include "layers_extensions.h"

namespace vulkan {
namespace api {

namespace {

// Provide overridden layer names when there are implicit layers.  No effect
// otherwise.
class OverrideLayerNames {
   public:
    OverrideLayerNames(bool is_instance, const VkAllocationCallbacks& allocator)
        : is_instance_(is_instance),
          allocator_(allocator),
          scope_(VK_SYSTEM_ALLOCATION_SCOPE_COMMAND),
          names_(nullptr),
          name_count_(0),
          implicit_layers_() {
        implicit_layers_.result = VK_SUCCESS;
    }

    ~OverrideLayerNames() {
        allocator_.pfnFree(allocator_.pUserData, names_);
        allocator_.pfnFree(allocator_.pUserData, implicit_layers_.elements);
        allocator_.pfnFree(allocator_.pUserData, implicit_layers_.name_pool);
    }

    VkResult Parse(const char* const* names, uint32_t count) {
        AddImplicitLayers();

        const auto& arr = implicit_layers_;
        if (arr.result != VK_SUCCESS)
            return arr.result;

        // no need to override when there is no implicit layer
        if (!arr.count)
            return VK_SUCCESS;

        names_ = AllocateNameArray(arr.count + count);
        if (!names_)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        // add implicit layer names
        for (uint32_t i = 0; i < arr.count; i++)
            names_[i] = GetImplicitLayerName(i);

        name_count_ = arr.count;

        // add explicit layer names
        for (uint32_t i = 0; i < count; i++) {
            // ignore explicit layers that are also implicit
            if (IsImplicitLayer(names[i]))
                continue;

            names_[name_count_++] = names[i];
        }

        return VK_SUCCESS;
    }

    const char* const* Names() const { return names_; }

    uint32_t Count() const { return name_count_; }

   private:
    struct ImplicitLayer {
        int priority;
        size_t name_offset;
    };

    struct ImplicitLayerArray {
        ImplicitLayer* elements;
        uint32_t max_count;
        uint32_t count;

        char* name_pool;
        size_t max_pool_size;
        size_t pool_size;

        VkResult result;
    };

    void AddImplicitLayers() {
        if (!driver::Debuggable())
            return;

        ParseDebugVulkanLayers();
        property_list(ParseDebugVulkanLayer, this);

        // sort by priorities
        auto& arr = implicit_layers_;
        std::sort(arr.elements, arr.elements + arr.count,
                  [](const ImplicitLayer& a, const ImplicitLayer& b) {
                      return (a.priority < b.priority);
                  });
    }

    void ParseDebugVulkanLayers() {
        // debug.vulkan.layers specifies colon-separated layer names
        char prop[PROPERTY_VALUE_MAX];
        if (!property_get("debug.vulkan.layers", prop, ""))
            return;

        // assign negative/high priorities to them
        int prio = -PROPERTY_VALUE_MAX;

        const char* p = prop;
        const char* delim;
        while ((delim = strchr(p, ':'))) {
            if (delim > p)
                AddImplicitLayer(prio, p, static_cast<size_t>(delim - p));

            prio++;
            p = delim + 1;
        }

        if (p[0] != '\0')
            AddImplicitLayer(prio, p, strlen(p));
    }

    static void ParseDebugVulkanLayer(const char* key,
                                      const char* val,
                                      void* user_data) {
        static const char prefix[] = "debug.vulkan.layer.";
        const size_t prefix_len = sizeof(prefix) - 1;

        if (strncmp(key, prefix, prefix_len) || val[0] == '\0')
            return;
        key += prefix_len;

        // debug.vulkan.layer.<priority>
        int priority = -1;
        if (key[0] >= '0' && key[0] <= '9')
            priority = atoi(key);

        if (priority < 0) {
            ALOGW("Ignored implicit layer %s with invalid priority %s", val,
                  key);
            return;
        }

        OverrideLayerNames& override_layers =
            *reinterpret_cast<OverrideLayerNames*>(user_data);
        override_layers.AddImplicitLayer(priority, val, strlen(val));
    }

    void AddImplicitLayer(int priority, const char* name, size_t len) {
        if (!GrowImplicitLayerArray(1, 0))
            return;

        auto& arr = implicit_layers_;
        auto& layer = arr.elements[arr.count++];

        layer.priority = priority;
        layer.name_offset = AddImplicitLayerName(name, len);

        ALOGV("Added implicit layer %s", GetImplicitLayerName(arr.count - 1));
    }

    size_t AddImplicitLayerName(const char* name, size_t len) {
        if (!GrowImplicitLayerArray(0, len + 1))
            return 0;

        // add the name to the pool
        auto& arr = implicit_layers_;
        size_t offset = arr.pool_size;
        char* dst = arr.name_pool + offset;

        std::copy(name, name + len, dst);
        dst[len] = '\0';

        arr.pool_size += len + 1;

        return offset;
    }

    bool GrowImplicitLayerArray(uint32_t layer_count, size_t name_size) {
        const uint32_t initial_max_count = 16;
        const size_t initial_max_pool_size = 512;

        auto& arr = implicit_layers_;

        // grow the element array if needed
        while (arr.count + layer_count > arr.max_count) {
            uint32_t new_max_count =
                (arr.max_count) ? (arr.max_count << 1) : initial_max_count;
            void* new_mem = nullptr;

            if (new_max_count > arr.max_count) {
                new_mem = allocator_.pfnReallocation(
                    allocator_.pUserData, arr.elements,
                    sizeof(ImplicitLayer) * new_max_count,
                    alignof(ImplicitLayer), scope_);
            }

            if (!new_mem) {
                arr.result = VK_ERROR_OUT_OF_HOST_MEMORY;
                arr.count = 0;
                return false;
            }

            arr.elements = reinterpret_cast<ImplicitLayer*>(new_mem);
            arr.max_count = new_max_count;
        }

        // grow the name pool if needed
        while (arr.pool_size + name_size > arr.max_pool_size) {
            size_t new_max_pool_size = (arr.max_pool_size)
                                           ? (arr.max_pool_size << 1)
                                           : initial_max_pool_size;
            void* new_mem = nullptr;

            if (new_max_pool_size > arr.max_pool_size) {
                new_mem = allocator_.pfnReallocation(
                    allocator_.pUserData, arr.name_pool, new_max_pool_size,
                    alignof(char), scope_);
            }

            if (!new_mem) {
                arr.result = VK_ERROR_OUT_OF_HOST_MEMORY;
                arr.pool_size = 0;
                return false;
            }

            arr.name_pool = reinterpret_cast<char*>(new_mem);
            arr.max_pool_size = new_max_pool_size;
        }

        return true;
    }

    const char* GetImplicitLayerName(uint32_t index) const {
        const auto& arr = implicit_layers_;

        // this may return nullptr when arr.result is not VK_SUCCESS
        return implicit_layers_.name_pool + arr.elements[index].name_offset;
    }

    bool IsImplicitLayer(const char* name) const {
        const auto& arr = implicit_layers_;

        for (uint32_t i = 0; i < arr.count; i++) {
            if (strcmp(name, GetImplicitLayerName(i)) == 0)
                return true;
        }

        return false;
    }

    const char** AllocateNameArray(uint32_t count) const {
        return reinterpret_cast<const char**>(allocator_.pfnAllocation(
            allocator_.pUserData, sizeof(const char*) * count,
            alignof(const char*), scope_));
    }

    const bool is_instance_;
    const VkAllocationCallbacks& allocator_;
    const VkSystemAllocationScope scope_;

    const char** names_;
    uint32_t name_count_;

    ImplicitLayerArray implicit_layers_;
};

// Provide overridden extension names when there are implicit extensions.
// No effect otherwise.
//
// This is used only to enable VK_EXT_debug_report.
class OverrideExtensionNames {
   public:
    OverrideExtensionNames(bool is_instance,
                           const VkAllocationCallbacks& allocator)
        : is_instance_(is_instance),
          allocator_(allocator),
          scope_(VK_SYSTEM_ALLOCATION_SCOPE_COMMAND),
          names_(nullptr),
          name_count_(0),
          install_debug_callback_(false) {}

    ~OverrideExtensionNames() {
        allocator_.pfnFree(allocator_.pUserData, names_);
    }

    VkResult Parse(const char* const* names, uint32_t count) {
        // this is only for debug.vulkan.enable_callback
        if (!EnableDebugCallback())
            return VK_SUCCESS;

        names_ = AllocateNameArray(count + 1);
        if (!names_)
            return VK_ERROR_OUT_OF_HOST_MEMORY;

        std::copy(names, names + count, names_);

        name_count_ = count;
        names_[name_count_++] = "VK_EXT_debug_report";

        install_debug_callback_ = true;

        return VK_SUCCESS;
    }

    const char* const* Names() const { return names_; }

    uint32_t Count() const { return name_count_; }

    bool InstallDebugCallback() const { return install_debug_callback_; }

   private:
    bool EnableDebugCallback() const {
        return (is_instance_ && driver::Debuggable() &&
                property_get_bool("debug.vulkan.enable_callback", false));
    }

    const char** AllocateNameArray(uint32_t count) const {
        return reinterpret_cast<const char**>(allocator_.pfnAllocation(
            allocator_.pUserData, sizeof(const char*) * count,
            alignof(const char*), scope_));
    }

    const bool is_instance_;
    const VkAllocationCallbacks& allocator_;
    const VkSystemAllocationScope scope_;

    const char** names_;
    uint32_t name_count_;
    bool install_debug_callback_;
};

// vkCreateInstance and vkCreateDevice helpers with support for layer
// chaining.
class LayerChain {
   public:
    static VkResult create_instance(const VkInstanceCreateInfo* create_info,
                                    const VkAllocationCallbacks* allocator,
                                    VkInstance* instance_out);

    static VkResult create_device(VkPhysicalDevice physical_dev,
                                  const VkDeviceCreateInfo* create_info,
                                  const VkAllocationCallbacks* allocator,
                                  VkDevice* dev_out);

    static void destroy_instance(VkInstance instance,
                                 const VkAllocationCallbacks* allocator);

    static void destroy_device(VkDevice dev,
                               const VkAllocationCallbacks* allocator);

   private:
    struct ActiveLayer {
        LayerRef ref;
        union {
            VkLayerInstanceLink instance_link;
            VkLayerDeviceLink device_link;
        };
    };

    LayerChain(bool is_instance, const VkAllocationCallbacks& allocator);
    ~LayerChain();

    VkResult activate_layers(const char* const* layer_names,
                             uint32_t layer_count,
                             const char* const* extension_names,
                             uint32_t extension_count);
    ActiveLayer* allocate_layer_array(uint32_t count) const;
    VkResult load_layer(ActiveLayer& layer, const char* name);
    void setup_layer_links();

    bool empty() const;
    void modify_create_info(VkInstanceCreateInfo& info);
    void modify_create_info(VkDeviceCreateInfo& info);

    VkResult create(const VkInstanceCreateInfo* create_info,
                    const VkAllocationCallbacks* allocator,
                    VkInstance* instance_out);

    VkResult create(VkPhysicalDevice physical_dev,
                    const VkDeviceCreateInfo* create_info,
                    const VkAllocationCallbacks* allocator,
                    VkDevice* dev_out);

    VkResult validate_extensions(const char* const* extension_names,
                                 uint32_t extension_count);
    VkResult validate_extensions(VkPhysicalDevice physical_dev,
                                 const char* const* extension_names,
                                 uint32_t extension_count);
    VkExtensionProperties* allocate_driver_extension_array(
        uint32_t count) const;
    bool is_layer_extension(const char* name) const;
    bool is_driver_extension(const char* name) const;

    template <typename DataType>
    void steal_layers(DataType& data);

    static void destroy_layers(ActiveLayer* layers,
                               uint32_t count,
                               const VkAllocationCallbacks& allocator);

    static VKAPI_ATTR VkBool32
    debug_report_callback(VkDebugReportFlagsEXT flags,
                          VkDebugReportObjectTypeEXT obj_type,
                          uint64_t obj,
                          size_t location,
                          int32_t msg_code,
                          const char* layer_prefix,
                          const char* msg,
                          void* user_data);

    const bool is_instance_;
    const VkAllocationCallbacks& allocator_;

    OverrideLayerNames override_layers_;
    OverrideExtensionNames override_extensions_;

    ActiveLayer* layers_;
    uint32_t layer_count_;

    PFN_vkGetInstanceProcAddr get_instance_proc_addr_;
    PFN_vkGetDeviceProcAddr get_device_proc_addr_;

    union {
        VkLayerInstanceCreateInfo instance_chain_info_;
        VkLayerDeviceCreateInfo device_chain_info_;
    };

    VkExtensionProperties* driver_extensions_;
    uint32_t driver_extension_count_;
};

LayerChain::LayerChain(bool is_instance, const VkAllocationCallbacks& allocator)
    : is_instance_(is_instance),
      allocator_(allocator),
      override_layers_(is_instance, allocator),
      override_extensions_(is_instance, allocator),
      layers_(nullptr),
      layer_count_(0),
      get_instance_proc_addr_(nullptr),
      get_device_proc_addr_(nullptr),
      driver_extensions_(nullptr),
      driver_extension_count_(0) {}

LayerChain::~LayerChain() {
    allocator_.pfnFree(allocator_.pUserData, driver_extensions_);
    destroy_layers(layers_, layer_count_, allocator_);
}

VkResult LayerChain::activate_layers(const char* const* layer_names,
                                     uint32_t layer_count,
                                     const char* const* extension_names,
                                     uint32_t extension_count) {
    VkResult result = override_layers_.Parse(layer_names, layer_count);
    if (result != VK_SUCCESS)
        return result;

    result = override_extensions_.Parse(extension_names, extension_count);
    if (result != VK_SUCCESS)
        return result;

    if (override_layers_.Count()) {
        layer_names = override_layers_.Names();
        layer_count = override_layers_.Count();
    }

    if (!layer_count) {
        // point head of chain to the driver
        get_instance_proc_addr_ = driver::GetInstanceProcAddr;
        if (!is_instance_)
            get_device_proc_addr_ = driver::GetDeviceProcAddr;

        return VK_SUCCESS;
    }

    layers_ = allocate_layer_array(layer_count);
    if (!layers_)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    // load layers
    for (uint32_t i = 0; i < layer_count; i++) {
        result = load_layer(layers_[i], layer_names[i]);
        if (result != VK_SUCCESS)
            return result;

        // count loaded layers for proper destructions on errors
        layer_count_++;
    }

    setup_layer_links();

    return VK_SUCCESS;
}

LayerChain::ActiveLayer* LayerChain::allocate_layer_array(
    uint32_t count) const {
    VkSystemAllocationScope scope = (is_instance_)
                                        ? VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE
                                        : VK_SYSTEM_ALLOCATION_SCOPE_DEVICE;

    return reinterpret_cast<ActiveLayer*>(allocator_.pfnAllocation(
        allocator_.pUserData, sizeof(ActiveLayer) * count, alignof(ActiveLayer),
        scope));
}

VkResult LayerChain::load_layer(ActiveLayer& layer, const char* name) {
    if (is_instance_)
        new (&layer) ActiveLayer{GetInstanceLayerRef(name), {}};
    else
        new (&layer) ActiveLayer{GetDeviceLayerRef(name), {}};

    if (!layer.ref) {
        ALOGE("Failed to load layer %s", name);
        layer.ref.~LayerRef();
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    ALOGI("Loaded %s layer %s", (is_instance_) ? "instance" : "device", name);

    return VK_SUCCESS;
}

void LayerChain::setup_layer_links() {
    if (is_instance_) {
        for (uint32_t i = 0; i < layer_count_; i++) {
            ActiveLayer& layer = layers_[i];

            // point head of chain to the first layer
            if (i == 0)
                get_instance_proc_addr_ = layer.ref.GetGetInstanceProcAddr();

            // point tail of chain to the driver
            if (i == layer_count_ - 1) {
                layer.instance_link.pNext = nullptr;
                layer.instance_link.pfnNextGetInstanceProcAddr =
                    driver::GetInstanceProcAddr;
                break;
            }

            const ActiveLayer& next = layers_[i + 1];

            // const_cast as some naughty layers want to modify our links!
            layer.instance_link.pNext =
                const_cast<VkLayerInstanceLink*>(&next.instance_link);
            layer.instance_link.pfnNextGetInstanceProcAddr =
                next.ref.GetGetInstanceProcAddr();
        }
    } else {
        for (uint32_t i = 0; i < layer_count_; i++) {
            ActiveLayer& layer = layers_[i];

            // point head of chain to the first layer
            if (i == 0) {
                get_instance_proc_addr_ = layer.ref.GetGetInstanceProcAddr();
                get_device_proc_addr_ = layer.ref.GetGetDeviceProcAddr();
            }

            // point tail of chain to the driver
            if (i == layer_count_ - 1) {
                layer.device_link.pNext = nullptr;
                layer.device_link.pfnNextGetInstanceProcAddr =
                    driver::GetInstanceProcAddr;
                layer.device_link.pfnNextGetDeviceProcAddr =
                    driver::GetDeviceProcAddr;
                break;
            }

            const ActiveLayer& next = layers_[i + 1];

            // const_cast as some naughty layers want to modify our links!
            layer.device_link.pNext =
                const_cast<VkLayerDeviceLink*>(&next.device_link);
            layer.device_link.pfnNextGetInstanceProcAddr =
                next.ref.GetGetInstanceProcAddr();
            layer.device_link.pfnNextGetDeviceProcAddr =
                next.ref.GetGetDeviceProcAddr();
        }
    }
}

bool LayerChain::empty() const {
    return (!layer_count_ && !override_layers_.Count() &&
            !override_extensions_.Count());
}

void LayerChain::modify_create_info(VkInstanceCreateInfo& info) {
    if (layer_count_) {
        const ActiveLayer& layer = layers_[0];

        instance_chain_info_.sType =
            VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
        instance_chain_info_.function = VK_LAYER_FUNCTION_LINK;
        // TODO fix vk_layer_interface.h and get rid of const_cast?
        instance_chain_info_.u.pLayerInfo =
            const_cast<VkLayerInstanceLink*>(&layer.instance_link);

        // insert layer info
        instance_chain_info_.pNext = info.pNext;
        info.pNext = &instance_chain_info_;
    }

    if (override_layers_.Count()) {
        info.enabledLayerCount = override_layers_.Count();
        info.ppEnabledLayerNames = override_layers_.Names();
    }

    if (override_extensions_.Count()) {
        info.enabledExtensionCount = override_extensions_.Count();
        info.ppEnabledExtensionNames = override_extensions_.Names();
    }
}

void LayerChain::modify_create_info(VkDeviceCreateInfo& info) {
    if (layer_count_) {
        const ActiveLayer& layer = layers_[0];

        device_chain_info_.sType = VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;
        device_chain_info_.function = VK_LAYER_FUNCTION_LINK;
        // TODO fix vk_layer_interface.h and get rid of const_cast?
        device_chain_info_.u.pLayerInfo =
            const_cast<VkLayerDeviceLink*>(&layer.device_link);

        // insert layer info
        device_chain_info_.pNext = info.pNext;
        info.pNext = &device_chain_info_;
    }

    if (override_layers_.Count()) {
        info.enabledLayerCount = override_layers_.Count();
        info.ppEnabledLayerNames = override_layers_.Names();
    }

    if (override_extensions_.Count()) {
        info.enabledExtensionCount = override_extensions_.Count();
        info.ppEnabledExtensionNames = override_extensions_.Names();
    }
}

VkResult LayerChain::create(const VkInstanceCreateInfo* create_info,
                            const VkAllocationCallbacks* allocator,
                            VkInstance* instance_out) {
    VkResult result = validate_extensions(create_info->ppEnabledExtensionNames,
                                          create_info->enabledExtensionCount);
    if (result != VK_SUCCESS)
        return result;

    // call down the chain
    PFN_vkCreateInstance create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(
            get_instance_proc_addr_(VK_NULL_HANDLE, "vkCreateInstance"));
    VkInstance instance;
    result = create_instance(create_info, allocator, &instance);
    if (result != VK_SUCCESS)
        return result;

    // initialize InstanceData
    InstanceData& data = GetData(instance);
    memset(&data, 0, sizeof(data));

    data.instance = instance;

    if (!InitDispatchTable(instance, get_instance_proc_addr_)) {
        if (data.dispatch.DestroyInstance)
            data.dispatch.DestroyInstance(instance, allocator);

        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // install debug report callback
    if (override_extensions_.InstallDebugCallback()) {
        PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback =
            reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
                get_instance_proc_addr_(instance,
                                        "vkCreateDebugReportCallbackEXT"));
        data.destroy_debug_callback =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
                get_instance_proc_addr_(instance,
                                        "vkDestroyDebugReportCallbackEXT"));
        if (!create_debug_report_callback || !data.destroy_debug_callback) {
            ALOGE("Broken VK_EXT_debug_report support");
            data.dispatch.DestroyInstance(instance, allocator);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkDebugReportCallbackCreateInfoEXT debug_callback_info = {};
        debug_callback_info.sType =
            VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        debug_callback_info.flags =
            VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debug_callback_info.pfnCallback = debug_report_callback;

        VkDebugReportCallbackEXT debug_callback;
        result = create_debug_report_callback(instance, &debug_callback_info,
                                              nullptr, &debug_callback);
        if (result != VK_SUCCESS) {
            ALOGE("Failed to install debug report callback");
            data.dispatch.DestroyInstance(instance, allocator);
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        data.debug_callback = debug_callback;

        ALOGI("Installed debug report callback");
    }

    steal_layers(data);

    *instance_out = instance;

    return VK_SUCCESS;
}

VkResult LayerChain::create(VkPhysicalDevice physical_dev,
                            const VkDeviceCreateInfo* create_info,
                            const VkAllocationCallbacks* allocator,
                            VkDevice* dev_out) {
    VkResult result =
        validate_extensions(physical_dev, create_info->ppEnabledExtensionNames,
                            create_info->enabledExtensionCount);
    if (result != VK_SUCCESS)
        return result;

    // call down the chain
    //
    // TODO Instance call chain available at
    // GetData(physical_dev).dispatch.CreateDevice is ignored.  Is that
    // right?
    VkInstance instance = GetData(physical_dev).instance;
    PFN_vkCreateDevice create_device = reinterpret_cast<PFN_vkCreateDevice>(
        get_instance_proc_addr_(instance, "vkCreateDevice"));
    VkDevice dev;
    result = create_device(physical_dev, create_info, allocator, &dev);
    if (result != VK_SUCCESS)
        return result;

    // initialize DeviceData
    DeviceData& data = GetData(dev);
    memset(&data, 0, sizeof(data));

    if (!InitDispatchTable(dev, get_device_proc_addr_)) {
        if (data.dispatch.DestroyDevice)
            data.dispatch.DestroyDevice(dev, allocator);

        return VK_ERROR_INITIALIZATION_FAILED;
    }

    steal_layers(data);

    *dev_out = dev;

    return VK_SUCCESS;
}

VkResult LayerChain::validate_extensions(const char* const* extension_names,
                                         uint32_t extension_count) {
    if (!extension_count)
        return VK_SUCCESS;

    // query driver instance extensions
    uint32_t count;
    VkResult result =
        EnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (result == VK_SUCCESS && count) {
        driver_extensions_ = allocate_driver_extension_array(count);
        result = (driver_extensions_) ? EnumerateInstanceExtensionProperties(
                                            nullptr, &count, driver_extensions_)
                                      : VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (result != VK_SUCCESS)
        return result;

    driver_extension_count_ = count;

    for (uint32_t i = 0; i < extension_count; i++) {
        const char* name = extension_names[i];
        if (!is_layer_extension(name) && !is_driver_extension(name)) {
            ALOGE("Failed to enable missing instance extension %s", name);
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    return VK_SUCCESS;
}

VkResult LayerChain::validate_extensions(VkPhysicalDevice physical_dev,
                                         const char* const* extension_names,
                                         uint32_t extension_count) {
    if (!extension_count)
        return VK_SUCCESS;

    // query driver device extensions
    uint32_t count;
    VkResult result = EnumerateDeviceExtensionProperties(physical_dev, nullptr,
                                                         &count, nullptr);
    if (result == VK_SUCCESS && count) {
        driver_extensions_ = allocate_driver_extension_array(count);
        result = (driver_extensions_)
                     ? EnumerateDeviceExtensionProperties(
                           physical_dev, nullptr, &count, driver_extensions_)
                     : VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (result != VK_SUCCESS)
        return result;

    driver_extension_count_ = count;

    for (uint32_t i = 0; i < extension_count; i++) {
        const char* name = extension_names[i];
        if (!is_layer_extension(name) && !is_driver_extension(name)) {
            ALOGE("Failed to enable missing device extension %s", name);
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    return VK_SUCCESS;
}

VkExtensionProperties* LayerChain::allocate_driver_extension_array(
    uint32_t count) const {
    return reinterpret_cast<VkExtensionProperties*>(allocator_.pfnAllocation(
        allocator_.pUserData, sizeof(VkExtensionProperties) * count,
        alignof(VkExtensionProperties), VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));
}

bool LayerChain::is_layer_extension(const char* name) const {
    for (uint32_t i = 0; i < layer_count_; i++) {
        const ActiveLayer& layer = layers_[i];
        if (layer.ref.SupportsExtension(name))
            return true;
    }

    return false;
}

bool LayerChain::is_driver_extension(const char* name) const {
    for (uint32_t i = 0; i < driver_extension_count_; i++) {
        if (strcmp(driver_extensions_[i].extensionName, name) == 0)
            return true;
    }

    return false;
}

template <typename DataType>
void LayerChain::steal_layers(DataType& data) {
    data.layers = layers_;
    data.layer_count = layer_count_;

    layers_ = nullptr;
    layer_count_ = 0;
}

void LayerChain::destroy_layers(ActiveLayer* layers,
                                uint32_t count,
                                const VkAllocationCallbacks& allocator) {
    for (uint32_t i = 0; i < count; i++)
        layers[i].ref.~LayerRef();

    allocator.pfnFree(allocator.pUserData, layers);
}

VkBool32 LayerChain::debug_report_callback(VkDebugReportFlagsEXT flags,
                                           VkDebugReportObjectTypeEXT obj_type,
                                           uint64_t obj,
                                           size_t location,
                                           int32_t msg_code,
                                           const char* layer_prefix,
                                           const char* msg,
                                           void* user_data) {
    int prio;

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        prio = ANDROID_LOG_ERROR;
    else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
        prio = ANDROID_LOG_WARN;
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        prio = ANDROID_LOG_INFO;
    else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        prio = ANDROID_LOG_DEBUG;
    else
        prio = ANDROID_LOG_UNKNOWN;

    LOG_PRI(prio, LOG_TAG, "[%s] Code %d : %s", layer_prefix, msg_code, msg);

    (void)obj_type;
    (void)obj;
    (void)location;
    (void)user_data;

    return false;
}

VkResult LayerChain::create_instance(const VkInstanceCreateInfo* create_info,
                                     const VkAllocationCallbacks* allocator,
                                     VkInstance* instance_out) {
    LayerChain chain(true,
                     (allocator) ? *allocator : driver::GetDefaultAllocator());

    VkResult result = chain.activate_layers(
        create_info->ppEnabledLayerNames, create_info->enabledLayerCount,
        create_info->ppEnabledExtensionNames,
        create_info->enabledExtensionCount);
    if (result != VK_SUCCESS)
        return result;

    // use a local create info when the chain is not empty
    VkInstanceCreateInfo local_create_info;
    if (!chain.empty()) {
        local_create_info = *create_info;
        chain.modify_create_info(local_create_info);
        create_info = &local_create_info;
    }

    return chain.create(create_info, allocator, instance_out);
}

VkResult LayerChain::create_device(VkPhysicalDevice physical_dev,
                                   const VkDeviceCreateInfo* create_info,
                                   const VkAllocationCallbacks* allocator,
                                   VkDevice* dev_out) {
    LayerChain chain(false, (allocator)
                                ? *allocator
                                : driver::GetData(physical_dev).allocator);

    VkResult result = chain.activate_layers(
        create_info->ppEnabledLayerNames, create_info->enabledLayerCount,
        create_info->ppEnabledExtensionNames,
        create_info->enabledExtensionCount);
    if (result != VK_SUCCESS)
        return result;

    // use a local create info when the chain is not empty
    VkDeviceCreateInfo local_create_info;
    if (!chain.empty()) {
        local_create_info = *create_info;
        chain.modify_create_info(local_create_info);
        create_info = &local_create_info;
    }

    return chain.create(physical_dev, create_info, allocator, dev_out);
}

void LayerChain::destroy_instance(VkInstance instance,
                                  const VkAllocationCallbacks* allocator) {
    InstanceData& data = GetData(instance);

    if (data.debug_callback != VK_NULL_HANDLE)
        data.destroy_debug_callback(instance, data.debug_callback, allocator);

    ActiveLayer* layers = reinterpret_cast<ActiveLayer*>(data.layers);
    uint32_t layer_count = data.layer_count;

    VkAllocationCallbacks local_allocator;
    if (!allocator)
        local_allocator = driver::GetData(instance).allocator;

    // this also destroys InstanceData
    data.dispatch.DestroyInstance(instance, allocator);

    destroy_layers(layers, layer_count,
                   (allocator) ? *allocator : local_allocator);
}

void LayerChain::destroy_device(VkDevice device,
                                const VkAllocationCallbacks* allocator) {
    DeviceData& data = GetData(device);

    ActiveLayer* layers = reinterpret_cast<ActiveLayer*>(data.layers);
    uint32_t layer_count = data.layer_count;

    VkAllocationCallbacks local_allocator;
    if (!allocator)
        local_allocator = driver::GetData(device).allocator;

    // this also destroys DeviceData
    data.dispatch.DestroyDevice(device, allocator);

    destroy_layers(layers, layer_count,
                   (allocator) ? *allocator : local_allocator);
}

// ----------------------------------------------------------------------------

bool EnsureInitialized() {
    static std::once_flag once_flag;
    static bool initialized;

    std::call_once(once_flag, []() {
        if (driver::OpenHAL()) {
            DiscoverLayers();
            initialized = true;
        }
    });

    return initialized;
}

}  // anonymous namespace

VkResult CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                        const VkAllocationCallbacks* pAllocator,
                        VkInstance* pInstance) {
    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    return LayerChain::create_instance(pCreateInfo, pAllocator, pInstance);
}

void DestroyInstance(VkInstance instance,
                     const VkAllocationCallbacks* pAllocator) {
    if (instance != VK_NULL_HANDLE)
        LayerChain::destroy_instance(instance, pAllocator);
}

VkResult CreateDevice(VkPhysicalDevice physicalDevice,
                      const VkDeviceCreateInfo* pCreateInfo,
                      const VkAllocationCallbacks* pAllocator,
                      VkDevice* pDevice) {
    return LayerChain::create_device(physicalDevice, pCreateInfo, pAllocator,
                                     pDevice);
}

void DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    if (device != VK_NULL_HANDLE)
        LayerChain::destroy_device(device, pAllocator);
}

VkResult EnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                          VkLayerProperties* pProperties) {
    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    uint32_t count =
        EnumerateInstanceLayers(pProperties ? *pPropertyCount : 0, pProperties);

    if (!pProperties || *pPropertyCount > count)
        *pPropertyCount = count;

    return *pPropertyCount < count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult EnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
    if (!EnsureInitialized())
        return VK_ERROR_INITIALIZATION_FAILED;

    if (pLayerName) {
        const VkExtensionProperties* props;
        uint32_t count;
        GetInstanceLayerExtensions(pLayerName, &props, &count);

        if (!pProperties || *pPropertyCount > count)
            *pPropertyCount = count;
        if (pProperties)
            std::copy(props, props + *pPropertyCount, pProperties);

        return *pPropertyCount < count ? VK_INCOMPLETE : VK_SUCCESS;
    }

    // TODO how about extensions from implicitly enabled layers?
    return vulkan::driver::EnumerateInstanceExtensionProperties(
        nullptr, pPropertyCount, pProperties);
}

VkResult EnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                        uint32_t* pPropertyCount,
                                        VkLayerProperties* pProperties) {
    (void)physicalDevice;

    uint32_t count =
        EnumerateDeviceLayers(pProperties ? *pPropertyCount : 0, pProperties);

    if (!pProperties || *pPropertyCount > count)
        *pPropertyCount = count;

    return *pPropertyCount < count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) {
    if (pLayerName) {
        const VkExtensionProperties* props;
        uint32_t count;
        GetDeviceLayerExtensions(pLayerName, &props, &count);

        if (!pProperties || *pPropertyCount > count)
            *pPropertyCount = count;
        if (pProperties)
            std::copy(props, props + *pPropertyCount, pProperties);

        return *pPropertyCount < count ? VK_INCOMPLETE : VK_SUCCESS;
    }

    // TODO how about extensions from implicitly enabled layers?
    const InstanceData& data = GetData(physicalDevice);
    return data.dispatch.EnumerateDeviceExtensionProperties(
        physicalDevice, nullptr, pPropertyCount, pProperties);
}

}  // namespace api
}  // namespace vulkan
