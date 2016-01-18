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

#include <algorithm>
#include <array>
#include <inttypes.h>
#include <stdlib.h>
#include <sstream>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vk_ext_debug_report.h>

#define LOG_TAG "vkinfo"
#include <log/log.h>

namespace {

struct GpuInfo {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceFeatures features;
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

bool HasExtension(const std::vector<VkExtensionProperties>& extensions,
                  const char* name) {
    return std::find_if(extensions.cbegin(), extensions.cend(),
                        [=](const VkExtensionProperties& prop) {
                            return strcmp(prop.extensionName, name) == 0;
                        }) != extensions.end();
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

void GatherGpuInfo(VkPhysicalDevice gpu, GpuInfo& info) {
    VkResult result;
    uint32_t count;

    vkGetPhysicalDeviceProperties(gpu, &info.properties);
    vkGetPhysicalDeviceMemoryProperties(gpu, &info.memory);
    vkGetPhysicalDeviceFeatures(gpu, &info.features);

    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    info.queue_families.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count,
                                             info.queue_families.data());

    result = vkEnumerateDeviceLayerProperties(gpu, &count, nullptr);
    if (result != VK_SUCCESS)
        die("vkEnumerateDeviceLayerProperties (count)", result);
    do {
        info.layers.resize(count);
        result =
            vkEnumerateDeviceLayerProperties(gpu, &count, info.layers.data());
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS)
        die("vkEnumerateDeviceLayerProperties (data)", result);
    info.layer_extensions.resize(info.layers.size());

    EnumerateDeviceExtensions(gpu, nullptr, &info.extensions);
    for (size_t i = 0; i < info.layers.size(); i++) {
        EnumerateDeviceExtensions(gpu, info.layers[i].layerName,
                                  &info.layer_extensions[i]);
    }

    const std::array<const char*, 1> kDesiredExtensions = {
        {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
    };
    const char* extensions[kDesiredExtensions.size()];
    uint32_t num_extensions = 0;
    for (const auto& desired_ext : kDesiredExtensions) {
        bool available = HasExtension(info.extensions, desired_ext);
        for (size_t i = 0; !available && i < info.layer_extensions.size(); i++)
            available = HasExtension(info.layer_extensions[i], desired_ext);
        if (available)
            extensions[num_extensions++] = desired_ext;
    }

    VkDevice device;
    const VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
    };
    const VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = num_extensions,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &info.features,
    };
    result = vkCreateDevice(gpu, &create_info, nullptr, &device);
    if (result != VK_SUCCESS)
        die("vkCreateDevice", result);
    vkDestroyDevice(device, nullptr);
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

    const char* kDesiredExtensions[] = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };
    const char*
        extensions[sizeof(kDesiredExtensions) / sizeof(kDesiredExtensions[0])];
    uint32_t num_extensions = 0;
    for (const auto& desired_ext : kDesiredExtensions) {
        bool available = HasExtension(info->extensions, desired_ext);
        for (size_t i = 0; !available && i < info->layer_extensions.size(); i++)
            available = HasExtension(info->layer_extensions[i], desired_ext);
        if (available)
            extensions[num_extensions++] = desired_ext;
    }

    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .enabledExtensionCount = num_extensions,
        .ppEnabledExtensionNames = extensions,
    };
    VkInstance instance;
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
    for (size_t i = 0; i < gpus.size(); i++)
        GatherGpuInfo(gpus[i], info->gpus.at(i));

    vkDestroyInstance(instance, nullptr);
}

// ----------------------------------------------------------------------------

struct Options {
    bool layer_description;
    bool layer_extensions;
};

const size_t kMaxIndent = 8;
const size_t kIndentSize = 3;
std::array<char, kMaxIndent * kIndentSize + 1> kIndent;
const char* Indent(size_t n) {
    static bool initialized = false;
    if (!initialized) {
        kIndent.fill(' ');
        kIndent.back() = '\0';
        initialized = true;
    }
    return kIndent.data() +
           (kIndent.size() - (kIndentSize * std::min(n, kMaxIndent) + 1));
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

void PrintExtensions(const std::vector<VkExtensionProperties>& extensions,
                     const Options& /*options*/,
                     size_t indent) {
    for (const auto& e : extensions)
        printf("%s%s (v%u)\n", Indent(indent), e.extensionName, e.specVersion);
}

void PrintLayers(
    const std::vector<VkLayerProperties>& layers,
    const std::vector<std::vector<VkExtensionProperties>> extensions,
    const Options& options,
    size_t indent) {
    for (size_t i = 0; i < layers.size(); i++) {
        printf("%s%s %u.%u.%u/%u\n", Indent(indent), layers[i].layerName,
               ExtractMajorVersion(layers[i].specVersion),
               ExtractMinorVersion(layers[i].specVersion),
               ExtractPatchVersion(layers[i].specVersion),
               layers[i].implementationVersion);
        if (options.layer_description)
            printf("%s%s\n", Indent(indent + 1), layers[i].description);
        if (options.layer_extensions && !extensions[i].empty()) {
            if (!extensions[i].empty()) {
                printf("%sExtensions [%zu]:\n", Indent(indent + 1),
                       extensions[i].size());
                PrintExtensions(extensions[i], options, indent + 2);
            }
        }
    }
}

void PrintGpuInfo(const GpuInfo& info, const Options& options, size_t indent) {
    VkResult result;
    std::ostringstream strbuf;

    printf("%s\"%s\" (%s) %u.%u.%u/%#x [%04x:%04x]\n", Indent(indent),
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
        printf("%sHeap %u: %" PRIu64 " MiB (0x%" PRIx64 " B) %s\n",
               Indent(indent + 1), heap,
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
            printf("%sType %u:%s\n", Indent(indent + 2), type,
                   strbuf.str().c_str());
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
            "%sQueue Family %u: %ux %s\n"
            "%stimestampValidBits: %ub\n"
            "%sminImageTransferGranularity: (%u,%u,%u)\n",
            Indent(indent + 1), family, qprops.queueCount, flags_str,
            Indent(indent + 2), qprops.timestampValidBits, Indent(indent + 2),
            qprops.minImageTransferGranularity.width,
            qprops.minImageTransferGranularity.height,
            qprops.minImageTransferGranularity.depth);
    }

    printf("%sExtensions [%zu]:\n", Indent(indent + 1), info.extensions.size());
    if (!info.extensions.empty())
        PrintExtensions(info.extensions, options, indent + 2);
    printf("%sLayers [%zu]:\n", Indent(indent + 1), info.layers.size());
    if (!info.layers.empty())
        PrintLayers(info.layers, info.layer_extensions, options, indent + 2);
}

void PrintInfo(const VulkanInfo& info, const Options& options) {
    std::ostringstream strbuf;
    size_t indent = 0;

    printf("%sInstance Extensions [%zu]:\n", Indent(indent),
           info.extensions.size());
    PrintExtensions(info.extensions, options, indent + 1);
    printf("%sInstance Layers [%zu]:\n", Indent(indent), info.layers.size());
    if (!info.layers.empty())
        PrintLayers(info.layers, info.layer_extensions, options, indent + 1);

    printf("%sPhysicalDevices [%zu]:\n", Indent(indent), info.gpus.size());
    for (const auto& gpu : info.gpus)
        PrintGpuInfo(gpu, options, indent + 1);
}

}  // namespace

// ----------------------------------------------------------------------------

int main(int argc, char const* argv[]) {
    Options options = {
        .layer_description = false, .layer_extensions = false,
    };
    for (int argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "-v") == 0) {
            options.layer_description = true;
            options.layer_extensions = true;
        } else if (strcmp(argv[argi], "-layer_description") == 0) {
            options.layer_description = true;
        } else if (strcmp(argv[argi], "-layer_extensions") == 0) {
            options.layer_extensions = true;
        }
    }

    VulkanInfo info;
    GatherInfo(&info);
    PrintInfo(info, options);
    return 0;
}
