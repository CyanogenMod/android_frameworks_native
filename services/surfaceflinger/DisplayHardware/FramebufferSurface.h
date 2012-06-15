/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef ANDROID_SF_FRAMEBUFFER_SURFACE_H
#define ANDROID_SF_FRAMEBUFFER_SURFACE_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>

#include <gui/SurfaceTextureClient.h>

#define NUM_FRAME_BUFFERS  2

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class Rect;
class String8;

// ---------------------------------------------------------------------------

class FramebufferSurface : public SurfaceTextureClient {
public:
    FramebufferSurface();

    virtual void onFirstRef();

    framebuffer_device_t const * getDevice() const { return fbDev; }

    bool isUpdateOnDemand() const { return mUpdateOnDemand; }
    status_t setUpdateRectangle(const Rect& updateRect);
    status_t compositionComplete();

    void dump(String8& result);

private:
    virtual ~FramebufferSurface(); // this class cannot be overloaded
    virtual int query(int what, int* value) const;

    framebuffer_device_t* fbDev;

    sp<BufferQueue> mBufferQueue;
    int mCurrentBufferIndex;
    sp<GraphicBuffer> mBuffers[NUM_FRAME_BUFFERS];

    mutable Mutex mLock;
    bool mUpdateOnDemand;
};

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_FRAMEBUFFER_SURFACE_H

