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
#include "HWComposer.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

VirtualDisplaySurface::VirtualDisplaySurface(HWComposer& hwc, int disp,
        const sp<IGraphicBufferProducer>& sink, const String8& name)
:   mHwc(hwc),
    mDisplayId(disp),
    mSource(new BufferQueueInterposer(sink, name)),
    mName(name),
    mReleaseFence(Fence::NO_FENCE)
{}

VirtualDisplaySurface::~VirtualDisplaySurface() {
    if (mAcquiredBuffer != NULL) {
        status_t result = mSource->releaseBuffer(mReleaseFence);
        ALOGE_IF(result != NO_ERROR, "VirtualDisplaySurface \"%s\": "
                "failed to release previous buffer: %d",
                mName.string(), result);
    }
}

sp<IGraphicBufferProducer> VirtualDisplaySurface::getIGraphicBufferProducer() const {
    return mSource;
}

status_t VirtualDisplaySurface::compositionComplete() {
    return NO_ERROR;
}

status_t VirtualDisplaySurface::advanceFrame() {
    Mutex::Autolock lock(mMutex);
    status_t result = NO_ERROR;

    if (mAcquiredBuffer != NULL) {
        result = mSource->releaseBuffer(mReleaseFence);
        ALOGE_IF(result != NO_ERROR, "VirtualDisplaySurface \"%s\": "
                "failed to release previous buffer: %d",
                mName.string(), result);
        mAcquiredBuffer.clear();
        mReleaseFence = Fence::NO_FENCE;
    }

    sp<Fence> fence;
    result = mSource->acquireBuffer(&mAcquiredBuffer, &fence);
    if (result == BufferQueueInterposer::NO_BUFFER_AVAILABLE) {
        result = mSource->pullEmptyBuffer();
        if (result != NO_ERROR)
            return result;
        result = mSource->acquireBuffer(&mAcquiredBuffer, &fence);
    }
    if (result != NO_ERROR)
        return result;

    return mHwc.fbPost(mDisplayId, fence, mAcquiredBuffer);
}

status_t VirtualDisplaySurface::setReleaseFenceFd(int fenceFd) {
    if (fenceFd >= 0) {
        sp<Fence> fence(new Fence(fenceFd));
        Mutex::Autolock lock(mMutex);
        sp<Fence> mergedFence = Fence::merge(
                String8::format("VirtualDisplaySurface \"%s\"",
                        mName.string()),
                mReleaseFence, fence);
        if (!mergedFence->isValid()) {
            ALOGE("VirtualDisplaySurface \"%s\": failed to merge release fence",
                    mName.string());
            // synchronization is broken, the best we can do is hope fences
            // signal in order so the new fence will act like a union
            mReleaseFence = fence;
            return BAD_VALUE;
        }
        mReleaseFence = mergedFence;
    }
    return NO_ERROR;
}

void VirtualDisplaySurface::dump(String8& result) const {
}

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------
