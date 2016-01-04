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

#include <inttypes.h>
#include <stdlib.h>
#include <sstream>
#include <vector>

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#define LOG_TAG "vkinfo"
#include <log/log.h>

namespace {

[[noreturn]] void die(const char* proc, VkResult result) {
    const char* result_str;
    switch (result) {
        // clang-format off
        case VK_SUCCESS: result_str = "VK_SUCCESS"; break;
        case VK_NOT_READY: result_str = "VK_NOT_READY"; break;
        case VK_TIMEOUT: result_str = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: result_str = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: result_str = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: result_str = "VK_INCOMPLETE"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: result_str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: result_str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_INITIALIZATION_FAILED: result_str = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_DEVICE_LOST: result_str = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: result_str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_LAYER_NOT_PRESENT: result_str = "VK_ERROR_LAYER_NOT_PRESENT"; break;
        case VK_ERROR_EXTENSION_NOT_PRESENT: result_str = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: result_str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        default: result_str = "<unknown VkResult>"; break;
            // clang-format on
    }
    fprintf(stderr, "%s failed: %s (%d)\n", proc, result_str, result);
    exit(1);
}

uint32_t ExtractMajorVersion(uint32_t version) {
    return (version >> 22) & 0x3FF;
}
uint32_t ExtractMinorVersion(uint32_t version) {
    return (version >> 12) & 0x3FF;
}
uint32_t ExtractPatchVersion(uint32_t version) {
    return (version >> 0) & 0xFFF;
}

const char* VkPhysicalDeviceTypeStr(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "OTHER";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "<UNKNOWN>";
    }
}

const char* VkQueueFlagBitStr(VkQueueFlagBits bit) {
    switch (bit) {
        case VK_QUEUE_GRAPHICS_BIT:
            return "GRAPHICS";
        case VK_QUEUE_COMPUTE_BIT:
            return "COMPUTE";
        case VK_QUEUE_TRANSFER_BIT:
            return "TRANSFER";
        case VK_QUEUE_SPARSE_BINDING_BIT:
            return "SPARSE";
    }
}

struct VulkanInfo {
    std::vector<VkExtensionProperties> extensions;
    std::vector<VkLayerProperties> layers;
    std::vector<std::vector<VkExtensionProperties>> layer_extensions;
};

void DumpPhysicalDevice(uint32_t idx, VkPhysicalDevice pdev) {
    VkResult result;
    std::ostringstream strbuf;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pdev, &props);
    printf("  %u: \"%s\" (%s) %u.%u.%u/%#x [%04x:%04x]\n", idx,
           props.deviceName, VkPhysicalDeviceTypeStr(props.deviceType),
           ExtractMajorVersion(props.apiVersion),
           ExtractMinorVersion(props.apiVersion),
           ExtractPatchVersion(props.apiVersion), props.driverVersion,
           props.vendorID, props.deviceID);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pdev, &mem_props);
    for (uint32_t heap = 0; heap < mem_props.memoryHeapCount; heap++) {
        if ((mem_props.memoryHeaps[heap].flags &
             VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
            strbuf << "DEVICE_LOCAL";
        printf("     Heap %u: 0x%" PRIx64 " %s\n", heap,
               mem_props.memoryHeaps[heap].size, strbuf.str().c_str());
        strbuf.str(std::string());

        for (uint32_t type = 0; type < mem_props.memoryTypeCount; type++) {
            if (mem_props.memoryTypes[type].heapIndex != heap)
                continue;
            VkMemoryPropertyFlags flags =
                mem_props.memoryTypes[type].propertyFlags;
            if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
                strbuf << " DEVICE_LOCAL";
            if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
                strbuf << " HOST_VISIBLE";
            if ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
                strbuf << " COHERENT";
            if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0)
                strbuf << " CACHED";
            if ((flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0)
                strbuf << " LAZILY_ALLOCATED";
            printf("       Type %u:%s\n", type, strbuf.str().c_str());
            strbuf.str(std::string());
        }
    }

    uint32_t num_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &num_queue_families,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(
        num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &num_queue_families,
                                             queue_family_properties.data());
    for (uint32_t family = 0; family < num_queue_families; family++) {
        const VkQueueFamilyProperties& qprops = queue_family_properties[family];
        const char* sep = "";
        int bit, queue_flags = static_cast<int>(qprops.queueFlags);
        while ((bit = __builtin_ffs(queue_flags)) != 0) {
            VkQueueFlagBits flag = VkQueueFlagBits(1 << (bit - 1));
            strbuf << sep << VkQueueFlagBitStr(flag);
            queue_flags &= ~flag;
            sep = "+";
        }
        printf("     Queue Family %u: %2ux %s timestamps:%ub\n", family,
               qprops.queueCount, strbuf.str().c_str(),
               qprops.timestampValidBits);
        strbuf.str(std::string());
    }
}

void EnumerateInstanceExtensions(
    const char* layer_name,
    std::vector<VkExtensionProperties>* extensions) {
    VkResult result;
    uint32_t count;
    result =
        vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceExtensionProperties (count)", result);
    extensions->resize(count);
    result = vkEnumerateInstanceExtensionProperties(layer_name, &count,
                                                    extensions->data());
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceExtensionProperties (data)", result);
}

void GatherInfo(VulkanInfo* info) {
    VkResult result;
    uint32_t count;

    result = vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceLayerProperties (count)", result);
    info->layers.resize(count);
    result = vkEnumerateInstanceLayerProperties(&count, info->layers.data());
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceLayerProperties (data)", result);
    info->layer_extensions.resize(info->layers.size());

    EnumerateInstanceExtensions(nullptr, &info->extensions);
    for (size_t i = 0; i < info->extensions.size(); i++)
        EnumerateInstanceExtensions(info->layers[i].layerName,
                                    &info->layer_extensions[i]);
}

void PrintExtensions(const std::vector<VkExtensionProperties>& extensions,
                     const char* prefix) {
    for (const auto& e : extensions)
        printf("%s%s (v%u)\n", prefix, e.extensionName, e.specVersion);
}

void PrintInfo(const VulkanInfo& info) {
    printf("Instance Extensions [%u]:\n", info.extensions.size());
    PrintExtensions(info.extensions, "  ");
    if (!info.layers.empty()) {
        printf("Instance Layers [%u]:\n", info.layers.size());
        for (size_t i = 0; i < info.layers.size(); i++) {
            const auto& layer = info.layers[i];
            printf(
                "  %s %u.%u.%u/%u \"%s\"\n",
                layer.layerName, ExtractMajorVersion(layer.specVersion),
                ExtractMinorVersion(layer.specVersion),
                ExtractPatchVersion(layer.specVersion),
                layer.implementationVersion, layer.description);
            if (!info.layer_extensions[i].empty()) {
                printf("     Extensions [%zu]:\n",
                       info.layer_extensions.size());
                PrintExtensions(info.layer_extensions[i], "       ");
            }
        }
    }
}

}  // namespace

int main(int /*argc*/, char const* /*argv*/ []) {
    VulkanInfo info;
    GatherInfo(&info);
    PrintInfo(info);

    VkResult result;

    VkInstance instance;
    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = nullptr,
        .enabledLayerNameCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionNameCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };
    result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS)
        die("vkCreateInstance", result);

    uint32_t num_physical_devices;
    result =
        vkEnumeratePhysicalDevices(instance, &num_physical_devices, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (count)", result);
    std::vector<VkPhysicalDevice> physical_devices(num_physical_devices,
                                                   VK_NULL_HANDLE);
    result = vkEnumeratePhysicalDevices(instance, &num_physical_devices,
                                        physical_devices.data());
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (data)", result);
    if (num_physical_devices != physical_devices.size()) {
        fprintf(stderr,
                "number of physical devices decreased from %zu to %u!\n",
                physical_devices.size(), num_physical_devices);
        physical_devices.resize(num_physical_devices);
    }
    printf("PhysicalDevices [%zu]:\n", physical_devices.size());
    for (uint32_t i = 0; i < physical_devices.size(); i++)
        DumpPhysicalDevice(i, physical_devices[i]);

    vkDestroyInstance(instance, nullptr);

    return 0;
}
