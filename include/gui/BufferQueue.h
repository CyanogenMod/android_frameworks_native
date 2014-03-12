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

#ifndef ANDROID_GUI_BUFFERQUEUE_H
#define ANDROID_GUI_BUFFERQUEUE_H

#include <gui/BufferQueueProducer.h>
#include <gui/BufferQueueConsumer.h>
#include <gui/IConsumerListener.h>

// These are only required to keep other parts of the framework with incomplete
// dependencies building successfully
#include <gui/IGraphicBufferAlloc.h>

#include <binder/IBinder.h>

namespace android {

// BQProducer and BQConsumer are thin shim classes to allow methods with the
// same signature in both IGraphicBufferProducer and IGraphicBufferConsumer.
// This will stop being an issue when we deprecate creating BufferQueues
// directly (as opposed to using the *Producer and *Consumer interfaces).
class BQProducer : public BnGraphicBufferProducer {
public:
    virtual status_t detachProducerBuffer(int slot) = 0;
    virtual status_t attachProducerBuffer(int* slot,
            const sp<GraphicBuffer>& buffer) = 0;

    virtual status_t detachBuffer(int slot) {
        return detachProducerBuffer(slot);
    }

    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer) {
        return attachProducerBuffer(slot, buffer);
    }
};

class BQConsumer : public BnGraphicBufferConsumer {
public:
    virtual status_t detachConsumerBuffer(int slot) = 0;
    virtual status_t attachConsumerBuffer(int* slot,
            const sp<GraphicBuffer>& buffer) = 0;

    virtual status_t detachBuffer(int slot) {
        return detachConsumerBuffer(slot);
    }

    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer) {
        return attachConsumerBuffer(slot, buffer);
    }
};

class BufferQueue : public BQProducer,
                    public BQConsumer,
                    private IBinder::DeathRecipient {
public:
    // BufferQueue will keep track of at most this value of buffers.
    // Attempts at runtime to increase the number of buffers past this will fail.
    enum { NUM_BUFFER_SLOTS = 32 };
    // Used as a placeholder slot# when the value isn't pointing to an existing buffer.
    enum { INVALID_BUFFER_SLOT = IGraphicBufferConsumer::BufferItem::INVALID_BUFFER_SLOT };
    // Alias to <IGraphicBufferConsumer.h> -- please scope from there in future code!
    enum {
        NO_BUFFER_AVAILABLE = IGraphicBufferConsumer::NO_BUFFER_AVAILABLE,
        PRESENT_LATER = IGraphicBufferConsumer::PRESENT_LATER,
    };

    // When in async mode we reserve two slots in order to guarantee that the
    // producer and consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // for backward source compatibility
    typedef ::android::ConsumerListener ConsumerListener;

    // ProxyConsumerListener is a ConsumerListener implementation that keeps a weak
    // reference to the actual consumer object.  It forwards all calls to that
    // consumer object so long as it exists.
    //
    // This class exists to avoid having a circular reference between the
    // BufferQueue object and the consumer object.  The reason this can't be a weak
    // reference in the BufferQueue class is because we're planning to expose the
    // consumer side of a BufferQueue as a binder interface, which doesn't support
    // weak references.
    class ProxyConsumerListener : public BnConsumerListener {
    public:
        ProxyConsumerListener(const wp<ConsumerListener>& consumerListener);
        virtual ~ProxyConsumerListener();
        virtual void onFrameAvailable();
        virtual void onBuffersReleased();
    private:
        // mConsumerListener is a weak reference to the IConsumerListener.  This is
        // the raison d'etre of ProxyConsumerListener.
        wp<ConsumerListener> mConsumerListener;
    };

    // BufferQueue manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the
    // needed gralloc buffers.
    BufferQueue(const sp<IGraphicBufferAlloc>& allocator = NULL);

    static void createBufferQueue(sp<BnGraphicBufferProducer>* outProducer,
            sp<BnGraphicBufferConsumer>* outConsumer,
            const sp<IGraphicBufferAlloc>& allocator = NULL);

    static void createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
            sp<IGraphicBufferConsumer>* outConsumer,
            const sp<IGraphicBufferAlloc>& allocator = NULL);

    virtual ~BufferQueue();

    /*
     * IBinder::DeathRecipient interface
     */

    virtual void binderDied(const wp<IBinder>& who);

    /*
     * IGraphicBufferProducer interface
     */

    // Query native window attributes.  The "what" values are enumerated in
    // window.h (e.g. NATIVE_WINDOW_FORMAT).
    virtual int query(int what, int* value);

    // setBufferCount updates the number of available buffer slots.  If this
    // method succeeds, buffer slots will be both unallocated and owned by
    // the BufferQueue object (i.e. they are not owned by the producer or
    // consumer).
    //
    // This will fail if the producer has dequeued any buffers, or if
    // bufferCount is invalid.  bufferCount must generally be a value
    // between the minimum undequeued buffer count (exclusive) and NUM_BUFFER_SLOTS
    // (inclusive).  It may also be set to zero (the default) to indicate
    // that the producer does not wish to set a value.  The minimum value
    // can be obtained by calling query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
    // ...).
    //
    // This may only be called by the producer.  The consumer will be told
    // to discard buffers through the onBuffersReleased callback.
    virtual status_t setBufferCount(int bufferCount);

    // requestBuffer returns the GraphicBuffer for slot N.
    //
    // In normal operation, this is called the first time slot N is returned
    // by dequeueBuffer.  It must be called again if dequeueBuffer returns
    // flags indicating that previously-returned buffers are no longer valid.
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);

    // dequeueBuffer gets the next buffer slot index for the producer to use.
    // If a buffer slot is available then that slot index is written to the
    // location pointed to by the buf argument and a status of OK is returned.
    // If no slot is available then a status of -EBUSY is returned and buf is
    // unmodified.
    //
    // The fence parameter will be updated to hold the fence associated with
    // the buffer. The contents of the buffer must not be overwritten until the
    // fence signals. If the fence is Fence::NO_FENCE, the buffer may be
    // written immediately.
    //
    // The width and height parameters must be no greater than the minimum of
    // GL_MAX_VIEWPORT_DIMS and GL_MAX_TEXTURE_SIZE (see: glGetIntegerv).
    // An error due to invalid dimensions might not be reported until
    // updateTexImage() is called.  If width and height are both zero, the
    // default values specified by setDefaultBufferSize() are used instead.
    //
    // The pixel formats are enumerated in graphics.h, e.g.
    // HAL_PIXEL_FORMAT_RGBA_8888.  If the format is 0, the default format
    // will be used.
    //
    // The usage argument specifies gralloc buffer usage flags.  The values
    // are enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER.  These
    // will be merged with the usage flags specified by setConsumerUsageBits.
    //
    // The return value may be a negative error value or a non-negative
    // collection of flags.  If the flags are set, the return values are
    // valid, but additional actions must be performed.
    //
    // If IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION is set, the
    // producer must discard cached GraphicBuffer references for the slot
    // returned in buf.
    // If IGraphicBufferProducer::RELEASE_ALL_BUFFERS is set, the producer
    // must discard cached GraphicBuffer references for all slots.
    //
    // In both cases, the producer will need to call requestBuffer to get a
    // GraphicBuffer handle for the returned slot.
    virtual status_t dequeueBuffer(int *buf, sp<Fence>* fence, bool async,
            uint32_t width, uint32_t height, uint32_t format, uint32_t usage);

    // See IGraphicBufferProducer::detachBuffer
    virtual status_t detachProducerBuffer(int slot);

    // See IGraphicBufferProducer::attachBuffer
    virtual status_t attachProducerBuffer(int* slot,
            const sp<GraphicBuffer>& buffer);

    // queueBuffer returns a filled buffer to the BufferQueue.
    //
    // Additional data is provided in the QueueBufferInput struct.  Notably,
    // a timestamp must be provided for the buffer. The timestamp is in
    // nanoseconds, and must be monotonically increasing. Its other semantics
    // (zero point, etc) are producer-specific and should be documented by the
    // producer.
    //
    // The caller may provide a fence that signals when all rendering
    // operations have completed.  Alternatively, NO_FENCE may be used,
    // indicating that the buffer is ready immediately.
    //
    // Some values are returned in the output struct: the current settings
    // for default width and height, the current transform hint, and the
    // number of queued buffers.
    virtual status_t queueBuffer(int buf,
            const QueueBufferInput& input, QueueBufferOutput* output);

    // cancelBuffer returns a dequeued buffer to the BufferQueue, but doesn't
    // queue it for use by the consumer.
    //
    // The buffer will not be overwritten until the fence signals.  The fence
    // will usually be the one obtained from dequeueBuffer.
    virtual void cancelBuffer(int buf, const sp<Fence>& fence);

    // connect attempts to connect a producer API to the BufferQueue.  This
    // must be called before any other IGraphicBufferProducer methods are
    // called except for getAllocator.  A consumer must already be connected.
    //
    // This method will fail if connect was previously called on the
    // BufferQueue and no corresponding disconnect call was made (i.e. if
    // it's still connected to a producer).
    //
    // APIs are enumerated in window.h (e.g. NATIVE_WINDOW_API_CPU).
    virtual status_t connect(const sp<IBinder>& token,
            int api, bool producerControlledByApp, QueueBufferOutput* output);

    // disconnect attempts to disconnect a producer API from the BufferQueue.
    // Calling this method will cause any subsequent calls to other
    // IGraphicBufferProducer methods to fail except for getAllocator and connect.
    // Successfully calling connect after this will allow the other methods to
    // succeed again.
    //
    // This method will fail if the the BufferQueue is not currently
    // connected to the specified producer API.
    virtual status_t disconnect(int api);

    /*
     * IGraphicBufferConsumer interface
     */

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the BufferQueue.  If no buffer is pending then it returns NO_BUFFER_AVAILABLE. If a
    // buffer is successfully acquired, the information about the buffer is
    // returned in BufferItem.  If the buffer returned had previously been
    // acquired then the BufferItem::mGraphicBuffer field of buffer is set to
    // NULL and it is assumed that the consumer still holds a reference to the
    // buffer.
    //
    // If presentWhen is nonzero, it indicates the time when the buffer will
    // be displayed on screen.  If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    virtual status_t acquireBuffer(BufferItem* buffer, nsecs_t presentWhen);

    // See IGraphicBufferConsumer::detachBuffer
    virtual status_t detachConsumerBuffer(int slot);

    // See IGraphicBufferConsumer::attachBuffer
    virtual status_t attachConsumerBuffer(int* slot,
            const sp<GraphicBuffer>& buffer);

    // releaseBuffer releases a buffer slot from the consumer back to the
    // BufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use. frameNumber is used to indentify the exact buffer returned.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    //
    // Note that the dependencies on EGL will be removed once we switch to using
    // the Android HW Sync HAL.
    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
            EGLDisplay display, EGLSyncKHR fence,
            const sp<Fence>& releaseFence);

    // consumerConnect connects a consumer to the BufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // BufferQueue is placed into the "abandoned" state, causing most
    // interactions with the BufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumer may not be NULL.
    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp);

    // consumerDisconnect disconnects a consumer from the BufferQueue. All
    // buffers will be freed and the BufferQueue is placed in the "abandoned"
    // state, causing most interactions with the BufferQueue by the producer to
    // fail.
    virtual status_t consumerDisconnect();

    // getReleasedBuffers sets the value pointed to by slotMask to a bit mask
    // indicating which buffer slots have been released by the BufferQueue
    // but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    virtual status_t getReleasedBuffers(uint32_t* slotMask);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    virtual status_t setDefaultBufferSize(uint32_t w, uint32_t h);

    // setDefaultMaxBufferCount sets the default value for the maximum buffer
    // count (the initial default is 2). If the producer has requested a
    // buffer count using setBufferCount, the default buffer count will only
    // take effect if the producer sets the count back to zero.
    //
    // The count must be between 2 and NUM_BUFFER_SLOTS, inclusive.
    virtual status_t setDefaultMaxBufferCount(int bufferCount);

    // disableAsyncBuffer disables the extra buffer used in async mode
    // (when both producer and consumer have set their "isControlledByApp"
    // flag) and has dequeueBuffer() return WOULD_BLOCK instead.
    //
    // This can only be called before consumerConnect().
    virtual status_t disableAsyncBuffer();

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the BufferQueue.
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers);

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name);

    // setDefaultBufferFormat allows the BufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.  Formats are enumerated in graphics.h; the
    // initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    virtual status_t setDefaultBufferFormat(uint32_t defaultFormat);

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    virtual status_t setConsumerUsageBits(uint32_t usage);

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    virtual status_t setTransformHint(uint32_t hint);

    // dump our state in a String
    virtual void dump(String8& result, const char* prefix) const;

private:
    sp<BufferQueueProducer> mProducer;
    sp<BufferQueueConsumer> mConsumer;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_BUFFERQUEUE_H
