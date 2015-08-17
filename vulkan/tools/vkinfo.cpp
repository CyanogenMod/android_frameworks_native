#include <stdlib.h>
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
        case VK_UNSUPPORTED: result_str = "VK_UNSUPPORTED"; break;
        case VK_NOT_READY: result_str = "VK_NOT_READY"; break;
        case VK_TIMEOUT: result_str = "VK_TIMEOUT"; break;
        case VK_EVENT_SET: result_str = "VK_EVENT_SET"; break;
        case VK_EVENT_RESET: result_str = "VK_EVENT_RESET"; break;
        case VK_INCOMPLETE: result_str = "VK_INCOMPLETE"; break;
        case VK_ERROR_UNKNOWN: result_str = "VK_ERROR_UNKNOWN"; break;
        case VK_ERROR_UNAVAILABLE: result_str = "VK_ERROR_UNAVAILABLE"; break;
        case VK_ERROR_INITIALIZATION_FAILED: result_str = "VK_ERROR_INITIALIZATION_FAILED"; break;
        case VK_ERROR_OUT_OF_HOST_MEMORY: result_str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: result_str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
        case VK_ERROR_DEVICE_ALREADY_CREATED: result_str = "VK_ERROR_DEVICE_ALREADY_CREATED"; break;
        case VK_ERROR_DEVICE_LOST: result_str = "VK_ERROR_DEVICE_LOST"; break;
        case VK_ERROR_INVALID_POINTER: result_str = "VK_ERROR_INVALID_POINTER"; break;
        case VK_ERROR_INVALID_VALUE: result_str = "VK_ERROR_INVALID_VALUE"; break;
        case VK_ERROR_INVALID_HANDLE: result_str = "VK_ERROR_INVALID_HANDLE"; break;
        case VK_ERROR_INVALID_ORDINAL: result_str = "VK_ERROR_INVALID_ORDINAL"; break;
        case VK_ERROR_INVALID_MEMORY_SIZE: result_str = "VK_ERROR_INVALID_MEMORY_SIZE"; break;
        case VK_ERROR_INVALID_EXTENSION: result_str = "VK_ERROR_INVALID_EXTENSION"; break;
        case VK_ERROR_INVALID_FLAGS: result_str = "VK_ERROR_INVALID_FLAGS"; break;
        case VK_ERROR_INVALID_ALIGNMENT: result_str = "VK_ERROR_INVALID_ALIGNMENT"; break;
        case VK_ERROR_INVALID_FORMAT: result_str = "VK_ERROR_INVALID_FORMAT"; break;
        case VK_ERROR_INVALID_IMAGE: result_str = "VK_ERROR_INVALID_IMAGE"; break;
        case VK_ERROR_INVALID_DESCRIPTOR_SET_DATA: result_str = "VK_ERROR_INVALID_DESCRIPTOR_SET_DATA"; break;
        case VK_ERROR_INVALID_QUEUE_TYPE: result_str = "VK_ERROR_INVALID_QUEUE_TYPE"; break;
        case VK_ERROR_UNSUPPORTED_SHADER_IL_VERSION: result_str = "VK_ERROR_UNSUPPORTED_SHADER_IL_VERSION"; break;
        case VK_ERROR_BAD_SHADER_CODE: result_str = "VK_ERROR_BAD_SHADER_CODE"; break;
        case VK_ERROR_BAD_PIPELINE_DATA: result_str = "VK_ERROR_BAD_PIPELINE_DATA"; break;
        case VK_ERROR_NOT_MAPPABLE: result_str = "VK_ERROR_NOT_MAPPABLE"; break;
        case VK_ERROR_MEMORY_MAP_FAILED: result_str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
        case VK_ERROR_MEMORY_UNMAP_FAILED: result_str = "VK_ERROR_MEMORY_UNMAP_FAILED"; break;
        case VK_ERROR_INCOMPATIBLE_DEVICE: result_str = "VK_ERROR_INCOMPATIBLE_DEVICE"; break;
        case VK_ERROR_INCOMPATIBLE_DRIVER: result_str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
        case VK_ERROR_INCOMPLETE_COMMAND_BUFFER: result_str = "VK_ERROR_INCOMPLETE_COMMAND_BUFFER"; break;
        case VK_ERROR_BUILDING_COMMAND_BUFFER: result_str = "VK_ERROR_BUILDING_COMMAND_BUFFER"; break;
        case VK_ERROR_MEMORY_NOT_BOUND: result_str = "VK_ERROR_MEMORY_NOT_BOUND"; break;
        case VK_ERROR_INCOMPATIBLE_QUEUE: result_str = "VK_ERROR_INCOMPATIBLE_QUEUE"; break;
        case VK_ERROR_INVALID_LAYER: result_str = "VK_ERROR_INVALID_LAYER"; break;
        default: result_str = "<unknown VkResult>"; break;
            // clang-format on
    }
    fprintf(stderr, "%s failed: %s (%d)\n", proc, result_str, result);
    exit(1);
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

void DumpPhysicalDevice(uint32_t idx, VkPhysicalDevice pdev) {
    VkResult result;

    VkPhysicalDeviceProperties props;
    result = vkGetPhysicalDeviceProperties(pdev, &props);
    if (result != VK_SUCCESS)
        die("vkGetPhysicalDeviceProperties", result);
    printf("  %u: \"%s\" (%s) %u.%u.%u/%#x [%04x:%04x]\n", idx,
           props.deviceName, VkPhysicalDeviceTypeStr(props.deviceType),
           (props.apiVersion >> 22) & 0x3FF, (props.apiVersion >> 12) & 0x3FF,
           (props.apiVersion >> 0) & 0xFFF, props.driverVersion, props.vendorId,
           props.deviceId);
}

}  // namespace

int main(int /*argc*/, char const* /*argv*/ []) {
    VkResult result;

    VkInstance instance;
    const VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pAppInfo = nullptr,
        .pAllocCb = nullptr,
        .layerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .extensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };
    result = vkCreateInstance(&create_info, &instance);
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
    printf("PhysicalDevices:\n");
    for (uint32_t i = 0; i < physical_devices.size(); i++)
        DumpPhysicalDevice(i, physical_devices[i]);

    result = vkDestroyInstance(instance);
    if (result != VK_SUCCESS)
        die("vkDestroyInstance", result);

    return 0;
}
