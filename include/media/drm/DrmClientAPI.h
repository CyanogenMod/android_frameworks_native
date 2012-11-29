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

#ifndef DRM_CLIENT_API_H_
#define DRM_CLIENT_API_H_

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/List.h>
#include <media/stagefright/foundation/ABase.h>

namespace android {

    // A DrmMessageStatus object aggregates a sessionId, which uniquely
    // identifies a playback context with a status code and opaque message
    // data.
    struct DrmMessageStatus {
        Vector<uint8_t> mSessionId;
        status_t mStatus;
        Vector<uint8_t> mData;
    };

    class DrmClientPlugin {
    public:

        // A license can be for downloaded, offline content or for online streaming
        // Offline licenses are persisted on the device and may be used when the device
        // is disconnected from the network.
        enum LicenseType {
            kLicenseType_Offline,
            kLicenseType_Streaming
        };

        DrmClientPlugin() {}
        virtual ~DrmClientPlugin() {}

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
        // The DrmMessageStatus returned from getLicenseRequest contains a sessionId for
        // the new session, a status code indicating whether the operation was successful
        // and if so, the request blob is placed into the mData field.
        virtual DrmMessageStatus getLicenseRequest(Vector<uint8_t> const &initData,
                String8 const &mimeType, LicenseType licenseType) = 0;

        // After a license response is received by the app, it is provided to the
        // DrmClient plugin using provideLicenseResponse.  The response data is provided
        // in the mData field of the response parameter.
        virtual status_t provideLicenseResponse(DrmMessageStatus const &response) = 0;

        // Remove the keys associated with a license and release the session
        virtual status_t clearLicense(Vector<uint8_t> const &sessionId) = 0;

        // A provision request/response exchange occurs between the app and a
        // provisioning server to retrieve a device certificate.  getProvisionRequest
        // is used to obtain an opaque license request blob that is delivered to the
        // provisioning server.
        //
        // The DrmMessageStatus returned from getLicenseRequest contains a status code
        // indicating whether the operation was successful and if so, the request blob
        // is placed into the mData field.
        virtual DrmMessageStatus getProvisionRequest() = 0;

        // After a provision response is received by the app, it is provided to the
        // DrmClient plugin using provideProvisionResponse.  The response data is
        // provided in the mData field of the response parameter.
        virtual status_t provideProvisionResponse(DrmMessageStatus const &response) = 0;

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
        virtual List<DrmMessageStatus> getSecureStops() = 0;
        virtual status_t releaseSecureStops(DrmMessageStatus const &ssRelease) = 0;

        // Retrieve the device unique identifier for this device.  The device unique
        // identifier is established during device provisioning.
        virtual Vector<uint8_t> getDeviceUniqueId() const = 0;

    private:
        DISALLOW_EVIL_CONSTRUCTORS(DrmClientPlugin);
    };

}  // namespace android

#endif // DRM_CLIENT_API_H_
