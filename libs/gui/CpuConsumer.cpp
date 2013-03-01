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

//#define LOG_NDEBUG 0
#define LOG_TAG "CpuConsumer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Log.h>

#include <gui/CpuConsumer.h>

#define CC_LOGV(x, ...) ALOGV("[%s] "x, mName.string(), ##__VA_ARGS__)
#define CC_LOGD(x, ...) ALOGD("[%s] "x, mName.string(), ##__VA_ARGS__)
#define CC_LOGI(x, ...) ALOGI("[%s] "x, mName.string(), ##__VA_ARGS__)
#define CC_LOGW(x, ...) ALOGW("[%s] "x, mName.string(), ##__VA_ARGS__)
#define CC_LOGE(x, ...) ALOGE("[%s] "x, mName.string(), ##__VA_ARGS__)

namespace android {

CpuConsumer::CpuConsumer(uint32_t maxLockedBuffers) :
    ConsumerBase(new BufferQueue(true) ),
    mMaxLockedBuffers(maxLockedBuffers),
    mCurrentLockedBuffers(0)
{
    for (size_t i=0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        mLockedSlots[i].mBufferPointer = NULL;
    }

    mBufferQueue->setSynchronousMode(true);
    mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_SW_READ_OFTEN);
    mBufferQueue->setMaxAcquiredBufferCount(maxLockedBuffers);
}

CpuConsumer::~CpuConsumer() {
    status_t err;
    for (size_t i=0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        if (mLockedSlots[i].mBufferPointer != NULL) {
            mLockedSlots[i].mBufferPointer = NULL;
            err = mLockedSlots[i].mGraphicBuffer->unlock();
            mLockedSlots[i].mGraphicBuffer.clear();
            if (err != OK) {
                CC_LOGE("%s: Unable to unlock graphic buffer %d", __FUNCTION__,
                        i);
            }

        }
    }
}

void CpuConsumer::setName(const String8& name) {
    Mutex::Autolock _l(mMutex);
    mName = name;
    mBufferQueue->setConsumerName(name);
}

status_t CpuConsumer::lockNextBuffer(LockedBuffer *nativeBuffer) {
    status_t err;

    if (!nativeBuffer) return BAD_VALUE;
    if (mCurrentLockedBuffers == mMaxLockedBuffers) {
        return INVALID_OPERATION;
    }

    BufferQueue::BufferItem b;

    Mutex::Autolock _l(mMutex);

    err = acquireBufferLocked(&b);
    if (err != OK) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            return BAD_VALUE;
        } else {
            CC_LOGE("Error acquiring buffer: %s (%d)", strerror(err), err);
            return err;
        }
    }

    int buf = b.mBuf;

    if (b.mFence.get()) {
        err = b.mFence->waitForever(1000, "CpuConsumer::lockNextBuffer");
        if (err != OK) {
            CC_LOGE("Failed to wait for fence of acquired buffer: %s (%d)",
                    strerror(-err), err);
            return err;
        }
    }

    void *bufferPointer = NULL;
    err = mSlots[buf].mGraphicBuffer->lock(
        GraphicBuffer::USAGE_SW_READ_OFTEN,
        b.mCrop,
        &bufferPointer);

    if (bufferPointer != NULL && err != OK) {
        CC_LOGE("Unable to lock buffer for CPU reading: %s (%d)", strerror(-err),
                err);
        return err;
    }
    mLockedSlots[buf].mBufferPointer = bufferPointer;
    mLockedSlots[buf].mGraphicBuffer = mSlots[buf].mGraphicBuffer;

    nativeBuffer->data   =
            reinterpret_cast<uint8_t*>(bufferPointer);
    nativeBuffer->width  = mSlots[buf].mGraphicBuffer->getWidth();
    nativeBuffer->height = mSlots[buf].mGraphicBuffer->getHeight();
    nativeBuffer->format = mSlots[buf].mGraphicBuffer->getPixelFormat();
    nativeBuffer->stride = mSlots[buf].mGraphicBuffer->getStride();

    nativeBuffer->crop        = b.mCrop;
    nativeBuffer->transform   = b.mTransform;
    nativeBuffer->scalingMode = b.mScalingMode;
    nativeBuffer->timestamp   = b.mTimestamp;
    nativeBuffer->frameNumber = b.mFrameNumber;

    mCurrentLockedBuffers++;

    return OK;
}

status_t CpuConsumer::unlockBuffer(const LockedBuffer &nativeBuffer) {
    Mutex::Autolock _l(mMutex);
    int slotIndex = 0;
    status_t err;

    void *bufPtr = reinterpret_cast<void *>(nativeBuffer.data);
    for (; slotIndex < BufferQueue::NUM_BUFFER_SLOTS; slotIndex++) {
        if (bufPtr == mLockedSlots[slotIndex].mBufferPointer) break;
    }
    if (slotIndex == BufferQueue::NUM_BUFFER_SLOTS) {
        CC_LOGE("%s: Can't find buffer to free", __FUNCTION__);
        return BAD_VALUE;
    }

    mLockedSlots[slotIndex].mBufferPointer = NULL;
    err = mLockedSlots[slotIndex].mGraphicBuffer->unlock();
    mLockedSlots[slotIndex].mGraphicBuffer.clear();
    if (err != OK) {
        CC_LOGE("%s: Unable to unlock graphic buffer %d", __FUNCTION__, slotIndex);
        return err;
    }
    releaseBufferLocked(slotIndex, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);

    mCurrentLockedBuffers--;

    return OK;
}

void CpuConsumer::freeBufferLocked(int slotIndex) {
    ConsumerBase::freeBufferLocked(slotIndex);
}

} // namespace android
