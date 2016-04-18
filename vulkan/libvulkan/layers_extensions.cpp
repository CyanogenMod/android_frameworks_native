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

#include "layers_extensions.h"
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
namespace api {

struct Layer {
    VkLayerProperties properties;
    size_t library_idx;
    std::vector<VkExtensionProperties> extensions;
};

namespace {

class LayerLibrary {
   public:
    LayerLibrary(const std::string& path)
        : path_(path), dlhandle_(nullptr), refcount_(0) {}

    LayerLibrary(LayerLibrary&& other)
        : path_(std::move(other.path_)),
          dlhandle_(other.dlhandle_),
          refcount_(other.refcount_) {
        other.dlhandle_ = nullptr;
        other.refcount_ = 0;
    }

    LayerLibrary(const LayerLibrary&) = delete;
    LayerLibrary& operator=(const LayerLibrary&) = delete;

    // these are thread-safe
    bool Open();
    void Close();

    bool EnumerateLayers(size_t library_idx,
                         std::vector<Layer>& instance_layers,
                         std::vector<Layer>& device_layers) const;

    void* GetGPA(const Layer& layer,
                 const char* gpa_name,
                 size_t gpa_name_len) const;

   private:
    const std::string path_;

    std::mutex mutex_;
    void* dlhandle_;
    size_t refcount_;
};

bool LayerLibrary::Open() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (refcount_++ == 0) {
        dlhandle_ = dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL);
        ALOGV("Opening library %s", path_.c_str());
        if (!dlhandle_) {
            ALOGE("failed to load layer library '%s': %s", path_.c_str(),
                  dlerror());
            refcount_ = 0;
            return false;
        }
    }
    ALOGV("Refcount on activate is %zu", refcount_);
    return true;
}

void LayerLibrary::Close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (--refcount_ == 0) {
        ALOGV("Closing library %s", path_.c_str());
        dlclose(dlhandle_);
        dlhandle_ = nullptr;
    }
    ALOGV("Refcount on destruction is %zu", refcount_);
}

bool LayerLibrary::EnumerateLayers(size_t library_idx,
                                   std::vector<Layer>& instance_layers,
                                   std::vector<Layer>& device_layers) const {
    PFN_vkEnumerateInstanceLayerProperties enumerate_instance_layers =
        reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            dlsym(dlhandle_, "vkEnumerateInstanceLayerProperties"));
    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            dlsym(dlhandle_, "vkEnumerateInstanceExtensionProperties"));
    if (!enumerate_instance_layers || !enumerate_instance_extensions) {
        ALOGV("layer library '%s' misses some instance enumeraion functions",
              path_.c_str());
        return false;
    }

    // device functions are optional
    PFN_vkEnumerateDeviceLayerProperties enumerate_device_layers =
        reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
            dlsym(dlhandle_, "vkEnumerateDeviceLayerProperties"));
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions =
        reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            dlsym(dlhandle_, "vkEnumerateDeviceExtensionProperties"));

    // get layer counts
    uint32_t num_instance_layers = 0;
    uint32_t num_device_layers = 0;
    VkResult result = enumerate_instance_layers(&num_instance_layers, nullptr);
    if (result != VK_SUCCESS || !num_instance_layers) {
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceLayerProperties failed for library '%s': "
                "%d",
                path_.c_str(), result);
        }
        return false;
    }
    if (enumerate_device_layers) {
        result = enumerate_device_layers(VK_NULL_HANDLE, &num_device_layers,
                                         nullptr);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateDeviceLayerProperties failed for library '%s': %d",
                path_.c_str(), result);
            return false;
        }
    }

    // get layer properties
    VkLayerProperties* properties = static_cast<VkLayerProperties*>(alloca(
        (num_instance_layers + num_device_layers) * sizeof(VkLayerProperties)));
    result = enumerate_instance_layers(&num_instance_layers, properties);
    if (result != VK_SUCCESS) {
        ALOGW("vkEnumerateInstanceLayerProperties failed for library '%s': %d",
              path_.c_str(), result);
        return false;
    }
    if (num_device_layers > 0) {
        result = enumerate_device_layers(VK_NULL_HANDLE, &num_device_layers,
                                         properties + num_instance_layers);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateDeviceLayerProperties failed for library '%s': %d",
                path_.c_str(), result);
            return false;
        }
    }

    // append layers to instance_layers/device_layers
    size_t prev_num_instance_layers = instance_layers.size();
    size_t prev_num_device_layers = device_layers.size();
    instance_layers.reserve(prev_num_instance_layers + num_instance_layers);
    device_layers.reserve(prev_num_device_layers + num_device_layers);
    for (size_t i = 0; i < num_instance_layers; i++) {
        const VkLayerProperties& props = properties[i];

        Layer layer;
        layer.properties = props;
        layer.library_idx = library_idx;

        uint32_t count = 0;
        result =
            enumerate_instance_extensions(props.layerName, &count, nullptr);
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceExtensionProperties(%s) failed for library "
                "'%s': %d",
                props.layerName, path_.c_str(), result);
            instance_layers.resize(prev_num_instance_layers);
            return false;
        }
        layer.extensions.resize(count);
        result = enumerate_instance_extensions(props.layerName, &count,
                                               layer.extensions.data());
        if (result != VK_SUCCESS) {
            ALOGW(
                "vkEnumerateInstanceExtensionProperties(%s) failed for library "
                "'%s': %d",
                props.layerName, path_.c_str(), result);
            instance_layers.resize(prev_num_instance_layers);
            return false;
        }

        instance_layers.push_back(layer);
        ALOGV("  added instance layer '%s'", props.layerName);

        bool is_global = false;
        for (size_t j = 0; j < num_device_layers; j++) {
            const auto& dev_props = properties[num_instance_layers + j];
            if (memcmp(&props, &dev_props, sizeof(props)) == 0) {
                is_global = true;
                break;
            }
        }

        if (!is_global)
            continue;

        layer.extensions.clear();
        if (enumerate_device_extensions) {
            result = enumerate_device_extensions(
                VK_NULL_HANDLE, props.layerName, &count, nullptr);
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateDeviceExtensionProperties(%s) failed for "
                    "library '%s': %d",
                    props.layerName, path_.c_str(), result);
                instance_layers.resize(prev_num_instance_layers);
                device_layers.resize(prev_num_device_layers);
                return false;
            }
            layer.extensions.resize(count);
            result =
                enumerate_device_extensions(VK_NULL_HANDLE, props.layerName,
                                            &count, layer.extensions.data());
            if (result != VK_SUCCESS) {
                ALOGW(
                    "vkEnumerateDeviceExtensionProperties(%s) failed for "
                    "library '%s': %d",
                    props.layerName, path_.c_str(), result);
                instance_layers.resize(prev_num_instance_layers);
                device_layers.resize(prev_num_device_layers);
                return false;
            }
        }

        device_layers.push_back(layer);
        ALOGV("  added device layer '%s'", props.layerName);
    }

    return true;
}

void* LayerLibrary::GetGPA(const Layer& layer,
                           const char* gpa_name,
                           size_t gpa_name_len) const {
    void* gpa;
    size_t layer_name_len =
        std::max(size_t{2}, strlen(layer.properties.layerName));
    char* name = static_cast<char*>(alloca(layer_name_len + gpa_name_len + 1));
    strcpy(name, layer.properties.layerName);
    strcpy(name + layer_name_len, gpa_name);
    if (!(gpa = dlsym(dlhandle_, name))) {
        strcpy(name, "vk");
        strcpy(name + 2, gpa_name);
        gpa = dlsym(dlhandle_, name);
    }
    return gpa;
}

std::vector<LayerLibrary> g_layer_libraries;
std::vector<Layer> g_instance_layers;
std::vector<Layer> g_device_layers;

void AddLayerLibrary(const std::string& path) {
    ALOGV("examining layer library '%s'", path.c_str());

    LayerLibrary library(path);
    if (!library.Open())
        return;

    if (!library.EnumerateLayers(g_layer_libraries.size(), g_instance_layers,
                                 g_device_layers)) {
        library.Close();
        return;
    }

    library.Close();

    g_layer_libraries.emplace_back(std::move(library));
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

const Layer* FindLayer(const std::vector<Layer>& layers, const char* name) {
    auto layer =
        std::find_if(layers.cbegin(), layers.cend(), [=](const Layer& entry) {
            return strcmp(entry.properties.layerName, name) == 0;
        });
    return (layer != layers.cend()) ? &*layer : nullptr;
}

void* GetLayerGetProcAddr(const Layer& layer,
                          const char* gpa_name,
                          size_t gpa_name_len) {
    const LayerLibrary& library = g_layer_libraries[layer.library_idx];
    return library.GetGPA(layer, gpa_name, gpa_name_len);
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
    const Layer* layer = FindLayer(layers, name);
    if (layer) {
        *properties = layer->extensions.data();
        *count = static_cast<uint32_t>(layer->extensions.size());
    } else {
        *properties = nullptr;
        *count = 0;
    }
}

LayerRef GetLayerRef(std::vector<Layer>& layers, const char* name) {
    const Layer* layer = FindLayer(layers, name);
    if (layer) {
        LayerLibrary& library = g_layer_libraries[layer->library_idx];
        if (!library.Open())
            layer = nullptr;
    }

    return LayerRef(layer);
}

}  // anonymous namespace

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

LayerRef::LayerRef(const Layer* layer) : layer_(layer) {}

LayerRef::~LayerRef() {
    if (layer_) {
        LayerLibrary& library = g_layer_libraries[layer_->library_idx];
        library.Close();
    }
}

const char* LayerRef::GetName() const {
    return layer_->properties.layerName;
}

uint32_t LayerRef::GetSpecVersion() const {
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

}  // namespace api
}  // namespace vulkan
