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

#ifndef LIBVULKAN_LAYERS_EXTENSIONS_H
#define LIBVULKAN_LAYERS_EXTENSIONS_H 1

#include <vulkan/vulkan.h>

namespace vulkan {
namespace api {

struct Layer;
class LayerRef {
   public:
    LayerRef(Layer* layer);
    LayerRef(LayerRef&& other);
    ~LayerRef();
    LayerRef(const LayerRef&) = delete;
    LayerRef& operator=(const LayerRef&) = delete;

    const char* GetName() const;
    uint32_t GetSpecVersion();

    // provides bool-like behavior
    operator const Layer*() const { return layer_; }

    PFN_vkGetInstanceProcAddr GetGetInstanceProcAddr() const;
    PFN_vkGetDeviceProcAddr GetGetDeviceProcAddr() const;

    bool SupportsExtension(const char* name) const;

   private:
    Layer* layer_;
};

void DiscoverLayers();
uint32_t EnumerateInstanceLayers(uint32_t count, VkLayerProperties* properties);
uint32_t EnumerateDeviceLayers(uint32_t count, VkLayerProperties* properties);
void GetInstanceLayerExtensions(const char* name,
                                const VkExtensionProperties** properties,
                                uint32_t* count);
void GetDeviceLayerExtensions(const char* name,
                              const VkExtensionProperties** properties,
                              uint32_t* count);
LayerRef GetInstanceLayerRef(const char* name);
LayerRef GetDeviceLayerRef(const char* name);

}  // namespace api
}  // namespace vulkan

#endif  // LIBVULKAN_LAYERS_EXTENSIONS_H
