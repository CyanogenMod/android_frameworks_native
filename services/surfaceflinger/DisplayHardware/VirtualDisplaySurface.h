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

#include "DisplaySurface.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class HWComposer;

/* This DisplaySurface implementation is a stub used for developing HWC
 * virtual display support. It is currently just a passthrough.
 */
class VirtualDisplaySurface : public DisplaySurface {
public:
    VirtualDisplaySurface(HWComposer& hwc, int32_t dispId,
            const sp<IGraphicBufferProducer>& sink,
            const String8& name);

    virtual sp<IGraphicBufferProducer> getIGraphicBufferProducer() const;

    virtual status_t compositionComplete();
    virtual status_t advanceFrame();
    virtual void onFrameCommitted();
    virtual void dump(String8& result) const;

private:
    virtual ~VirtualDisplaySurface();

    sp<IGraphicBufferProducer> mSink;
};

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H

