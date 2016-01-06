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

#include <inttypes.h>

#define LOG_TAG "BufferQueueProducer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define EGL_EGLEXT_PROTOTYPES

#include <gui/BufferItem.h>
#include <gui/BufferQueueCore.h>
#include <gui/BufferQueueProducer.h>
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IProducerListener.h>

#include <utils/Log.h>
#include <utils/Trace.h>

namespace android {

BufferQueueProducer::BufferQueueProducer(const sp<BufferQueueCore>& core) :
    mCore(core),
    mSlots(core->mSlots),
    mConsumerName(),
    mStickyTransform(0),
    mLastQueueBufferFence(Fence::NO_FENCE),
    mCallbackMutex(),
    mNextCallbackTicket(0),
    mCurrentCallbackTicket(0),
    mCallbackCondition(),
    mDequeueTimeout(-1) {}

BufferQueueProducer::~BufferQueueProducer() {}

status_t BufferQueueProducer::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ATRACE_CALL();
    BQ_LOGV("requestBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("requestBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("requestBuffer: BufferQueue has no connected producer");
        return NO_INIT;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("requestBuffer: slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (!mSlots[slot].mBufferState.isDequeued()) {
        BQ_LOGE("requestBuffer: slot %d is not owned by the producer "
                "(state = %s)", slot, mSlots[slot].mBufferState.string());
        return BAD_VALUE;
    }

    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t BufferQueueProducer::setMaxDequeuedBufferCount(
        int maxDequeuedBuffers) {
    ATRACE_CALL();
    BQ_LOGV("setMaxDequeuedBufferCount: maxDequeuedBuffers = %d",
            maxDequeuedBuffers);

    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            BQ_LOGE("setMaxDequeuedBufferCount: BufferQueue has been "
                    "abandoned");
            return NO_INIT;
        }

        // There must be no dequeued buffers when changing the buffer count.
        for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            if (mSlots[s].mBufferState.isDequeued()) {
                BQ_LOGE("setMaxDequeuedBufferCount: buffer owned by producer");
                return BAD_VALUE;
            }
        }

        int bufferCount = mCore->getMinUndequeuedBufferCountLocked();
        bufferCount += maxDequeuedBuffers;

        if (bufferCount > BufferQueueDefs::NUM_BUFFER_SLOTS) {
            BQ_LOGE("setMaxDequeuedBufferCount: bufferCount %d too large "
                    "(max %d)", bufferCount, BufferQueueDefs::NUM_BUFFER_SLOTS);
            return BAD_VALUE;
        }

        const int minBufferSlots = mCore->getMinMaxBufferCountLocked();
        if (bufferCount < minBufferSlots) {
            BQ_LOGE("setMaxDequeuedBufferCount: requested buffer count %d is "
                    "less than minimum %d", bufferCount, minBufferSlots);
            return BAD_VALUE;
        }

        if (bufferCount > mCore->mMaxBufferCount) {
            BQ_LOGE("setMaxDequeuedBufferCount: %d dequeued buffers would "
                    "exceed the maxBufferCount (%d) (maxAcquired %d async %d "
                    "mDequeuedBufferCannotBlock %d)", maxDequeuedBuffers,
                    mCore->mMaxBufferCount, mCore->mMaxAcquiredBufferCount,
                    mCore->mAsyncMode, mCore->mDequeueBufferCannotBlock);
            return BAD_VALUE;
        }

        // Here we are guaranteed that the producer doesn't have any dequeued
        // buffers and will release all of its buffer references. We don't
        // clear the queue, however, so that currently queued buffers still
        // get displayed.
        mCore->freeAllBuffersLocked();
        mCore->mMaxDequeuedBufferCount = maxDequeuedBuffers;
        mCore->mDequeueCondition.broadcast();
        listener = mCore->mConsumerListener;
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return NO_ERROR;
}

status_t BufferQueueProducer::setAsyncMode(bool async) {
    ATRACE_CALL();
    BQ_LOGV("setAsyncMode: async = %d", async);

    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            BQ_LOGE("setAsyncMode: BufferQueue has been abandoned");
            return NO_INIT;
        }

        if ((mCore->mMaxAcquiredBufferCount + mCore->mMaxDequeuedBufferCount +
                (async || mCore->mDequeueBufferCannotBlock ? 1 : 0)) >
                mCore->mMaxBufferCount) {
            BQ_LOGE("setAsyncMode(%d): this call would cause the "
                    "maxBufferCount (%d) to be exceeded (maxAcquired %d "
                    "maxDequeued %d mDequeueBufferCannotBlock %d)", async,
                    mCore->mMaxBufferCount, mCore->mMaxAcquiredBufferCount,
                    mCore->mMaxDequeuedBufferCount,
                    mCore->mDequeueBufferCannotBlock);
            return BAD_VALUE;
        }

        mCore->mAsyncMode = async;
        mCore->mDequeueCondition.broadcast();
        listener = mCore->mConsumerListener;
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
    return NO_ERROR;
}

int BufferQueueProducer::getFreeBufferLocked() const {
    if (mCore->mFreeBuffers.empty()) {
        return BufferQueueCore::INVALID_BUFFER_SLOT;
    }
    auto slot = mCore->mFreeBuffers.front();
    mCore->mFreeBuffers.pop_front();
    return slot;
}

int BufferQueueProducer::getFreeSlotLocked(int maxBufferCount) const {
    if (mCore->mFreeSlots.empty()) {
        return BufferQueueCore::INVALID_BUFFER_SLOT;
    }
    auto slot = *(mCore->mFreeSlots.begin());
    if (slot < maxBufferCount) {
        mCore->mFreeSlots.erase(slot);
        return slot;
    }
    return BufferQueueCore::INVALID_BUFFER_SLOT;
}

status_t BufferQueueProducer::waitForFreeSlotThenRelock(FreeSlotCaller caller,
        int* found, status_t* returnFlags) const {
    auto callerString = (caller == FreeSlotCaller::Dequeue) ?
            "dequeueBuffer" : "attachBuffer";
    bool tryAgain = true;
    while (tryAgain) {
        if (mCore->mIsAbandoned) {
            BQ_LOGE("%s: BufferQueue has been abandoned", callerString);
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked();

        // Free up any buffers that are in slots beyond the max buffer count
        for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
            assert(mSlots[s].mBufferState.isFree());
            if (mSlots[s].mGraphicBuffer != NULL) {
                mCore->freeBufferLocked(s);
                *returnFlags |= RELEASE_ALL_BUFFERS;
            }
        }

        int dequeuedCount = 0;
        int acquiredCount = 0;
        for (int s = 0; s < maxBufferCount; ++s) {
            if (mSlots[s].mBufferState.isDequeued()) {
                ++dequeuedCount;
            }
            if (mSlots[s].mBufferState.isAcquired()) {
                ++acquiredCount;
            }
        }

        // Producers are not allowed to dequeue more than
        // mMaxDequeuedBufferCount buffers.
        // This check is only done if a buffer has already been queued
        if (mCore->mBufferHasBeenQueued &&
                dequeuedCount >= mCore->mMaxDequeuedBufferCount) {
            BQ_LOGE("%s: attempting to exceed the max dequeued buffer count "
                    "(%d)", callerString, mCore->mMaxDequeuedBufferCount);
            return INVALID_OPERATION;
        }

        *found = BufferQueueCore::INVALID_BUFFER_SLOT;

        // If we disconnect and reconnect quickly, we can be in a state where
        // our slots are empty but we have many buffers in the queue. This can
        // cause us to run out of memory if we outrun the consumer. Wait here if
        // it looks like we have too many buffers queued up.
        bool tooManyBuffers = mCore->mQueue.size()
                            > static_cast<size_t>(maxBufferCount);
        if (tooManyBuffers) {
            BQ_LOGV("%s: queue size is %zu, waiting", callerString,
                    mCore->mQueue.size());
        } else {
            // If in single buffer mode and a shared buffer exists, always
            // return it.
            if (mCore->mSingleBufferMode && mCore->mSingleBufferSlot !=
                    BufferQueueCore::INVALID_BUFFER_SLOT) {
                *found = mCore->mSingleBufferSlot;
            } else {
                if (caller == FreeSlotCaller::Dequeue) {
                    // If we're calling this from dequeue, prefer free buffers
                    auto slot = getFreeBufferLocked();
                    if (slot != BufferQueueCore::INVALID_BUFFER_SLOT) {
                        *found = slot;
                    } else if (mCore->mAllowAllocation) {
                        *found = getFreeSlotLocked(maxBufferCount);
                    }
                } else {
                    // If we're calling this from attach, prefer free slots
                    auto slot = getFreeSlotLocked(maxBufferCount);
                    if (slot != BufferQueueCore::INVALID_BUFFER_SLOT) {
                        *found = slot;
                    } else {
                        *found = getFreeBufferLocked();
                    }
                }
            }
        }

        // If no buffer is found, or if the queue has too many buffers
        // outstanding, wait for a buffer to be acquired or released, or for the
        // max buffer count to change.
        tryAgain = (*found == BufferQueueCore::INVALID_BUFFER_SLOT) ||
                   tooManyBuffers;
        if (tryAgain) {
            // Return an error if we're in non-blocking mode (producer and
            // consumer are controlled by the application).
            // However, the consumer is allowed to briefly acquire an extra
            // buffer (which could cause us to have to wait here), which is
            // okay, since it is only used to implement an atomic acquire +
            // release (e.g., in GLConsumer::updateTexImage())
            if ((mCore->mDequeueBufferCannotBlock || mCore->mAsyncMode) &&
                    (acquiredCount <= mCore->mMaxAcquiredBufferCount)) {
                return WOULD_BLOCK;
            }
            if (mDequeueTimeout >= 0) {
                status_t result = mCore->mDequeueCondition.waitRelative(
                        mCore->mMutex, mDequeueTimeout);
                if (result == TIMED_OUT) {
                    return result;
                }
            } else {
                mCore->mDequeueCondition.wait(mCore->mMutex);
            }
        }
    } // while (tryAgain)

    return NO_ERROR;
}

status_t BufferQueueProducer::dequeueBuffer(int *outSlot,
        sp<android::Fence> *outFence, uint32_t width, uint32_t height,
        PixelFormat format, uint32_t usage) {
    ATRACE_CALL();
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mConsumerName = mCore->mConsumerName;

        if (mCore->mIsAbandoned) {
            BQ_LOGE("dequeueBuffer: BufferQueue has been abandoned");
            return NO_INIT;
        }

        if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
            BQ_LOGE("dequeueBuffer: BufferQueue has no connected producer");
            return NO_INIT;
        }
    } // Autolock scope

    BQ_LOGV("dequeueBuffer: w=%u h=%u format=%#x, usage=%#x", width, height,
            format, usage);

    if ((width && !height) || (!width && height)) {
        BQ_LOGE("dequeueBuffer: invalid size: w=%u h=%u", width, height);
        return BAD_VALUE;
    }

    status_t returnFlags = NO_ERROR;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSyncKHR eglFence = EGL_NO_SYNC_KHR;
    bool attachedByConsumer = false;

    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (format == 0) {
            format = mCore->mDefaultBufferFormat;
        }

        // Enable the usage bits the consumer requested
        usage |= mCore->mConsumerUsageBits;

        const bool useDefaultSize = !width && !height;
        if (useDefaultSize) {
            width = mCore->mDefaultWidth;
            height = mCore->mDefaultHeight;
        }

        int found = BufferItem::INVALID_BUFFER_SLOT;
        while (found == BufferItem::INVALID_BUFFER_SLOT) {
            status_t status = waitForFreeSlotThenRelock(FreeSlotCaller::Dequeue,
                    &found, &returnFlags);
            if (status != NO_ERROR) {
                return status;
            }

            // This should not happen
            if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
                BQ_LOGE("dequeueBuffer: no available buffer slots");
                return -EBUSY;
            }

            const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);

            // If we are not allowed to allocate new buffers,
            // waitForFreeSlotThenRelock must have returned a slot containing a
            // buffer. If this buffer would require reallocation to meet the
            // requested attributes, we free it and attempt to get another one.
            if (!mCore->mAllowAllocation) {
                if (buffer->needsReallocation(width, height, format, usage)) {
                    if (mCore->mSingleBufferMode &&
                            mCore->mSingleBufferSlot == found) {
                        BQ_LOGE("dequeueBuffer: cannot re-allocate a shared"
                                "buffer");
                        return BAD_VALUE;
                    }

                    mCore->freeBufferLocked(found);
                    found = BufferItem::INVALID_BUFFER_SLOT;
                    continue;
                }
            }
        }

        *outSlot = found;
        ATRACE_BUFFER_INDEX(found);

        attachedByConsumer = mSlots[found].mAttachedByConsumer;

        mSlots[found].mBufferState.dequeue();

        // If single buffer mode has just been enabled, cache the slot of the
        // first buffer that is dequeued and mark it as the shared buffer.
        if (mCore->mSingleBufferMode && mCore->mSingleBufferSlot ==
                BufferQueueCore::INVALID_BUFFER_SLOT) {
            mCore->mSingleBufferSlot = found;
            mSlots[found].mBufferState.mShared = true;
        }

        const sp<GraphicBuffer>& buffer(mSlots[found].mGraphicBuffer);
        if ((buffer == NULL) ||
                buffer->needsReallocation(width, height, format, usage))
        {
            mSlots[found].mAcquireCalled = false;
            mSlots[found].mGraphicBuffer = NULL;
            mSlots[found].mRequestBufferCalled = false;
            mSlots[found].mEglDisplay = EGL_NO_DISPLAY;
            mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
            mSlots[found].mFence = Fence::NO_FENCE;
            mCore->mBufferAge = 0;
            mCore->mIsAllocating = true;

            returnFlags |= BUFFER_NEEDS_REALLOCATION;
        } else {
            // We add 1 because that will be the frame number when this buffer
            // is queued
            mCore->mBufferAge =
                    mCore->mFrameCounter + 1 - mSlots[found].mFrameNumber;
        }

        BQ_LOGV("dequeueBuffer: setting buffer age to %" PRIu64,
                mCore->mBufferAge);

        if (CC_UNLIKELY(mSlots[found].mFence == NULL)) {
            BQ_LOGE("dequeueBuffer: about to return a NULL fence - "
                    "slot=%d w=%d h=%d format=%u",
                    found, buffer->width, buffer->height, buffer->format);
        }

        eglDisplay = mSlots[found].mEglDisplay;
        eglFence = mSlots[found].mEglFence;
        *outFence = mSlots[found].mFence;
        mSlots[found].mEglFence = EGL_NO_SYNC_KHR;
        mSlots[found].mFence = Fence::NO_FENCE;

        mCore->validateConsistencyLocked();
    } // Autolock scope

    if (returnFlags & BUFFER_NEEDS_REALLOCATION) {
        status_t error;
        BQ_LOGV("dequeueBuffer: allocating a new buffer for slot %d", *outSlot);
        sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
                width, height, format, usage, &error));
        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);

            if (graphicBuffer != NULL && !mCore->mIsAbandoned) {
                graphicBuffer->setGenerationNumber(mCore->mGenerationNumber);
                mSlots[*outSlot].mGraphicBuffer = graphicBuffer;
            }

            mCore->mIsAllocating = false;
            mCore->mIsAllocatingCondition.broadcast();

            if (graphicBuffer == NULL) {
                BQ_LOGE("dequeueBuffer: createGraphicBuffer failed");
                return error;
            }

            if (mCore->mIsAbandoned) {
                BQ_LOGE("dequeueBuffer: BufferQueue has been abandoned");
                return NO_INIT;
            }
        } // Autolock scope
    }

    if (attachedByConsumer) {
        returnFlags |= BUFFER_NEEDS_REALLOCATION;
    }

    if (eglFence != EGL_NO_SYNC_KHR) {
        EGLint result = eglClientWaitSyncKHR(eglDisplay, eglFence, 0,
                1000000000);
        // If something goes wrong, log the error, but return the buffer without
        // synchronizing access to it. It's too late at this point to abort the
        // dequeue operation.
        if (result == EGL_FALSE) {
            BQ_LOGE("dequeueBuffer: error %#x waiting for fence",
                    eglGetError());
        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
            BQ_LOGE("dequeueBuffer: timeout waiting for fence");
        }
        eglDestroySyncKHR(eglDisplay, eglFence);
    }

    BQ_LOGV("dequeueBuffer: returning slot=%d/%" PRIu64 " buf=%p flags=%#x",
            *outSlot,
            mSlots[*outSlot].mFrameNumber,
            mSlots[*outSlot].mGraphicBuffer->handle, returnFlags);

    return returnFlags;
}

status_t BufferQueueProducer::detachBuffer(int slot) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);
    BQ_LOGV("detachBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("detachBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("detachBuffer: BufferQueue has no connected producer");
        return NO_INIT;
    }

    if (mCore->mSingleBufferMode) {
        BQ_LOGE("detachBuffer: cannot detach a buffer in single buffer"
                "mode");
        return BAD_VALUE;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("detachBuffer: slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (!mSlots[slot].mBufferState.isDequeued()) {
        BQ_LOGE("detachBuffer: slot %d is not owned by the producer "
                "(state = %s)", slot, mSlots[slot].mBufferState.string());
        return BAD_VALUE;
    } else if (!mSlots[slot].mRequestBufferCalled) {
        BQ_LOGE("detachBuffer: buffer in slot %d has not been requested",
                slot);
        return BAD_VALUE;
    }

    mSlots[slot].mBufferState.detachProducer();
    mCore->freeBufferLocked(slot);
    mCore->mDequeueCondition.broadcast();
    mCore->validateConsistencyLocked();

    return NO_ERROR;
}

status_t BufferQueueProducer::detachNextBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence) {
    ATRACE_CALL();

    if (outBuffer == NULL) {
        BQ_LOGE("detachNextBuffer: outBuffer must not be NULL");
        return BAD_VALUE;
    } else if (outFence == NULL) {
        BQ_LOGE("detachNextBuffer: outFence must not be NULL");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("detachNextBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("detachNextBuffer: BufferQueue has no connected producer");
        return NO_INIT;
    }

    if (mCore->mSingleBufferMode) {
        BQ_LOGE("detachNextBuffer: cannot detach a buffer in single buffer"
                "mode");
        return BAD_VALUE;
    }

    mCore->waitWhileAllocatingLocked();

    if (mCore->mFreeBuffers.empty()) {
        return NO_MEMORY;
    }

    int found = mCore->mFreeBuffers.front();
    mCore->mFreeBuffers.remove(found);

    BQ_LOGV("detachNextBuffer detached slot %d", found);

    *outBuffer = mSlots[found].mGraphicBuffer;
    *outFence = mSlots[found].mFence;
    mCore->freeBufferLocked(found);
    mCore->validateConsistencyLocked();

    return NO_ERROR;
}

status_t BufferQueueProducer::attachBuffer(int* outSlot,
        const sp<android::GraphicBuffer>& buffer) {
    ATRACE_CALL();

    if (outSlot == NULL) {
        BQ_LOGE("attachBuffer: outSlot must not be NULL");
        return BAD_VALUE;
    } else if (buffer == NULL) {
        BQ_LOGE("attachBuffer: cannot attach NULL buffer");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("attachBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("attachBuffer: BufferQueue has no connected producer");
        return NO_INIT;
    }

    if (mCore->mSingleBufferMode) {
        BQ_LOGE("attachBuffer: cannot atach a buffer in single buffer mode");
        return BAD_VALUE;
    }

    if (buffer->getGenerationNumber() != mCore->mGenerationNumber) {
        BQ_LOGE("attachBuffer: generation number mismatch [buffer %u] "
                "[queue %u]", buffer->getGenerationNumber(),
                mCore->mGenerationNumber);
        return BAD_VALUE;
    }

    mCore->waitWhileAllocatingLocked();

    status_t returnFlags = NO_ERROR;
    int found;
    status_t status = waitForFreeSlotThenRelock(FreeSlotCaller::Attach, &found,
            &returnFlags);
    if (status != NO_ERROR) {
        return status;
    }

    // This should not happen
    if (found == BufferQueueCore::INVALID_BUFFER_SLOT) {
        BQ_LOGE("attachBuffer: no available buffer slots");
        return -EBUSY;
    }

    *outSlot = found;
    ATRACE_BUFFER_INDEX(*outSlot);
    BQ_LOGV("attachBuffer: returning slot %d flags=%#x",
            *outSlot, returnFlags);

    mSlots[*outSlot].mGraphicBuffer = buffer;
    mSlots[*outSlot].mBufferState.attachProducer();
    mSlots[*outSlot].mEglFence = EGL_NO_SYNC_KHR;
    mSlots[*outSlot].mFence = Fence::NO_FENCE;
    mSlots[*outSlot].mRequestBufferCalled = true;

    mCore->validateConsistencyLocked();

    return returnFlags;
}

status_t BufferQueueProducer::queueBuffer(int slot,
        const QueueBufferInput &input, QueueBufferOutput *output) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);

    int64_t timestamp;
    bool isAutoTimestamp;
    android_dataspace dataSpace;
    Rect crop(Rect::EMPTY_RECT);
    int scalingMode;
    uint32_t transform;
    uint32_t stickyTransform;
    sp<Fence> fence;
    input.deflate(&timestamp, &isAutoTimestamp, &dataSpace, &crop, &scalingMode,
            &transform, &fence, &stickyTransform);
    Region surfaceDamage = input.getSurfaceDamage();

    if (fence == NULL) {
        BQ_LOGE("queueBuffer: fence is NULL");
        return BAD_VALUE;
    }

    switch (scalingMode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
        case NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP:
            break;
        default:
            BQ_LOGE("queueBuffer: unknown scaling mode %d", scalingMode);
            return BAD_VALUE;
    }

    sp<IConsumerListener> frameAvailableListener;
    sp<IConsumerListener> frameReplacedListener;
    int callbackTicket = 0;
    BufferItem item;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);

        if (mCore->mIsAbandoned) {
            BQ_LOGE("queueBuffer: BufferQueue has been abandoned");
            return NO_INIT;
        }

        if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
            BQ_LOGE("queueBuffer: BufferQueue has no connected producer");
            return NO_INIT;
        }

        const int maxBufferCount = mCore->getMaxBufferCountLocked();

        if (slot < 0 || slot >= maxBufferCount) {
            BQ_LOGE("queueBuffer: slot index %d out of range [0, %d)",
                    slot, maxBufferCount);
            return BAD_VALUE;
        } else if (!mSlots[slot].mBufferState.isDequeued()) {
            BQ_LOGE("queueBuffer: slot %d is not owned by the producer "
                    "(state = %s)", slot, mSlots[slot].mBufferState.string());
            return BAD_VALUE;
        } else if (!mSlots[slot].mRequestBufferCalled) {
            BQ_LOGE("queueBuffer: slot %d was queued without requesting "
                    "a buffer", slot);
            return BAD_VALUE;
        }

        BQ_LOGV("queueBuffer: slot=%d/%" PRIu64 " time=%" PRIu64 " dataSpace=%d"
                " crop=[%d,%d,%d,%d] transform=%#x scale=%s",
                slot, mCore->mFrameCounter + 1, timestamp, dataSpace,
                crop.left, crop.top, crop.right, crop.bottom, transform,
                BufferItem::scalingModeName(static_cast<uint32_t>(scalingMode)));

        const sp<GraphicBuffer>& graphicBuffer(mSlots[slot].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedRect(Rect::EMPTY_RECT);
        crop.intersect(bufferRect, &croppedRect);
        if (croppedRect != crop) {
            BQ_LOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", slot);
            return BAD_VALUE;
        }

        // Override UNKNOWN dataspace with consumer default
        if (dataSpace == HAL_DATASPACE_UNKNOWN) {
            dataSpace = mCore->mDefaultBufferDataSpace;
        }

        mSlots[slot].mFence = fence;
        mSlots[slot].mBufferState.queue();

        ++mCore->mFrameCounter;
        mSlots[slot].mFrameNumber = mCore->mFrameCounter;

        item.mAcquireCalled = mSlots[slot].mAcquireCalled;
        item.mGraphicBuffer = mSlots[slot].mGraphicBuffer;
        item.mCrop = crop;
        item.mTransform = transform &
                ~static_cast<uint32_t>(NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY);
        item.mTransformToDisplayInverse =
                (transform & NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY) != 0;
        item.mScalingMode = static_cast<uint32_t>(scalingMode);
        item.mTimestamp = timestamp;
        item.mIsAutoTimestamp = isAutoTimestamp;
        item.mDataSpace = dataSpace;
        item.mFrameNumber = mCore->mFrameCounter;
        item.mSlot = slot;
        item.mFence = fence;
        item.mIsDroppable = mCore->mAsyncMode ||
                mCore->mDequeueBufferCannotBlock ||
                (mCore->mSingleBufferMode && mCore->mSingleBufferSlot == slot);
        item.mSurfaceDamage = surfaceDamage;
        item.mSingleBufferMode = mCore->mSingleBufferMode;
        item.mQueuedBuffer = true;

        mStickyTransform = stickyTransform;

        // Cache the shared buffer data so that the BufferItem can be recreated.
        if (mCore->mSingleBufferMode) {
            mCore->mSingleBufferCache.crop = crop;
            mCore->mSingleBufferCache.transform = transform;
            mCore->mSingleBufferCache.scalingMode = static_cast<uint32_t>(
                    scalingMode);
            mCore->mSingleBufferCache.dataspace = dataSpace;
        }

        if (mCore->mQueue.empty()) {
            // When the queue is empty, we can ignore mDequeueBufferCannotBlock
            // and simply queue this buffer
            mCore->mQueue.push_back(item);
            frameAvailableListener = mCore->mConsumerListener;
        } else {
            // When the queue is not empty, we need to look at the front buffer
            // state to see if we need to replace it
            BufferQueueCore::Fifo::iterator front(mCore->mQueue.begin());
            if (front->mIsDroppable) {
                // If the front queued buffer is still being tracked, we first
                // mark it as freed
                if (mCore->stillTracking(front)) {
                    mSlots[front->mSlot].mBufferState.freeQueued();

                    // After leaving single buffer mode, the shared buffer will
                    // still be around. Mark it as no longer shared if this
                    // operation causes it to be free.
                    if (!mCore->mSingleBufferMode &&
                            mSlots[front->mSlot].mBufferState.isFree()) {
                        mSlots[front->mSlot].mBufferState.mShared = false;
                    }
                    // Don't put the shared buffer on the free list.
                    if (!mSlots[front->mSlot].mBufferState.isShared()) {
                        mCore->mFreeBuffers.push_front(front->mSlot);
                    }
                }
                // Overwrite the droppable buffer with the incoming one
                *front = item;
                frameReplacedListener = mCore->mConsumerListener;
            } else {
                mCore->mQueue.push_back(item);
                frameAvailableListener = mCore->mConsumerListener;
            }
        }

        mCore->mBufferHasBeenQueued = true;
        mCore->mDequeueCondition.broadcast();

        output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
                mCore->mTransformHint,
                static_cast<uint32_t>(mCore->mQueue.size()));

        ATRACE_INT(mCore->mConsumerName.string(), mCore->mQueue.size());

        // Take a ticket for the callback functions
        callbackTicket = mNextCallbackTicket++;

        mCore->validateConsistencyLocked();
    } // Autolock scope

    // Don't send the GraphicBuffer through the callback, and don't send
    // the slot number, since the consumer shouldn't need it
    item.mGraphicBuffer.clear();
    item.mSlot = BufferItem::INVALID_BUFFER_SLOT;

    // Call back without the main BufferQueue lock held, but with the callback
    // lock held so we can ensure that callbacks occur in order
    {
        Mutex::Autolock lock(mCallbackMutex);
        while (callbackTicket != mCurrentCallbackTicket) {
            mCallbackCondition.wait(mCallbackMutex);
        }

        if (frameAvailableListener != NULL) {
            frameAvailableListener->onFrameAvailable(item);
        } else if (frameReplacedListener != NULL) {
            frameReplacedListener->onFrameReplaced(item);
        }

        ++mCurrentCallbackTicket;
        mCallbackCondition.broadcast();
    }

    // Wait without lock held
    if (mCore->mConnectedApi == NATIVE_WINDOW_API_EGL) {
        // Waiting here allows for two full buffers to be queued but not a
        // third. In the event that frames take varying time, this makes a
        // small trade-off in favor of latency rather than throughput.
        mLastQueueBufferFence->waitForever("Throttling EGL Production");
        mLastQueueBufferFence = fence;
    }

    return NO_ERROR;
}

status_t BufferQueueProducer::cancelBuffer(int slot, const sp<Fence>& fence) {
    ATRACE_CALL();
    BQ_LOGV("cancelBuffer: slot %d", slot);
    Mutex::Autolock lock(mCore->mMutex);

    if (mCore->mIsAbandoned) {
        BQ_LOGE("cancelBuffer: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConnectedApi == BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("cancelBuffer: BufferQueue has no connected producer");
        return NO_INIT;
    }

    if (mCore->mSingleBufferMode) {
        BQ_LOGE("cancelBuffer: cannot cancel a buffer in single buffer mode");
        return BAD_VALUE;
    }

    if (slot < 0 || slot >= BufferQueueDefs::NUM_BUFFER_SLOTS) {
        BQ_LOGE("cancelBuffer: slot index %d out of range [0, %d)",
                slot, BufferQueueDefs::NUM_BUFFER_SLOTS);
        return BAD_VALUE;
    } else if (!mSlots[slot].mBufferState.isDequeued()) {
        BQ_LOGE("cancelBuffer: slot %d is not owned by the producer "
                "(state = %s)", slot, mSlots[slot].mBufferState.string());
        return BAD_VALUE;
    } else if (fence == NULL) {
        BQ_LOGE("cancelBuffer: fence is NULL");
        return BAD_VALUE;
    }

    mSlots[slot].mBufferState.cancel();

    // After leaving single buffer mode, the shared buffer will still be around.
    // Mark it as no longer shared if this operation causes it to be free.
    if (!mCore->mSingleBufferMode && mSlots[slot].mBufferState.isFree()) {
        mSlots[slot].mBufferState.mShared = false;
    }

    // Don't put the shared buffer on the free list.
    if (!mSlots[slot].mBufferState.isShared()) {
        mCore->mFreeBuffers.push_front(slot);
    }
    mSlots[slot].mFence = fence;
    mCore->mDequeueCondition.broadcast();
    mCore->validateConsistencyLocked();

    return NO_ERROR;
}

int BufferQueueProducer::query(int what, int *outValue) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);

    if (outValue == NULL) {
        BQ_LOGE("query: outValue was NULL");
        return BAD_VALUE;
    }

    if (mCore->mIsAbandoned) {
        BQ_LOGE("query: BufferQueue has been abandoned");
        return NO_INIT;
    }

    int value;
    switch (what) {
        case NATIVE_WINDOW_WIDTH:
            value = static_cast<int32_t>(mCore->mDefaultWidth);
            break;
        case NATIVE_WINDOW_HEIGHT:
            value = static_cast<int32_t>(mCore->mDefaultHeight);
            break;
        case NATIVE_WINDOW_FORMAT:
            value = static_cast<int32_t>(mCore->mDefaultBufferFormat);
            break;
        case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
            value = mCore->getMinUndequeuedBufferCountLocked();
            break;
        case NATIVE_WINDOW_STICKY_TRANSFORM:
            value = static_cast<int32_t>(mStickyTransform);
            break;
        case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
            value = (mCore->mQueue.size() > 1);
            break;
        case NATIVE_WINDOW_CONSUMER_USAGE_BITS:
            value = static_cast<int32_t>(mCore->mConsumerUsageBits);
            break;
        case NATIVE_WINDOW_DEFAULT_DATASPACE:
            value = static_cast<int32_t>(mCore->mDefaultBufferDataSpace);
            break;
        case NATIVE_WINDOW_BUFFER_AGE:
            if (mCore->mBufferAge > INT32_MAX) {
                value = 0;
            } else {
                value = static_cast<int32_t>(mCore->mBufferAge);
            }
            break;
        default:
            return BAD_VALUE;
    }

    BQ_LOGV("query: %d? %d", what, value);
    *outValue = value;
    return NO_ERROR;
}

status_t BufferQueueProducer::connect(const sp<IProducerListener>& listener,
        int api, bool producerControlledByApp, QueueBufferOutput *output) {
    ATRACE_CALL();
    Mutex::Autolock lock(mCore->mMutex);
    mConsumerName = mCore->mConsumerName;
    BQ_LOGV("connect: api=%d producerControlledByApp=%s", api,
            producerControlledByApp ? "true" : "false");

    if (mCore->mIsAbandoned) {
        BQ_LOGE("connect: BufferQueue has been abandoned");
        return NO_INIT;
    }

    if (mCore->mConsumerListener == NULL) {
        BQ_LOGE("connect: BufferQueue has no consumer");
        return NO_INIT;
    }

    if (output == NULL) {
        BQ_LOGE("connect: output was NULL");
        return BAD_VALUE;
    }

    if (mCore->mConnectedApi != BufferQueueCore::NO_CONNECTED_API) {
        BQ_LOGE("connect: already connected (cur=%d req=%d)",
                mCore->mConnectedApi, api);
        return BAD_VALUE;
    }

    int status = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            mCore->mConnectedApi = api;
            output->inflate(mCore->mDefaultWidth, mCore->mDefaultHeight,
                    mCore->mTransformHint,
                    static_cast<uint32_t>(mCore->mQueue.size()));

            // Set up a death notification so that we can disconnect
            // automatically if the remote producer dies
            if (listener != NULL &&
                    IInterface::asBinder(listener)->remoteBinder() != NULL) {
                status = IInterface::asBinder(listener)->linkToDeath(
                        static_cast<IBinder::DeathRecipient*>(this));
                if (status != NO_ERROR) {
                    BQ_LOGE("connect: linkToDeath failed: %s (%d)",
                            strerror(-status), status);
                }
            }
            mCore->mConnectedProducerListener = listener;
            break;
        default:
            BQ_LOGE("connect: unknown API %d", api);
            status = BAD_VALUE;
            break;
    }

    mCore->mBufferHasBeenQueued = false;
    mCore->mDequeueBufferCannotBlock = false;
    if (mDequeueTimeout < 0) {
        mCore->mDequeueBufferCannotBlock =
                mCore->mConsumerControlledByApp && producerControlledByApp;
    }
    mCore->mAllowAllocation = true;

    return status;
}

status_t BufferQueueProducer::disconnect(int api) {
    ATRACE_CALL();
    BQ_LOGV("disconnect: api %d", api);

    int status = NO_ERROR;
    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock lock(mCore->mMutex);
        mCore->waitWhileAllocatingLocked();

        if (mCore->mIsAbandoned) {
            // It's not really an error to disconnect after the surface has
            // been abandoned; it should just be a no-op.
            return NO_ERROR;
        }

        switch (api) {
            case NATIVE_WINDOW_API_EGL:
            case NATIVE_WINDOW_API_CPU:
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                if (mCore->mConnectedApi == api) {
                    mCore->freeAllBuffersLocked();

                    // Remove our death notification callback if we have one
                    if (mCore->mConnectedProducerListener != NULL) {
                        sp<IBinder> token =
                                IInterface::asBinder(mCore->mConnectedProducerListener);
                        // This can fail if we're here because of the death
                        // notification, but we just ignore it
                        token->unlinkToDeath(
                                static_cast<IBinder::DeathRecipient*>(this));
                    }
                    mCore->mConnectedProducerListener = NULL;
                    mCore->mConnectedApi = BufferQueueCore::NO_CONNECTED_API;
                    mCore->mSidebandStream.clear();
                    mCore->mDequeueCondition.broadcast();
                    listener = mCore->mConsumerListener;
                } else if (mCore->mConnectedApi != BufferQueueCore::NO_CONNECTED_API) {
                    BQ_LOGE("disconnect: still connected to another API "
                            "(cur=%d req=%d)", mCore->mConnectedApi, api);
                    status = BAD_VALUE;
                }
                break;
            default:
                BQ_LOGE("disconnect: unknown API %d", api);
                status = BAD_VALUE;
                break;
        }
    } // Autolock scope

    // Call back without lock held
    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return status;
}

status_t BufferQueueProducer::setSidebandStream(const sp<NativeHandle>& stream) {
    sp<IConsumerListener> listener;
    { // Autolock scope
        Mutex::Autolock _l(mCore->mMutex);
        mCore->mSidebandStream = stream;
        listener = mCore->mConsumerListener;
    } // Autolock scope

    if (listener != NULL) {
        listener->onSidebandStreamChanged();
    }
    return NO_ERROR;
}

void BufferQueueProducer::allocateBuffers(uint32_t width, uint32_t height,
        PixelFormat format, uint32_t usage) {
    ATRACE_CALL();
    while (true) {
        Vector<int> freeSlots;
        size_t newBufferCount = 0;
        uint32_t allocWidth = 0;
        uint32_t allocHeight = 0;
        PixelFormat allocFormat = PIXEL_FORMAT_UNKNOWN;
        uint32_t allocUsage = 0;
        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);
            mCore->waitWhileAllocatingLocked();

            if (!mCore->mAllowAllocation) {
                BQ_LOGE("allocateBuffers: allocation is not allowed for this "
                        "BufferQueue");
                return;
            }

            int currentBufferCount = 0;
            for (int slot = 0; slot < BufferQueueDefs::NUM_BUFFER_SLOTS; ++slot) {
                if (mSlots[slot].mGraphicBuffer != NULL) {
                    ++currentBufferCount;
                } else {
                    if (!mSlots[slot].mBufferState.isFree()) {
                        BQ_LOGE("allocateBuffers: slot %d without buffer is not FREE",
                                slot);
                        continue;
                    }

                    freeSlots.push_back(slot);
                }
            }

            int maxBufferCount = mCore->getMaxBufferCountLocked();
            BQ_LOGV("allocateBuffers: allocating from %d buffers up to %d buffers",
                    currentBufferCount, maxBufferCount);
            if (maxBufferCount <= currentBufferCount)
                return;
            newBufferCount =
                    static_cast<size_t>(maxBufferCount - currentBufferCount);
            if (freeSlots.size() < newBufferCount) {
                BQ_LOGE("allocateBuffers: ran out of free slots");
                return;
            }
            allocWidth = width > 0 ? width : mCore->mDefaultWidth;
            allocHeight = height > 0 ? height : mCore->mDefaultHeight;
            allocFormat = format != 0 ? format : mCore->mDefaultBufferFormat;
            allocUsage = usage | mCore->mConsumerUsageBits;

            mCore->mIsAllocating = true;
        } // Autolock scope

        Vector<sp<GraphicBuffer>> buffers;
        for (size_t i = 0; i <  newBufferCount; ++i) {
            status_t result = NO_ERROR;
            sp<GraphicBuffer> graphicBuffer(mCore->mAllocator->createGraphicBuffer(
                    allocWidth, allocHeight, allocFormat, allocUsage, &result));
            if (result != NO_ERROR) {
                BQ_LOGE("allocateBuffers: failed to allocate buffer (%u x %u, format"
                        " %u, usage %u)", width, height, format, usage);
                Mutex::Autolock lock(mCore->mMutex);
                mCore->mIsAllocating = false;
                mCore->mIsAllocatingCondition.broadcast();
                return;
            }
            buffers.push_back(graphicBuffer);
        }

        { // Autolock scope
            Mutex::Autolock lock(mCore->mMutex);
            uint32_t checkWidth = width > 0 ? width : mCore->mDefaultWidth;
            uint32_t checkHeight = height > 0 ? height : mCore->mDefaultHeight;
            PixelFormat checkFormat = format != 0 ?
                    format : mCore->mDefaultBufferFormat;
            uint32_t checkUsage = usage | mCore->mConsumerUsageBits;
            if (checkWidth != allocWidth || checkHeight != allocHeight ||
                checkFormat != allocFormat || checkUsage != allocUsage) {
                // Something changed while we released the lock. Retry.
                BQ_LOGV("allocateBuffers: size/format/usage changed while allocating. Retrying.");
                mCore->mIsAllocating = false;
                mCore->mIsAllocatingCondition.broadcast();
                continue;
            }

            for (size_t i = 0; i < newBufferCount; ++i) {
                int slot = freeSlots[i];
                if (!mSlots[slot].mBufferState.isFree()) {
                    // A consumer allocated the FREE slot with attachBuffer. Discard the buffer we
                    // allocated.
                    BQ_LOGV("allocateBuffers: slot %d was acquired while allocating. "
                            "Dropping allocated buffer.", slot);
                    continue;
                }
                mCore->freeBufferLocked(slot); // Clean up the slot first
                mSlots[slot].mGraphicBuffer = buffers[i];
                mSlots[slot].mFence = Fence::NO_FENCE;

                // freeBufferLocked puts this slot on the free slots list. Since
                // we then attached a buffer, move the slot to free buffer list.
                mCore->mFreeSlots.erase(slot);
                mCore->mFreeBuffers.push_front(slot);

                BQ_LOGV("allocateBuffers: allocated a new buffer in slot %d", slot);
            }

            mCore->mIsAllocating = false;
            mCore->mIsAllocatingCondition.broadcast();
            mCore->validateConsistencyLocked();
        } // Autolock scope
    }
}

status_t BufferQueueProducer::allowAllocation(bool allow) {
    ATRACE_CALL();
    BQ_LOGV("allowAllocation: %s", allow ? "true" : "false");

    Mutex::Autolock lock(mCore->mMutex);
    mCore->mAllowAllocation = allow;
    return NO_ERROR;
}

status_t BufferQueueProducer::setGenerationNumber(uint32_t generationNumber) {
    ATRACE_CALL();
    BQ_LOGV("setGenerationNumber: %u", generationNumber);

    Mutex::Autolock lock(mCore->mMutex);
    mCore->mGenerationNumber = generationNumber;
    return NO_ERROR;
}

String8 BufferQueueProducer::getConsumerName() const {
    ATRACE_CALL();
    BQ_LOGV("getConsumerName: %s", mConsumerName.string());
    return mConsumerName;
}

uint64_t BufferQueueProducer::getNextFrameNumber() const {
    ATRACE_CALL();

    Mutex::Autolock lock(mCore->mMutex);
    uint64_t nextFrameNumber = mCore->mFrameCounter + 1;
    return nextFrameNumber;
}

status_t BufferQueueProducer::setSingleBufferMode(bool singleBufferMode) {
    ATRACE_CALL();
    BQ_LOGV("setSingleBufferMode: %d", singleBufferMode);

    Mutex::Autolock lock(mCore->mMutex);
    if (!singleBufferMode) {
        mCore->mSingleBufferSlot = BufferQueueCore::INVALID_BUFFER_SLOT;
    }
    mCore->mSingleBufferMode = singleBufferMode;
    return NO_ERROR;
}

status_t BufferQueueProducer::setDequeueTimeout(nsecs_t timeout) {
    ATRACE_CALL();
    BQ_LOGV("setDequeueTimeout: %" PRId64, timeout);

    Mutex::Autolock lock(mCore->mMutex);
    mDequeueTimeout = timeout;
    mCore->mDequeueBufferCannotBlock = false;
    return NO_ERROR;
}

void BufferQueueProducer::binderDied(const wp<android::IBinder>& /* who */) {
    // If we're here, it means that a producer we were connected to died.
    // We're guaranteed that we are still connected to it because we remove
    // this callback upon disconnect. It's therefore safe to read mConnectedApi
    // without synchronization here.
    int api = mCore->mConnectedApi;
    disconnect(api);
}

} // namespace android
