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

#ifndef HDCP_API_H_

#define HDCP_API_H_

#include <utils/Errors.h>

namespace android {

struct HDCPModule {
    typedef void (*ObserverFunc)(void *cookie, int msg, int ext1, int ext2);

    // The msg argument in calls to the observer notification function.
    enum {
        // Sent in response to a call to "HDCPModule::initAsync" once
        // initialization has either been successfully completed,
        // i.e. the HDCP session is now fully setup (AKE, Locality Check,
        // SKE and any authentication with repeaters completed) or failed.
        // ext1 should be a suitable error code (status_t), ext2 is
        // unused.
        HDCP_INITIALIZATION_COMPLETE,
        HDCP_INITIALIZATION_FAILED,

        // Sent upon completion of a call to "HDCPModule::shutdownAsync".
        // ext1 should be a suitable error code, ext2 is unused.
        HDCP_SHUTDOWN_COMPLETE,
        HDCP_SHUTDOWN_FAILED,

        HDCP_UNAUTHENTICATED_CONNECTION,
        HDCP_UNAUTHORIZED_CONNECTION,
        HDCP_REVOKED_CONNECTION,
        HDCP_TOPOLOGY_EXECEEDED,
        HDCP_UNKNOWN_ERROR,
    };

    // Module can call the notification function to signal completion/failure
    // of asynchronous operations (such as initialization) or out of band
    // events.
    HDCPModule(void *cookie, ObserverFunc observerNotify) {};

    virtual ~HDCPModule() {};

    // Request to setup an HDCP session with the specified host listening
    // on the specified port.
    virtual status_t initAsync(const char *host, unsigned port) = 0;

    // Request to shutdown the active HDCP session.
    virtual status_t shutdownAsync() = 0;

    // Encrypt a data according to the HDCP spec. The data is to be
    // encrypted in-place, only size bytes of data should be read/write,
    // even if the size is not a multiple of 128 bit (16 bytes).
    // This operation is to be synchronous, i.e. this call does not return
    // until outData contains size bytes of encrypted data.
    // streamCTR will be assigned by the caller (to 0 for the first PES stream,
    // 1 for the second and so on)
    // inputCTR will be maintained by the callee for each PES stream.
    virtual status_t encrypt(
            const void *inData, size_t size, uint32_t streamCTR,
            uint64_t *outInputCTR, void *outData) = 0;

private:
    HDCPModule(const HDCPModule &);
    HDCPModule &operator=(const HDCPModule &);
};

}  // namespace android

// A shared library exporting the following method should be included to
// support HDCP functionality. The shared library must be called
// "libstagefright_hdcp.so", it will be dynamically loaded into the
// mediaserver process.
extern "C" {
    extern android::HDCPModule *createHDCPModule(
            void *cookie, android::HDCPModule::ObserverFunc);
}

#endif  // HDCP_API_H_

