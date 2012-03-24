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

#define LOG_TAG "DummyConsumer"
// #define LOG_NDEBUG 0

#include <gui/DummyConsumer.h>

#include <utils/Log.h>
#include <utils/String8.h>

namespace android {

DummyConsumer::DummyConsumer(sp<BufferQueue> bufferQueue) {
    wp<BufferQueue::ConsumerListener> listener;
    sp<BufferQueue::ConsumerListener> proxy;
    listener = static_cast<BufferQueue::ConsumerListener*>(this);
    proxy = new BufferQueue::ProxyConsumerListener(listener);

    status_t err = bufferQueue->consumerConnect(proxy);
    if (err != NO_ERROR) {
        ALOGE("DummyConsumer: error connecting to BufferQueue: %s (%d)",
                strerror(-err), err);
    }
}

DummyConsumer::~DummyConsumer() {
    ALOGV("~DummyConsumer");
}

void DummyConsumer::onFrameAvailable() {
    ALOGV("onFrameAvailable");
}

void DummyConsumer::onBuffersReleased() {
    ALOGV("onBuffersReleased");
}

}; // namespace android
