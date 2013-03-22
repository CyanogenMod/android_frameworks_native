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

#undef LOG_TAG
#define LOG_TAG "BQInterposer"
//#define LOG_NDEBUG 0

#include "BufferQueueInterposer.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

#define BQI_LOGV(x, ...) ALOGV("[%s] "x, mName.string(), ##__VA_ARGS__)
#define BQI_LOGD(x, ...) ALOGD("[%s] "x, mName.string(), ##__VA_ARGS__)
#define BQI_LOGI(x, ...) ALOGI("[%s] "x, mName.string(), ##__VA_ARGS__)
#define BQI_LOGW(x, ...) ALOGW("[%s] "x, mName.string(), ##__VA_ARGS__)
#define BQI_LOGE(x, ...) ALOGE("[%s] "x, mName.string(), ##__VA_ARGS__)

// Get an ID that's unique within this process.
static int32_t createProcessUniqueId() {
    static volatile int32_t globalCounter = 0;
    return android_atomic_inc(&globalCounter);
}

BufferQueueInterposer::BufferQueueInterposer(
        const sp<IGraphicBufferProducer>& sink, const String8& name)
:   mSink(sink),
    mName(name),
    mAcquired(false)
{
    BQI_LOGV("BufferQueueInterposer sink=%p", sink.get());
}

BufferQueueInterposer::~BufferQueueInterposer() {
    Mutex::Autolock lock(mMutex);
    flushQueuedBuffersLocked();
    BQI_LOGV("~BufferQueueInterposer");
}

status_t BufferQueueInterposer::requestBuffer(int slot,
        sp<GraphicBuffer>* outBuf) {
    BQI_LOGV("requestBuffer slot=%d", slot);
    Mutex::Autolock lock(mMutex);

    if (size_t(slot) >= mBuffers.size()) {
        size_t size = mBuffers.size();
        mBuffers.insertAt(size, size - slot + 1);
    }
    sp<GraphicBuffer>& buf = mBuffers.editItemAt(slot);

    status_t result = mSink->requestBuffer(slot, &buf);
    *outBuf = buf;
    return result;
}

status_t BufferQueueInterposer::setBufferCount(int bufferCount) {
    BQI_LOGV("setBufferCount count=%d", bufferCount);
    Mutex::Autolock lock(mMutex);

    bufferCount += 1;

    status_t result = flushQueuedBuffersLocked();
    if (result != NO_ERROR)
        return result;

    result = mSink->setBufferCount(bufferCount);
    if (result != NO_ERROR)
        return result;

    for (size_t i = 0; i < mBuffers.size(); i++)
        mBuffers.editItemAt(i).clear();
    ssize_t n = mBuffers.resize(bufferCount);
    result = (n < 0) ? n : result;

    return result;
}

status_t BufferQueueInterposer::dequeueBuffer(int* slot, sp<Fence>* fence,
            uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    BQI_LOGV("dequeueBuffer %ux%u fmt=%u usage=%#x", w, h, format, usage);
    return mSink->dequeueBuffer(slot, fence, w, h, format, usage);
}

status_t BufferQueueInterposer::queueBuffer(int slot,
            const QueueBufferInput& input, QueueBufferOutput* output) {
    BQI_LOGV("queueBuffer slot=%d", slot);
    Mutex::Autolock lock(mMutex);
    mQueue.push(QueuedBuffer(slot, input));
    *output = mQueueBufferOutput;
    return NO_ERROR;
}

void BufferQueueInterposer::cancelBuffer(int slot, const sp<Fence>& fence) {
    BQI_LOGV("cancelBuffer slot=%d", slot);
    mSink->cancelBuffer(slot, fence);
}

int BufferQueueInterposer::query(int what, int* value) {
    BQI_LOGV("query what=%d", what);
    return mSink->query(what, value);
}

status_t BufferQueueInterposer::setSynchronousMode(bool enabled) {
    BQI_LOGV("setSynchronousMode %s", enabled ? "true" : "false");
    return mSink->setSynchronousMode(enabled);
}

status_t BufferQueueInterposer::connect(int api, QueueBufferOutput* output) {
    BQI_LOGV("connect api=%d", api);
    Mutex::Autolock lock(mMutex);
    status_t result = mSink->connect(api, &mQueueBufferOutput);
    if (result == NO_ERROR) {
        *output = mQueueBufferOutput;
    }
    return result;
}

status_t BufferQueueInterposer::disconnect(int api) {
    BQI_LOGV("disconnect: api=%d", api);
    Mutex::Autolock lock(mMutex);
    flushQueuedBuffersLocked();
    return mSink->disconnect(api);
}

status_t BufferQueueInterposer::pullEmptyBuffer() {
    status_t result;

    int slot;
    sp<Fence> fence;
    result = dequeueBuffer(&slot, &fence, 0, 0, 0, 0);
    if (result == IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) {
        sp<GraphicBuffer> buffer;
        result = requestBuffer(slot, &buffer);
    } else if (result != NO_ERROR) {
        return result;
    }

    uint32_t w, h, transformHint, numPendingBuffers;
    mQueueBufferOutput.deflate(&w, &h, &transformHint, &numPendingBuffers);

    IGraphicBufferProducer::QueueBufferInput qbi(0, Rect(w, h),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, fence);
    IGraphicBufferProducer::QueueBufferOutput qbo;
    result = queueBuffer(slot, qbi, &qbo);
    if (result != NO_ERROR)
        return result;

    return NO_ERROR;
}

status_t BufferQueueInterposer::acquireBuffer(sp<GraphicBuffer>* buf,
        sp<Fence>* fence) {
    Mutex::Autolock lock(mMutex);
    if (mQueue.empty()) {
        BQI_LOGV("acquireBuffer: no buffers available");
        return NO_BUFFER_AVAILABLE;
    }
    if (mAcquired) {
        BQI_LOGE("acquireBuffer: buffer already acquired");
        return BUFFER_ALREADY_ACQUIRED;
    }
    BQI_LOGV("acquireBuffer: acquiring slot %d", mQueue[0].slot);

    *buf = mBuffers[mQueue[0].slot];
    *fence = mQueue[0].fence;
    mAcquired = true;
    return NO_ERROR;
}

status_t BufferQueueInterposer::releaseBuffer(const sp<Fence>& fence) {
    Mutex::Autolock lock(mMutex);
    if (!mAcquired) {
        BQI_LOGE("releaseBuffer: releasing a non-acquired buffer");
        return BUFFER_NOT_ACQUIRED;
    }
    BQI_LOGV("releaseBuffer: releasing slot %d to sink", mQueue[0].slot);

    const QueuedBuffer& b = mQueue[0];
    status_t result = mSink->queueBuffer(b.slot,
            QueueBufferInput(b.timestamp, b.crop, b.scalingMode,
                b.transform, b.fence),
            &mQueueBufferOutput);
    mQueue.removeAt(0);
    mAcquired = false;

    return result;
}

status_t BufferQueueInterposer::flushQueuedBuffersLocked() {
    if (mAcquired) {
        BQI_LOGE("flushQueuedBuffersLocked: buffer acquired, can't flush");
        return INVALID_OPERATION;
    }

    status_t result = NO_ERROR;
    for (size_t i = 0; i < mQueue.size(); i++) {
        const QueuedBuffer& b = mQueue[i];
        BQI_LOGV("flushing queued slot %d to sink", b.slot);
        status_t err = mSink->queueBuffer(b.slot,
                QueueBufferInput(b.timestamp, b.crop, b.scalingMode,
                    b.transform, b.fence),
                &mQueueBufferOutput);
        if (err != NO_ERROR && result == NO_ERROR) // latch first error
            result = err;
    }
    mQueue.clear();
    return result;
}

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------
