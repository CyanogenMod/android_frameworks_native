 /*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef DRM_API_H_
#define DRM_API_H_

#include <utils/List.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <media/stagefright/foundation/ABase.h>

//  Loadable DrmEngine shared libraries should define the entry points
//  createDrmFactory and createCryptoFactory as shown below:
//
//  extern "C" {
//      extern android::DrmFactory *createDrmFactory();
//      extern android::CryptoFactory *createCryptoFactory();
//  }

namespace android {

    struct DrmPlugin;

    // DRMs are implemented in DrmEngine plugins, which are dynamically
    // loadable shared libraries that implement the entry points
    // createDrmFactory and createCryptoFactory.  createDrmFactory
    // constructs and returns an instance of a DrmFactory object.  Similarly,
    // createCryptoFactory creates an instance of a CryptoFactory object.
    // When a MediaCrypto or MediaDrm object needs to be constructed, all
    // available DrmEngines present in the plugins directory on the device
    // are scanned for a matching DrmEngine that can support the crypto
    // scheme.  When a match is found, the DrmEngine's createCryptoPlugin and
    // createDrmPlugin methods are used to create CryptoPlugin or
    // DrmPlugin instances to support that DRM scheme.

    class DrmFactory {
    public:
        DrmFactory() {}
        virtual ~DrmFactory() {}

        // DrmFactory::isCryptoSchemeSupported can be called to determine
        // if the plugin factory is able to construct plugins that support a
        // given crypto scheme, which is specified by a UUID.
        virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]) = 0;

        // Construct a DrmPlugin for the crypto scheme specified by UUID.
        virtual status_t createDrmPlugin(
                const uint8_t uuid[16], DrmPlugin **plugin) = 0;

    private:
        DrmFactory(const DrmFactory &);
        DrmFactory &operator=(const DrmFactory &);
    };

    class DrmPlugin {
    public:
        enum EventType {
            kDrmPluginEventProvisionRequired,
            kDrmPluginEventLicenseNeeded,
            kDrmPluginEventLicenseExpired,
            kDrmPluginEventVendorDefined
        };

        // A license can be for offline content or for online streaming.
        // Offline licenses are persisted on the device and may be used when the device
        // is disconnected from the network.
        enum LicenseType {
            kLicenseType_Offline,
            kLicenseType_Streaming
        };

        DrmPlugin() {}
        virtual ~DrmPlugin() {}

        // Open a new session with the DrmPlugin object.  A session ID is returned
        // in the sessionId parameter.
        virtual status_t openSession(Vector<uint8_t> &sessionId) = 0;

        // Close a session on the DrmPlugin object.
        virtual status_t closeSession(Vector<uint8_t> const &sessionId) = 0;

        // A license request/response exchange occurs between the app and a License
        // Server to obtain the keys required to decrypt the content.  getLicenseRequest()
        // is used to obtain an opaque license request blob that is delivered to the
        // license server.
        //
        // The init data passed to getLicenseRequest is container-specific and its
        // meaning is interpreted based on the mime type provided in the mimeType
        // parameter to getLicenseRequest.  It could contain, for example, the content
        // ID, key ID or other data obtained from the content metadata that is required
        // in generating the license request.
        //
        // licenseType specifes if the license is for streaming or offline content
        //
        // optionalParameters are included in the license server request message to
        // allow a client application to provide additional message parameters to the
        // server.
        //
        // If successful, the opaque license request blob is returned to the caller.
        virtual status_t
            getLicenseRequest(Vector<uint8_t> const &sessionId,
                              Vector<uint8_t> const &initData,
                              String8 const &mimeType, LicenseType licenseType,
                              KeyedVector<String8, String8> const &optionalParameters,
                              Vector<uint8_t> &request, String8 &defaultUrl) = 0;

        // After a license response is received by the app, it is provided to the
        // Drm plugin using provideLicenseResponse.
        virtual status_t provideLicenseResponse(Vector<uint8_t> const &sessionId,
                                                Vector<uint8_t> const &response) = 0;

        // Remove the keys associated with a license.
        virtual status_t removeLicense(Vector<uint8_t> const &sessionId) = 0;

        // Request an informative description of the license for the session.  The status
        // is in the form of {name, value} pairs.  Since DRM license policies vary by
        // vendor, the specific status field names are determined by each DRM vendor.
        // Refer to your DRM provider documentation for definitions of the field names
        // for a particular DrmEngine.
        virtual status_t
            queryLicenseStatus(Vector<uint8_t> const &sessionId,
                               KeyedVector<String8, String8> &infoMap) const = 0;

        // A provision request/response exchange occurs between the app and a
        // provisioning server to retrieve a device certificate.  getProvisionRequest
        // is used to obtain an opaque license request blob that is delivered to the
        // provisioning server.
        //
        // If successful, the opaque provision request blob is returned to the caller.
        virtual status_t getProvisionRequest(Vector<uint8_t> &request,
                                             String8 &defaultUrl) = 0;

        // After a provision response is received by the app, it is provided to the
        // Drm plugin using provideProvisionResponse.
        virtual status_t provideProvisionResponse(Vector<uint8_t> const &response) = 0;

        // A means of enforcing the contractual requirement for a concurrent stream
        // limit per subscriber across devices is provided via SecureStop.  SecureStop
        // is a means of securely monitoring the lifetime of sessions. Since playback
        // on a device can be interrupted due to reboot, power failure, etc. a means
        // of persisting the lifetime information on the device is needed.
        //
        // A signed version of the sessionID is written to persistent storage on the
        // device when each MediaCrypto object is created. The sessionID is signed by
        // the device private key to prevent tampering.
        //
        // In the normal case, playback will be completed, the session destroyed and
        // the Secure Stops will be queried. The App queries secure stops and forwards
        // the secure stop message to the server which verifies the signature and
        // notifies the server side database that the session destruction has been
        // confirmed. The persisted record on the client is only removed after positive
        // confirmation that the server received the message using releaseSecureStops().
        virtual status_t getSecureStops(List<Vector<uint8_t> > &secureStops) = 0;
        virtual status_t releaseSecureStops(Vector<uint8_t> const &ssRelease) = 0;

        // Read a property value given the device property string.  There are a few forms
        // of property access methods, depending on the data type returned.
        // Since DRM plugin properties may vary, additional field names may be defined
        // by each DRM vendor.  Refer to your DRM provider documentation for definitions
        // of its additional field names.
        //
        // Standard values are:
        //   "vendor" [string] identifies the maker of the plugin
        //   "version" [string] identifies the version of the plugin
        //   "description" [string] describes the plugin
        //   'deviceUniqueId' [byte array] The device unique identifier is established
        //   during device provisioning and provides a means of uniquely identifying
        //   each device.
        virtual status_t getPropertyString(String8 const &name, String8 &value ) const = 0;
        virtual status_t getPropertyByteArray(String8 const &name,
                                              Vector<uint8_t> &value ) const = 0;

        // Write  a property value given the device property string.  There are a few forms
        // of property setting methods, depending on the data type.
        // Since DRM plugin properties may vary, additional field names may be defined
        // by each DRM vendor.  Refer to your DRM provider documentation for definitions
        // of its field names.
        virtual status_t setPropertyString(String8 const &name,
                                           String8 const &value ) = 0;
        virtual status_t setPropertyByteArray(String8 const &name,
                                              Vector<uint8_t> const &value ) = 0;

        // TODO: provide way to send an event
    private:
        DISALLOW_EVIL_CONSTRUCTORS(DrmPlugin);
    };

}  // namespace android

#endif // DRM_API_H_
