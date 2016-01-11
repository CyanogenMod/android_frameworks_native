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

#define LOG_TAG "BufferQueueCore"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define EGL_EGLEXT_PROTOTYPES

#include <inttypes.h>

#include <gui/BufferItem.h>
#include <gui/BufferQueueCore.h>
#include <gui/IConsumerListener.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/IProducerListener.h>
#include <gui/ISurfaceComposer.h>
#include <private/gui/ComposerService.h>

namespace android {

static String8 getUniqueName() {
    static volatile int32_t counter = 0;
    return String8::format("unnamed-%d-%d", getpid(),
            android_atomic_inc(&counter));
}

BufferQueueCore::BufferQueueCore() :
    mAllocator(),
    mMutex(),
    mIsAbandoned(false),
    mConsumerControlledByApp(false),
    mConsumerName(getUniqueName()),
    mConsumerListener(),
    mConsumerUsageBits(0),
    mConnectedApi(NO_CONNECTED_API),
    mConnectedProducerListener(),
    mSlots(),
    mQueue(),
    mFreeSlots(),
    mFreeBuffers(),
    mDequeueCondition(),
    mDequeueBufferCannotBlock(false),
    mDefaultBufferFormat(PIXEL_FORMAT_RGBA_8888),
    mDefaultWidth(1),
    mDefaultHeight(1),
    mDefaultBufferDataSpace(HAL_DATASPACE_UNKNOWN),
    mMaxBufferCount(BufferQueueDefs::NUM_BUFFER_SLOTS),
    mMaxAcquiredBufferCount(1),
    mMaxDequeuedBufferCount(1),
    mBufferHasBeenQueued(false),
    mFrameCounter(0),
    mTransformHint(0),
    mIsAllocating(false),
    mIsAllocatingCondition(),
    mAllowAllocation(true),
    mBufferAge(0),
    mGenerationNumber(0),
    mAsyncMode(false),
    mSingleBufferMode(false),
    mSingleBufferSlot(INVALID_BUFFER_SLOT),
    mSingleBufferCache(Rect::INVALID_RECT, 0, NATIVE_WINDOW_SCALING_MODE_FREEZE,
            HAL_DATASPACE_UNKNOWN)
{
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    mAllocator = composer->createGraphicBufferAlloc();
    if (mAllocator == NULL) {
        BQ_LOGE("createGraphicBufferAlloc failed");
    }

    for (int slot = 0; slot < BufferQueueDefs::NUM_BUFFER_SLOTS; ++slot) {
        mFreeSlots.insert(slot);
    }
}

BufferQueueCore::~BufferQueueCore() {}

void BufferQueueCore::dump(String8& result, const char* prefix) const {
    Mutex::Autolock lock(mMutex);

    String8 fifo;
    Fifo::const_iterator current(mQueue.begin());
    while (current != mQueue.end()) {
        fifo.appendFormat("%02d:%p crop=[%d,%d,%d,%d], "
                "xform=0x%02x, time=%#" PRIx64 ", scale=%s\n",
                current->mSlot, current->mGraphicBuffer.get(),
                current->mCrop.left, current->mCrop.top, current->mCrop.right,
                current->mCrop.bottom, current->mTransform, current->mTimestamp,
                BufferItem::scalingModeName(current->mScalingMode));
        ++current;
    }

    result.appendFormat("%s-BufferQueue mMaxAcquiredBufferCount=%d, "
            "mMaxDequeuedBufferCount=%d, mDequeueBufferCannotBlock=%d "
            "mAsyncMode=%d, default-size=[%dx%d], default-format=%d, "
            "transform-hint=%02x, FIFO(%zu)={%s}\n", prefix,
            mMaxAcquiredBufferCount, mMaxDequeuedBufferCount,
            mDequeueBufferCannotBlock, mAsyncMode, mDefaultWidth,
            mDefaultHeight, mDefaultBufferFormat, mTransformHint, mQueue.size(),
            fifo.string());

    // Trim the free buffers so as to not spam the dump
    int maxBufferCount = 0;
    for (int s = BufferQueueDefs::NUM_BUFFER_SLOTS - 1; s >= 0; --s) {
        const BufferSlot& slot(mSlots[s]);
        if (!slot.mBufferState.isFree() ||
                slot.mGraphicBuffer != NULL) {
            maxBufferCount = s + 1;
            break;
        }
    }

    for (int s = 0; s < maxBufferCount; ++s) {
        const BufferSlot& slot(mSlots[s]);
        const sp<GraphicBuffer>& buffer(slot.mGraphicBuffer);
        result.appendFormat("%s%s[%02d:%p] state=%-8s", prefix,
                (slot.mBufferState.isAcquired()) ? ">" : " ",
                s, buffer.get(),
                slot.mBufferState.string());

        if (buffer != NULL) {
            result.appendFormat(", %p [%4ux%4u:%4u,%3X]", buffer->handle,
                    buffer->width, buffer->height, buffer->stride,
                    buffer->format);
        }

        result.append("\n");
    }
}

int BufferQueueCore::getMinUndequeuedBufferCountLocked() const {
    // If dequeueBuffer is allowed to error out, we don't have to add an
    // extra buffer.
    if (mAsyncMode || mDequeueBufferCannotBlock) {
        return mMaxAcquiredBufferCount + 1;
    }

    return mMaxAcquiredBufferCount;
}

int BufferQueueCore::getMinMaxBufferCountLocked() const {
    return getMinUndequeuedBufferCountLocked() + 1;
}

int BufferQueueCore::getMaxBufferCountLocked() const {
    int maxBufferCount = mMaxAcquiredBufferCount + mMaxDequeuedBufferCount +
            (mAsyncMode || mDequeueBufferCannotBlock ? 1 : 0);

    // limit maxBufferCount by mMaxBufferCount always
    maxBufferCount = std::min(mMaxBufferCount, maxBufferCount);

    // Any buffers that are dequeued by the producer or sitting in the queue
    // waiting to be consumed need to have their slots preserved. Such buffers
    // will temporarily keep the max buffer count up until the slots no longer
    // need to be preserved.
    for (int s = maxBufferCount; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        BufferState state = mSlots[s].mBufferState;
        if (state.isQueued() || state.isDequeued()) {
            maxBufferCount = s + 1;
        }
    }

    return maxBufferCount;
}

void BufferQueueCore::freeBufferLocked(int slot, bool validate) {
    BQ_LOGV("freeBufferLocked: slot %d", slot);
    bool hadBuffer = mSlots[slot].mGraphicBuffer != NULL;
    mSlots[slot].mGraphicBuffer.clear();
    if (mSlots[slot].mBufferState.isAcquired()) {
        mSlots[slot].mNeedsCleanupOnRelease = true;
    }
    if (!mSlots[slot].mBufferState.isFree()) {
        mFreeSlots.insert(slot);
    } else if (hadBuffer) {
        // If the slot was FREE, but we had a buffer, we need to move this slot
        // from the free buffers list to the the free slots list
        mFreeBuffers.remove(slot);
        mFreeSlots.insert(slot);
    }
    mSlots[slot].mAcquireCalled = false;
    mSlots[slot].mFrameNumber = 0;

    // Destroy fence as BufferQueue now takes ownership
    if (mSlots[slot].mEglFence != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(mSlots[slot].mEglDisplay, mSlots[slot].mEglFence);
        mSlots[slot].mEglFence = EGL_NO_SYNC_KHR;
    }
    mSlots[slot].mFence = Fence::NO_FENCE;
    if (validate) {
        validateConsistencyLocked();
    }
}

void BufferQueueCore::freeAllBuffersLocked() {
    mBufferHasBeenQueued = false;
    for (int s = 0; s < BufferQueueDefs::NUM_BUFFER_SLOTS; ++s) {
        freeBufferLocked(s, false);
        mSlots[s].mBufferState.reset();
    }
    mSingleBufferSlot = INVALID_BUFFER_SLOT;
    validateConsistencyLocked();
}

bool BufferQueueCore::stillTracking(const BufferItem* item) const {
    const BufferSlot& slot = mSlots[item->mSlot];

    BQ_LOGV("stillTracking: item { slot=%d/%" PRIu64 " buffer=%p } "
            "slot { slot=%d/%" PRIu64 " buffer=%p }",
            item->mSlot, item->mFrameNumber,
            (item->mGraphicBuffer.get() ? item->mGraphicBuffer->handle : 0),
            item->mSlot, slot.mFrameNumber,
            (slot.mGraphicBuffer.get() ? slot.mGraphicBuffer->handle : 0));

    // Compare item with its original buffer slot. We can check the slot as
    // the buffer would not be moved to a different slot by the producer.
    return (slot.mGraphicBuffer != NULL) &&
           (item->mGraphicBuffer->handle == slot.mGraphicBuffer->handle);
}

void BufferQueueCore::waitWhileAllocatingLocked() const {
    ATRACE_CALL();
    while (mIsAllocating) {
        mIsAllocatingCondition.wait(mMutex);
    }
}

void BufferQueueCore::validateConsistencyLocked() const {
    static const useconds_t PAUSE_TIME = 0;
    for (int slot = 0; slot < BufferQueueDefs::NUM_BUFFER_SLOTS; ++slot) {
        bool isInFreeSlots = mFreeSlots.count(slot) != 0;
        bool isInFreeBuffers =
                std::find(mFreeBuffers.cbegin(), mFreeBuffers.cend(), slot) !=
                mFreeBuffers.cend();
        if (mSlots[slot].mBufferState.isFree() &&
                !mSlots[slot].mBufferState.isShared()) {
            if (mSlots[slot].mGraphicBuffer == NULL) {
                if (!isInFreeSlots) {
                    BQ_LOGE("Slot %d is FREE but is not in mFreeSlots", slot);
                    usleep(PAUSE_TIME);
                }
                if (isInFreeBuffers) {
                    BQ_LOGE("Slot %d is in mFreeSlots "
                            "but is also in mFreeBuffers", slot);
                    usleep(PAUSE_TIME);
                }
            } else {
                if (!isInFreeBuffers) {
                    BQ_LOGE("Slot %d is FREE but is not in mFreeBuffers", slot);
                    usleep(PAUSE_TIME);
                }
                if (isInFreeSlots) {
                    BQ_LOGE("Slot %d is in mFreeBuffers "
                            "but is also in mFreeSlots", slot);
                    usleep(PAUSE_TIME);
                }
            }
        } else {
            if (isInFreeSlots) {
                BQ_LOGE("Slot %d is in mFreeSlots but is not FREE (%s)",
                        slot, mSlots[slot].mBufferState.string());
                usleep(PAUSE_TIME);
            }
            if (isInFreeBuffers) {
                BQ_LOGE("Slot %d is in mFreeBuffers but is not FREE (%s)",
                        slot, mSlots[slot].mBufferState.string());
                usleep(PAUSE_TIME);
            }
        }
    }
}

} // namespace android
