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

#include "MessageQueue.h"
#include "MonitoredProducer.h"
#include "SurfaceFlinger.h"

namespace android {

MonitoredProducer::MonitoredProducer(const sp<IGraphicBufferProducer>& producer,
        const sp<SurfaceFlinger>& flinger) :
    mProducer(producer),
    mFlinger(flinger) {}

MonitoredProducer::~MonitoredProducer() {
    // Remove ourselves from SurfaceFlinger's list. We do this asynchronously
    // because we don't know where this destructor is called from. It could be
    // called with the mStateLock held, leading to a dead-lock (it actually
    // happens).
    class MessageCleanUpList : public MessageBase {
    public:
        MessageCleanUpList(const sp<SurfaceFlinger>& flinger,
                const wp<IBinder>& producer)
            : mFlinger(flinger), mProducer(producer) {}

        virtual ~MessageCleanUpList() {}

        virtual bool handler() {
            Mutex::Autolock _l(mFlinger->mStateLock);
            mFlinger->mGraphicBufferProducerList.remove(mProducer);
            return true;
        }

    private:
        sp<SurfaceFlinger> mFlinger;
        wp<IBinder> mProducer;
    };

    mFlinger->postMessageAsync(new MessageCleanUpList(mFlinger, asBinder()));
}

status_t MonitoredProducer::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    return mProducer->requestBuffer(slot, buf);
}

status_t MonitoredProducer::setBufferCount(int bufferCount) {
    return mProducer->setBufferCount(bufferCount);
}

status_t MonitoredProducer::dequeueBuffer(int* slot, sp<Fence>* fence,
        bool async, uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    return mProducer->dequeueBuffer(slot, fence, async, w, h, format, usage);
}

status_t MonitoredProducer::detachBuffer(int slot) {
    return mProducer->detachBuffer(slot);
}

status_t MonitoredProducer::detachNextBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence) {
    return mProducer->detachNextBuffer(outBuffer, outFence);
}

status_t MonitoredProducer::attachBuffer(int* outSlot,
        const sp<GraphicBuffer>& buffer) {
    return mProducer->attachBuffer(outSlot, buffer);
}

status_t MonitoredProducer::queueBuffer(int slot, const QueueBufferInput& input,
        QueueBufferOutput* output) {
    return mProducer->queueBuffer(slot, input, output);
}

void MonitoredProducer::cancelBuffer(int slot, const sp<Fence>& fence) {
    mProducer->cancelBuffer(slot, fence);
}

int MonitoredProducer::query(int what, int* value) {
    return mProducer->query(what, value);
}

status_t MonitoredProducer::connect(const sp<IProducerListener>& listener,
        int api, bool producerControlledByApp, QueueBufferOutput* output) {
    return mProducer->connect(listener, api, producerControlledByApp, output);
}

status_t MonitoredProducer::disconnect(int api) {
    return mProducer->disconnect(api);
}

status_t MonitoredProducer::setSidebandStream(const sp<NativeHandle>& stream) {
    return mProducer->setSidebandStream(stream);
}

void MonitoredProducer::allocateBuffers(bool async, uint32_t width,
        uint32_t height, uint32_t format, uint32_t usage) {
    mProducer->allocateBuffers(async, width, height, format, usage);
}

IBinder* MonitoredProducer::onAsBinder() {
    return mProducer->asBinder().get();
}

// ---------------------------------------------------------------------------
}; // namespace android
