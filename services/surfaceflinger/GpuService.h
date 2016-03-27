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

#ifndef ANDROID_GPUSERVICE_H
#define ANDROID_GPUSERVICE_H

#include <binder/IInterface.h>
#include <cutils/compiler.h>

namespace android {

/*
 * This class defines the Binder IPC interface for GPU-related queries and
 * control.
 */
class IGpuService : public IInterface {
public:
    DECLARE_META_INTERFACE(GpuService);
};

class BnGpuService: public BnInterface<IGpuService> {
protected:
    virtual status_t shellCommand(int in, int out, int err,
        Vector<String16>& args) = 0;

    virtual status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0) override;
};

class GpuService : public BnGpuService
{
public:
    static const char* const SERVICE_NAME ANDROID_API;

    GpuService() ANDROID_API;

protected:
    virtual status_t shellCommand(int in, int out, int err,
        Vector<String16>& args) override;
};

} // namespace android

#endif // ANDROID_GPUSERVICE_H
