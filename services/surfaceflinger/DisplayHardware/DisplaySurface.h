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

#ifndef ANDROID_SF_DISPLAY_SURFACE_H
#define ANDROID_SF_DISPLAY_SURFACE_H

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class IGraphicBufferProducer;
class String8;

class DisplaySurface : public virtual RefBase {
public:
    virtual sp<IGraphicBufferProducer> getIGraphicBufferProducer() const = 0;

    // Should be called when composition rendering is complete for a frame (but
    // eglSwapBuffers hasn't necessarily been called). Required by certain
    // older drivers for synchronization.
    // TODO: Remove this when we drop support for HWC 1.0.
    virtual status_t compositionComplete() = 0;

    // Inform the surface that GLES composition is complete for this frame, and
    // the surface should make sure that HWComposer has the correct buffer for
    // this frame. Some implementations may only push a new buffer to
    // HWComposer if GLES composition took place, others need to push a new
    // buffer on every frame.
    virtual status_t advanceFrame() = 0;

    // setReleaseFenceFd stores a fence file descriptor that will signal when
    // the current buffer is no longer being read. This fence will be returned
    // to the producer when the current buffer is released by updateTexImage().
    // Multiple fences can be set for a given buffer; they will be merged into
    // a single union fence. The GLConsumer will close the file descriptor
    // when finished with it.
    virtual status_t setReleaseFenceFd(int fenceFd) = 0;

    virtual void dump(String8& result) const = 0;

protected:
    DisplaySurface() {}
    virtual ~DisplaySurface() {}
};

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_DISPLAY_SURFACE_H

