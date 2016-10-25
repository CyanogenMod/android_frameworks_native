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

#include <binder/IInterface.h>
#include <binder/Parcel.h>

#include <gui/IConsumerListener.h>
#include <gui/BufferItem.h>

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

enum {
    ON_FRAME_AVAILABLE = IBinder::FIRST_CALL_TRANSACTION,
    ON_BUFFER_RELEASED,
    ON_SIDEBAND_STREAM_CHANGED,
    GET_FRAME_TIMESTAMPS
};

class BpConsumerListener : public BpInterface<IConsumerListener>
{
public:
    BpConsumerListener(const sp<IBinder>& impl)
        : BpInterface<IConsumerListener>(impl) {
    }

    virtual ~BpConsumerListener();

    virtual void onFrameAvailable(const BufferItem& item) {
        Parcel data, reply;
        data.writeInterfaceToken(IConsumerListener::getInterfaceDescriptor());
        data.write(item);
        remote()->transact(ON_FRAME_AVAILABLE, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual void onBuffersReleased() {
        Parcel data, reply;
        data.writeInterfaceToken(IConsumerListener::getInterfaceDescriptor());
        remote()->transact(ON_BUFFER_RELEASED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual void onSidebandStreamChanged() {
        Parcel data, reply;
        data.writeInterfaceToken(IConsumerListener::getInterfaceDescriptor());
        remote()->transact(ON_SIDEBAND_STREAM_CHANGED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual bool getFrameTimestamps(uint64_t frameNumber,
            FrameTimestamps* outTimestamps) const {
        Parcel data, reply;
        status_t result = data.writeInterfaceToken(
                IConsumerListener::getInterfaceDescriptor());
        if (result != NO_ERROR) {
            ALOGE("getFrameTimestamps failed to write token: %d", result);
            return false;
        }
        result = data.writeUint64(frameNumber);
        if (result != NO_ERROR) {
            ALOGE("getFrameTimestamps failed to write: %d", result);
            return false;
        }
        result = remote()->transact(GET_FRAME_TIMESTAMPS, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getFrameTimestamps failed to transact: %d", result);
            return false;
        }
        bool found = false;
        result = reply.readBool(&found);
        if (result != NO_ERROR) {
            ALOGE("getFrameTimestamps failed to read: %d", result);
            return false;
        }
        if (found) {
            result = reply.read(*outTimestamps);
            if (result != NO_ERROR) {
                ALOGE("getFrameTimestamps failed to read timestamps: %d",
                        result);
                return false;
            }
        }
        return found;
    }
};

// Out-of-line virtual method definition to trigger vtable emission in this
// translation unit (see clang warning -Wweak-vtables)
BpConsumerListener::~BpConsumerListener() {}

IMPLEMENT_META_INTERFACE(ConsumerListener, "android.gui.IConsumerListener");

// ----------------------------------------------------------------------

status_t BnConsumerListener::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case ON_FRAME_AVAILABLE: {
            CHECK_INTERFACE(IConsumerListener, data, reply);
            BufferItem item;
            data.read(item);
            onFrameAvailable(item);
            return NO_ERROR; }
        case ON_BUFFER_RELEASED: {
            CHECK_INTERFACE(IConsumerListener, data, reply);
            onBuffersReleased();
            return NO_ERROR; }
        case ON_SIDEBAND_STREAM_CHANGED: {
            CHECK_INTERFACE(IConsumerListener, data, reply);
            onSidebandStreamChanged();
            return NO_ERROR; }
        case GET_FRAME_TIMESTAMPS: {
            CHECK_INTERFACE(IConsumerListener, data, reply);
            uint64_t frameNumber = 0;
            status_t result = data.readUint64(&frameNumber);
            if (result != NO_ERROR) {
                ALOGE("onTransact failed to read: %d", result);
                return result;
            }
            FrameTimestamps timestamps;
            bool found = getFrameTimestamps(frameNumber, &timestamps);
            result = reply->writeBool(found);
            if (result != NO_ERROR) {
                ALOGE("onTransact failed to write: %d", result);
                return result;
            }
            if (found) {
                result = reply->write(timestamps);
                if (result != NO_ERROR) {
                    ALOGE("onTransact failed to write timestamps: %d", result);
                    return result;
                }
            }
            return NO_ERROR;
        }
    }
    return BBinder::onTransact(code, data, reply, flags);
}


// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------
