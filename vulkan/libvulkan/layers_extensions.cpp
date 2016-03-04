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

// #define LOG_NDEBUG 0

#include "loader.h"
#include <alloca.h>
#include <dirent.h>
#include <dlfcn.h>
#include <mutex>
#include <sys/prctl.h>
#include <string>
#include <string.h>
#include <vector>
#include <log/log.h>
#include <vulkan/vulkan_loader_data.h>

using namespace vulkan;

// TODO(jessehall): The whole way we deal with extensions is pretty hokey, and
// not a good long-term solution. Having a hard-coded enum of extensions is
// bad, of course. Representing sets of extensions (requested, supported, etc.)
// as a bitset isn't necessarily bad, if the mapping from extension to bit were
// dynamic. Need to rethink this completely when there's a little more time.

// TODO(jessehall): This file currently builds up global data structures as it
// loads, and never cleans them up. This means we're doing heap allocations
// without going through an app-provided allocator, but worse, we'll leak those
// allocations if the loader is unloaded.
//
// We should allocate "enough" BSS space, and suballocate from there. Will
// probably want to intern strings, etc., and will need some custom/manual data
// structures.

// TODO(jessehall): Currently we have separate lists for instance and device
// layers. Most layers are both; we should use one entry for each layer name,
// with a mask saying what kind(s) it is.

namespace vulkan {
struct Layer {
    VkLayerProperties properties;
    size_t library_idx;
    std::vector<VkExtensionProperties> extensions;
};
}  // namespace vulkan

namespace {

std::mutex g_library_mutex;
struct LayerLibrary {
    std::string path;
    void* dlhandle;
    size_t refcount;
};
std::vector<LayerLibrary> g_layer_libraries;
std::vector<Layer> g_instance_layers;
std::vector<Layer> g_device_layers;

void AddLayerLibrary(const std::string& path) {
    ALOGV("examining layer library '%s'", path.c_str());

    void* dlhandle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!dlhandle) {
        ALOGW("failed to load layer library '%s': %s", path.c_str(), dlerror());
        return;
    }

    PFN_vkEnumerateInstanceLayerProperties enumerate_instance_layers =
        reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            dlsym(dlhandle, "vkEnumerateInstanceLayerProperties"));
    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            dlsym(dlhandle, "vkEnumerateInstanceExtensionProperties"));
    PFN_vkEnumerateDeviceLayerProperties enumerate_device_layers =
        reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
            dlsym(dlhandle, "vkEnumerateDeviceLayerProperties"));
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions =
        reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            dlsym(dlhandle, "vkEnumerateDeviceExtensionProperties"));
    if (!((enumerate_instance_layers && enumerate_instance_extensions) ||
          (enumerate_device_layers && enumerate_device_extensions))) {
        ALOGV(
            "layer library '%s' has neither instance nor device enumeraion "
            "functions",
            path.c_str());
        dlclose(dlhandle);
        return;
    }

    VkResult result;
    uint32_t num_instance_layers = 0;
    uint32_t num_device_layers = 0;
    if (enumerate_instance_layers) {
        result = enumerate_instance_layers(&num_instance_layers, nullptr);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceLayerProperties failed for library '%s': "
                "%d",
                path.c_str(), result);
            dlclose(dlhandle);
            return;
        }
    }
    if (enumerate_device_layers) {
        result = enumerate_device_layers(VK_NULL_HANDLE, &num_device_layers,
                                         nullptr);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateDeviceLayerProperties failed for library '%s': %d",
                path.c_str(), result);
            dlclose(dlhandle);
            return;
        }
    }
    VkLayerProperties* properties = static_cast<VkLayerProperties*>(alloca(
        (num_instance_layers + num_device_layers) * sizeof(VkLayerProperties)));
    if (num_instance_layers > 0) {
        result = enumerate_instance_layers(&num_instance_layers, properties);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceLayerProperties failed for library '%s': "
                "%d",
                path.c_str(), result);
            dlclose(dlhandle);
            return;
        }
    }
    if (num_device_layers > 0) {
        result = enumerate_device_layers(VK_NULL_HANDLE, &num_device_layers,
                                         properties + num_instance_layers);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateDeviceLayerProperties failed for library '%s': %d",
                path.c_str(), result);
            dlclose(dlhandle);
            return;
        }
    }

    size_t library_idx = g_layer_libraries.size();
    size_t prev_num_instance_layers = g_instance_layers.size();
    size_t prev_num_device_layers = g_device_layers.size();
    g_instance_layers.reserve(prev_num_instance_layers + num_instance_layers);
    g_device_layers.reserve(prev_num_device_layers + num_device_layers);
    for (size_t i = 0; i < num_instance_layers; i++) {
        const VkLayerProperties& props = properties[i];

        Layer layer;
        layer.properties = props;
        layer.library_idx = library_idx;

        if (enumerate_instance_extensions) {
            uint32_t count = 0;
            result =
                enumerate_instance_extensions(props.layerName, &count, nullptr);
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateInstanceExtensionProperties(%s) failed for "
                    "library "
                    "'%s': %d",
                    props.layerName, path.c_str(), result);
                g_instance_layers.resize(prev_num_instance_layers);
                dlclose(dlhandle);
                return;
            }
            layer.extensions.resize(count);
            result = enumerate_instance_extensions(props.layerName, &count,
                                                   layer.extensions.data());
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateInstanceExtensionProperties(%s) failed for "
                    "library "
                    "'%s': %d",
                    props.layerName, path.c_str(), result);
                g_instance_layers.resize(prev_num_instance_layers);
                dlclose(dlhandle);
                return;
            }
        }

        g_instance_layers.push_back(layer);
        ALOGV("  added instance layer '%s'", props.layerName);
    }
    for (size_t i = 0; i < num_device_layers; i++) {
        const VkLayerProperties& props = properties[num_instance_layers + i];

        Layer layer;
        layer.properties = props;
        layer.library_idx = library_idx;

        if (enumerate_device_extensions) {
            uint32_t count;
            result = enumerate_device_extensions(
                VK_NULL_HANDLE, props.layerName, &count, nullptr);
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateDeviceExtensionProperties(%s) failed for "
                    "library "
                    "'%s': %d",
                    props.layerName, path.c_str(), result);
                g_instance_layers.resize(prev_num_instance_layers);
                g_device_layers.resize(prev_num_device_layers);
                dlclose(dlhandle);
                return;
            }
            layer.extensions.resize(count);
            result =
                enumerate_device_extensions(VK_NULL_HANDLE, props.layerName,
                                            &count, layer.extensions.data());
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateDeviceExtensionProperties(%s) failed for "
                    "library "
                    "'%s': %d",
                    props.layerName, path.c_str(), result);
                g_instance_layers.resize(prev_num_instance_layers);
                g_device_layers.resize(prev_num_device_layers);
                dlclose(dlhandle);
                return;
            }
        }

        g_device_layers.push_back(layer);
        ALOGV("  added device layer '%s'", props.layerName);
    }

    dlclose(dlhandle);

    g_layer_libraries.push_back(LayerLibrary{path, nullptr, 0});
}

void DiscoverLayersInDirectory(const std::string& dir_path) {
    ALOGV("looking for layers in '%s'", dir_path.c_str());

    DIR* directory = opendir(dir_path.c_str());
    if (!directory) {
        int err = errno;
        ALOGV_IF(err != ENOENT, "failed to open layer directory '%s': %s (%d)",
                 dir_path.c_str(), strerror(err), err);
        return;
    }

    std::string path;
    path.reserve(dir_path.size() + 20);
    path.append(dir_path);
    path.append("/");

    struct dirent* entry;
    while ((entry = readdir(directory))) {
        size_t libname_len = strlen(entry->d_name);
        if (strncmp(entry->d_name, "libVkLayer", 10) != 0 ||
            strncmp(entry->d_name + libname_len - 3, ".so", 3) != 0)
            continue;
        path.append(entry->d_name);
        AddLayerLibrary(path);
        path.resize(dir_path.size() + 1);
    }

    closedir(directory);
}

void* GetLayerGetProcAddr(const Layer& layer,
                          const char* gpa_name,
                          size_t gpa_name_len) {
    const LayerLibrary& library = g_layer_libraries[layer.library_idx];
    void* gpa;
    size_t layer_name_len = std::max(size_t{2}, strlen(layer.properties.layerName));
    char* name = static_cast<char*>(alloca(layer_name_len + gpa_name_len + 1));
    strcpy(name, layer.properties.layerName);
    strcpy(name + layer_name_len, gpa_name);
    if (!(gpa = dlsym(library.dlhandle, name))) {
        strcpy(name, "vk");
        strcpy(name + 2, gpa_name);
        gpa = dlsym(library.dlhandle, name);
    }
    return gpa;
}

uint32_t EnumerateLayers(const std::vector<Layer>& layers,
                         uint32_t count,
                         VkLayerProperties* properties) {
    uint32_t n = std::min(count, static_cast<uint32_t>(layers.size()));
    for (uint32_t i = 0; i < n; i++) {
        properties[i] = layers[i].properties;
    }
    return static_cast<uint32_t>(layers.size());
}

void GetLayerExtensions(const std::vector<Layer>& layers,
                        const char* name,
                        const VkExtensionProperties** properties,
                        uint32_t* count) {
    auto layer =
        std::find_if(layers.cbegin(), layers.cend(), [=](const Layer& entry) {
            return strcmp(entry.properties.layerName, name) == 0;
        });
    if (layer == layers.cend()) {
        *properties = nullptr;
        *count = 0;
    } else {
        *properties = layer->extensions.data();
        *count = static_cast<uint32_t>(layer->extensions.size());
    }
}

LayerRef GetLayerRef(std::vector<Layer>& layers, const char* name) {
    for (uint32_t id = 0; id < layers.size(); id++) {
        if (strcmp(name, layers[id].properties.layerName) == 0) {
            LayerLibrary& library = g_layer_libraries[layers[id].library_idx];
            std::lock_guard<std::mutex> lock(g_library_mutex);
            if (library.refcount++ == 0) {
                library.dlhandle =
                    dlopen(library.path.c_str(), RTLD_NOW | RTLD_LOCAL);
                ALOGV("Opening library %s", library.path.c_str());
                if (!library.dlhandle) {
                    ALOGE("failed to load layer library '%s': %s",
                          library.path.c_str(), dlerror());
                    library.refcount = 0;
                    return LayerRef(nullptr);
                }
            }
            ALOGV("Refcount on activate is %zu", library.refcount);
            return LayerRef(&layers[id]);
        }
    }
    return LayerRef(nullptr);
}

}  // anonymous namespace

namespace vulkan {

void DiscoverLayers() {
    if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0))
        DiscoverLayersInDirectory("/data/local/debug/vulkan");
    if (!LoaderData::GetInstance().layer_path.empty())
        DiscoverLayersInDirectory(LoaderData::GetInstance().layer_path.c_str());
}

uint32_t EnumerateInstanceLayers(uint32_t count,
                                 VkLayerProperties* properties) {
    return EnumerateLayers(g_instance_layers, count, properties);
}

uint32_t EnumerateDeviceLayers(uint32_t count, VkLayerProperties* properties) {
    return EnumerateLayers(g_device_layers, count, properties);
}

void GetInstanceLayerExtensions(const char* name,
                                const VkExtensionProperties** properties,
                                uint32_t* count) {
    GetLayerExtensions(g_instance_layers, name, properties, count);
}

void GetDeviceLayerExtensions(const char* name,
                              const VkExtensionProperties** properties,
                              uint32_t* count) {
    GetLayerExtensions(g_device_layers, name, properties, count);
}

LayerRef GetInstanceLayerRef(const char* name) {
    return GetLayerRef(g_instance_layers, name);
}

LayerRef GetDeviceLayerRef(const char* name) {
    return GetLayerRef(g_device_layers, name);
}

LayerRef::LayerRef(Layer* layer) : layer_(layer) {}

LayerRef::~LayerRef() {
    if (layer_) {
        LayerLibrary& library = g_layer_libraries[layer_->library_idx];
        std::lock_guard<std::mutex> lock(g_library_mutex);
        if (--library.refcount == 0) {
            ALOGV("Closing library %s", library.path.c_str());
            dlclose(library.dlhandle);
            library.dlhandle = nullptr;
        }
        ALOGV("Refcount on destruction is %zu", library.refcount);
    }
}

const char* LayerRef::GetName() const {
    return layer_->properties.layerName;
}

uint32_t LayerRef::GetSpecVersion() {
    return layer_->properties.specVersion;
}

LayerRef::LayerRef(LayerRef&& other) : layer_(std::move(other.layer_)) {
    other.layer_ = nullptr;
}

PFN_vkGetInstanceProcAddr LayerRef::GetGetInstanceProcAddr() const {
    return layer_ ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        GetLayerGetProcAddr(*layer_, "GetInstanceProcAddr", 19))
                  : nullptr;
}

PFN_vkGetDeviceProcAddr LayerRef::GetGetDeviceProcAddr() const {
    return layer_ ? reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                        GetLayerGetProcAddr(*layer_, "GetDeviceProcAddr", 17))
                  : nullptr;
}

bool LayerRef::SupportsExtension(const char* name) const {
    return std::find_if(layer_->extensions.cbegin(), layer_->extensions.cend(),
                        [=](const VkExtensionProperties& ext) {
                            return strcmp(ext.extensionName, name) == 0;
                        }) != layer_->extensions.cend();
}

InstanceExtension InstanceExtensionFromName(const char* name) {
    if (strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        return kKHR_surface;
    if (strcmp(name, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0)
        return kKHR_android_surface;
    if (strcmp(name, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
        return kEXT_debug_report;
    return kInstanceExtensionCount;
}

DeviceExtension DeviceExtensionFromName(const char* name) {
    if (strcmp(name, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        return kKHR_swapchain;
    if (strcmp(name, VK_ANDROID_NATIVE_BUFFER_EXTENSION_NAME) == 0)
        return kANDROID_native_buffer;
    return kDeviceExtensionCount;
}

}  // namespace vulkan
