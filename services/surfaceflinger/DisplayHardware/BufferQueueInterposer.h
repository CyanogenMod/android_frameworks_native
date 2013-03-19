/*
 * Copyright 2013 The Android Open Source Project
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

#ifndef ANDROID_SF_BUFFERQUEUEINTERPOSER_H
#define ANDROID_SF_BUFFERQUEUEINTERPOSER_H

#include <gui/IGraphicBufferProducer.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

// BufferQueueInterposers introduce an extra stage between a buffer producer
// (the source) and a buffer consumer (the sink), which communicate via the
// IGraphicBufferProducer interface. It is designed to be as transparent as
// possible to both endpoints, so that they can work the same whether an
// interposer is present or not.
//
// When the interpose is present, the source queues buffers to the
// IGraphicBufferProducer implemented by BufferQueueInterposer. A client of
// the BufferQueueInterposer can acquire each buffer in turn and read or
// modify it, releasing the buffer when finished. When the buffer is released,
// the BufferQueueInterposer queues it to the original IGraphicBufferProducer
// interface representing the sink.
//
// A BufferQueueInterposer can be used to do additional rendering to a buffer
// before it is consumed -- essentially pipelining two producers. As an
// example, SurfaceFlinger uses this to implement mixed GLES and HWC
// compositing to the same buffer for virtual displays. If it used two separate
// buffer queues, then in GLES-only or mixed GLES+HWC compositing, the HWC
// would have to copy the GLES output buffer to the HWC output buffer, using
// more bandwidth than having HWC do additional composition "in place" on the
// GLES output buffer.
//
// The goal for this class is to be usable in a variety of situations and be
// part of libgui. But both the interface and implementation need some
// iteration before then, so for now it should only be used by
// VirtualDisplaySurface, which is why it's currently in SurfaceFlinger.
//
// Some of the problems that still need to be solved are:
//
// - Refactor the interposer interface along with BufferQueue and ConsumerBase,
//   so that there is a common interface for the consumer end of a queue. The
//   existing interfaces have some problems when the implementation isn't the
//   final consumer.
//
// - The interposer needs at least one buffer in addition to those used by the
//   source and sink. setBufferCount and QueueBufferOutput both need to
//   account for this. It's not possible currently to do this generically,
//   since we can't find out how many buffers the source and sink need. (See
//   the horrible hack in the BufferQueueInterposer constructor).
//
// - Abandoning, disconnecting, and connecting need to pass through somehow.
//   There needs to be a way to tell the interposer client to release its
//   buffer immediately so it can be queued/released, e.g. when the source
//   calls disconnect().
//
// - Right now the source->BQI queue is synchronous even if the BQI->sink
//   queue is asynchronous. Need to figure out how asynchronous should behave
//   and implement that.

class BufferQueueInterposer : public BnGraphicBufferProducer {
public:
    BufferQueueInterposer(const sp<IGraphicBufferProducer>& sink,
            const String8& name);

    //
    // IGraphicBufferProducer interface
    //
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* outBuf);
    virtual status_t setBufferCount(int bufferCount);
    virtual status_t dequeueBuffer(int* slot, sp<Fence>* fence,
            uint32_t w, uint32_t h, uint32_t format, uint32_t usage);
    virtual status_t queueBuffer(int slot,
            const QueueBufferInput& input, QueueBufferOutput* output);
    virtual void cancelBuffer(int slot, const sp<Fence>& fence);
    virtual int query(int what, int* value);
    virtual status_t setSynchronousMode(bool enabled);
    virtual status_t connect(int api, QueueBufferOutput* output);
    virtual status_t disconnect(int api);

    //
    // Interposer interface
    //

    enum {
        NO_BUFFER_AVAILABLE = 2,    // matches BufferQueue
        BUFFER_NOT_ACQUIRED,
        BUFFER_ALREADY_ACQUIRED,
    };

    // Acquire the oldest queued buffer. If no buffers are pending, returns
    // NO_BUFFER_AVAILABLE. If a buffer is currently acquired, returns
    // BUFFER_ALREADY_ACQUIRED.
    status_t acquireBuffer(sp<GraphicBuffer>* buf, sp<Fence>* fence);

    // Release the currently acquired buffer, queueing it to the sink. If the
    // current buffer hasn't been acquired, returns BUFFER_NOT_ACQUIRED.
    status_t releaseBuffer(const sp<Fence>& fence);

    // pullEmptyBuffer dequeues a buffer from the sink, then immediately
    // queues it to the interposer. This makes a buffer available for the
    // client to acquire even if the source hasn't queued one.
    status_t pullEmptyBuffer();

private:
    struct QueuedBuffer {
        QueuedBuffer(): slot(-1) {}
        QueuedBuffer(int slot, const QueueBufferInput& qbi): slot(slot) {
            qbi.deflate(&timestamp, &crop, &scalingMode, &transform, &fence);
        }
        int slot;
        int64_t timestamp;
        Rect crop;
        int scalingMode;
        uint32_t transform;
        sp<Fence> fence;
    };

    virtual ~BufferQueueInterposer();
    status_t flushQueuedBuffersLocked();

    const sp<IGraphicBufferProducer> mSink;
    String8 mName;

    Mutex mMutex;
    Vector<sp<GraphicBuffer> > mBuffers;
    Vector<QueuedBuffer> mQueue;
    bool mAcquired;
    QueueBufferOutput mQueueBufferOutput;
};

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_BUFFERQUEUEINTERPOSER_H
