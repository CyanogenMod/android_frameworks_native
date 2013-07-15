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

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <input/IInputFlinger.h>


namespace android {

class BpInputFlinger : public BpInterface<IInputFlinger> {
public:
    BpInputFlinger(const sp<IBinder>& impl) :
            BpInterface<IInputFlinger>(impl) { }

    virtual status_t doSomething() {
        Parcel data, reply;
        data.writeInterfaceToken(IInputFlinger::getInterfaceDescriptor());
        remote()->transact(BnInputFlinger::DO_SOMETHING_TRANSACTION, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(InputFlinger, "android.input.IInputFlinger");


status_t BnInputFlinger::onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    switch(code) {
    case DO_SOMETHING_TRANSACTION: {
        CHECK_INTERFACE(IInputFlinger, data, reply);
        reply->writeInt32(0);
        break;
    }
    default:
        return BBinder::onTransact(code, data, reply, flags);
    }
    return NO_ERROR;
}

};
