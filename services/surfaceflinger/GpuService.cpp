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

#include "GpuService.h"

#include <binder/Parcel.h>

namespace android {

// ----------------------------------------------------------------------------

class BpGpuService : public BpInterface<IGpuService>
{
public:
    BpGpuService(const sp<IBinder>& impl) : BpInterface<IGpuService>(impl) {}
};

IMPLEMENT_META_INTERFACE(GpuService, "android.ui.IGpuService");

status_t BnGpuService::onTransact(uint32_t code, const Parcel& data,
        Parcel* reply, uint32_t flags)
{
    switch (code) {
    case SHELL_COMMAND_TRANSACTION: {
        int in = data.readFileDescriptor();
        int out = data.readFileDescriptor();
        int err = data.readFileDescriptor();
        int argc = data.readInt32();
        Vector<String16> args;
        for (int i = 0; i < argc && data.dataAvail() > 0; i++) {
           args.add(data.readString16());
        }
        return shellCommand(in, out, err, args);
    }

    default:
        return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

const char* const GpuService::SERVICE_NAME = "gpu";

GpuService::GpuService() {}

status_t GpuService::shellCommand(int /*in*/, int /*out*/, int /*err*/,
        Vector<String16>& /*args*/)
{
    ALOGD("GpuService::shellCommand");
    return NO_ERROR;
}

} // namespace android
