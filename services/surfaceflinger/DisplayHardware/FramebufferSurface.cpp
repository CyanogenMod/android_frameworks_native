/*
 **
 ** Copyright 2012 The Android Open Source Project
 **
 ** Licensed under the Apache License Version 2.0(the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing software
 ** distributed under the License is distributed on an "AS IS" BASIS
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <cutils/log.h>

#include <utils/String8.h>

#include <ui/Rect.h>

#include <EGL/egl.h>

#include <hardware/hardware.h>
#include <gui/SurfaceTextureClient.h>
#include <ui/GraphicBuffer.h>

#include "DisplayHardware/FramebufferSurface.h"

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

sp<FramebufferSurface> FramebufferSurface::create() {
    sp<FramebufferSurface> result = new FramebufferSurface();
    if (result->fbDev == NULL) {
        result = NULL;
    }
    return result;
}

// ----------------------------------------------------------------------------

class GraphicBufferAlloc : public BnGraphicBufferAlloc {
public:
    GraphicBufferAlloc() { };
    virtual ~GraphicBufferAlloc() { };
    virtual sp<GraphicBuffer> createGraphicBuffer(uint32_t w, uint32_t h,
            PixelFormat format, uint32_t usage, status_t* error) {
        sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(w, h, format, usage));
        return graphicBuffer;
    }
};


/*
 * This implements the (main) framebuffer management. This class is used
 * mostly by SurfaceFlinger, but also by command line GL application.
 *
 */

FramebufferSurface::FramebufferSurface():
    ConsumerBase(new BufferQueue(true, NUM_FRAME_BUFFERS,
            new GraphicBufferAlloc())),
    fbDev(0),
    mCurrentBufferSlot(-1),
    mCurrentBuffer(0)
{
    hw_module_t const* module;

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        int stride;
        int err;
        int i;
        err = framebuffer_open(module, &fbDev);
        ALOGE_IF(err, "couldn't open framebuffer HAL (%s)", strerror(-err));

        // bail out if we can't initialize the modules
        if (!fbDev)
            return;

        mName = "FramebufferSurface";
        mBufferQueue->setConsumerName(mName);
        mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_HW_FB |
                GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER);
        mBufferQueue->setDefaultBufferFormat(fbDev->format);
        mBufferQueue->setDefaultBufferSize(fbDev->width, fbDev->height);
        mBufferQueue->setSynchronousMode(true);
        mBufferQueue->setBufferCountServer(NUM_FRAME_BUFFERS);
    } else {
        ALOGE("Couldn't get gralloc module");
    }
}

status_t FramebufferSurface::nextBuffer(sp<GraphicBuffer>* buffer) {
    Mutex::Autolock lock(mMutex);

    BufferQueue::BufferItem item;
    status_t err = acquireBufferLocked(&item);
    if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
        if (buffer != NULL) {
            *buffer = mCurrentBuffer;
        }
        return NO_ERROR;
    } else if (err != NO_ERROR) {
        ALOGE("error acquiring buffer: %s (%d)", strerror(-err), err);
        return err;
    }

    // If the BufferQueue has freed and reallocated a buffer in mCurrentSlot
    // then we may have acquired the slot we already own.  If we had released
    // our current buffer before we call acquireBuffer then that release call
    // would have returned STALE_BUFFER_SLOT, and we would have called
    // freeBufferLocked on that slot.  Because the buffer slot has already
    // been overwritten with the new buffer all we have to do is skip the
    // releaseBuffer call and we should be in the same state we'd be in if we
    // had released the old buffer first.
    if (mCurrentBufferSlot != BufferQueue::INVALID_BUFFER_SLOT &&
        item.mBuf != mCurrentBufferSlot) {
        // Release the previous buffer.
        err = releaseBufferLocked(mCurrentBufferSlot, EGL_NO_DISPLAY,
                EGL_NO_SYNC_KHR, Fence::NO_FENCE);
        if (err != NO_ERROR && err != BufferQueue::STALE_BUFFER_SLOT) {
            ALOGE("error releasing buffer: %s (%d)", strerror(-err), err);
            return err;
        }
    }

    mCurrentBufferSlot = item.mBuf;
    mCurrentBuffer = mSlots[mCurrentBufferSlot].mGraphicBuffer;
    if (item.mFence != NULL) {
        item.mFence->wait(Fence::TIMEOUT_NEVER);
    }

    if (buffer != NULL) {
        *buffer = mCurrentBuffer;
    }

    return NO_ERROR;
}

FramebufferSurface::~FramebufferSurface() {
    if (fbDev) {
        framebuffer_close(fbDev);
    }
}

void FramebufferSurface::onFrameAvailable() {
    // XXX: The following code is here temporarily as part of the transition
    // away from the framebuffer HAL.
    sp<GraphicBuffer> buf;
    status_t err = nextBuffer(&buf);
    if (err != NO_ERROR) {
        ALOGE("error latching next FramebufferSurface buffer: %s (%d)",
                strerror(-err), err);
        return;
    }
    err = fbDev->post(fbDev, buf->handle);
    if (err != NO_ERROR) {
        ALOGE("error posting framebuffer: %d", err);
    }
}

void FramebufferSurface::freeBufferLocked(int slotIndex) {
    ConsumerBase::freeBufferLocked(slotIndex);
    if (slotIndex == mCurrentBufferSlot) {
        mCurrentBufferSlot = BufferQueue::INVALID_BUFFER_SLOT;
    }
}

status_t FramebufferSurface::setUpdateRectangle(const Rect& r)
{
    return INVALID_OPERATION;
}

status_t FramebufferSurface::compositionComplete()
{
    if (fbDev->compositionComplete) {
        return fbDev->compositionComplete(fbDev);
    }
    return INVALID_OPERATION;
}

void FramebufferSurface::dump(String8& result) {
    if (fbDev->common.version >= 1 && fbDev->dump) {
        const size_t SIZE = 4096;
        char buffer[SIZE];
        fbDev->dump(fbDev, buffer, SIZE);
        result.append(buffer);
    }
    ConsumerBase::dump(result);
}

// ----------------------------------------------------------------------------
}; // namespace android
// ----------------------------------------------------------------------------
