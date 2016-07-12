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

#ifndef ANDROID_MONITORED_PRODUCER_H
#define ANDROID_MONITORED_PRODUCER_H

#include <gui/IGraphicBufferProducer.h>

namespace android {

class IProducerListener;
class NativeHandle;
class SurfaceFlinger;

// MonitoredProducer wraps an IGraphicBufferProducer so that SurfaceFlinger will
// be notified upon its destruction
class MonitoredProducer : public IGraphicBufferProducer {
public:
    MonitoredProducer(const sp<IGraphicBufferProducer>& producer,
            const sp<SurfaceFlinger>& flinger);
    virtual ~MonitoredProducer();

    // From IGraphicBufferProducer
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);
    virtual status_t setMaxDequeuedBufferCount(int maxDequeuedBuffers);
    virtual status_t setAsyncMode(bool async);
    virtual status_t dequeueBuffer(int* slot, sp<Fence>* fence, uint32_t w,
            uint32_t h, PixelFormat format, uint32_t usage);
    virtual status_t detachBuffer(int slot);
    virtual status_t detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence);
    virtual status_t attachBuffer(int* outSlot,
            const sp<GraphicBuffer>& buffer);
    virtual status_t queueBuffer(int slot, const QueueBufferInput& input,
            QueueBufferOutput* output);
    virtual status_t cancelBuffer(int slot, const sp<Fence>& fence);
    virtual int query(int what, int* value);
    virtual status_t connect(const sp<IProducerListener>& token, int api,
            bool producerControlledByApp, QueueBufferOutput* output);
    virtual status_t disconnect(int api);
    virtual status_t setSidebandStream(const sp<NativeHandle>& stream);
    virtual void allocateBuffers(uint32_t width, uint32_t height,
            PixelFormat format, uint32_t usage);
    virtual status_t allowAllocation(bool allow);
    virtual status_t setGenerationNumber(uint32_t generationNumber);
    virtual String8 getConsumerName() const override;
    virtual status_t setDequeueTimeout(nsecs_t timeout) override;
    virtual status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence, float outTransformMatrix[16]) override;
    virtual IBinder* onAsBinder();
    virtual status_t setSharedBufferMode(bool sharedBufferMode) override;
    virtual status_t setAutoRefresh(bool autoRefresh) override;
    virtual status_t getUniqueId(uint64_t* outId) const override;

private:
    sp<IGraphicBufferProducer> mProducer;
    sp<SurfaceFlinger> mFlinger;
};

}; // namespace android

#endif // ANDROID_MONITORED_PRODUCER_H
