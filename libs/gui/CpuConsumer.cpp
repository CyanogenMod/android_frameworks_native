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

// Get an ID that's unique within this process.
static int32_t createProcessUniqueId() {
    static volatile int32_t globalCounter = 0;
    return android_atomic_inc(&globalCounter);
}

CpuConsumer::CpuConsumer(uint32_t maxLockedBuffers) :
    mMaxLockedBuffers(maxLockedBuffers),
    mCurrentLockedBuffers(0)
{
    mName = String8::format("cc-unnamed-%d-%d", getpid(),
            createProcessUniqueId());

    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        mBufferPointers[i] = NULL;
    }

    mBufferQueue = new BufferQueue(true);

    wp<BufferQueue::ConsumerListener> listener;
    sp<BufferQueue::ConsumerListener> proxy;
    listener = static_cast<BufferQueue::ConsumerListener*>(this);
    proxy = new BufferQueue::ProxyConsumerListener(listener);

    status_t err = mBufferQueue->consumerConnect(proxy);
    if (err != NO_ERROR) {
        ALOGE("CpuConsumer: error connecting to BufferQueue: %s (%d)",
                strerror(-err), err);
    } else {
        mBufferQueue->setSynchronousMode(true);
        mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_SW_READ_OFTEN);
        mBufferQueue->setConsumerName(mName);
    }
}

CpuConsumer::~CpuConsumer()
{
    Mutex::Autolock _l(mMutex);
    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        freeBufferLocked(i);
    }
    mBufferQueue->consumerDisconnect();
    mBufferQueue.clear();
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

    err = mBufferQueue->acquireBuffer(&b);
    if (err != OK) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            return BAD_VALUE;
        } else {
            CC_LOGE("Error acquiring buffer: %s (%d)", strerror(err), err);
            return err;
        }
    }

    int buf = b.mBuf;

    if (b.mGraphicBuffer != NULL) {
        if (mBufferPointers[buf] != NULL) {
            CC_LOGE("Reallocation of buffer %d while in consumer use!", buf);
            mBufferQueue->releaseBuffer(buf, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR,
                    Fence::NO_FENCE);
            return BAD_VALUE;
        }
        mBufferSlot[buf] = b.mGraphicBuffer;
    }

    err = mBufferSlot[buf]->lock(
        GraphicBuffer::USAGE_SW_READ_OFTEN,
        b.mCrop,
        &mBufferPointers[buf]);

    if (mBufferPointers[buf] != NULL && err != OK) {
        CC_LOGE("Unable to lock buffer for CPU reading: %s (%d)", strerror(-err),
                err);
        return err;
    }

    nativeBuffer->data   = reinterpret_cast<uint8_t*>(mBufferPointers[buf]);
    nativeBuffer->width  = mBufferSlot[buf]->getWidth();
    nativeBuffer->height = mBufferSlot[buf]->getHeight();
    nativeBuffer->format = mBufferSlot[buf]->getPixelFormat();
    nativeBuffer->stride = mBufferSlot[buf]->getStride();

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
    int buf = 0;
    status_t err;

    void *bufPtr = reinterpret_cast<void *>(nativeBuffer.data);
    for (; buf < BufferQueue::NUM_BUFFER_SLOTS; buf++) {
        if (bufPtr == mBufferPointers[buf]) break;
    }
    if (buf == BufferQueue::NUM_BUFFER_SLOTS) {
        CC_LOGE("%s: Can't find buffer to free", __FUNCTION__);
        return BAD_VALUE;
    }

    mBufferPointers[buf] = NULL;
    err = mBufferSlot[buf]->unlock();
    if (err != OK) {
        CC_LOGE("%s: Unable to unlock graphic buffer %d", __FUNCTION__, buf);
        return err;
    }
    err = mBufferQueue->releaseBuffer(buf, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR,
            Fence::NO_FENCE);
    if (err == BufferQueue::STALE_BUFFER_SLOT) {
        freeBufferLocked(buf);
    } else if (err != OK) {
        CC_LOGE("%s: Unable to release graphic buffer %d to queue", __FUNCTION__,
                buf);
        return err;
    }

    mCurrentLockedBuffers--;

    return OK;
}

void CpuConsumer::setFrameAvailableListener(
        const sp<FrameAvailableListener>& listener) {
    CC_LOGV("setFrameAvailableListener");
    Mutex::Autolock lock(mMutex);
    mFrameAvailableListener = listener;
}


void CpuConsumer::onFrameAvailable() {
    CC_LOGV("onFrameAvailable");
    sp<FrameAvailableListener> listener;
    { // scope for the lock
        Mutex::Autolock _l(mMutex);
        listener = mFrameAvailableListener;
    }

    if (listener != NULL) {
        CC_LOGV("actually calling onFrameAvailable");
        listener->onFrameAvailable();
    }
}

void CpuConsumer::onBuffersReleased() {
    CC_LOGV("onBuffersReleased");

    Mutex::Autolock lock(mMutex);

    uint32_t mask = 0;
    mBufferQueue->getReleasedBuffers(&mask);
    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        if (mask & (1 << i)) {
            freeBufferLocked(i);
        }
    }

}

status_t CpuConsumer::freeBufferLocked(int buf) {
    status_t err = OK;

    if (mBufferPointers[buf] != NULL) {
        CC_LOGW("Buffer %d freed while locked by consumer", buf);
        mBufferPointers[buf] = NULL;
        err = mBufferSlot[buf]->unlock();
        if (err != OK) {
            CC_LOGE("%s: Unable to unlock graphic buffer %d", __FUNCTION__, buf);
        }
        mCurrentLockedBuffers--;
    }
    mBufferSlot[buf] = NULL;
    return err;
}

} // namespace android
