/*
 **
 ** Copyright 2007 The Android Open Source Project
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

/*
 * This implements the (main) framebuffer management. This class is used
 * mostly by SurfaceFlinger, but also by command line GL application.
 *
 */

FramebufferSurface::FramebufferSurface()
    : SurfaceTextureClient(),
      fbDev(0), mCurrentBufferIndex(-1), mUpdateOnDemand(false)
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

        mUpdateOnDemand = (fbDev->setUpdateRect != 0);

        const_cast<uint32_t&>(ANativeWindow::flags) = fbDev->flags;
        const_cast<int&>(ANativeWindow::minSwapInterval) =  fbDev->minSwapInterval;
        const_cast<int&>(ANativeWindow::maxSwapInterval) =  fbDev->maxSwapInterval;

        if (fbDev->xdpi == 0 || fbDev->ydpi == 0) {
            ALOGE("invalid screen resolution from fb HAL (xdpi=%f, ydpi=%f), "
                   "defaulting to 160 dpi", fbDev->xdpi, fbDev->ydpi);
            const_cast<float&>(ANativeWindow::xdpi) = 160;
            const_cast<float&>(ANativeWindow::ydpi) = 160;
        } else {
            const_cast<float&>(ANativeWindow::xdpi) = fbDev->xdpi;
            const_cast<float&>(ANativeWindow::ydpi) = fbDev->ydpi;
        }

    } else {
        ALOGE("Couldn't get gralloc module");
    }

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

    mBufferQueue = new BufferQueue(true, NUM_FRAME_BUFFERS, new GraphicBufferAlloc());
    mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_HW_FB|GRALLOC_USAGE_HW_RENDER|GRALLOC_USAGE_HW_COMPOSER);
    mBufferQueue->setDefaultBufferFormat(fbDev->format);
    mBufferQueue->setDefaultBufferSize(fbDev->width, fbDev->height);
    mBufferQueue->setSynchronousMode(true);
    mBufferQueue->setBufferCountServer(NUM_FRAME_BUFFERS);
    setISurfaceTexture(mBufferQueue);
}

void FramebufferSurface::onFirstRef() {
    class Listener : public BufferQueue::ConsumerListener {
        const wp<FramebufferSurface> that;
        virtual ~Listener() { }
        virtual void onBuffersReleased() { }
        void onFrameAvailable() {
            sp<FramebufferSurface> self = that.promote();
            if (self != NULL) {
                BufferQueue::BufferItem item;
                status_t err = self->mBufferQueue->acquireBuffer(&item);
                if (err == 0) {
                    if (item.mGraphicBuffer != 0) {
                        self->mBuffers[item.mBuf] = item.mGraphicBuffer;
                    }
                    if (item.mFence.get()) {
                        err = item.mFence->wait(Fence::TIMEOUT_NEVER);
                        if (err) {
                            ALOGE("failed waiting for buffer's fence: %d", err);
                            self->mBufferQueue->releaseBuffer(item.mBuf,
                                    EGL_NO_DISPLAY, EGL_NO_SYNC_KHR,
                                    item.mFence);
                            return;
                        }
                    }
                    self->fbDev->post(self->fbDev, self->mBuffers[item.mBuf]->handle);
                    if (self->mCurrentBufferIndex >= 0) {
                        self->mBufferQueue->releaseBuffer(self->mCurrentBufferIndex,
                                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, Fence::NO_FENCE);
                    }
                    self->mCurrentBufferIndex = item.mBuf;
                }
            }
        }
    public:
        Listener(const sp<FramebufferSurface>& that) : that(that) { }
    };

    mBufferQueue->setConsumerName(String8("FramebufferSurface"));
    mBufferQueue->consumerConnect(new Listener(this));
}

FramebufferSurface::~FramebufferSurface() {
    if (fbDev) {
        framebuffer_close(fbDev);
    }
}

float FramebufferSurface::getRefreshRate() const {
    /* FIXME: REFRESH_RATE is a temporary HACK until we are able to report the
     * refresh rate properly from the HAL. The WindowManagerService now relies
     * on this value.
     */
#ifndef REFRESH_RATE
    return fbDev->fps;
#else
    return REFRESH_RATE;
#warning "refresh rate set via makefile to REFRESH_RATE"
#endif
}

status_t FramebufferSurface::setUpdateRectangle(const Rect& r)
{
    if (!mUpdateOnDemand) {
        return INVALID_OPERATION;
    }
    return fbDev->setUpdateRect(fbDev, r.left, r.top, r.width(), r.height());
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
}

int FramebufferSurface::query(int what, int* value) const {
    Mutex::Autolock _l(mLock);
    framebuffer_device_t* fb = fbDev;
    switch (what) {
        case NATIVE_WINDOW_DEFAULT_WIDTH:
        case NATIVE_WINDOW_WIDTH:
            *value = fb->width;
            return NO_ERROR;
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
        case NATIVE_WINDOW_HEIGHT:
            *value = fb->height;
            return NO_ERROR;
        case NATIVE_WINDOW_FORMAT:
            *value = fb->format;
            return NO_ERROR;
        case NATIVE_WINDOW_CONCRETE_TYPE:
            *value = NATIVE_WINDOW_FRAMEBUFFER;
            return NO_ERROR;
        case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER:
            *value = 0;
            return NO_ERROR;
        case NATIVE_WINDOW_TRANSFORM_HINT:
            *value = 0;
            return NO_ERROR;
    }
    return SurfaceTextureClient::query(what, value);
}

// ----------------------------------------------------------------------------
}; // namespace android
// ----------------------------------------------------------------------------
