/*
 * Copyright (C) 2013 The Android Open Source Project
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

#define LOG_TAG "IGraphicBufferProducer_test"
//#define LOG_NDEBUG 0

#include <gtest/gtest.h>

#include <utils/String8.h>
#include <utils/threads.h>

#include <ui/GraphicBuffer.h>

#include <gui/BufferQueue.h>
#include <gui/IProducerListener.h>

#include <vector>

#define ASSERT_OK(x) ASSERT_EQ(OK, (x))
#define EXPECT_OK(x) EXPECT_EQ(OK, (x))

#define TEST_TOKEN ((IProducerListener*)(NULL))
#define TEST_API NATIVE_WINDOW_API_CPU
#define TEST_API_OTHER NATIVE_WINDOW_API_EGL // valid API that's not TEST_API
#define TEST_CONTROLLED_BY_APP false
#define TEST_PRODUCER_USAGE_BITS (0)

// TODO: Make these public constants in a header
enum {
    // Default dimensions before setDefaultBufferSize is called
    DEFAULT_WIDTH = 1,
    DEFAULT_HEIGHT = 1,

    // Default format before setDefaultBufferFormat is called
    DEFAULT_FORMAT = HAL_PIXEL_FORMAT_RGBA_8888,

    // Default transform hint before setTransformHint is called
    DEFAULT_TRANSFORM_HINT = 0,
};

namespace android {

namespace {
// Parameters for a generic "valid" input for queueBuffer.
const int64_t QUEUE_BUFFER_INPUT_TIMESTAMP = 1384888611;
const bool QUEUE_BUFFER_INPUT_IS_AUTO_TIMESTAMP = false;
const Rect QUEUE_BUFFER_INPUT_RECT = Rect(DEFAULT_WIDTH, DEFAULT_HEIGHT);
const int QUEUE_BUFFER_INPUT_SCALING_MODE = 0;
const int QUEUE_BUFFER_INPUT_TRANSFORM = 0;
const bool QUEUE_BUFFER_INPUT_ASYNC = false;
const sp<Fence> QUEUE_BUFFER_INPUT_FENCE = Fence::NO_FENCE;
}; // namespace anonymous

struct DummyConsumer : public BnConsumerListener {
    virtual void onFrameAvailable(const BufferItem& /* item */) {}
    virtual void onBuffersReleased() {}
    virtual void onSidebandStreamChanged() {}
};

class IGraphicBufferProducerTest : public ::testing::Test {
protected:

    IGraphicBufferProducerTest() {}

    virtual void SetUp() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("Begin test: %s.%s", testInfo->test_case_name(),
                testInfo->name());

        mDC = new DummyConsumer;

        BufferQueue::createBufferQueue(&mProducer, &mConsumer);

        // Test check: Can't connect producer if no consumer yet
        ASSERT_EQ(NO_INIT, TryConnectProducer());

        // Must connect consumer before producer connects will succeed.
        ASSERT_OK(mConsumer->consumerConnect(mDC, /*controlledByApp*/false));
    }

    virtual void TearDown() {
        const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
        ALOGV("End test:   %s.%s", testInfo->test_case_name(),
                testInfo->name());
    }

    status_t TryConnectProducer() {
        IGraphicBufferProducer::QueueBufferOutput output;
        return mProducer->connect(TEST_TOKEN,
                                  TEST_API,
                                  TEST_CONTROLLED_BY_APP,
                                  &output);
        // TODO: use params to vary token, api, producercontrolledbyapp, etc
    }

    // Connect to a producer in a 'correct' fashion.
    //   Precondition: Consumer is connected.
    void ConnectProducer() {
        ASSERT_OK(TryConnectProducer());
    }

    // Create a generic "valid" input for queueBuffer
    // -- uses the default buffer format, width, etc.
    static IGraphicBufferProducer::QueueBufferInput CreateBufferInput() {
        return QueueBufferInputBuilder().build();
    }

    // Builder pattern to slightly vary *almost* correct input
    // -- avoids copying and pasting
    struct QueueBufferInputBuilder {
        QueueBufferInputBuilder() {
           timestamp = QUEUE_BUFFER_INPUT_TIMESTAMP;
           isAutoTimestamp = QUEUE_BUFFER_INPUT_IS_AUTO_TIMESTAMP;
           crop = QUEUE_BUFFER_INPUT_RECT;
           scalingMode = QUEUE_BUFFER_INPUT_SCALING_MODE;
           transform = QUEUE_BUFFER_INPUT_TRANSFORM;
           async = QUEUE_BUFFER_INPUT_ASYNC;
           fence = QUEUE_BUFFER_INPUT_FENCE;
        }

        IGraphicBufferProducer::QueueBufferInput build() {
            return IGraphicBufferProducer::QueueBufferInput(
                    timestamp,
                    isAutoTimestamp,
                    crop,
                    scalingMode,
                    transform,
                    async,
                    fence);
        }

        QueueBufferInputBuilder& setTimestamp(int64_t timestamp) {
            this->timestamp = timestamp;
            return *this;
        }

        QueueBufferInputBuilder& setIsAutoTimestamp(bool isAutoTimestamp) {
            this->isAutoTimestamp = isAutoTimestamp;
            return *this;
        }

        QueueBufferInputBuilder& setCrop(Rect crop) {
            this->crop = crop;
            return *this;
        }

        QueueBufferInputBuilder& setScalingMode(int scalingMode) {
            this->scalingMode = scalingMode;
            return *this;
        }

        QueueBufferInputBuilder& setTransform(uint32_t transform) {
            this->transform = transform;
            return *this;
        }

        QueueBufferInputBuilder& setAsync(bool async) {
            this->async = async;
            return *this;
        }

        QueueBufferInputBuilder& setFence(sp<Fence> fence) {
            this->fence = fence;
            return *this;
        }

    private:
        int64_t timestamp;
        bool isAutoTimestamp;
        Rect crop;
        int scalingMode;
        uint32_t transform;
        int async;
        sp<Fence> fence;
    }; // struct QueueBufferInputBuilder

    // To easily store dequeueBuffer results into containers
    struct DequeueBufferResult {
        int slot;
        sp<Fence> fence;
    };

    status_t dequeueBuffer(bool async, uint32_t w, uint32_t h, uint32_t format, uint32_t usage, DequeueBufferResult* result) {
        return mProducer->dequeueBuffer(&result->slot, &result->fence, async, w, h, format, usage);
    }

private: // hide from test body
    sp<DummyConsumer> mDC;

protected: // accessible from test body
    sp<IGraphicBufferProducer> mProducer;
    sp<IGraphicBufferConsumer> mConsumer;
};

TEST_F(IGraphicBufferProducerTest, ConnectFirst_ReturnsError) {
    IGraphicBufferProducer::QueueBufferOutput output;

    // NULL output returns BAD_VALUE
    EXPECT_EQ(BAD_VALUE, mProducer->connect(TEST_TOKEN,
                                            TEST_API,
                                            TEST_CONTROLLED_BY_APP,
                                            /*output*/NULL));

    // Invalid API returns bad value
    EXPECT_EQ(BAD_VALUE, mProducer->connect(TEST_TOKEN,
                                            /*api*/0xDEADBEEF,
                                            TEST_CONTROLLED_BY_APP,
                                            &output));

    // TODO: get a token from a dead process somehow
}

TEST_F(IGraphicBufferProducerTest, ConnectAgain_ReturnsError) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    // Can't connect when there is already a producer connected
    IGraphicBufferProducer::QueueBufferOutput output;
    EXPECT_EQ(BAD_VALUE, mProducer->connect(TEST_TOKEN,
                                            TEST_API,
                                            TEST_CONTROLLED_BY_APP,
                                            &output));

    ASSERT_OK(mConsumer->consumerDisconnect());
    // Can't connect when IGBP is abandoned
    EXPECT_EQ(NO_INIT, mProducer->connect(TEST_TOKEN,
                                          TEST_API,
                                          TEST_CONTROLLED_BY_APP,
                                          &output));
}

TEST_F(IGraphicBufferProducerTest, Disconnect_Succeeds) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    ASSERT_OK(mProducer->disconnect(TEST_API));
}


TEST_F(IGraphicBufferProducerTest, Disconnect_ReturnsError) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    // Must disconnect with same API number
    ASSERT_EQ(BAD_VALUE, mProducer->disconnect(TEST_API_OTHER));
    // API must not be out of range
    ASSERT_EQ(BAD_VALUE, mProducer->disconnect(/*api*/0xDEADBEEF));

    // TODO: somehow kill mProducer so that this returns DEAD_OBJECT
}

TEST_F(IGraphicBufferProducerTest, Query_Succeeds) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    // TODO: Make these constants in header
    const int DEFAULT_CONSUMER_USAGE_BITS = 0;

    int value = -1;
    EXPECT_OK(mProducer->query(NATIVE_WINDOW_WIDTH, &value));
    EXPECT_EQ(DEFAULT_WIDTH, value);

    EXPECT_OK(mProducer->query(NATIVE_WINDOW_HEIGHT, &value));
    EXPECT_EQ(DEFAULT_HEIGHT, value);

    EXPECT_OK(mProducer->query(NATIVE_WINDOW_FORMAT, &value));
    EXPECT_EQ(DEFAULT_FORMAT, value);

    EXPECT_OK(mProducer->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &value));
    EXPECT_LE(0, value);
    EXPECT_GE(BufferQueue::NUM_BUFFER_SLOTS, value);

    EXPECT_OK(mProducer->query(NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND, &value));
    EXPECT_FALSE(value); // Can't run behind when we haven't touched the queue

    EXPECT_OK(mProducer->query(NATIVE_WINDOW_CONSUMER_USAGE_BITS, &value));
    EXPECT_EQ(DEFAULT_CONSUMER_USAGE_BITS, value);

}

TEST_F(IGraphicBufferProducerTest, Query_ReturnsError) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    // One past the end of the last 'query' enum value. Update this if we add more enums.
    const int NATIVE_WINDOW_QUERY_LAST_OFF_BY_ONE = NATIVE_WINDOW_CONSUMER_USAGE_BITS + 1;

    int value;
    // What was out of range
    EXPECT_EQ(BAD_VALUE, mProducer->query(/*what*/-1, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(/*what*/0xDEADBEEF, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_QUERY_LAST_OFF_BY_ONE, &value));

    // Some enums from window.h are 'invalid'
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_CONCRETE_TYPE, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_DEFAULT_WIDTH, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_DEFAULT_HEIGHT, &value));
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_TRANSFORM_HINT, &value));
    // TODO: Consider documented the above enums as unsupported or make a new enum for IGBP

    // Value was NULL
    EXPECT_EQ(BAD_VALUE, mProducer->query(NATIVE_WINDOW_FORMAT, /*value*/NULL));

    ASSERT_OK(mConsumer->consumerDisconnect());

    // BQ was abandoned
    EXPECT_EQ(NO_INIT, mProducer->query(NATIVE_WINDOW_FORMAT, &value));

    // TODO: other things in window.h that are supported by Surface::query
    // but not by BufferQueue::query
}

// TODO: queue under more complicated situations not involving just a single buffer
TEST_F(IGraphicBufferProducerTest, Queue_Succeeds) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    int dequeuedSlot = -1;
    sp<Fence> dequeuedFence;

    // XX: OK to assume first call returns this flag or not? Not really documented.
    ASSERT_EQ(OK | IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&dequeuedSlot, &dequeuedFence,
                                     QUEUE_BUFFER_INPUT_ASYNC,
                                     DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                                     TEST_PRODUCER_USAGE_BITS));

    EXPECT_LE(0, dequeuedSlot);
    EXPECT_GT(BufferQueue::NUM_BUFFER_SLOTS, dequeuedSlot);

    // Request the buffer (pre-requisite for queueing)
    sp<GraphicBuffer> dequeuedBuffer;
    ASSERT_OK(mProducer->requestBuffer(dequeuedSlot, &dequeuedBuffer));

    // A generic "valid" input
    IGraphicBufferProducer::QueueBufferInput input = CreateBufferInput();
    IGraphicBufferProducer::QueueBufferOutput output;

    // Queue the buffer back into the BQ
    ASSERT_OK(mProducer->queueBuffer(dequeuedSlot, input, &output));

    {
        uint32_t width;
        uint32_t height;
        uint32_t transformHint;
        uint32_t numPendingBuffers;

        output.deflate(&width, &height, &transformHint, &numPendingBuffers);

        EXPECT_EQ(DEFAULT_WIDTH, width);
        EXPECT_EQ(DEFAULT_HEIGHT, height);
        EXPECT_EQ(DEFAULT_TRANSFORM_HINT, transformHint);
        EXPECT_EQ(1, numPendingBuffers); // since queueBuffer was called exactly once
    }

    // Buffer was not in the dequeued state
    EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));
}

TEST_F(IGraphicBufferProducerTest, Queue_ReturnsError) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    // Invalid slot number
    {
        // A generic "valid" input
        IGraphicBufferProducer::QueueBufferInput input = CreateBufferInput();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(/*slot*/-1, input, &output));
        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(/*slot*/0xDEADBEEF, input, &output));
        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(BufferQueue::NUM_BUFFER_SLOTS,
                                                    input, &output));
    }

    // Slot was not in the dequeued state (all slots start out in Free state)
    {
        IGraphicBufferProducer::QueueBufferInput input = CreateBufferInput();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(/*slot*/0, input, &output));
    }

    // Put the slot into the "dequeued" state for the rest of the test
    int dequeuedSlot = -1;
    sp<Fence> dequeuedFence;

    ASSERT_EQ(OK | IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&dequeuedSlot, &dequeuedFence,
                                     QUEUE_BUFFER_INPUT_ASYNC,
                                     DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                                     TEST_PRODUCER_USAGE_BITS));

    // Slot was enqueued without requesting a buffer
    {
        IGraphicBufferProducer::QueueBufferInput input = CreateBufferInput();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));
    }

    // Request the buffer so that the rest of the tests don't fail on earlier checks.
    sp<GraphicBuffer> dequeuedBuffer;
    ASSERT_OK(mProducer->requestBuffer(dequeuedSlot, &dequeuedBuffer));

    // Fence was NULL
    {
        sp<Fence> nullFence = NULL;

        IGraphicBufferProducer::QueueBufferInput input =
                QueueBufferInputBuilder().setFence(nullFence).build();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));
    }

    // Scaling mode was unknown
    {
        IGraphicBufferProducer::QueueBufferInput input =
                QueueBufferInputBuilder().setScalingMode(-1).build();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));

        input = QueueBufferInputBuilder().setScalingMode(0xDEADBEEF).build();

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));
    }

    // Crop rect is out of bounds of the buffer dimensions
    {
        IGraphicBufferProducer::QueueBufferInput input =
                QueueBufferInputBuilder().setCrop(Rect(DEFAULT_WIDTH + 1, DEFAULT_HEIGHT + 1))
                .build();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(BAD_VALUE, mProducer->queueBuffer(dequeuedSlot, input, &output));
    }

    // Abandon the buffer queue so that the last test fails
    ASSERT_OK(mConsumer->consumerDisconnect());

    // The buffer queue has been abandoned.
    {
        IGraphicBufferProducer::QueueBufferInput input = CreateBufferInput();
        IGraphicBufferProducer::QueueBufferOutput output;

        EXPECT_EQ(NO_INIT, mProducer->queueBuffer(dequeuedSlot, input, &output));
    }
}

TEST_F(IGraphicBufferProducerTest, CancelBuffer_DoesntCrash) {
    ASSERT_NO_FATAL_FAILURE(ConnectProducer());

    int dequeuedSlot = -1;
    sp<Fence> dequeuedFence;

    ASSERT_EQ(OK | IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION,
            mProducer->dequeueBuffer(&dequeuedSlot, &dequeuedFence,
                                     QUEUE_BUFFER_INPUT_ASYNC,
                                     DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                                     TEST_PRODUCER_USAGE_BITS));

    // No return code, but at least test that it doesn't blow up...
    // TODO: add a return code
    mProducer->cancelBuffer(dequeuedSlot, dequeuedFence);
}

TEST_F(IGraphicBufferProducerTest, SetBufferCount_Succeeds) {

    // The producer does not wish to set a buffer count
    EXPECT_OK(mProducer->setBufferCount(0)) << "bufferCount: " << 0;
    // TODO: how to test "0" buffer count?

    int minBuffers;
    ASSERT_OK(mProducer->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minBuffers));

    // The MIN_UNDEQUEUED_BUFFERS limit is exclusive, so need to increment by at least 1
    minBuffers++;

    ASSERT_OK(mProducer->setBufferCount(minBuffers)) << "bufferCount: " << minBuffers;

    std::vector<DequeueBufferResult> dequeueList;

    // Should now be able to dequeue up to minBuffers times
    for (int i = 0; i < minBuffers; ++i) {
        DequeueBufferResult result;

        EXPECT_LE(OK,
                dequeueBuffer(QUEUE_BUFFER_INPUT_ASYNC,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                              TEST_PRODUCER_USAGE_BITS, &result))
                << "iteration: " << i << ", slot: " << result.slot;

        dequeueList.push_back(result);
    }

    // Cancel every buffer, so we can set buffer count again
    for (int i = 0; i < minBuffers; ++i) {
        DequeueBufferResult& result = dequeueList[i];
        mProducer->cancelBuffer(result.slot, result.fence);
    }

    ASSERT_OK(mProducer->setBufferCount(BufferQueue::NUM_BUFFER_SLOTS));

    // Should now be able to dequeue up to NUM_BUFFER_SLOTS times
    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; ++i) {
        int dequeuedSlot = -1;
        sp<Fence> dequeuedFence;

        EXPECT_LE(OK,
                mProducer->dequeueBuffer(&dequeuedSlot, &dequeuedFence,
                                         QUEUE_BUFFER_INPUT_ASYNC,
                                         DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                                         TEST_PRODUCER_USAGE_BITS))
                << "iteration: " << i << ", slot: " << dequeuedSlot;
    }
}

TEST_F(IGraphicBufferProducerTest, SetBufferCount_Fails) {
    int minBuffers;
    ASSERT_OK(mProducer->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minBuffers));

    // The MIN_UNDEQUEUED_BUFFERS limit is exclusive, so need to increment by at least 1
    minBuffers++;

    // Buffer count was out of range
    EXPECT_EQ(BAD_VALUE, mProducer->setBufferCount(-1)) << "bufferCount: " << -1;
    EXPECT_EQ(BAD_VALUE, mProducer->setBufferCount(minBuffers - 1)) << "bufferCount: " << minBuffers - 1;
    EXPECT_EQ(BAD_VALUE, mProducer->setBufferCount(BufferQueue::NUM_BUFFER_SLOTS + 1))
            << "bufferCount: " << BufferQueue::NUM_BUFFER_SLOTS + 1;

    // Pre-requisite to fail out a valid setBufferCount call
    {
        int dequeuedSlot = -1;
        sp<Fence> dequeuedFence;

        ASSERT_LE(OK,
                mProducer->dequeueBuffer(&dequeuedSlot, &dequeuedFence,
                                         QUEUE_BUFFER_INPUT_ASYNC,
                                         DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FORMAT,
                                         TEST_PRODUCER_USAGE_BITS))
                << "slot: " << dequeuedSlot;
    }

    // Client has one or more buffers dequeued
    EXPECT_EQ(BAD_VALUE, mProducer->setBufferCount(minBuffers)) << "bufferCount: " << minBuffers;

    // Abandon buffer queue
    ASSERT_OK(mConsumer->consumerDisconnect());

    // Fail because the buffer queue was abandoned
    EXPECT_EQ(NO_INIT, mProducer->setBufferCount(minBuffers)) << "bufferCount: " << minBuffers;

}

} // namespace android
