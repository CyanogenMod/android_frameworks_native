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

struct GpuInfo {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    std::vector<VkQueueFamilyProperties> queue_families;
    std::vector<VkExtensionProperties> extensions;
    std::vector<VkLayerProperties> layers;
    std::vector<std::vector<VkExtensionProperties>> layer_extensions;
};
struct VulkanInfo {
    std::vector<VkExtensionProperties> extensions;
    std::vector<VkLayerProperties> layers;
    std::vector<std::vector<VkExtensionProperties>> layer_extensions;
    std::vector<GpuInfo> gpus;
};

// ----------------------------------------------------------------------------

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

void EnumerateInstanceExtensions(
    const char* layer_name,
    std::vector<VkExtensionProperties>* extensions) {
    VkResult result;
    uint32_t count;
    result =
        vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceExtensionProperties (count)", result);
    do {
        extensions->resize(count);
        result = vkEnumerateInstanceExtensionProperties(layer_name, &count,
                                                        extensions->data());
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceExtensionProperties (data)", result);
}

void EnumerateDeviceExtensions(VkPhysicalDevice gpu,
                               const char* layer_name,
                               std::vector<VkExtensionProperties>* extensions) {
    VkResult result;
    uint32_t count;
    result =
        vkEnumerateDeviceExtensionProperties(gpu, layer_name, &count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateDeviceExtensionProperties (count)", result);
    do {
        extensions->resize(count);
        result = vkEnumerateDeviceExtensionProperties(gpu, layer_name, &count,
                                                      extensions->data());
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS)
        die("vkEnumerateDeviceExtensionProperties (data)", result);
}

void GatherInfo(VulkanInfo* info) {
    VkResult result;
    uint32_t count;

    result = vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceLayerProperties (count)", result);
    do {
        info->layers.resize(count);
        result =
            vkEnumerateInstanceLayerProperties(&count, info->layers.data());
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS)
        die("vkEnumerateInstanceLayerProperties (data)", result);
    info->layer_extensions.resize(info->layers.size());

    EnumerateInstanceExtensions(nullptr, &info->extensions);
    for (size_t i = 0; i < info->layers.size(); i++) {
        EnumerateInstanceExtensions(info->layers[i].layerName,
                                    &info->layer_extensions[i]);
    }

    VkInstance instance;
    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };
    result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS)
        die("vkCreateInstance", result);

    uint32_t num_gpus;
    result = vkEnumeratePhysicalDevices(instance, &num_gpus, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (count)", result);
    std::vector<VkPhysicalDevice> gpus(num_gpus, VK_NULL_HANDLE);
    do {
        gpus.resize(num_gpus, VK_NULL_HANDLE);
        result = vkEnumeratePhysicalDevices(instance, &num_gpus, gpus.data());
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS)
        die("vkEnumeratePhysicalDevices (data)", result);

    info->gpus.resize(num_gpus);
    for (size_t gpu_idx = 0; gpu_idx < gpus.size(); gpu_idx++) {
        VkPhysicalDevice gpu = gpus[gpu_idx];
        GpuInfo& gpu_info = info->gpus.at(gpu_idx);

        vkGetPhysicalDeviceProperties(gpu, &gpu_info.properties);
        vkGetPhysicalDeviceMemoryProperties(gpu, &gpu_info.memory);

        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
        gpu_info.queue_families.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            gpu, &count, gpu_info.queue_families.data());

        result = vkEnumerateDeviceLayerProperties(gpu, &count, nullptr);
        if (result != VK_SUCCESS)
            die("vkEnumerateDeviceLayerProperties (count)", result);
        do {
            gpu_info.layers.resize(count);
            result = vkEnumerateDeviceLayerProperties(gpu, &count,
                                                      gpu_info.layers.data());
        } while (result == VK_INCOMPLETE);
        if (result != VK_SUCCESS)
            die("vkEnumerateDeviceLayerProperties (data)", result);
        gpu_info.layer_extensions.resize(gpu_info.layers.size());

        EnumerateDeviceExtensions(gpu, nullptr, &gpu_info.extensions);
        for (size_t i = 0; i < gpu_info.layers.size(); i++) {
            EnumerateDeviceExtensions(gpu, gpu_info.layers[i].layerName,
                                      &gpu_info.layer_extensions[i]);
        }
    }

    vkDestroyInstance(instance, nullptr);
}

// ----------------------------------------------------------------------------

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

void PrintExtensions(const std::vector<VkExtensionProperties>& extensions,
                     const char* prefix) {
    for (const auto& e : extensions)
        printf("%s%s (v%u)\n", prefix, e.extensionName, e.specVersion);
}

void PrintGpuInfo(const GpuInfo& info) {
    VkResult result;
    std::ostringstream strbuf;

    printf("  \"%s\" (%s) %u.%u.%u/%#x [%04x:%04x]\n",
           info.properties.deviceName,
           VkPhysicalDeviceTypeStr(info.properties.deviceType),
           ExtractMajorVersion(info.properties.apiVersion),
           ExtractMinorVersion(info.properties.apiVersion),
           ExtractPatchVersion(info.properties.apiVersion),
           info.properties.driverVersion, info.properties.vendorID,
           info.properties.deviceID);

    for (uint32_t heap = 0; heap < info.memory.memoryHeapCount; heap++) {
        if ((info.memory.memoryHeaps[heap].flags &
             VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
            strbuf << "DEVICE_LOCAL";
        printf("    Heap %u: %" PRIu64 " MiB (0x%" PRIx64 " B) %s\n", heap,
               info.memory.memoryHeaps[heap].size / 0x1000000,
               info.memory.memoryHeaps[heap].size, strbuf.str().c_str());
        strbuf.str(std::string());

        for (uint32_t type = 0; type < info.memory.memoryTypeCount; type++) {
            if (info.memory.memoryTypes[type].heapIndex != heap)
                continue;
            VkMemoryPropertyFlags flags =
                info.memory.memoryTypes[type].propertyFlags;
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
            printf("      Type %u:%s\n", type, strbuf.str().c_str());
            strbuf.str(std::string());
        }
    }

    for (uint32_t family = 0; family < info.queue_families.size(); family++) {
        const VkQueueFamilyProperties& qprops = info.queue_families[family];
        VkQueueFlags flags = qprops.queueFlags;
        char flags_str[5];
        flags_str[0] = (flags & VK_QUEUE_GRAPHICS_BIT) ? 'G' : '_';
        flags_str[1] = (flags & VK_QUEUE_COMPUTE_BIT) ? 'C' : '_';
        flags_str[2] = (flags & VK_QUEUE_TRANSFER_BIT) ? 'T' : '_';
        flags_str[3] = (flags & VK_QUEUE_SPARSE_BINDING_BIT) ? 'S' : '_';
        flags_str[4] = '\0';
        printf(
            "    Queue Family %u: %ux %s\n"
            "      timestampValidBits: %ub\n"
            "      minImageTransferGranularity: (%u,%u,%u)\n",
            family, qprops.queueCount, flags_str, qprops.timestampValidBits,
            qprops.minImageTransferGranularity.width,
            qprops.minImageTransferGranularity.height,
            qprops.minImageTransferGranularity.depth);

        if (!info.extensions.empty()) {
            printf("    Extensions [%zu]:\n", info.extensions.size());
            PrintExtensions(info.extensions, "    ");
        }
        if (!info.layers.empty()) {
            printf("    Layers [%zu]:\n", info.layers.size());
            for (size_t i = 0; i < info.layers.size(); i++) {
                const auto& layer = info.layers[i];
                printf("    - %s %u.%u.%u/%u \"%s\"\n", layer.layerName,
                       ExtractMajorVersion(layer.specVersion),
                       ExtractMinorVersion(layer.specVersion),
                       ExtractPatchVersion(layer.specVersion),
                       layer.implementationVersion, layer.description);
                if (!info.layer_extensions[i].empty()) {
                    printf("       Extensions [%zu]:\n",
                           info.layer_extensions.size());
                    PrintExtensions(info.layer_extensions[i], "       ");
                }
            }
        }
    }
}

void PrintInfo(const VulkanInfo& info) {
    std::ostringstream strbuf;

    printf("Instance Extensions [%zu]:\n", info.extensions.size());
    PrintExtensions(info.extensions, "  ");
    if (!info.layers.empty()) {
        printf("Instance Layers [%zu]:\n", info.layers.size());
        for (size_t i = 0; i < info.layers.size(); i++) {
            const auto& layer = info.layers[i];
            printf("  %s %u.%u.%u/%u \"%s\"\n", layer.layerName,
                   ExtractMajorVersion(layer.specVersion),
                   ExtractMinorVersion(layer.specVersion),
                   ExtractPatchVersion(layer.specVersion),
                   layer.implementationVersion, layer.description);
            if (!info.layer_extensions[i].empty()) {
                PrintExtensions(info.layer_extensions[i], "    ");
            }
        }
    }

    printf("PhysicalDevices [%zu]:\n", info.gpus.size());
    for (const auto& gpu : info.gpus)
        PrintGpuInfo(gpu);
}

}  // namespace

// ----------------------------------------------------------------------------

int main(int /*argc*/, char const* /*argv*/ []) {
    VulkanInfo info;
    GatherInfo(&info);
    PrintInfo(info);
    return 0;
}
