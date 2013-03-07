/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ANDROID_GUI_CONSUMER_H
#define ANDROID_GUI_CONSUMER_H

#include <gui/BufferQueue.h>
#include <gui/ConsumerBase.h>
#include <gui/ISurfaceTexture.h>

namespace android {

class GLConsumer : public ConsumerBase {
public:
    GLConsumer(GLuint, bool = false, GLenum = 0, bool = false, const sp<BufferQueue>& = 0) :
        ConsumerBase(0) {}
    void incStrong(const void*) const {}
    void decStrong(const void*) const {}
    status_t updateTexImage() { return 0; }
    void abandon() {}
    sp<ISurfaceTexture> getBufferQueue() const { return 0; }
    GLenum getCurrentTextureTarget() const { return 0; }
    status_t setSynchronousMode(bool) { return 0; }
    void getTransformMatrix(float[16]) {}
    int64_t getTimestamp() {}
    void setFrameAvailableListener(const wp<FrameAvailableListener>&) {}
    sp<GraphicBuffer> getCurrentBuffer() const { return 0; }
};

}; // namespace android

#endif // ANDROID_GUI_CONSUMER_H

