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

#include "VirtualDisplaySurface.h"

#include <cutils/log.h>
#include <gui/IGraphicBufferProducer.h>

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

VirtualDisplaySurface::VirtualDisplaySurface(HWComposer& hwc, int32_t dispId,
        const sp<IGraphicBufferProducer>& sink, const String8& name)
:   mSink(sink){
}

VirtualDisplaySurface::~VirtualDisplaySurface() {
}

sp<IGraphicBufferProducer> VirtualDisplaySurface::getIGraphicBufferProducer() const {
    return mSink;
}

status_t VirtualDisplaySurface::compositionComplete() {
    return NO_ERROR;
}

status_t VirtualDisplaySurface::advanceFrame() {
    return NO_ERROR;
}

void VirtualDisplaySurface::onFrameCommitted() {
}

void VirtualDisplaySurface::dump(String8& result) const {
}

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------
