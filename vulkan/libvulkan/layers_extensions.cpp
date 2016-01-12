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

#define LOG_NDEBUG 0

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
std::vector<Layer> g_layers;

void AddLayerLibrary(const std::string& path) {
    ALOGV("examining layer library '%s'", path.c_str());

    void* dlhandle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!dlhandle) {
        ALOGW("failed to load layer library '%s': %s", path.c_str(), dlerror());
        return;
    }

    PFN_vkEnumerateInstanceLayerProperties enumerate_layer_properties =
        reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            dlsym(dlhandle, "vkEnumerateInstanceLayerProperties"));
    if (!enumerate_layer_properties) {
        ALOGW(
            "failed to find vkEnumerateInstanceLayerProperties in library "
            "'%s': %s",
            path.c_str(), dlerror());
        dlclose(dlhandle);
        return;
    }
    PFN_vkEnumerateInstanceExtensionProperties enumerate_extension_properties =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            dlsym(dlhandle, "vkEnumerateInstanceExtensionProperties"));
    if (!enumerate_extension_properties) {
        ALOGW(
            "failed to find vkEnumerateInstanceExtensionProperties in library "
            "'%s': %s",
            path.c_str(), dlerror());
        dlclose(dlhandle);
        return;
    }

    uint32_t layer_count;
    VkResult result = enumerate_layer_properties(&layer_count, nullptr);
    if (result != VK_SUCCESS) {
        ALOGW("vkEnumerateInstanceLayerProperties failed for library '%s': %d",
              path.c_str(), result);
        dlclose(dlhandle);
        return;
    }
    VkLayerProperties* properties = static_cast<VkLayerProperties*>(
        alloca(layer_count * sizeof(VkLayerProperties)));
    result = enumerate_layer_properties(&layer_count, properties);
    if (result != VK_SUCCESS) {
        ALOGW("vkEnumerateInstanceLayerProperties failed for library '%s': %d",
              path.c_str(), result);
        dlclose(dlhandle);
        return;
    }

    size_t library_idx = g_layer_libraries.size();
    g_layers.reserve(g_layers.size() + layer_count);
    for (size_t i = 0; i < layer_count; i++) {
        Layer layer;
        layer.properties = properties[i];
        layer.library_idx = library_idx;

        uint32_t count;
        result = enumerate_extension_properties(properties[i].layerName, &count,
                                                nullptr);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceExtensionProperties(%s) failed for library "
                "'%s': %d",
                properties[i].layerName, path.c_str(), result);
            g_layers.resize(g_layers.size() - (i + 1));
            dlclose(dlhandle);
            return;
        }
        layer.extensions.resize(count);
        result = enumerate_extension_properties(properties[i].layerName, &count,
                                                layer.extensions.data());
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceExtensionProperties(%s) failed for library "
                "'%s': %d",
                properties[i].layerName, path.c_str(), result);
            g_layers.resize(g_layers.size() - (i + 1));
            dlclose(dlhandle);
            return;
        }

        g_layers.push_back(layer);
        ALOGV("found layer '%s'", properties[i].layerName);
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

}  // anonymous namespace

namespace vulkan {

void DiscoverLayers() {
    if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0))
        DiscoverLayersInDirectory("/data/local/debug/vulkan");
    if (!LoaderData::GetInstance().layer_path.empty())
        DiscoverLayersInDirectory(LoaderData::GetInstance().layer_path.c_str());
}

uint32_t EnumerateLayers(uint32_t count, VkLayerProperties* properties) {
    uint32_t n = std::min(count, static_cast<uint32_t>(g_layers.size()));
    for (uint32_t i = 0; i < n; i++) {
        properties[i] = g_layers[i].properties;
    }
    return static_cast<uint32_t>(g_layers.size());
}

void GetLayerExtensions(const char* name,
                        const VkExtensionProperties** properties,
                        uint32_t* count) {
    for (const auto& layer : g_layers) {
        if (strcmp(name, layer.properties.layerName) != 0)
            continue;
        *properties = layer.extensions.data();
        *count = static_cast<uint32_t>(layer.extensions.size());
    }
}

LayerRef GetLayerRef(const char* name) {
    for (uint32_t id = 0; id < g_layers.size(); id++) {
        if (strcmp(name, g_layers[id].properties.layerName) != 0) {
            LayerLibrary& library = g_layer_libraries[g_layers[id].library_idx];
            std::lock_guard<std::mutex> lock(g_library_mutex);
            if (library.refcount++ == 0) {
                library.dlhandle =
                    dlopen(library.path.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (!library.dlhandle) {
                    ALOGE("failed to load layer library '%s': %s",
                          library.path.c_str(), dlerror());
                    library.refcount = 0;
                    return LayerRef(nullptr);
                }
            }
            return LayerRef(&g_layers[id]);
        }
    }
    return LayerRef(nullptr);
}

LayerRef::LayerRef(Layer* layer) : layer_(layer) {}

LayerRef::~LayerRef() {
    if (layer_) {
        LayerLibrary& library = g_layer_libraries[layer_->library_idx];
        std::lock_guard<std::mutex> lock(g_library_mutex);
        if (--library.refcount == 0) {
            dlclose(library.dlhandle);
            library.dlhandle = nullptr;
        }
    }
}

LayerRef::LayerRef(LayerRef&& other) : layer_(std::move(other.layer_)) {}

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

}  // namespace vulkan
