/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef DRM_ENGINE_API_H_
#define DRM_ENGINE_API_H_

#include <utils/Errors.h>
#include <media/stagefright/foundation/ABase.h>


namespace android {

    class CryptoPlugin;
    class DrmClientPlugin;

     // DRMs are implemented in DrmEngine plugins, which are dynamically
     // loadable shared libraries that implement the entry point
     // createDrmPluginFactory.  createDrmPluginFactory constructs and returns
     // an instance of a DrmPluginFactory object.  When a MediaCrypto or
     // DrmClient object needs to be constructed, all available
     // DrmEngines present in the plugins directory on the device are scanned
     // for a matching DrmEngine that can support the crypto scheme.  When a
     // match is found, the DrmEngine’s createCryptoPlugin or
     // createDrmClientPlugin methods are used to create CryptoPlugin or
     // DrmClientPlugin instances to support that DRM scheme.

    class DrmPluginFactory {
    public:
        DrmPluginFactory() {}
        virtual ~DrmPluginFactory() {}

         // DrmPluginFactory::isCryptoSchemeSupported can be called to determine
         // if the plugin factory is able to construct plugins that support a
         // given crypto scheme, which is specified by a UUID.
        virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]) const = 0;

        // Construct a CryptoPlugin for the crypto scheme specified by UUID.
        // {data, size} provide scheme-specific initialization data.
        virtual status_t createCryptoPlugin(
                const uint8_t uuid[16], const void *data, size_t size,
                CryptoPlugin **plugin) = 0;

        // Construct a DrmClientPlugin for the crypto scheme specified by UUID.
        // {data, size} provide scheme-specific initialization data.
        virtual status_t createDrmClientPlugin(
                const uint8_t uuid[16], const void *data, size_t size,
                DrmClientPlugin **plugin) = 0;

    private:
        DISALLOW_EVIL_CONSTRUCTORS(DrmPluginFactory);
    };

}  // namespace android

 //  Loadable DrmEngine shared libraries should define the entry point
 //  createDrmPluginFactory as shown below:
 //
 //  extern "C" {
 //      extern android::DrmPluginFactory *createDrmPluginFactory();
 //  }

#endif // DRM_ENGINE_API_H_
