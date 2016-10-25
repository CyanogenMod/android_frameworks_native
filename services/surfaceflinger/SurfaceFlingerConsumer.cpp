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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include "SurfaceFlingerConsumer.h"
#include "Layer.h"

#include <private/gui/SyncFeatures.h>

#include <gui/BufferItem.h>

#include <utils/Errors.h>
#include <utils/NativeHandle.h>
#include <utils/Trace.h>

namespace android {

// ---------------------------------------------------------------------------

status_t SurfaceFlingerConsumer::updateTexImage(BufferRejecter* rejecter,
        const DispSync& dispSync, bool* autoRefresh, bool* queuedBuffer,
        uint64_t maxFrameNumber)
{
    ATRACE_CALL();
    ALOGV("updateTexImage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ALOGE("updateTexImage: GLConsumer is abandoned!");
        return NO_INIT;
    }

    // Make sure the EGL state is the same as in previous calls.
    status_t err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        return err;
    }

    BufferItem item;

    // Acquire the next buffer.
    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    err = acquireBufferLocked(&item, computeExpectedPresent(dispSync),
            maxFrameNumber);
    if (err != NO_ERROR) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            err = NO_ERROR;
        } else if (err == BufferQueue::PRESENT_LATER) {
            // return the error, without logging
        } else {
            ALOGE("updateTexImage: acquire failed: %s (%d)",
                strerror(-err), err);
        }
        return err;
    }

    // We call the rejecter here, in case the caller has a reason to
    // not accept this buffer.  This is used by SurfaceFlinger to
    // reject buffers which have the wrong size
    int slot = item.mSlot;
    if (rejecter && rejecter->reject(mSlots[slot].mGraphicBuffer, item)) {
        releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer, EGL_NO_SYNC_KHR);
        return BUFFER_REJECTED;
    }

    if (autoRefresh) {
        *autoRefresh = item.mAutoRefresh;
    }

    if (queuedBuffer) {
        *queuedBuffer = item.mQueuedBuffer;
    }

    // Release the previous buffer.
#ifdef USE_HWC2
    err = updateAndReleaseLocked(item, &mPendingRelease);
#else
    err = updateAndReleaseLocked(item);
#endif
    if (err != NO_ERROR) {
        return err;
    }

    if (!SyncFeatures::getInstance().useNativeFenceSync()) {
        // Bind the new buffer to the GL texture.
        //
        // Older devices require the "implicit" synchronization provided
        // by glEGLImageTargetTexture2DOES, which this method calls.  Newer
        // devices will either call this in Layer::onDraw, or (if it's not
        // a GL-composited layer) not at all.
        err = bindTextureImageLocked();
    }

    return err;
}

status_t SurfaceFlingerConsumer::bindTextureImage()
{
    Mutex::Autolock lock(mMutex);

    return bindTextureImageLocked();
}

status_t SurfaceFlingerConsumer::acquireBufferLocked(BufferItem* item,
        nsecs_t presentWhen, uint64_t maxFrameNumber) {
    status_t result = GLConsumer::acquireBufferLocked(item, presentWhen,
            maxFrameNumber);
    if (result == NO_ERROR) {
        mTransformToDisplayInverse = item->mTransformToDisplayInverse;
        mSurfaceDamage = item->mSurfaceDamage;
    }
    return result;
}

bool SurfaceFlingerConsumer::getTransformToDisplayInverse() const {
    Mutex::Autolock lock(mMutex);
    return mTransformToDisplayInverse;
}

const Region& SurfaceFlingerConsumer::getSurfaceDamage() const {
    return mSurfaceDamage;
}

sp<NativeHandle> SurfaceFlingerConsumer::getSidebandStream() const {
    return mConsumer->getSidebandStream();
}

// We need to determine the time when a buffer acquired now will be
// displayed.  This can be calculated:
//   time when previous buffer's actual-present fence was signaled
//    + current display refresh rate * HWC latency
//    + a little extra padding
//
// Buffer producers are expected to set their desired presentation time
// based on choreographer time stamps, which (coming from vsync events)
// will be slightly later then the actual-present timing.  If we get a
// desired-present time that is unintentionally a hair after the next
// vsync, we'll hold the frame when we really want to display it.  We
// need to take the offset between actual-present and reported-vsync
// into account.
//
// If the system is configured without a DispSync phase offset for the app,
// we also want to throw in a bit of padding to avoid edge cases where we
// just barely miss.  We want to do it here, not in every app.  A major
// source of trouble is the app's use of the display's ideal refresh time
// (via Display.getRefreshRate()), which could be off of the actual refresh
// by a few percent, with the error multiplied by the number of frames
// between now and when the buffer should be displayed.
//
// If the refresh reported to the app has a phase offset, we shouldn't need
// to tweak anything here.
nsecs_t SurfaceFlingerConsumer::computeExpectedPresent(const DispSync& dispSync)
{
    // The HWC doesn't currently have a way to report additional latency.
    // Assume that whatever we submit now will appear right after the flip.
    // For a smart panel this might be 1.  This is expressed in frames,
    // rather than time, because we expect to have a constant frame delay
    // regardless of the refresh rate.
    const uint32_t hwcLatency = 0;

    // Ask DispSync when the next refresh will be (CLOCK_MONOTONIC).
    const nsecs_t nextRefresh = dispSync.computeNextRefresh(hwcLatency);

    // The DispSync time is already adjusted for the difference between
    // vsync and reported-vsync (PRESENT_TIME_OFFSET_FROM_VSYNC_NS), so
    // we don't need to factor that in here.  Pad a little to avoid
    // weird effects if apps might be requesting times right on the edge.
    nsecs_t extraPadding = 0;
    if (VSYNC_EVENT_PHASE_OFFSET_NS == 0) {
        extraPadding = 1000000;        // 1ms (6% of 60Hz)
    }

    return nextRefresh + extraPadding;
}

#ifdef USE_HWC2
void SurfaceFlingerConsumer::setReleaseFence(const sp<Fence>& fence)
{
    mPrevReleaseFence = fence;
    if (!mPendingRelease.isPending) {
        GLConsumer::setReleaseFence(fence);
        return;
    }
    auto currentTexture = mPendingRelease.currentTexture;
    if (fence->isValid() &&
            currentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        status_t result = addReleaseFence(currentTexture,
                mPendingRelease.graphicBuffer, fence);
        ALOGE_IF(result != NO_ERROR, "setReleaseFence: failed to add the"
                " fence: %s (%d)", strerror(-result), result);
    }
}

void SurfaceFlingerConsumer::releasePendingBuffer()
{
    if (!mPendingRelease.isPending) {
        ALOGV("Pending buffer already released");
        return;
    }
    ALOGV("Releasing pending buffer");
    Mutex::Autolock lock(mMutex);
    status_t result = releaseBufferLocked(mPendingRelease.currentTexture,
            mPendingRelease.graphicBuffer, mPendingRelease.display,
            mPendingRelease.fence);
    ALOGE_IF(result != NO_ERROR, "releasePendingBuffer failed: %s (%d)",
            strerror(-result), result);
    mPendingRelease = PendingRelease();
}
#else
void SurfaceFlingerConsumer::setReleaseFence(const sp<Fence>& fence) {
    mPrevReleaseFence = fence;
    GLConsumer::setReleaseFence(fence);
}
#endif

sp<Fence> SurfaceFlingerConsumer::getPrevReleaseFence() const {
    return mPrevReleaseFence;
}

void SurfaceFlingerConsumer::setContentsChangedListener(
        const wp<ContentsChangedListener>& listener) {
    setFrameAvailableListener(listener);
    Mutex::Autolock lock(mMutex);
    mContentsChangedListener = listener;
}

void SurfaceFlingerConsumer::onSidebandStreamChanged() {
    sp<ContentsChangedListener> listener;
    {   // scope for the lock
        Mutex::Autolock lock(mMutex);
        ALOG_ASSERT(mFrameAvailableListener.unsafe_get() == mContentsChangedListener.unsafe_get());
        listener = mContentsChangedListener.promote();
    }

    if (listener != NULL) {
        listener->onSidebandStreamChanged();
    }
}

bool SurfaceFlingerConsumer::getFrameTimestamps(uint64_t frameNumber,
        FrameTimestamps* outTimestamps) const {
    sp<const Layer> l = mLayer.promote();
    return l.get() ? l->getFrameTimestamps(frameNumber, outTimestamps) : false;
}

// ---------------------------------------------------------------------------
}; // namespace android

