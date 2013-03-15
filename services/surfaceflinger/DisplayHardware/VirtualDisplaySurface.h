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

#ifndef ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H
#define ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H

#include "BufferQueueInterposer.h"
#include "DisplaySurface.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class HWComposer;

/* This DisplaySurface implementation uses a BufferQueueInterposer to pass
 * partially- or fully-composited buffers from the OpenGL ES driver to
 * HWComposer to use as the output buffer for virtual displays. Allowing HWC
 * to compose into the same buffer that contains GLES results saves bandwidth
 * compared to having two separate BufferQueues for frames with at least some
 * GLES composition.
 *
 * The alternative would be to have two complete BufferQueues, one from GLES
 * to HWC and one from HWC to the virtual display sink (e.g. video encoder).
 * For GLES-only frames, the same bandwidth saving could be achieved if buffers
 * could be acquired from the GLES->HWC queue and inserted into the HWC->sink
 * queue. That would be complicated and doesn't help the mixed GLES+HWC case.
 *
 * On frames with no GLES composition, the VirtualDisplaySurface dequeues a
 * buffer directly from the sink IGraphicBufferProducer and passes it to HWC,
 * bypassing the GLES driver. This is only guaranteed to work if
 * eglSwapBuffers doesn't immediately dequeue a buffer for the next frame,
 * since we can't rely on being able to dequeue more than one buffer at a time.
 *
 * TODO(jessehall): Add a libgui test that ensures that EGL/GLES do lazy
 * dequeBuffers; we've wanted to require that for other reasons anyway.
 */
class VirtualDisplaySurface : public DisplaySurface {
public:
    VirtualDisplaySurface(HWComposer& hwc, int disp,
            const sp<IGraphicBufferProducer>& sink,
            const String8& name);

    virtual sp<IGraphicBufferProducer> getIGraphicBufferProducer() const;

    virtual status_t compositionComplete();
    virtual status_t advanceFrame();
    virtual status_t setReleaseFenceFd(int fenceFd);
    virtual void dump(String8& result) const;

private:
    virtual ~VirtualDisplaySurface();

    // immutable after construction
    HWComposer& mHwc;
    int mDisplayId;
    sp<BufferQueueInterposer> mSource;
    String8 mName;

    // mutable, must be synchronized with mMutex
    Mutex mMutex;
    sp<GraphicBuffer> mAcquiredBuffer;
    sp<Fence> mReleaseFence;
};

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H

