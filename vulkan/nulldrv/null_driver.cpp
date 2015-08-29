#include <hardware/hwvulkan.h>

#include <string.h>
#include <algorithm>

// #define LOG_NDEBUG 0
#include <log/log.h>
#include <utils/Errors.h>

#include "null_driver.h"

using namespace null_driver;

struct VkPhysicalDevice_T {
    hwvulkan_dispatch_t dispatch;
};

struct VkInstance_T {
    hwvulkan_dispatch_t dispatch;
    const VkAllocCallbacks* alloc;
    VkPhysicalDevice_T physical_device;
};

struct VkQueue_T {
    hwvulkan_dispatch_t dispatch;
};

struct VkCmdBuffer_T {
    hwvulkan_dispatch_t dispatch;
};

struct VkDevice_T {
    hwvulkan_dispatch_t dispatch;
    VkInstance_T* instance;
    VkQueue_T queue;
};

// -----------------------------------------------------------------------------
// Declare HAL_MODULE_INFO_SYM early so it can be referenced by nulldrv_device
// later.

namespace {
int OpenDevice(const hw_module_t* module, const char* id, hw_device_t** device);
hw_module_methods_t nulldrv_module_methods = {.open = OpenDevice};
}  // namespace

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
__attribute__((visibility("default"))) hwvulkan_module_t HAL_MODULE_INFO_SYM = {
    .common =
        {
         .tag = HARDWARE_MODULE_TAG,
         .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
         .hal_api_version = HARDWARE_HAL_API_VERSION,
         .id = HWVULKAN_HARDWARE_MODULE_ID,
         .name = "Null Vulkan Driver",
         .author = "The Android Open Source Project",
         .methods = &nulldrv_module_methods,
        },
};
#pragma clang diagnostic pop

// -----------------------------------------------------------------------------

namespace {

VkResult CreateInstance(const VkInstanceCreateInfo* create_info,
                        VkInstance* out_instance) {
    VkInstance_T* instance =
        static_cast<VkInstance_T*>(create_info->pAllocCb->pfnAlloc(
            create_info->pAllocCb->pUserData, sizeof(VkInstance_T),
            alignof(VkInstance_T), VK_SYSTEM_ALLOC_TYPE_API_OBJECT));
    if (!instance)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    instance->dispatch.magic = HWVULKAN_DISPATCH_MAGIC;
    instance->alloc = create_info->pAllocCb;
    instance->physical_device.dispatch.magic = HWVULKAN_DISPATCH_MAGIC;

    *out_instance = instance;
    return VK_SUCCESS;
}

int CloseDevice(struct hw_device_t* /*device*/) {
    // nothing to do - opening a device doesn't allocate any resources
    return 0;
}

hwvulkan_device_t nulldrv_device = {
    .common =
        {
         .tag = HARDWARE_DEVICE_TAG,
         .version = HWVULKAN_DEVICE_API_VERSION_0_1,
         .module = &HAL_MODULE_INFO_SYM.common,
         .close = CloseDevice,
        },
    .GetGlobalExtensionProperties = GetGlobalExtensionProperties,
    .CreateInstance = CreateInstance,
    .GetInstanceProcAddr = GetInstanceProcAddr};

int OpenDevice(const hw_module_t* /*module*/,
               const char* id,
               hw_device_t** device) {
    if (strcmp(id, HWVULKAN_DEVICE_0) == 0) {
        *device = &nulldrv_device.common;
        return 0;
    }
    return -ENOENT;
}

VkInstance_T* GetInstanceFromPhysicalDevice(
    VkPhysicalDevice_T* physical_device) {
    return reinterpret_cast<VkInstance_T*>(
        reinterpret_cast<uintptr_t>(physical_device) -
        offsetof(VkInstance_T, physical_device));
}

}  // namespace

namespace null_driver {

template <typename HandleT>
struct HandleTraits {};

template <typename HandleT>
typename HandleTraits<HandleT>::PointerType GetObjectFromHandle(
    const HandleT& h) {
    return reinterpret_cast<typename HandleTraits<HandleT>::PointerType>(
        uintptr_t(h.handle));
}

template <typename T>
typename T::HandleType GetHandleToObject(const T* obj) {
    return typename T::HandleType(reinterpret_cast<uintptr_t>(obj));
}

// -----------------------------------------------------------------------------
// Global

VkResult GetGlobalExtensionProperties(const char*,
                                      uint32_t* count,
                                      VkExtensionProperties*) {
    *count = 0;
    return VK_SUCCESS;
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance, const char* name) {
    PFN_vkVoidFunction proc = LookupInstanceProcAddr(name);
    if (!proc && strcmp(name, "vkGetDeviceProcAddr") == 0)
        proc = reinterpret_cast<PFN_vkVoidFunction>(GetDeviceProcAddr);
    return proc;
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice, const char* name) {
    return LookupDeviceProcAddr(name);
}

// -----------------------------------------------------------------------------
// Instance

VkResult DestroyInstance(VkInstance instance) {
    instance->alloc->pfnFree(instance->alloc->pUserData, instance);
    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// PhysicalDevice

VkResult EnumeratePhysicalDevices(VkInstance instance,
                                  uint32_t* physical_device_count,
                                  VkPhysicalDevice* physical_devices) {
    if (physical_devices && *physical_device_count >= 1)
        physical_devices[0] = &instance->physical_device;
    *physical_device_count = 1;
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceProperties(VkPhysicalDevice,
                                     VkPhysicalDeviceProperties* properties) {
    properties->apiVersion = VK_API_VERSION;
    properties->driverVersion = VK_MAKE_VERSION(0, 0, 1);
    properties->vendorId = 0xC0DE;
    properties->deviceId = 0xCAFE;
    properties->deviceType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    strcpy(properties->deviceName, "Android Vulkan Null Driver");
    memset(properties->pipelineCacheUUID, 0,
           sizeof(properties->pipelineCacheUUID));
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice,
    VkPhysicalDeviceMemoryProperties* properties) {
    properties->memoryTypeCount = 1;
    properties->memoryTypes[0].propertyFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties->memoryTypes[0].heapIndex = 0;
    properties->memoryHeapCount = 1;
    properties->memoryHeaps[0].size =
        INTPTR_MAX;  // TODO: do something smarter?
    properties->memoryHeaps[0].flags = VK_MEMORY_HEAP_HOST_LOCAL;
    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// Device

VkResult CreateDevice(VkPhysicalDevice physical_device,
                      const VkDeviceCreateInfo*,
                      VkDevice* out_device) {
    VkInstance_T* instance = GetInstanceFromPhysicalDevice(physical_device);
    VkDevice_T* device = static_cast<VkDevice_T*>(instance->alloc->pfnAlloc(
        instance->alloc->pUserData, sizeof(VkDevice_T), alignof(VkDevice_T),
        VK_SYSTEM_ALLOC_TYPE_API_OBJECT));
    if (!device)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    device->dispatch.magic = HWVULKAN_DISPATCH_MAGIC;
    device->instance = instance;
    device->queue.dispatch.magic = HWVULKAN_DISPATCH_MAGIC;

    *out_device = device;
    return VK_SUCCESS;
}

VkResult DestroyDevice(VkDevice device) {
    if (!device)
        return VK_SUCCESS;
    const VkAllocCallbacks* alloc = device->instance->alloc;
    alloc->pfnFree(alloc->pUserData, device);
    return VK_SUCCESS;
}

VkResult GetDeviceQueue(VkDevice device, uint32_t, uint32_t, VkQueue* queue) {
    *queue = &device->queue;
    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// DeviceMemory

struct DeviceMemory {
    typedef VkDeviceMemory HandleType;
    VkDeviceSize size;
    alignas(16) uint8_t data[0];
};
template <>
struct HandleTraits<VkDeviceMemory> {
    typedef DeviceMemory* PointerType;
};

VkResult AllocMemory(VkDevice device,
                     const VkMemoryAllocInfo* alloc_info,
                     VkDeviceMemory* mem_handle) {
    if (SIZE_MAX - sizeof(DeviceMemory) <= alloc_info->allocationSize)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    const VkAllocCallbacks* alloc = device->instance->alloc;
    size_t size = sizeof(DeviceMemory) + size_t(alloc_info->allocationSize);
    DeviceMemory* mem = static_cast<DeviceMemory*>(
        alloc->pfnAlloc(alloc->pUserData, size, alignof(DeviceMemory),
                        VK_SYSTEM_ALLOC_TYPE_API_OBJECT));
    if (!mem)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    mem->size = size;
    *mem_handle = GetHandleToObject(mem);
    return VK_SUCCESS;
}

VkResult FreeMemory(VkDevice device, VkDeviceMemory mem_handle) {
    const VkAllocCallbacks* alloc = device->instance->alloc;
    DeviceMemory* mem = GetObjectFromHandle(mem_handle);
    alloc->pfnFree(alloc->pUserData, mem);
    return VK_SUCCESS;
}

VkResult MapMemory(VkDevice,
                   VkDeviceMemory mem_handle,
                   VkDeviceSize offset,
                   VkDeviceSize,
                   VkMemoryMapFlags,
                   void** out_ptr) {
    DeviceMemory* mem = GetObjectFromHandle(mem_handle);
    *out_ptr = &mem->data[0] + offset;
    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// Buffer

struct Buffer {
    typedef VkBuffer HandleType;
    VkDeviceSize size;
};
template <>
struct HandleTraits<VkBuffer> {
    typedef Buffer* PointerType;
};

VkResult CreateBuffer(VkDevice device,
                      const VkBufferCreateInfo* create_info,
                      VkBuffer* buffer_handle) {
    const VkAllocCallbacks* alloc = device->instance->alloc;
    Buffer* buffer = static_cast<Buffer*>(
        alloc->pfnAlloc(alloc->pUserData, sizeof(Buffer), alignof(Buffer),
                        VK_SYSTEM_ALLOC_TYPE_API_OBJECT));
    if (!buffer)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    buffer->size = create_info->size;
    *buffer_handle = GetHandleToObject(buffer);
    return VK_SUCCESS;
}

VkResult GetBufferMemoryRequirements(VkDevice,
                                     VkBuffer buffer_handle,
                                     VkMemoryRequirements* requirements) {
    Buffer* buffer = GetObjectFromHandle(buffer_handle);
    requirements->size = buffer->size;
    requirements->alignment = 16;  // allow fast Neon/SSE memcpy
    requirements->memoryTypeBits = 0x1;
    return VK_SUCCESS;
}

VkResult DestroyBuffer(VkDevice device, VkBuffer buffer_handle) {
    const VkAllocCallbacks* alloc = device->instance->alloc;
    Buffer* buffer = GetObjectFromHandle(buffer_handle);
    alloc->pfnFree(alloc->pUserData, buffer);
    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// No-op entrypoints

// clang-format off
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

VkResult GetPhysicalDeviceQueueCount(VkPhysicalDevice physicalDevice, uint32_t* pCount) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceQueueProperties(VkPhysicalDevice physicalDevice, uint32_t count, VkPhysicalDeviceQueueProperties* pQueueProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageFormatProperties* pImageFormatProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceLimits(VkPhysicalDevice physicalDevice, VkPhysicalDeviceLimits* pLimits) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetGlobalLayerProperties(uint32_t* pCount, VkLayerProperties* pProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pCount, VkLayerProperties* pProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pCount, VkExtensionProperties* pProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueSubmit(VkQueue queue, uint32_t cmdBufferCount, const VkCmdBuffer* pCmdBuffers, VkFence fence) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueWaitIdle(VkQueue queue) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DeviceWaitIdle(VkDevice device) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult UnmapMemory(VkDevice device, VkDeviceMemory mem) {
    return VK_SUCCESS;
}

VkResult FlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult InvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount, const VkMappedMemoryRange* pMemRanges) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset) {
    return VK_SUCCESS;
}

VkResult GetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult BindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pNumRequirements, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, uint32_t samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pNumProperties, VkSparseImageFormatProperties* pProperties) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueBindSparseBufferMemory(VkQueue queue, VkBuffer buffer, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueBindSparseImageOpaqueMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseMemoryBindInfo* pBindInfo) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueBindSparseImageMemory(VkQueue queue, VkImage image, uint32_t numBindings, const VkSparseImageMemoryBindInfo* pBindInfo) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, VkFence* pFence) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyFence(VkDevice device, VkFence fence) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult ResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetFenceStatus(VkDevice device, VkFence fence) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult WaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, VkSemaphore* pSemaphore) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroySemaphore(VkDevice device, VkSemaphore semaphore) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult QueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, VkEvent* pEvent) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyEvent(VkDevice device, VkEvent event) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetEventStatus(VkDevice device, VkEvent event) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult SetEvent(VkDevice device, VkEvent event) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult ResetEvent(VkDevice device, VkEvent event) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, VkQueryPool* pQueryPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyQueryPool(VkDevice device, VkQueryPool queryPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, size_t* pDataSize, void* pData, VkQueryResultFlags flags) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, VkBufferView* pView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyBufferView(VkDevice device, VkBufferView bufferView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, VkImage* pImage) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyImage(VkDevice device, VkImage image) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView* pView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyImageView(VkDevice device, VkImageView imageView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateAttachmentView(VkDevice device, const VkAttachmentViewCreateInfo* pCreateInfo, VkAttachmentView* pView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyAttachmentView(VkDevice device, VkAttachmentView attachmentView) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModule* pShaderModule) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyShaderModule(VkDevice device, VkShaderModule shaderModule) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateShader(VkDevice device, const VkShaderCreateInfo* pCreateInfo, VkShader* pShader) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyShader(VkDevice device, VkShader shader) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

size_t GetPipelineCacheSize(VkDevice device, VkPipelineCache pipelineCache) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, void* pData) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult MergePipelineCaches(VkDevice device, VkPipelineCache destCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t count, const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyPipeline(VkDevice device, VkPipeline pipeline) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, VkSampler* pSampler) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroySampler(VkDevice device, VkSampler sampler) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayout* pSetLayout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDescriptorPool(VkDevice device, VkDescriptorPoolUsage poolUsage, uint32_t maxSets, const VkDescriptorPoolCreateInfo* pCreateInfo, VkDescriptorPool* pDescriptorPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult ResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult AllocDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetUsage setUsage, uint32_t count, const VkDescriptorSetLayout* pSetLayouts, VkDescriptorSet* pDescriptorSets, uint32_t* pCount) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult UpdateDescriptorSets(VkDevice device, uint32_t writeCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t copyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult FreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t count, const VkDescriptorSet* pDescriptorSets) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDynamicViewportState(VkDevice device, const VkDynamicViewportStateCreateInfo* pCreateInfo, VkDynamicViewportState* pState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDynamicViewportState(VkDevice device, VkDynamicViewportState dynamicViewportState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDynamicRasterState(VkDevice device, const VkDynamicRasterStateCreateInfo* pCreateInfo, VkDynamicRasterState* pState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDynamicRasterState(VkDevice device, VkDynamicRasterState dynamicRasterState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDynamicColorBlendState(VkDevice device, const VkDynamicColorBlendStateCreateInfo* pCreateInfo, VkDynamicColorBlendState* pState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDynamicColorBlendState(VkDevice device, VkDynamicColorBlendState dynamicColorBlendState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateDynamicDepthStencilState(VkDevice device, const VkDynamicDepthStencilStateCreateInfo* pCreateInfo, VkDynamicDepthStencilState* pState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyDynamicDepthStencilState(VkDevice device, VkDynamicDepthStencilState dynamicDepthStencilState) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer* pFramebuffer) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyRenderPass(VkDevice device, VkRenderPass renderPass) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult GetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateCommandPool(VkDevice device, const VkCmdPoolCreateInfo* pCreateInfo, VkCmdPool* pCmdPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyCommandPool(VkDevice device, VkCmdPool cmdPool) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult ResetCommandPool(VkDevice device, VkCmdPool cmdPool, VkCmdPoolResetFlags flags) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult CreateCommandBuffer(VkDevice device, const VkCmdBufferCreateInfo* pCreateInfo, VkCmdBuffer* pCmdBuffer) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult DestroyCommandBuffer(VkDevice device, VkCmdBuffer commandBuffer) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult BeginCommandBuffer(VkCmdBuffer cmdBuffer, const VkCmdBufferBeginInfo* pBeginInfo) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult EndCommandBuffer(VkCmdBuffer cmdBuffer) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

VkResult ResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags) {
    ALOGV("TODO: vk%s", __FUNCTION__);
    return VK_SUCCESS;
}

void CmdBindPipeline(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
}

void CmdBindDynamicViewportState(VkCmdBuffer cmdBuffer, VkDynamicViewportState dynamicViewportState) {
}

void CmdBindDynamicRasterState(VkCmdBuffer cmdBuffer, VkDynamicRasterState dynamicRasterState) {
}

void CmdBindDynamicColorBlendState(VkCmdBuffer cmdBuffer, VkDynamicColorBlendState dynamicColorBlendState) {
}

void CmdBindDynamicDepthStencilState(VkCmdBuffer cmdBuffer, VkDynamicDepthStencilState dynamicDepthStencilState) {
}

void CmdBindDescriptorSets(VkCmdBuffer cmdBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
}

void CmdBindIndexBuffer(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
}

void CmdBindVertexBuffers(VkCmdBuffer cmdBuffer, uint32_t startBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
}

void CmdDraw(VkCmdBuffer cmdBuffer, uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount) {
}

void CmdDrawIndexed(VkCmdBuffer cmdBuffer, uint32_t firstIndex, uint32_t indexCount, int32_t vertexOffset, uint32_t firstInstance, uint32_t instanceCount) {
}

void CmdDrawIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride) {
}

void CmdDrawIndexedIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t count, uint32_t stride) {
}

void CmdDispatch(VkCmdBuffer cmdBuffer, uint32_t x, uint32_t y, uint32_t z) {
}

void CmdDispatchIndirect(VkCmdBuffer cmdBuffer, VkBuffer buffer, VkDeviceSize offset) {
}

void CmdCopyBuffer(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer destBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
}

void CmdCopyImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
}

void CmdBlitImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkTexFilter filter) {
}

void CmdCopyBufferToImage(VkCmdBuffer cmdBuffer, VkBuffer srcBuffer, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
}

void CmdCopyImageToBuffer(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer destBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
}

void CmdUpdateBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize dataSize, const uint32_t* pData) {
}

void CmdFillBuffer(VkCmdBuffer cmdBuffer, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data) {
}

void CmdClearColorImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
}

void CmdClearDepthStencilImage(VkCmdBuffer cmdBuffer, VkImage image, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
}

void CmdClearColorAttachment(VkCmdBuffer cmdBuffer, uint32_t colorAttachment, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rectCount, const VkRect3D* pRects) {
}

void CmdClearDepthStencilAttachment(VkCmdBuffer cmdBuffer, VkImageAspectFlags imageAspectMask, VkImageLayout imageLayout, float depth, uint32_t stencil, uint32_t rectCount, const VkRect3D* pRects) {
}

void CmdResolveImage(VkCmdBuffer cmdBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage destImage, VkImageLayout destImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
}

void CmdSetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
}

void CmdResetEvent(VkCmdBuffer cmdBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
}

void CmdWaitEvents(VkCmdBuffer cmdBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, uint32_t memBarrierCount, const void* const* ppMemBarriers) {
}

void CmdPipelineBarrier(VkCmdBuffer cmdBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags destStageMask, VkBool32 byRegion, uint32_t memBarrierCount, const void* const* ppMemBarriers) {
}

void CmdBeginQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot, VkQueryControlFlags flags) {
}

void CmdEndQuery(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t slot) {
}

void CmdResetQueryPool(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount) {
}

void CmdWriteTimestamp(VkCmdBuffer cmdBuffer, VkTimestampType timestampType, VkBuffer destBuffer, VkDeviceSize destOffset) {
}

void CmdCopyQueryPoolResults(VkCmdBuffer cmdBuffer, VkQueryPool queryPool, uint32_t startQuery, uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride, VkQueryResultFlags flags) {
}

void CmdPushConstants(VkCmdBuffer cmdBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t start, uint32_t length, const void* values) {
}

void CmdBeginRenderPass(VkCmdBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkRenderPassContents contents) {
}

void CmdNextSubpass(VkCmdBuffer cmdBuffer, VkRenderPassContents contents) {
}

void CmdEndRenderPass(VkCmdBuffer cmdBuffer) {
}

void CmdExecuteCommands(VkCmdBuffer cmdBuffer, uint32_t cmdBuffersCount, const VkCmdBuffer* pCmdBuffers) {
}

#pragma clang diagnostic pop
// clang-format on

}  // namespace null_driver
