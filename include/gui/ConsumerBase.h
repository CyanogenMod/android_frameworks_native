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

#ifndef ANDROID_GUI_CONSUMERBASE_H
#define ANDROID_GUI_CONSUMERBASE_H

#include <gui/BufferQueue.h>

#include <ui/GraphicBuffer.h>

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

namespace android {
// ----------------------------------------------------------------------------

class String8;

// ConsumerBase is a base class for BufferQueue consumer end-points. It
// handles common tasks like management of the connection to the BufferQueue
// and the buffer pool.
class ConsumerBase : public virtual RefBase,
        protected BufferQueue::ConsumerListener {
public:
    struct FrameAvailableListener : public virtual RefBase {
        // onFrameAvailable() is called each time an additional frame becomes
        // available for consumption. This means that frames that are queued
        // while in asynchronous mode only trigger the callback if no previous
        // frames are pending. Frames queued while in synchronous mode always
        // trigger the callback.
        //
        // This is called without any lock held and can be called concurrently
        // by multiple threads.
        virtual void onFrameAvailable() = 0;
    };

    virtual ~ConsumerBase();

    // abandon frees all the buffers and puts the ConsumerBase into the
    // 'abandoned' state.  Once put in this state the ConsumerBase can never
    // leave it.  When in the 'abandoned' state, all methods of the
    // ISurfaceTexture interface will fail with the NO_INIT error.
    //
    // Note that while calling this method causes all the buffers to be freed
    // from the perspective of the the ConsumerBase, if there are additional
    // references on the buffers (e.g. if a buffer is referenced by a client
    // or by OpenGL ES as a texture) then those buffer will remain allocated.
    void abandon();

    // set the name of the ConsumerBase that will be used to identify it in
    // log messages.
    void setName(const String8& name);

    // getBufferQueue returns the BufferQueue object to which this
    // ConsumerBase is connected.
    sp<BufferQueue> getBufferQueue() const;

    // dump writes the current state to a string.  These methods should NOT be
    // overridden by child classes.  Instead they should override the
    // dumpLocked method, which is called by these methods after locking the
    // mutex.
    void dump(String8& result) const;
    void dump(String8& result, const char* prefix, char* buffer, size_t SIZE) const;

    // setFrameAvailableListener sets the listener object that will be notified
    // when a new frame becomes available.
    void setFrameAvailableListener(const sp<FrameAvailableListener>& listener);

private:
    ConsumerBase(const ConsumerBase&);
    void operator=(const ConsumerBase&);

protected:

    // TODO: Fix this comment
    // ConsumerBase constructs a new ConsumerBase object. tex indicates the
    // name of the OpenGL ES texture to which images are to be streamed.
    // allowSynchronousMode specifies whether or not synchronous mode can be
    // enabled. texTarget specifies the OpenGL ES texture target to which the
    // texture will be bound in updateTexImage. useFenceSync specifies whether
    // fences should be used to synchronize access to buffers if that behavior
    // is enabled at compile-time. A custom bufferQueue can be specified
    // if behavior for queue/dequeue/connect etc needs to be customized.
    // Otherwise a default BufferQueue will be created and used.
    //
    // For legacy reasons, the ConsumerBase is created in a state where it is
    // considered attached to an OpenGL ES context for the purposes of the
    // attachToContext and detachFromContext methods. However, despite being
    // considered "attached" to a context, the specific OpenGL ES context
    // doesn't get latched until the first call to updateTexImage. After that
    // point, all calls to updateTexImage must be made with the same OpenGL ES
    // context current.
    //
    // A ConsumerBase may be detached from one OpenGL ES context and then
    // attached to a different context using the detachFromContext and
    // attachToContext methods, respectively. The intention of these methods is
    // purely to allow a ConsumerBase to be transferred from one consumer
    // context to another. If such a transfer is not needed there is no
    // requirement that either of these methods be called.
    ConsumerBase(const sp<BufferQueue> &bufferQueue);

    // Implementation of the BufferQueue::ConsumerListener interface.  These
    // calls are used to notify the ConsumerBase of asynchronous events in the
    // BufferQueue.
    virtual void onFrameAvailable();
    virtual void onBuffersReleased();

    // freeBufferLocked frees up the given buffer slot.  If the slot has been
    // initialized this will release the reference to the GraphicBuffer in that
    // slot and destroy the EGLImage in that slot.  Otherwise it has no effect.
    //
    // This method must be called with mMutex locked.
    virtual void freeBufferLocked(int slotIndex);

    // abandonLocked puts the BufferQueue into the abandoned state, causing
    // all future operations on it to fail. This method rather than the public
    // abandon method should be overridden by child classes to add abandon-
    // time behavior.
    //
    // This method must be called with mMutex locked.
    virtual void abandonLocked();

    virtual void dumpLocked(String8& result, const char* prefix, char* buffer,
            size_t SIZE) const;

    // acquireBufferLocked fetches the next buffer from the BufferQueue and
    // updates the buffer slot for the buffer returned.
    virtual status_t acquireBufferLocked(BufferQueue::BufferItem *item);

    // releaseBufferLocked relinquishes control over a buffer, returning that
    // control to the BufferQueue.
    virtual status_t releaseBufferLocked(int buf, EGLDisplay display,
           EGLSyncKHR eglFence, const sp<Fence>& fence);

    // Slot contains the information and object references that
    // ConsumerBase maintains about a BufferQueue buffer slot.
    struct Slot {
        // mGraphicBuffer is the Gralloc buffer store in the slot or NULL if
        // no Gralloc buffer is in the slot.
        sp<GraphicBuffer> mGraphicBuffer;

        // mFence is a fence which will signal when the buffer associated with
        // this buffer slot is no longer being used by the consumer and can be
        // overwritten. The buffer can be dequeued before the fence signals;
        // the producer is responsible for delaying writes until it signals.
        sp<Fence> mFence;
    };

    // mSlots stores the buffers that have been allocated by the BufferQueue
    // for each buffer slot.  It is initialized to null pointers, and gets
    // filled in with the result of BufferQueue::acquire when the
    // client dequeues a buffer from a
    // slot that has not yet been used. The buffer allocated to a slot will also
    // be replaced if the requested buffer usage or geometry differs from that
    // of the buffer allocated to a slot.
    Slot mSlots[BufferQueue::NUM_BUFFER_SLOTS];

    // mAbandoned indicates that the BufferQueue will no longer be used to
    // consume images buffers pushed to it using the ISurfaceTexture
    // interface. It is initialized to false, and set to true in the abandon
    // method.  A BufferQueue that has been abandoned will return the NO_INIT
    // error from all IConsumerBase methods capable of returning an error.
    bool mAbandoned;

    // mName is a string used to identify the ConsumerBase in log messages.
    // It can be set by the setName method.
    String8 mName;

    // mFrameAvailableListener is the listener object that will be called when a
    // new frame becomes available. If it is not NULL it will be called from
    // queueBuffer.
    sp<FrameAvailableListener> mFrameAvailableListener;

    // The ConsumerBase has-a BufferQueue and is responsible for creating this object
    // if none is supplied
    sp<BufferQueue> mBufferQueue;

    // mAttached indicates whether the ConsumerBase is currently attached to
    // an OpenGL ES context.  For legacy reasons, this is initialized to true,
    // indicating that the ConsumerBase is considered to be attached to
    // whatever context is current at the time of the first updateTexImage call.
    // It is set to false by detachFromContext, and then set to true again by
    // attachToContext.
    bool mAttached;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of ConsumerBase objects. It must be locked whenever the
    // member variables are accessed.
    mutable Mutex mMutex;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_CONSUMERBASE_H
