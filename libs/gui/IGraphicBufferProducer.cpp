/*
 * Copyright (C) 2010 The Android Open Source Project
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
#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/Vector.h>

#include <binder/Parcel.h>
#include <binder/IInterface.h>

#include <gui/IGraphicBufferProducer.h>
#include <gui/IProducerListener.h>

namespace android {
// ----------------------------------------------------------------------------

enum {
    REQUEST_BUFFER = IBinder::FIRST_CALL_TRANSACTION,
    DEQUEUE_BUFFER,
    DETACH_BUFFER,
    DETACH_NEXT_BUFFER,
    ATTACH_BUFFER,
    QUEUE_BUFFER,
    CANCEL_BUFFER,
    QUERY,
    CONNECT,
    DISCONNECT,
    SET_SIDEBAND_STREAM,
    ALLOCATE_BUFFERS,
    ALLOW_ALLOCATION,
    SET_GENERATION_NUMBER,
    GET_CONSUMER_NAME,
    SET_MAX_DEQUEUED_BUFFER_COUNT,
    SET_ASYNC_MODE,
    SET_SHARED_BUFFER_MODE,
    SET_AUTO_REFRESH,
    SET_DEQUEUE_TIMEOUT,
    GET_LAST_QUEUED_BUFFER,
    GET_FRAME_TIMESTAMPS,
    GET_UNIQUE_ID
};

class BpGraphicBufferProducer : public BpInterface<IGraphicBufferProducer>
{
public:
    BpGraphicBufferProducer(const sp<IBinder>& impl)
        : BpInterface<IGraphicBufferProducer>(impl)
    {
    }

    virtual ~BpGraphicBufferProducer();

    virtual status_t requestBuffer(int bufferIdx, sp<GraphicBuffer>* buf) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(bufferIdx);
        status_t result =remote()->transact(REQUEST_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        bool nonNull = reply.readInt32();
        if (nonNull) {
            *buf = new GraphicBuffer();
            result = reply.read(**buf);
            if(result != NO_ERROR) {
                (*buf).clear();
                return result;
            }
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t setMaxDequeuedBufferCount(int maxDequeuedBuffers) {
        Parcel data, reply;
        data.writeInterfaceToken(
                IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(maxDequeuedBuffers);
        status_t result = remote()->transact(SET_MAX_DEQUEUED_BUFFER_COUNT,
                data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t setAsyncMode(bool async) {
        Parcel data, reply;
        data.writeInterfaceToken(
                IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(async);
        status_t result = remote()->transact(SET_ASYNC_MODE,
                data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t dequeueBuffer(int *buf, sp<Fence>* fence, uint32_t width,
            uint32_t height, PixelFormat format, uint32_t usage) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeUint32(width);
        data.writeUint32(height);
        data.writeInt32(static_cast<int32_t>(format));
        data.writeUint32(usage);
        status_t result = remote()->transact(DEQUEUE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        *buf = reply.readInt32();
        bool nonNull = reply.readInt32();
        if (nonNull) {
            *fence = new Fence();
            result = reply.read(**fence);
            if (result != NO_ERROR) {
                fence->clear();
                return result;
            }
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t detachBuffer(int slot) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(slot);
        status_t result = remote()->transact(DETACH_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence) {
        if (outBuffer == NULL) {
            ALOGE("detachNextBuffer: outBuffer must not be NULL");
            return BAD_VALUE;
        } else if (outFence == NULL) {
            ALOGE("detachNextBuffer: outFence must not be NULL");
            return BAD_VALUE;
        }
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        status_t result = remote()->transact(DETACH_NEXT_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        if (result == NO_ERROR) {
            bool nonNull = reply.readInt32();
            if (nonNull) {
                *outBuffer = new GraphicBuffer;
                result = reply.read(**outBuffer);
                if (result != NO_ERROR) {
                    outBuffer->clear();
                    return result;
                }
            }
            nonNull = reply.readInt32();
            if (nonNull) {
                *outFence = new Fence;
                result = reply.read(**outFence);
                if (result != NO_ERROR) {
                    outBuffer->clear();
                    outFence->clear();
                    return result;
                }
            }
        }
        return result;
    }

    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.write(*buffer.get());
        status_t result = remote()->transact(ATTACH_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        *slot = reply.readInt32();
        result = reply.readInt32();
        return result;
    }

    virtual status_t queueBuffer(int buf,
            const QueueBufferInput& input, QueueBufferOutput* output) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(buf);
        data.write(input);
        status_t result = remote()->transact(QUEUE_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        memcpy(output, reply.readInplace(sizeof(*output)), sizeof(*output));
        result = reply.readInt32();
        return result;
    }

    virtual status_t cancelBuffer(int buf, const sp<Fence>& fence) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(buf);
        data.write(*fence.get());
        status_t result = remote()->transact(CANCEL_BUFFER, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual int query(int what, int* value) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(what);
        status_t result = remote()->transact(QUERY, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        value[0] = reply.readInt32();
        result = reply.readInt32();
        return result;
    }

    virtual status_t connect(const sp<IProducerListener>& listener,
            int api, bool producerControlledByApp, QueueBufferOutput* output) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        if (listener != NULL) {
            data.writeInt32(1);
            data.writeStrongBinder(IInterface::asBinder(listener));
        } else {
            data.writeInt32(0);
        }
        data.writeInt32(api);
        data.writeInt32(producerControlledByApp);
        status_t result = remote()->transact(CONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        memcpy(output, reply.readInplace(sizeof(*output)), sizeof(*output));
        result = reply.readInt32();
        return result;
    }

    virtual status_t disconnect(int api, DisconnectMode mode) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(api);
        data.writeInt32(static_cast<int32_t>(mode));
        status_t result =remote()->transact(DISCONNECT, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t setSidebandStream(const sp<NativeHandle>& stream) {
        Parcel data, reply;
        status_t result;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        if (stream.get()) {
            data.writeInt32(true);
            data.writeNativeHandle(stream->handle());
        } else {
            data.writeInt32(false);
        }
        if ((result = remote()->transact(SET_SIDEBAND_STREAM, data, &reply)) == NO_ERROR) {
            result = reply.readInt32();
        }
        return result;
    }

    virtual void allocateBuffers(uint32_t width, uint32_t height,
            PixelFormat format, uint32_t usage) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeUint32(width);
        data.writeUint32(height);
        data.writeInt32(static_cast<int32_t>(format));
        data.writeUint32(usage);
        status_t result = remote()->transact(ALLOCATE_BUFFERS, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("allocateBuffers failed to transact: %d", result);
        }
    }

    virtual status_t allowAllocation(bool allow) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(static_cast<int32_t>(allow));
        status_t result = remote()->transact(ALLOW_ALLOCATION, data, &reply);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readInt32();
        return result;
    }

    virtual status_t setGenerationNumber(uint32_t generationNumber) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeUint32(generationNumber);
        status_t result = remote()->transact(SET_GENERATION_NUMBER, data, &reply);
        if (result == NO_ERROR) {
            result = reply.readInt32();
        }
        return result;
    }

    virtual String8 getConsumerName() const {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        status_t result = remote()->transact(GET_CONSUMER_NAME, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getConsumerName failed to transact: %d", result);
            return String8("TransactFailed");
        }
        return reply.readString8();
    }

    virtual status_t setSharedBufferMode(bool sharedBufferMode) {
        Parcel data, reply;
        data.writeInterfaceToken(
                IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(sharedBufferMode);
        status_t result = remote()->transact(SET_SHARED_BUFFER_MODE, data,
                &reply);
        if (result == NO_ERROR) {
            result = reply.readInt32();
        }
        return result;
    }

    virtual status_t setAutoRefresh(bool autoRefresh) {
        Parcel data, reply;
        data.writeInterfaceToken(
                IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt32(autoRefresh);
        status_t result = remote()->transact(SET_AUTO_REFRESH, data, &reply);
        if (result == NO_ERROR) {
            result = reply.readInt32();
        }
        return result;
    }

    virtual status_t setDequeueTimeout(nsecs_t timeout) {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        data.writeInt64(timeout);
        status_t result = remote()->transact(SET_DEQUEUE_TIMEOUT, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("setDequeueTimeout failed to transact: %d", result);
            return result;
        }
        return reply.readInt32();
    }

    virtual status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence, float outTransformMatrix[16]) override {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        status_t result = remote()->transact(GET_LAST_QUEUED_BUFFER, data,
                &reply);
        if (result != NO_ERROR) {
            ALOGE("getLastQueuedBuffer failed to transact: %d", result);
            return result;
        }
        result = reply.readInt32();
        if (result != NO_ERROR) {
            return result;
        }
        bool hasBuffer = reply.readBool();
        sp<GraphicBuffer> buffer;
        if (hasBuffer) {
            buffer = new GraphicBuffer();
            result = reply.read(*buffer);
            if (result == NO_ERROR) {
                result = reply.read(outTransformMatrix, sizeof(float) * 16);
            }
        }
        if (result != NO_ERROR) {
            ALOGE("getLastQueuedBuffer failed to read buffer: %d", result);
            return result;
        }
        sp<Fence> fence(new Fence);
        result = reply.read(*fence);
        if (result != NO_ERROR) {
            ALOGE("getLastQueuedBuffer failed to read fence: %d", result);
            return result;
        }
        *outBuffer = buffer;
        *outFence = fence;
        return result;
    }

    virtual bool getFrameTimestamps(uint64_t frameNumber,
                FrameTimestamps* outTimestamps) const {
        Parcel data, reply;
        status_t result = data.writeInterfaceToken(
                IGraphicBufferProducer::getInterfaceDescriptor());
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

    virtual status_t getUniqueId(uint64_t* outId) const {
        Parcel data, reply;
        data.writeInterfaceToken(IGraphicBufferProducer::getInterfaceDescriptor());
        status_t result = remote()->transact(GET_UNIQUE_ID, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getUniqueId failed to transact: %d", result);
        }
        status_t actualResult = NO_ERROR;
        result = reply.readInt32(&actualResult);
        if (result != NO_ERROR) {
            return result;
        }
        result = reply.readUint64(outId);
        if (result != NO_ERROR) {
            return result;
        }
        return actualResult;
    }
};

// Out-of-line virtual method definition to trigger vtable emission in this
// translation unit (see clang warning -Wweak-vtables)
BpGraphicBufferProducer::~BpGraphicBufferProducer() {}

IMPLEMENT_META_INTERFACE(GraphicBufferProducer, "android.gui.IGraphicBufferProducer");

// ----------------------------------------------------------------------

status_t BnGraphicBufferProducer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case REQUEST_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int bufferIdx   = data.readInt32();
            sp<GraphicBuffer> buffer;
            int result = requestBuffer(bufferIdx, &buffer);
            reply->writeInt32(buffer != 0);
            if (buffer != 0) {
                reply->write(*buffer);
            }
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_MAX_DEQUEUED_BUFFER_COUNT: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int maxDequeuedBuffers = data.readInt32();
            int result = setMaxDequeuedBufferCount(maxDequeuedBuffers);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_ASYNC_MODE: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            bool async = data.readInt32();
            int result = setAsyncMode(async);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case DEQUEUE_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            uint32_t width = data.readUint32();
            uint32_t height = data.readUint32();
            PixelFormat format = static_cast<PixelFormat>(data.readInt32());
            uint32_t usage = data.readUint32();
            int buf = 0;
            sp<Fence> fence;
            int result = dequeueBuffer(&buf, &fence, width, height, format,
                    usage);
            reply->writeInt32(buf);
            reply->writeInt32(fence != NULL);
            if (fence != NULL) {
                reply->write(*fence);
            }
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case DETACH_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int slot = data.readInt32();
            int result = detachBuffer(slot);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case DETACH_NEXT_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            sp<GraphicBuffer> buffer;
            sp<Fence> fence;
            int32_t result = detachNextBuffer(&buffer, &fence);
            reply->writeInt32(result);
            if (result == NO_ERROR) {
                reply->writeInt32(buffer != NULL);
                if (buffer != NULL) {
                    reply->write(*buffer);
                }
                reply->writeInt32(fence != NULL);
                if (fence != NULL) {
                    reply->write(*fence);
                }
            }
            return NO_ERROR;
        }
        case ATTACH_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            sp<GraphicBuffer> buffer = new GraphicBuffer();
            status_t result = data.read(*buffer.get());
            int slot = 0;
            if (result == NO_ERROR) {
                result = attachBuffer(&slot, buffer);
            }
            reply->writeInt32(slot);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case QUEUE_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int buf = data.readInt32();
            QueueBufferInput input(data);
            QueueBufferOutput* const output =
                    reinterpret_cast<QueueBufferOutput *>(
                            reply->writeInplace(sizeof(QueueBufferOutput)));
            memset(output, 0, sizeof(QueueBufferOutput));
            status_t result = queueBuffer(buf, input, output);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case CANCEL_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int buf = data.readInt32();
            sp<Fence> fence = new Fence();
            status_t result = data.read(*fence.get());
            if (result == NO_ERROR) {
                result = cancelBuffer(buf, fence);
            }
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case QUERY: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int value = 0;
            int what = data.readInt32();
            int res = query(what, &value);
            reply->writeInt32(value);
            reply->writeInt32(res);
            return NO_ERROR;
        }
        case CONNECT: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            sp<IProducerListener> listener;
            if (data.readInt32() == 1) {
                listener = IProducerListener::asInterface(data.readStrongBinder());
            }
            int api = data.readInt32();
            bool producerControlledByApp = data.readInt32();
            QueueBufferOutput* const output =
                    reinterpret_cast<QueueBufferOutput *>(
                            reply->writeInplace(sizeof(QueueBufferOutput)));
            memset(output, 0, sizeof(QueueBufferOutput));
            status_t res = connect(listener, api, producerControlledByApp, output);
            reply->writeInt32(res);
            return NO_ERROR;
        }
        case DISCONNECT: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            int api = data.readInt32();
            DisconnectMode mode = static_cast<DisconnectMode>(data.readInt32());
            status_t res = disconnect(api, mode);
            reply->writeInt32(res);
            return NO_ERROR;
        }
        case SET_SIDEBAND_STREAM: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            sp<NativeHandle> stream;
            if (data.readInt32()) {
                stream = NativeHandle::create(data.readNativeHandle(), true);
            }
            status_t result = setSidebandStream(stream);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case ALLOCATE_BUFFERS: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            uint32_t width = data.readUint32();
            uint32_t height = data.readUint32();
            PixelFormat format = static_cast<PixelFormat>(data.readInt32());
            uint32_t usage = data.readUint32();
            allocateBuffers(width, height, format, usage);
            return NO_ERROR;
        }
        case ALLOW_ALLOCATION: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            bool allow = static_cast<bool>(data.readInt32());
            status_t result = allowAllocation(allow);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_GENERATION_NUMBER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            uint32_t generationNumber = data.readUint32();
            status_t result = setGenerationNumber(generationNumber);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case GET_CONSUMER_NAME: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            reply->writeString8(getConsumerName());
            return NO_ERROR;
        }
        case SET_SHARED_BUFFER_MODE: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            bool sharedBufferMode = data.readInt32();
            status_t result = setSharedBufferMode(sharedBufferMode);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_AUTO_REFRESH: {
            CHECK_INTERFACE(IGraphicBuffer, data, reply);
            bool autoRefresh = data.readInt32();
            status_t result = setAutoRefresh(autoRefresh);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_DEQUEUE_TIMEOUT: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            nsecs_t timeout = data.readInt64();
            status_t result = setDequeueTimeout(timeout);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case GET_LAST_QUEUED_BUFFER: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            sp<GraphicBuffer> buffer(nullptr);
            sp<Fence> fence(Fence::NO_FENCE);
            float transform[16] = {};
            status_t result = getLastQueuedBuffer(&buffer, &fence, transform);
            reply->writeInt32(result);
            if (result != NO_ERROR) {
                return result;
            }
            if (!buffer.get()) {
                reply->writeBool(false);
            } else {
                reply->writeBool(true);
                result = reply->write(*buffer);
                if (result == NO_ERROR) {
                    reply->write(transform, sizeof(float) * 16);
                }
            }
            if (result != NO_ERROR) {
                ALOGE("getLastQueuedBuffer failed to write buffer: %d", result);
                return result;
            }
            result = reply->write(*fence);
            if (result != NO_ERROR) {
                ALOGE("getLastQueuedBuffer failed to write fence: %d", result);
                return result;
            }
            return NO_ERROR;
        }
        case GET_FRAME_TIMESTAMPS: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
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
        case GET_UNIQUE_ID: {
            CHECK_INTERFACE(IGraphicBufferProducer, data, reply);
            uint64_t outId = 0;
            status_t actualResult = getUniqueId(&outId);
            status_t result = reply->writeInt32(actualResult);
            if (result != NO_ERROR) {
                return result;
            }
            result = reply->writeUint64(outId);
            if (result != NO_ERROR) {
                return result;
            }
            return NO_ERROR;
        }
    }
    return BBinder::onTransact(code, data, reply, flags);
}

// ----------------------------------------------------------------------------

IGraphicBufferProducer::QueueBufferInput::QueueBufferInput(const Parcel& parcel) {
    parcel.read(*this);
}

size_t IGraphicBufferProducer::QueueBufferInput::getFlattenedSize() const {
    return sizeof(timestamp)
         + sizeof(isAutoTimestamp)
         + sizeof(dataSpace)
         + sizeof(crop)
         + sizeof(scalingMode)
         + sizeof(transform)
         + sizeof(stickyTransform)
         + fence->getFlattenedSize()
         + surfaceDamage.getFlattenedSize();
}

size_t IGraphicBufferProducer::QueueBufferInput::getFdCount() const {
    return fence->getFdCount();
}

status_t IGraphicBufferProducer::QueueBufferInput::flatten(
        void*& buffer, size_t& size, int*& fds, size_t& count) const
{
    if (size < getFlattenedSize()) {
        return NO_MEMORY;
    }
    FlattenableUtils::write(buffer, size, timestamp);
    FlattenableUtils::write(buffer, size, isAutoTimestamp);
    FlattenableUtils::write(buffer, size, dataSpace);
    FlattenableUtils::write(buffer, size, crop);
    FlattenableUtils::write(buffer, size, scalingMode);
    FlattenableUtils::write(buffer, size, transform);
    FlattenableUtils::write(buffer, size, stickyTransform);
    status_t result = fence->flatten(buffer, size, fds, count);
    if (result != NO_ERROR) {
        return result;
    }
    return surfaceDamage.flatten(buffer, size);
}

status_t IGraphicBufferProducer::QueueBufferInput::unflatten(
        void const*& buffer, size_t& size, int const*& fds, size_t& count)
{
    size_t minNeeded =
              sizeof(timestamp)
            + sizeof(isAutoTimestamp)
            + sizeof(dataSpace)
            + sizeof(crop)
            + sizeof(scalingMode)
            + sizeof(transform)
            + sizeof(stickyTransform);

    if (size < minNeeded) {
        return NO_MEMORY;
    }

    FlattenableUtils::read(buffer, size, timestamp);
    FlattenableUtils::read(buffer, size, isAutoTimestamp);
    FlattenableUtils::read(buffer, size, dataSpace);
    FlattenableUtils::read(buffer, size, crop);
    FlattenableUtils::read(buffer, size, scalingMode);
    FlattenableUtils::read(buffer, size, transform);
    FlattenableUtils::read(buffer, size, stickyTransform);

    fence = new Fence();
    status_t result = fence->unflatten(buffer, size, fds, count);
    if (result != NO_ERROR) {
        return result;
    }
    return surfaceDamage.unflatten(buffer, size);
}

}; // namespace android
