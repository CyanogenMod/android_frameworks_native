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

#define LOG_TAG "BufferQueue_test"
//#define LOG_NDEBUG 0

#include "DummyConsumer.h"

#include <gui/BufferItem.h>
#include <gui/BufferQueue.h>
#include <gui/IProducerListener.h>

#include <ui/GraphicBuffer.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <utils/String8.h>
#include <utils/threads.h>

#include <gtest/gtest.h>

namespace android {

class BufferQueueTest : public ::testing::Test {

public:
protected:
    BufferQueueTest() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("Begin test: %s.%s", testInfo->test_case_name(),
                testInfo->name());
    }

    ~BufferQueueTest() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("End test:   %s.%s", testInfo->test_case_name(),
                testInfo->name());
    }

    void GetMinUndequeuedBufferCount(int* bufferCount) {
        ASSERT_TRUE(bufferCount != NULL);
        ASSERT_EQ(OK, mProducer->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                    bufferCount));
        ASSERT_GE(*bufferCount, 0);
    }

    void createBufferQueue() {
        BufferQueue::createBufferQueue(&mProducer, &mConsumer);
    }

    sp<IGraphicBufferProducer> mProducer;
    sp<IGraphicBufferConsumer> mConsumer;
};

static const uint32_t TEST_DATA = 0x12345678u;

// XXX: Tests that fork a process to hold the BufferQueue must run before tests
// that use a local BufferQueue, or else Binder will get unhappy
TEST_F(BufferQueueTest, BufferQueueInAnotherProcess) {
    const String16 PRODUCER_NAME = String16("BQTestProducer");
    const String16 CONSUMER_NAME = String16("BQTestConsumer");

    pid_t forkPid = fork();
    ASSERT_NE(forkPid, -1);

    if (forkPid == 0) {
        // Child process
        sp<IGraphicBufferProducer> producer;
        sp<IGraphicBufferConsumer> consumer;
        BufferQueue::createBufferQueue(&producer, &consumer);
        sp<IServiceManager> serviceManager = defaultServiceManager();
        serviceManager->addService(PRODUCER_NAME, IInterface::asBinder(producer));
        serviceManager->addService(CONSUMER_NAME, IInterface::asBinder(consumer));
        ProcessState::self()->startThreadPool();
        IPCThreadState::self()->joinThreadPool();
        LOG_ALWAYS_FATAL("Shouldn't be here");
    }

    sp<IServiceManager> serviceManager = defaultServiceManager();
    sp<IBinder> binderProducer =
        serviceManager->getService(PRODUCER_NAME);
    mProducer = interface_cast<IGraphicBufferProducer>(binderProducer);
    EXPECT_TRUE(mProducer != NULL);
    sp<IBinder> binderConsumer =
        serviceManager->getService(CONSUMER_NAME);
    mConsumer = interface_cast<IGraphicBufferConsumer>(binderConsumer);
    EXPECT_TRUE(mConsumer != NULL);

    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, false));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK,
            mProducer->connect(NULL, NATIVE_WINDOW_API_CPU, false, &output));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));

    uint32_t* dataIn;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = TEST_DATA;
    ASSERT_EQ(OK, buffer->unlock());

    IGraphicBufferProducer::QueueBufferInput input(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));

    BufferItem item;
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));

    uint32_t* dataOut;
    ASSERT_EQ(OK, item.mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
            reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(*dataOut, TEST_DATA);
    ASSERT_EQ(OK, item.mGraphicBuffer->unlock());
}

TEST_F(BufferQueueTest, AcquireBuffer_ExceedsMaxAcquireCount_Fails) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    mConsumer->consumerConnect(dc, false);
    IGraphicBufferProducer::QueueBufferOutput qbo;
    mProducer->connect(new DummyProducerListener, NATIVE_WINDOW_API_CPU, false,
            &qbo);
    mProducer->setMaxDequeuedBufferCount(3);

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buf;
    IGraphicBufferProducer::QueueBufferInput qbi(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    BufferItem item;

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
                mProducer->dequeueBuffer(&slot, &fence, 1, 1, 0,
                    GRALLOC_USAGE_SW_READ_OFTEN));
        ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buf));
        ASSERT_EQ(OK, mProducer->queueBuffer(slot, qbi, &qbo));
        ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));
    }

    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 1, 1, 0,
                GRALLOC_USAGE_SW_READ_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buf));
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, qbi, &qbo));

    // Acquire the third buffer, which should fail.
    ASSERT_EQ(INVALID_OPERATION, mConsumer->acquireBuffer(&item, 0));
}

TEST_F(BufferQueueTest, SetMaxAcquiredBufferCountWithIllegalValues_ReturnsError) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    mConsumer->consumerConnect(dc, false);

    EXPECT_EQ(OK, mConsumer->setMaxBufferCount(10));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(10));

    IGraphicBufferProducer::QueueBufferOutput qbo;
    mProducer->connect(new DummyProducerListener, NATIVE_WINDOW_API_CPU, false,
            &qbo);
    mProducer->setMaxDequeuedBufferCount(3);

    int minBufferCount;
    ASSERT_NO_FATAL_FAILURE(GetMinUndequeuedBufferCount(&minBufferCount));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(
                minBufferCount - 1));

    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(0));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(-3));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(
            BufferQueue::MAX_MAX_ACQUIRED_BUFFERS+1));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(100));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buf;
    IGraphicBufferProducer::QueueBufferInput qbi(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    BufferItem item;
    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(3));
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
                mProducer->dequeueBuffer(&slot, &fence, 1, 1, 0,
                    GRALLOC_USAGE_SW_READ_OFTEN));
        ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buf));
        ASSERT_EQ(OK, mProducer->queueBuffer(slot, qbi, &qbo));
        ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));
    }

    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxAcquiredBufferCount(2));
}

TEST_F(BufferQueueTest, SetMaxAcquiredBufferCountWithLegalValues_Succeeds) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    mConsumer->consumerConnect(dc, false);

    IGraphicBufferProducer::QueueBufferOutput qbo;
    mProducer->connect(new DummyProducerListener, NATIVE_WINDOW_API_CPU, false,
            &qbo);
    mProducer->setMaxDequeuedBufferCount(2);

    int minBufferCount;
    ASSERT_NO_FATAL_FAILURE(GetMinUndequeuedBufferCount(&minBufferCount));

    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(1));
    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(2));
    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(minBufferCount));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buf;
    IGraphicBufferProducer::QueueBufferInput qbi(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    BufferItem item;

    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 1, 1, 0,
            GRALLOC_USAGE_SW_READ_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buf));
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, qbi, &qbo));
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));

    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(3));

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
                mProducer->dequeueBuffer(&slot, &fence, 1, 1, 0,
                GRALLOC_USAGE_SW_READ_OFTEN));
        ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buf));
        ASSERT_EQ(OK, mProducer->queueBuffer(slot, qbi, &qbo));
        ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));
    }

    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(
            BufferQueue::MAX_MAX_ACQUIRED_BUFFERS));
}

TEST_F(BufferQueueTest, SetMaxBufferCountWithLegalValues_Succeeds) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    mConsumer->consumerConnect(dc, false);

    // Test single buffer mode
    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(1));
}

TEST_F(BufferQueueTest, SetMaxBufferCountWithIllegalValues_ReturnsError) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    mConsumer->consumerConnect(dc, false);

    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxBufferCount(0));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxBufferCount(
            BufferQueue::NUM_BUFFER_SLOTS + 1));

    EXPECT_EQ(OK, mConsumer->setMaxAcquiredBufferCount(5));
    EXPECT_EQ(BAD_VALUE, mConsumer->setMaxBufferCount(3));
}

TEST_F(BufferQueueTest, DetachAndReattachOnProducerSide) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, false));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &output));

    ASSERT_EQ(BAD_VALUE, mProducer->detachBuffer(-1)); // Index too low
    ASSERT_EQ(BAD_VALUE, mProducer->detachBuffer(
                BufferQueueDefs::NUM_BUFFER_SLOTS)); // Index too high
    ASSERT_EQ(BAD_VALUE, mProducer->detachBuffer(0)); // Not dequeued

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(BAD_VALUE, mProducer->detachBuffer(slot)); // Not requested
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));
    ASSERT_EQ(OK, mProducer->detachBuffer(slot));
    ASSERT_EQ(BAD_VALUE, mProducer->detachBuffer(slot)); // Not dequeued

    sp<GraphicBuffer> safeToClobberBuffer;
    // Can no longer request buffer from this slot
    ASSERT_EQ(BAD_VALUE, mProducer->requestBuffer(slot, &safeToClobberBuffer));

    uint32_t* dataIn;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = TEST_DATA;
    ASSERT_EQ(OK, buffer->unlock());

    int newSlot;
    ASSERT_EQ(BAD_VALUE, mProducer->attachBuffer(NULL, safeToClobberBuffer));
    ASSERT_EQ(BAD_VALUE, mProducer->attachBuffer(&newSlot, NULL));

    ASSERT_EQ(OK, mProducer->attachBuffer(&newSlot, buffer));
    IGraphicBufferProducer::QueueBufferInput input(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(newSlot, input, &output));

    BufferItem item;
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, static_cast<nsecs_t>(0)));

    uint32_t* dataOut;
    ASSERT_EQ(OK, item.mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
            reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(*dataOut, TEST_DATA);
    ASSERT_EQ(OK, item.mGraphicBuffer->unlock());
}

TEST_F(BufferQueueTest, DetachAndReattachOnConsumerSide) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, false));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &output));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));
    IGraphicBufferProducer::QueueBufferInput input(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));

    ASSERT_EQ(BAD_VALUE, mConsumer->detachBuffer(-1)); // Index too low
    ASSERT_EQ(BAD_VALUE, mConsumer->detachBuffer(
            BufferQueueDefs::NUM_BUFFER_SLOTS)); // Index too high
    ASSERT_EQ(BAD_VALUE, mConsumer->detachBuffer(0)); // Not acquired

    BufferItem item;
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, static_cast<nsecs_t>(0)));

    ASSERT_EQ(OK, mConsumer->detachBuffer(item.mSlot));
    ASSERT_EQ(BAD_VALUE, mConsumer->detachBuffer(item.mSlot)); // Not acquired

    uint32_t* dataIn;
    ASSERT_EQ(OK, item.mGraphicBuffer->lock(
            GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = TEST_DATA;
    ASSERT_EQ(OK, item.mGraphicBuffer->unlock());

    int newSlot;
    sp<GraphicBuffer> safeToClobberBuffer;
    ASSERT_EQ(BAD_VALUE, mConsumer->attachBuffer(NULL, safeToClobberBuffer));
    ASSERT_EQ(BAD_VALUE, mConsumer->attachBuffer(&newSlot, NULL));
    ASSERT_EQ(OK, mConsumer->attachBuffer(&newSlot, item.mGraphicBuffer));

    ASSERT_EQ(OK, mConsumer->releaseBuffer(newSlot, 0, EGL_NO_DISPLAY,
            EGL_NO_SYNC_KHR, Fence::NO_FENCE));

    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));

    uint32_t* dataOut;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
            reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(*dataOut, TEST_DATA);
    ASSERT_EQ(OK, buffer->unlock());
}

TEST_F(BufferQueueTest, MoveFromConsumerToProducer) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, false));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, false, &output));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
                    GRALLOC_USAGE_SW_WRITE_OFTEN));
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));

    uint32_t* dataIn;
    ASSERT_EQ(OK, buffer->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
            reinterpret_cast<void**>(&dataIn)));
    *dataIn = TEST_DATA;
    ASSERT_EQ(OK, buffer->unlock());

    IGraphicBufferProducer::QueueBufferInput input(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));

    BufferItem item;
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, static_cast<nsecs_t>(0)));
    ASSERT_EQ(OK, mConsumer->detachBuffer(item.mSlot));

    int newSlot;
    ASSERT_EQ(OK, mProducer->attachBuffer(&newSlot, item.mGraphicBuffer));
    ASSERT_EQ(OK, mProducer->queueBuffer(newSlot, input, &output));
    ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, static_cast<nsecs_t>(0)));

    uint32_t* dataOut;
    ASSERT_EQ(OK, item.mGraphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN,
            reinterpret_cast<void**>(&dataOut)));
    ASSERT_EQ(*dataOut, TEST_DATA);
    ASSERT_EQ(OK, item.mGraphicBuffer->unlock());
}

TEST_F(BufferQueueTest, TestDisallowingAllocation) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, true));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, true, &output));

    static const uint32_t WIDTH = 320;
    static const uint32_t HEIGHT = 240;

    ASSERT_EQ(OK, mConsumer->setDefaultBufferSize(WIDTH, HEIGHT));

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    // This should return an error since it would require an allocation
    ASSERT_EQ(OK, mProducer->allowAllocation(false));
    ASSERT_EQ(WOULD_BLOCK, mProducer->dequeueBuffer(&slot, &fence, 0, 0,
            0, GRALLOC_USAGE_SW_WRITE_OFTEN));

    // This should succeed, now that we've lifted the prohibition
    ASSERT_EQ(OK, mProducer->allowAllocation(true));
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0,
            GRALLOC_USAGE_SW_WRITE_OFTEN));

    // Release the previous buffer back to the BufferQueue
    mProducer->cancelBuffer(slot, fence);

    // This should fail since we're requesting a different size
    ASSERT_EQ(OK, mProducer->allowAllocation(false));
    ASSERT_EQ(WOULD_BLOCK, mProducer->dequeueBuffer(&slot, &fence,
            WIDTH * 2, HEIGHT * 2, 0, GRALLOC_USAGE_SW_WRITE_OFTEN));
}

TEST_F(BufferQueueTest, TestGenerationNumbers) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, true));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, true, &output));

    ASSERT_EQ(OK, mProducer->setGenerationNumber(1));

    // Get one buffer to play with
    int slot;
    sp<Fence> fence;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, 0));

    sp<GraphicBuffer> buffer;
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));

    // Ensure that the generation number we set propagates to allocated buffers
    ASSERT_EQ(1U, buffer->getGenerationNumber());

    ASSERT_EQ(OK, mProducer->detachBuffer(slot));

    ASSERT_EQ(OK, mProducer->setGenerationNumber(2));

    // These should fail, since we've changed the generation number on the queue
    int outSlot;
    ASSERT_EQ(BAD_VALUE, mProducer->attachBuffer(&outSlot, buffer));
    ASSERT_EQ(BAD_VALUE, mConsumer->attachBuffer(&outSlot, buffer));

    buffer->setGenerationNumber(2);

    // This should succeed now that we've changed the buffer's generation number
    ASSERT_EQ(OK, mProducer->attachBuffer(&outSlot, buffer));

    ASSERT_EQ(OK, mProducer->detachBuffer(outSlot));

    // This should also succeed with the new generation number
    ASSERT_EQ(OK, mConsumer->attachBuffer(&outSlot, buffer));
}

TEST_F(BufferQueueTest, TestSingleBufferMode) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, true));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, true, &output));

    ASSERT_EQ(OK, mProducer->setSingleBufferMode(true));

    // Get a buffer
    int singleSlot;
    sp<Fence> fence;
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&singleSlot, &fence, 0, 0, 0, 0));
    ASSERT_EQ(OK, mProducer->requestBuffer(singleSlot, &buffer));

    // Queue the buffer
    IGraphicBufferProducer::QueueBufferInput input(0, false,
            HAL_DATASPACE_UNKNOWN, Rect(0, 0, 1, 1),
            NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
    ASSERT_EQ(OK, mProducer->queueBuffer(singleSlot, input, &output));

    // Repeatedly acquire and release a buffer from the consumer side, it should
    // always return the same one.
    BufferItem item;
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));
        ASSERT_EQ(singleSlot, item.mSlot);
        ASSERT_EQ(0, item.mTimestamp);
        ASSERT_EQ(false, item.mIsAutoTimestamp);
        ASSERT_EQ(HAL_DATASPACE_UNKNOWN, item.mDataSpace);
        ASSERT_EQ(Rect(0, 0, 1, 1), item.mCrop);
        ASSERT_EQ(NATIVE_WINDOW_SCALING_MODE_FREEZE, item.mScalingMode);
        ASSERT_EQ(0u, item.mTransform);
        ASSERT_EQ(Fence::NO_FENCE, item.mFence);

        ASSERT_EQ(OK, mConsumer->releaseBuffer(item.mSlot, item.mFrameNumber,
                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, Fence::NO_FENCE));
    }

    // Repeatedly queue and dequeue a buffer from the producer side, it should
    // always return the same one.
    int slot;
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(OK, mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, 0));
        ASSERT_EQ(singleSlot, slot);
        ASSERT_EQ(OK, mProducer->queueBuffer(singleSlot, input, &output));
    }

    // Repeatedly acquire and release a buffer from the consumer side, it should
    // always return the same one. First grabbing them from the queue and then
    // when the queue is empty, returning the single buffer.
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(OK, mConsumer->acquireBuffer(&item, 0));
        ASSERT_EQ(singleSlot, item.mSlot);
        ASSERT_EQ(0, item.mTimestamp);
        ASSERT_EQ(false, item.mIsAutoTimestamp);
        ASSERT_EQ(HAL_DATASPACE_UNKNOWN, item.mDataSpace);
        ASSERT_EQ(Rect(0, 0, 1, 1), item.mCrop);
        ASSERT_EQ(NATIVE_WINDOW_SCALING_MODE_FREEZE, item.mScalingMode);
        ASSERT_EQ(0u, item.mTransform);
        ASSERT_EQ(Fence::NO_FENCE, item.mFence);

        ASSERT_EQ(OK, mConsumer->releaseBuffer(item.mSlot, item.mFrameNumber,
                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, Fence::NO_FENCE));
    }
}

TEST_F(BufferQueueTest, TestTimeouts) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, true));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, true, &output));

    // Fill up the queue. Since the controlledByApp flags are set to true, this
    // queue should be in non-blocking mode, and we should be recycling the same
    // two buffers
    for (int i = 0; i < 5; ++i) {
        int slot = BufferQueue::INVALID_BUFFER_SLOT;
        sp<Fence> fence = Fence::NO_FENCE;
        auto result = mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, 0);
        if (i < 2) {
            ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
                    result);
        } else {
            ASSERT_EQ(OK, result);
        }
        sp<GraphicBuffer> buffer;
        ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));
        IGraphicBufferProducer::QueueBufferInput input(0ull, true,
                HAL_DATASPACE_UNKNOWN, Rect::INVALID_RECT,
                NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
        IGraphicBufferProducer::QueueBufferOutput output{};
        ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));
    }

    const auto TIMEOUT = ms2ns(250);
    mProducer->setDequeueTimeout(TIMEOUT);

    // Setting a timeout will change the BufferQueue into blocking mode (with
    // one droppable buffer in the queue and one free from the previous
    // dequeue/queues), so dequeue and queue two more buffers: one to replace
    // the current droppable buffer, and a second to max out the buffer count
    sp<GraphicBuffer> buffer; // Save a buffer to attach later
    for (int i = 0; i < 2; ++i) {
        int slot = BufferQueue::INVALID_BUFFER_SLOT;
        sp<Fence> fence = Fence::NO_FENCE;
        ASSERT_EQ(OK, mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, 0));
        ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));
        IGraphicBufferProducer::QueueBufferInput input(0ull, true,
                HAL_DATASPACE_UNKNOWN, Rect::INVALID_RECT,
                NATIVE_WINDOW_SCALING_MODE_FREEZE, 0, Fence::NO_FENCE);
        ASSERT_EQ(OK, mProducer->queueBuffer(slot, input, &output));
    }

    int slot = BufferQueue::INVALID_BUFFER_SLOT;
    sp<Fence> fence = Fence::NO_FENCE;
    auto startTime = systemTime();
    ASSERT_EQ(TIMED_OUT, mProducer->dequeueBuffer(&slot, &fence, 0, 0, 0, 0));
    ASSERT_GE(systemTime() - startTime, TIMEOUT);

    // We're technically attaching the same buffer multiple times (since we
    // queued it previously), but that doesn't matter for this test
    startTime = systemTime();
    ASSERT_EQ(TIMED_OUT, mProducer->attachBuffer(&slot, buffer));
    ASSERT_GE(systemTime() - startTime, TIMEOUT);
}

TEST_F(BufferQueueTest, CanAttachWhileDisallowingAllocation) {
    createBufferQueue();
    sp<DummyConsumer> dc(new DummyConsumer);
    ASSERT_EQ(OK, mConsumer->consumerConnect(dc, true));
    IGraphicBufferProducer::QueueBufferOutput output;
    ASSERT_EQ(OK, mProducer->connect(new DummyProducerListener,
            NATIVE_WINDOW_API_CPU, true, &output));

    int slot = BufferQueue::INVALID_BUFFER_SLOT;
    sp<Fence> sourceFence;
    ASSERT_EQ(IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&slot, &sourceFence, 0, 0, 0, 0));
    sp<GraphicBuffer> buffer;
    ASSERT_EQ(OK, mProducer->requestBuffer(slot, &buffer));
    ASSERT_EQ(OK, mProducer->detachBuffer(slot));

    ASSERT_EQ(OK, mProducer->allowAllocation(false));

    slot = BufferQueue::INVALID_BUFFER_SLOT;
    ASSERT_EQ(OK, mProducer->attachBuffer(&slot, buffer));
}

} // namespace android
