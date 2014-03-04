/*
 * Copyright 2014 The Android Open Source Project
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

#ifndef ANDROID_GUI_BUFFERQUEUECORE_H
#define ANDROID_GUI_BUFFERQUEUECORE_H

#include <gui/BufferSlot.h>

#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#define BQ_LOGV(x, ...) ALOGV("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGD(x, ...) ALOGD("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGI(x, ...) ALOGI("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGW(x, ...) ALOGW("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGE(x, ...) ALOGE("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)

#define ATRACE_BUFFER_INDEX(index)                                   \
    if (ATRACE_ENABLED()) {                                          \
        char ___traceBuf[1024];                                      \
        snprintf(___traceBuf, 1024, "%s: %d",                        \
                mCore->mConsumerName.string(), (index));             \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);  \
    }

namespace android {

class BufferItem;
class IBinder;
class IConsumerListener;
class IGraphicBufferAlloc;

class BufferQueueCore : public virtual RefBase {

    friend class BufferQueueProducer;
    friend class BufferQueueConsumer;

public:
    // BufferQueue will keep track of at most this value of buffers. Attempts
    // at runtime to increase the number of buffers past this will fail.
    enum { NUM_BUFFER_SLOTS = 32 };

    // Used as a placeholder slot number when the value isn't pointing to an
    // existing buffer.
    enum { INVALID_BUFFER_SLOT = -1 }; // TODO: Extract from IGBC::BufferItem

    // We reserve two slots in order to guarantee that the producer and
    // consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // The default API number used to indicate that no producer is connected
    enum { NO_CONNECTED_API = 0 };

    typedef BufferSlot SlotsType[NUM_BUFFER_SLOTS];
    typedef Vector<BufferItem> Fifo;

    // BufferQueueCore manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the needed
    // gralloc buffers.
    BufferQueueCore(const sp<IGraphicBufferAlloc>& allocator = NULL);
    virtual ~BufferQueueCore();

private:
    void dump(String8& result, const char* prefix) const;

    int getMinUndequeuedBufferCountLocked(bool async) const;
    int getMinMaxBufferCountLocked(bool async) const;
    int getMaxBufferCountLocked(bool async) const;
    status_t setDefaultMaxBufferCountLocked(int count);
    void freeBufferLocked(int slot);
    void freeAllBuffersLocked();
    bool stillTracking(const BufferItem* item) const;

    const sp<IGraphicBufferAlloc>& mAllocator;
    mutable Mutex mMutex;
    bool mIsAbandoned;
    bool mConsumerControlledByApp;
    String8 mConsumerName;
    sp<IConsumerListener> mConsumerListener;
    uint32_t mConsumerUsageBits;
    int mConnectedApi;
    sp<IBinder> mConnectedProducerToken;
    BufferSlot mSlots[NUM_BUFFER_SLOTS];
    Fifo mQueue;
    int mOverrideMaxBufferCount;
    mutable Condition mDequeueCondition;
    bool mUseAsyncBuffer;
    bool mDequeueBufferCannotBlock;
    uint32_t mDefaultBufferFormat;
    int mDefaultWidth;
    int mDefaultHeight;
    int mDefaultMaxBufferCount;
    int mMaxAcquiredBufferCount;
    bool mBufferHasBeenQueued;
    uint64_t mFrameCounter;
    uint32_t mTransformHint;

}; // class BufferQueueCore

} // namespace android

#endif
