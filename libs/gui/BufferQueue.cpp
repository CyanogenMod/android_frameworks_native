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

#define LOG_TAG "BufferQueue"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <gui/BufferQueue.h>
#include <gui/BufferQueueCore.h>

namespace android {

BufferQueue::ProxyConsumerListener::ProxyConsumerListener(
        const wp<ConsumerListener>& consumerListener):
        mConsumerListener(consumerListener) {}

BufferQueue::ProxyConsumerListener::~ProxyConsumerListener() {}

void BufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}

void BufferQueue::ProxyConsumerListener::onBuffersReleased() {
    sp<ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
}

BufferQueue::BufferQueue(const sp<IGraphicBufferAlloc>& allocator) :
    mProducer(),
    mConsumer()
{
    sp<BufferQueueCore> core(new BufferQueueCore(allocator));
    mProducer = new BufferQueueProducer(core);
    mConsumer = new BufferQueueConsumer(core);
}

BufferQueue::~BufferQueue() {}

void BufferQueue::binderDied(const wp<IBinder>& who) {
    mProducer->binderDied(who);
}

int BufferQueue::query(int what, int* outValue) {
    return mProducer->query(what, outValue);
}

status_t BufferQueue::setBufferCount(int bufferCount) {
    return mProducer->setBufferCount(bufferCount);
}

status_t BufferQueue::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    return mProducer->requestBuffer(slot, buf);
}

status_t BufferQueue::dequeueBuffer(int *outBuf, sp<Fence>* outFence, bool async,
        uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    return mProducer->dequeueBuffer(outBuf, outFence, async, w, h, format, usage);
}

status_t BufferQueue::queueBuffer(int buf,
        const QueueBufferInput& input, QueueBufferOutput* output) {
    return mProducer->queueBuffer(buf, input, output);
}

void BufferQueue::cancelBuffer(int buf, const sp<Fence>& fence) {
    mProducer->cancelBuffer(buf, fence);
}

status_t BufferQueue::connect(const sp<IBinder>& token,
        int api, bool producerControlledByApp, QueueBufferOutput* output) {
    return mProducer->connect(token, api, producerControlledByApp, output);
}

status_t BufferQueue::disconnect(int api) {
    return mProducer->disconnect(api);
}

status_t BufferQueue::acquireBuffer(BufferItem* buffer, nsecs_t presentWhen) {
    return mConsumer->acquireBuffer(buffer, presentWhen);
}

status_t BufferQueue::releaseBuffer(
        int buf, uint64_t frameNumber, EGLDisplay display,
        EGLSyncKHR eglFence, const sp<Fence>& fence) {
    return mConsumer->releaseBuffer(buf, frameNumber, fence, display, eglFence);
}

status_t BufferQueue::consumerConnect(const sp<IConsumerListener>& consumerListener,
        bool controlledByApp) {
    return mConsumer->connect(consumerListener, controlledByApp);
}

status_t BufferQueue::consumerDisconnect() {
    return mConsumer->disconnect();
}

status_t BufferQueue::getReleasedBuffers(uint32_t* slotMask) {
    return mConsumer->getReleasedBuffers(slotMask);
}

status_t BufferQueue::setDefaultBufferSize(uint32_t w, uint32_t h) {
    return mConsumer->setDefaultBufferSize(w, h);
}

status_t BufferQueue::setDefaultMaxBufferCount(int bufferCount) {
    return mConsumer->setDefaultMaxBufferCount(bufferCount);
}

status_t BufferQueue::disableAsyncBuffer() {
    return mConsumer->disableAsyncBuffer();
}

status_t BufferQueue::setMaxAcquiredBufferCount(int maxAcquiredBuffers) {
    return mConsumer->setMaxAcquiredBufferCount(maxAcquiredBuffers);
}

void BufferQueue::setConsumerName(const String8& name) {
    mConsumer->setConsumerName(name);
}

status_t BufferQueue::setDefaultBufferFormat(uint32_t defaultFormat) {
    return mConsumer->setDefaultBufferFormat(defaultFormat);
}

status_t BufferQueue::setConsumerUsageBits(uint32_t usage) {
    return mConsumer->setConsumerUsageBits(usage);
}

status_t BufferQueue::setTransformHint(uint32_t hint) {
    return mConsumer->setTransformHint(hint);
}

void BufferQueue::dump(String8& result, const char* prefix) const {
    mConsumer->dump(result, prefix);
}

}; // namespace android
