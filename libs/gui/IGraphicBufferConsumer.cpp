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

#include <utils/Errors.h>
#include <utils/NativeHandle.h>

#include <binder/Parcel.h>
#include <binder/IInterface.h>

#include <gui/BufferItem.h>
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferConsumer.h>

#include <ui/GraphicBuffer.h>
#include <ui/Fence.h>

#include <system/window.h>

namespace android {

enum {
    ACQUIRE_BUFFER = IBinder::FIRST_CALL_TRANSACTION,
    DETACH_BUFFER,
    ATTACH_BUFFER,
    RELEASE_BUFFER,
    CONSUMER_CONNECT,
    CONSUMER_DISCONNECT,
    GET_RELEASED_BUFFERS,
    SET_DEFAULT_BUFFER_SIZE,
    SET_MAX_BUFFER_COUNT,
    SET_MAX_ACQUIRED_BUFFER_COUNT,
    SET_CONSUMER_NAME,
    SET_DEFAULT_BUFFER_FORMAT,
    SET_DEFAULT_BUFFER_DATA_SPACE,
    SET_CONSUMER_USAGE_BITS,
    SET_TRANSFORM_HINT,
    GET_SIDEBAND_STREAM,
    DISCARD_FREE_BUFFERS,
    DUMP,
};


class BpGraphicBufferConsumer : public BpInterface<IGraphicBufferConsumer>
{
public:
    BpGraphicBufferConsumer(const sp<IBinder>& impl)
        : BpInterface<IGraphicBufferConsumer>(impl)
    {
    }

    virtual ~BpGraphicBufferConsumer();

    virtual status_t acquireBuffer(BufferItem *buffer, nsecs_t presentWhen,
            uint64_t maxFrameNumber) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt64(presentWhen);
        data.writeUint64(maxFrameNumber);
        status_t result = remote()->transact(ACQUIRE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.read(*buffer);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t detachBuffer(int slot) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(slot);
        status_t result = remote()->transact(DETACH_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.write(*buffer.get());
        status_t result = remote()->transact(ATTACH_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        *slot = reply.readInt32();
        result = reply.readInt32();
        return result;
    }

    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
            EGLDisplay display __attribute__((unused)), EGLSyncKHR fence __attribute__((unused)),
            const sp<Fence>& releaseFence) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(buf);
        data.writeInt64(static_cast<int64_t>(frameNumber));
        data.write(*releaseFence);
        status_t result = remote()->transact(RELEASE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeStrongBinder(IInterface::asBinder(consumer));
        data.writeInt32(controlledByApp);
        status_t result = remote()->transact(CONSUMER_CONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t consumerDisconnect() {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        status_t result = remote()->transact(CONSUMER_DISCONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t getReleasedBuffers(uint64_t* slotMask) {
        Parcel data, reply;
        if (slotMask == NULL) {
            ALOGE("getReleasedBuffers: slotMask must not be NULL");
            return BAD_VALUE;
        }
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        status_t result = remote()->transact(GET_RELEASED_BUFFERS, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        *slotMask = static_cast<uint64_t>(reply.readInt64());
        return reply.readInt32();
    }

    virtual status_t setDefaultBufferSize(uint32_t width, uint32_t height) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeUint32(width);
        data.writeUint32(height);
        status_t result = remote()->transact(SET_DEFAULT_BUFFER_SIZE, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setMaxBufferCount(int bufferCount) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(bufferCount);
        status_t result = remote()->transact(SET_MAX_BUFFER_COUNT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(maxAcquiredBuffers);
        status_t result = remote()->transact(SET_MAX_ACQUIRED_BUFFER_COUNT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual void setConsumerName(const String8& name) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeString8(name);
        remote()->transact(SET_CONSUMER_NAME, data, &reply);
    }

    virtual status_t setDefaultBufferFormat(PixelFormat defaultFormat) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(static_cast<int32_t>(defaultFormat));
        status_t result = remote()->transact(SET_DEFAULT_BUFFER_FORMAT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setDefaultBufferDataSpace(
            android_dataspace defaultDataSpace) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeInt32(static_cast<int32_t>(defaultDataSpace));
        status_t result = remote()->transact(SET_DEFAULT_BUFFER_DATA_SPACE,
                data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setConsumerUsageBits(uint32_t usage) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeUint32(usage);
        status_t result = remote()->transact(SET_CONSUMER_USAGE_BITS, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t setTransformHint(uint32_t hint) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeUint32(hint);
        status_t result = remote()->transact(SET_TRANSFORM_HINT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        return reply.readInt32();
    }

    virtual sp<NativeHandle> getSidebandStream() const {
        Parcel data, reply;
        status_t err;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        if ((err = remote()->transact(GET_SIDEBAND_STREAM, data, &reply)) != NO_ERROR) {
            return NULL;
        }
        sp<NativeHandle> stream;
        if (reply.readInt32()) {
            stream = NativeHandle::create(reply.readNativeHandle(), true);
        }
        return stream;
    }

    virtual status_t discardFreeBuffers() {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        status_t error = remote()->transact(DISCARD_FREE_BUFFERS, data, &reply);
        if (error != NO_ERROR) {
            return error;
        }
        int32_t result = NO_ERROR;
        error = reply.readInt32(&result);
        if (error != NO_ERROR) {
            return error;
        }
        return result;
    }

    virtual void dumpState(String8& result, const char* prefix) const {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferConsumer::getInterfaceDescriptor());
        data.writeString8(result);
        data.writeString8(String8(prefix ? prefix : ""));
        remote()->transact(DUMP, data, &reply);
        reply.readString8();
    }
};

// Out-of-line virtual method definition to trigger vtable emission in this
// translation unit (see clang warning -Wweak-vtables)
BpGraphicBufferConsumer::~BpGraphicBufferConsumer() {}

IMPLEMENT_META_INTERFACE(GraphicBufferConsumer, "android.gui.IGraphicBufferConsumer");

// ----------------------------------------------------------------------

status_t BnGraphicBufferConsumer::onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case ACQUIRE_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            BufferItem item;
            int64_t presentWhen = data.readInt64();
            uint64_t maxFrameNumber = data.readUint64();
            status_t result = acquireBuffer(&item, presentWhen, maxFrameNumber);
            status_t err = reply->write(item);
            if (err) return err;
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case DETACH_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            int slot = data.readInt32();
            int result = detachBuffer(slot);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case ATTACH_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            sp<GraphicBuffer> buffer = new GraphicBuffer();
            data.read(*buffer.get());
            int slot = -1;
            int result = attachBuffer(&slot, buffer);
            reply->writeInt32(slot);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case RELEASE_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            int buf = data.readInt32();
            uint64_t frameNumber = static_cast<uint64_t>(data.readInt64());
            sp<Fence> releaseFence = new Fence();
            status_t err = data.read(*releaseFence);
            if (err) return err;
            status_t result = releaseBuffer(buf, frameNumber,
                    EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, releaseFence);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case CONSUMER_CONNECT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            sp<IConsumerListener> consumer = IConsumerListener::asInterface( data.readStrongBinder() );
            bool controlledByApp = data.readInt32();
            status_t result = consumerConnect(consumer, controlledByApp);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case CONSUMER_DISCONNECT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            status_t result = consumerDisconnect();
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case GET_RELEASED_BUFFERS: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            uint64_t slotMask = 0;
            status_t result = getReleasedBuffers(&slotMask);
            reply->writeInt64(static_cast<int64_t>(slotMask));
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_DEFAULT_BUFFER_SIZE: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            uint32_t width = data.readUint32();
            uint32_t height = data.readUint32();
            status_t result = setDefaultBufferSize(width, height);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_MAX_BUFFER_COUNT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            int bufferCount = data.readInt32();
            status_t result = setMaxBufferCount(bufferCount);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_MAX_ACQUIRED_BUFFER_COUNT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            int maxAcquiredBuffers = data.readInt32();
            status_t result = setMaxAcquiredBufferCount(maxAcquiredBuffers);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_CONSUMER_NAME: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            setConsumerName( data.readString8() );
            return NO_ERROR;
        }
        case SET_DEFAULT_BUFFER_FORMAT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            PixelFormat defaultFormat = static_cast<PixelFormat>(data.readInt32());
            status_t result = setDefaultBufferFormat(defaultFormat);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_DEFAULT_BUFFER_DATA_SPACE: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            android_dataspace defaultDataSpace =
                    static_cast<android_dataspace>(data.readInt32());
            status_t result = setDefaultBufferDataSpace(defaultDataSpace);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_CONSUMER_USAGE_BITS: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            uint32_t usage = data.readUint32();
            status_t result = setConsumerUsageBits(usage);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_TRANSFORM_HINT: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            uint32_t hint = data.readUint32();
            status_t result = setTransformHint(hint);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case GET_SIDEBAND_STREAM: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            sp<NativeHandle> stream = getSidebandStream();
            reply->writeInt32(static_cast<int32_t>(stream != NULL));
            if (stream != NULL) {
                reply->writeNativeHandle(stream->handle());
            }
            return NO_ERROR;
        }
        case DISCARD_FREE_BUFFERS: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            status_t result = discardFreeBuffers();
            status_t error = reply->writeInt32(result);
            return error;
        }
        case DUMP: {
            CHECK_INTERFACE(IGraphicBufferConsumer, data, reply);
            String8 result = data.readString8();
            String8 prefix = data.readString8();
            static_cast<IGraphicBufferConsumer*>(this)->dumpState(result, prefix);
            reply->writeString8(result);
            return NO_ERROR;
        }
    }
    return BBinder::onTransact(code, data, reply, flags);
}

}; // namespace android
