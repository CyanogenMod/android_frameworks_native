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

#define LOG_TAG "StreamSplitter_test"
//#define LOG_NDEBUG 0

#include <gui/BufferQueue.h>
#include <gui/IConsumerListener.h>
#include <gui/ISurfaceComposer.h>
#include <gui/StreamSplitter.h>
#include <private/gui/ComposerService.h>

#include <gtest/gtest.h>

namespace android {

class StreamSplitterTest : public ::testing::Test {

protected:
    StreamSplitterTest() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("Begin test: %s.%s", testInfo->test_case_name(),
                testInfo->name());
    }

    ~StreamSplitterTest() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("End test:   %s.%s", testInfo->test_case_name(),
                testInfo->name());
    }
};

struct DummyListener : public BnConsumerListener {
    virtual void onFrameAvailable(const BufferItem& /* item */) {}
    virtual void onBuffersReleased() {}
    virtual void onSidebandStreamChanged() {}
};

class CountedAllocator : public BnGraphicBufferAlloc {
public:
    CountedAllocator() : mAllocCount(0) {
        sp<ISurfaceComposer> composer(ComposerService::getComposerService());
        mAllocator = composer->createGraphicBufferAlloc();
    }

    virtual ~CountedAllocator() {}

    virtual sp<GraphicBuffer> createGraphicBuffer(uint32_t w, uint32_t h,
            PixelFormat format, uint32_t usage, status_t* error) {
        ++mAllocCount;
        sp<GraphicBuffer> buffer = mAllocator->createGraphicBuffer(w, h, format,
                usage, error);
        return buffer;
    }

    int getAllocCount() const { return mAllocCount; }

private:
    sp<IGraphicBufferAlloc> mAllocator;
    int mAllocCount;
};

TEST_F(StreamSplitterTest, OneInputOneOutput) {
    sp<CountedAllocator> allocator(new CountedAllocator);

    sp<IGraphicBufferProducer> inputProducer;
    sp<IGraphicBufferConsumer> inputConsumer;
    BufferQueue::createBufferQueue(&inputProducer, &inputConsumer, allocator);

    sp<IGraphicBufferProducer> outputProducer;
    sp<IGraphicBufferConsumer> outputConsumer;
    BufferQueue::createBufferQueue(&outputProducer, &outputConsumer, allocator);
    ASSERT_EQ(OK, outputConsumer->consumerConnect(new DummyListener, false));

    sp<StreamSplitter> splitter;
    status_t status = StreamSplitter::createSplitter(inputConsumer, &splitter);
    ASSERT_EQ(OK, status);
    ASSERT_EQ(OK, splitter->addOutput(outputProducer));

    IGraphicBufferProducer::QueueBufferOutput qbOutput;
    ASSERT_EQ(OK, inputProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &qbOutput));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, inputProducer->requestBuffer(slot, &buffer));

    uint32_t* dataIn;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = 0x12345678;
    ASSERT_EQ(OK, buffer->unlock());

    IGraphicBufferProducer::QueueBufferInput qbInput(0, false,
            Rect(0, 0, 1, 1), NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, false,
            Fence::NO_FENCE);
    ASSERT_EQ(OK, inputProducer->queueBuffer(slot, qbInput, &qbOutput));

    IGraphicBufferConsumer::BufferItem item;
    ASSERT_EQ(OK, outputConsumer->acquireBuffer(&item, 0));

    uint32_t* dataOut;
    ASSERT_EQ(OK, item.mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
            reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(*dataOut, 0x12345678);
    ASSERT_EQ(OK, item.mGraphicBuffer->unlock());

    ASSERT_EQ(OK, outputConsumer->releaseBuffer(item.mBuf, item.mFrameNumber,
            EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, Fence::NO_FENCE));

    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));

    ASSERT_EQ(1, allocator->getAllocCount());
}

TEST_F(StreamSplitterTest, OneInputMultipleOutputs) {
    const int NUM_OUTPUTS = 4;
    sp<CountedAllocator> allocator(new CountedAllocator);

    sp<IGraphicBufferProducer> inputProducer;
    sp<IGraphicBufferConsumer> inputConsumer;
    BufferQueue::createBufferQueue(&inputProducer, &inputConsumer, allocator);

    sp<IGraphicBufferProducer> outputProducers[NUM_OUTPUTS] = {};
    sp<IGraphicBufferConsumer> outputConsumers[NUM_OUTPUTS] = {};
    for (int output = 0; output < NUM_OUTPUTS; ++output) {
        BufferQueue::createBufferQueue(&outputProducers[output],
                &outputConsumers[output], allocator);
        ASSERT_EQ(OK, outputConsumers[output]->consumerConnect(
                    new DummyListener, false));
    }

    sp<StreamSplitter> splitter;
    status_t status = StreamSplitter::createSplitter(inputConsumer, &splitter);
    ASSERT_EQ(OK, status);
    for (int output = 0; output < NUM_OUTPUTS; ++output) {
        ASSERT_EQ(OK, splitter->addOutput(outputProducers[output]));
    }

    IGraphicBufferProducer::QueueBufferOutput qbOutput;
    ASSERT_EQ(OK, inputProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &qbOutput));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, inputProducer->requestBuffer(slot, &buffer));

    uint32_t* dataIn;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = 0x12345678;
    ASSERT_EQ(OK, buffer->unlock());

    IGraphicBufferProducer::QueueBufferInput qbInput(0, false,
            Rect(0, 0, 1, 1), NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, false,
            Fence::NO_FENCE);
    ASSERT_EQ(OK, inputProducer->queueBuffer(slot, qbInput, &qbOutput));

    for (int output = 0; output < NUM_OUTPUTS; ++output) {
        IGraphicBufferConsumer::BufferItem item;
        ASSERT_EQ(OK, outputConsumers[output]->acquireBuffer(&item, 0));

        uint32_t* dataOut;
        ASSERT_EQ(OK, item.mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
                    reinterpret_cast<void**>(&dataOut)));
        ASSERT_EQ(*dataOut, 0x12345678);
        ASSERT_EQ(OK, item.mGraphicBuffer->unlock());

        ASSERT_EQ(OK, outputConsumers[output]->releaseBuffer(item.mBuf,
                    item.mFrameNumber, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR,
                    Fence::NO_FENCE));
    }

    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));

    ASSERT_EQ(1, allocator->getAllocCount());
}

TEST_F(StreamSplitterTest, OutputAbandonment) {
    sp<IGraphicBufferProducer> inputProducer;
    sp<IGraphicBufferConsumer> inputConsumer;
    BufferQueue::createBufferQueue(&inputProducer, &inputConsumer);

    sp<IGraphicBufferProducer> outputProducer;
    sp<IGraphicBufferConsumer> outputConsumer;
    BufferQueue::createBufferQueue(&outputProducer, &outputConsumer);
    ASSERT_EQ(OK, outputConsumer->consumerConnect(new DummyListener, false));

    sp<StreamSplitter> splitter;
    status_t status = StreamSplitter::createSplitter(inputConsumer, &splitter);
    ASSERT_EQ(OK, status);
    ASSERT_EQ(OK, splitter->addOutput(outputProducer));

    IGraphicBufferProducer::QueueBufferOutput qbOutput;
    ASSERT_EQ(OK, inputProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &qbOutput));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, inputProducer->requestBuffer(slot, &buffer));

    // Abandon the output
    outputConsumer->consumerDisconnect();

    IGraphicBufferProducer::QueueBufferInput qbInput(0, false,
            Rect(0, 0, 1, 1), NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, false,
            Fence::NO_FENCE);
    ASSERT_EQ(OK, inputProducer->queueBuffer(slot, qbInput, &qbOutput));

    // Input should be abandoned
    ASSERT_EQ(NO_INIT, inputProducer->dequeueBuffer(&slot, &fence, false, 0, 0,
            0, GRALLOC_USAGE_SW_WRITE_OFTEN));
}

} // namespace android
