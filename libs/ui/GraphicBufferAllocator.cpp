/* 
**
** Copyright 2009, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#define LOG_TAG "GraphicBufferAllocator"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <cutils/log.h>

#include <utils/Singleton.h>
#include <utils/String8.h>
#include <utils/Trace.h>

#include <ui/GraphicBufferAllocator.h>

namespace android {
// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE( GraphicBufferAllocator )

Mutex GraphicBufferAllocator::sLock;
KeyedVector<buffer_handle_t,
    GraphicBufferAllocator::alloc_rec_t> GraphicBufferAllocator::sAllocList;

GraphicBufferAllocator::GraphicBufferAllocator()
    : mAllocDev(0)
{
    hw_module_t const* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    ALOGE_IF(err, "FATAL: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
    if (err == 0) {
        gralloc_open(module, &mAllocDev);
    }
}

GraphicBufferAllocator::~GraphicBufferAllocator()
{
    gralloc_close(mAllocDev);
}

void GraphicBufferAllocator::dump(String8& result) const
{
    Mutex::Autolock _l(sLock);
    KeyedVector<buffer_handle_t, alloc_rec_t>& list(sAllocList);
    size_t total = 0;
    const size_t SIZE = 4096;
    char buffer[SIZE];
    snprintf(buffer, SIZE, "Allocated buffers:\n");
    result.append(buffer);
    const size_t c = list.size();
    for (size_t i=0 ; i<c ; i++) {
        const alloc_rec_t& rec(list.valueAt(i));
        if (rec.size) {
            snprintf(buffer, SIZE, "%10p: %7.2f KiB | %4u (%4u) x %4u | %8X | 0x%08x\n",
                    list.keyAt(i), rec.size/1024.0f,
                    rec.w, rec.s, rec.h, rec.format, rec.usage);
        } else {
            snprintf(buffer, SIZE, "%10p: unknown     | %4u (%4u) x %4u | %8X | 0x%08x\n",
                    list.keyAt(i),
                    rec.w, rec.s, rec.h, rec.format, rec.usage);
        }
        result.append(buffer);
        total += rec.size;
    }
    snprintf(buffer, SIZE, "Total allocated (estimate): %.2f KB\n", total/1024.0f);
    result.append(buffer);
    if (mAllocDev->common.version >= 1 && mAllocDev->dump) {
        mAllocDev->dump(mAllocDev, buffer, SIZE);
        result.append(buffer);
    }
}

void GraphicBufferAllocator::dumpToSystemLog()
{
    String8 s;
    GraphicBufferAllocator::getInstance().dump(s);
    ALOGD("%s", s.string());
}

class BufferLiberatorThread : public Thread {
public:

    static void queueCaptiveBuffer(buffer_handle_t handle) {
        size_t queueSize;
        {
            Mutex::Autolock lock(sMutex);
            if (sThread == NULL) {
                sThread = new BufferLiberatorThread;
                sThread->run("BufferLiberator");
            }

            sThread->mQueue.push_back(handle);
            sThread->mQueuedCondition.signal();
            queueSize = sThread->mQueue.size();
        }
    }

    static void waitForLiberation() {
        Mutex::Autolock lock(sMutex);

        waitForLiberationLocked();
    }

    static void maybeWaitForLiberation() {
        Mutex::Autolock lock(sMutex);
        if (sThread != NULL) {
            if (sThread->mQueue.size() > 8) {
                waitForLiberationLocked();
            }
        }
    }

private:

    BufferLiberatorThread() {}

    virtual bool threadLoop() {
        buffer_handle_t handle;
        { // Scope for mutex
            Mutex::Autolock lock(sMutex);
            while (mQueue.isEmpty()) {
                mQueuedCondition.wait(sMutex);
            }
            handle = mQueue[0];
        }

        status_t err;
        GraphicBufferAllocator& gba(GraphicBufferAllocator::get());
        { // Scope for tracing
            ATRACE_NAME("gralloc::free");
            err = gba.mAllocDev->free(gba.mAllocDev, handle);
        }
        ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));

        if (err == NO_ERROR) {
            Mutex::Autolock _l(GraphicBufferAllocator::sLock);
            KeyedVector<buffer_handle_t, GraphicBufferAllocator::alloc_rec_t>&
                    list(GraphicBufferAllocator::sAllocList);
            list.removeItem(handle);
        }

        { // Scope for mutex
            Mutex::Autolock lock(sMutex);
            mQueue.removeAt(0);
            mFreedCondition.broadcast();
        }

        return true;
    }

    static void waitForLiberationLocked() {
        if (sThread == NULL) {
            return;
        }

        const nsecs_t timeout = 500 * 1000 * 1000;
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        nsecs_t timeToStop = now + timeout;
        while (!sThread->mQueue.isEmpty() && now < timeToStop) {
            sThread->mFreedCondition.waitRelative(sMutex, timeToStop - now);
            now = systemTime(SYSTEM_TIME_MONOTONIC);
        }

        if (!sThread->mQueue.isEmpty()) {
            ALOGW("waitForLiberationLocked timed out");
        }
    }

    static Mutex sMutex;
    static sp<BufferLiberatorThread> sThread;
    Vector<buffer_handle_t> mQueue;
    Condition mQueuedCondition;
    Condition mFreedCondition;
};

Mutex BufferLiberatorThread::sMutex;
sp<BufferLiberatorThread> BufferLiberatorThread::sThread;

status_t GraphicBufferAllocator::alloc(uint32_t w, uint32_t h, PixelFormat format,
        int usage, buffer_handle_t* handle, int32_t* stride)
{
#ifdef QCOM_BSP
    status_t err = alloc(w, h, format, usage, handle, stride, 0);
    return err;
}

status_t GraphicBufferAllocator::alloc(uint32_t w, uint32_t h,
                                       PixelFormat format, int usage,
                                       buffer_handle_t* handle,
                                       int32_t* stride, uint32_t bufferSize)
{
#endif
    ATRACE_CALL();
    // make sure to not allocate a N x 0 or 0 x N buffer, since this is
    // allowed from an API stand-point allocate a 1x1 buffer instead.
    if (!w || !h)
        w = h = 1;

    // we have a h/w allocator and h/w buffer is requested

#ifdef EXYNOS4_ENHANCEMENTS
    if ((format == 0x101) || (format == 0x105) || (format == 0x107)) {
        // 0x101 = HAL_PIXEL_FORMAT_YCbCr_420_P (Samsung-specific pixel format)
        // 0x105 = HAL_PIXEL_FORMAT_YCbCr_420_SP (Samsung-specific pixel format)
        // 0x107 = HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED (Samsung-specific pixel format)
        usage |= GRALLOC_USAGE_HW_FIMC1; // Exynos HWC wants FIMC-friendly memory allocation
    }
#endif

    status_t err;

    // If too many async frees are queued up then wait for some of them to
    // complete before attempting to allocate more memory.  This is exercised
    // by the android.opengl.cts.GLSurfaceViewTest CTS test.
    BufferLiberatorThread::maybeWaitForLiberation();

#ifdef QCOM_BSP
    err = mAllocDev->allocSize(mAllocDev, w, h,
                               format, usage, handle, stride, bufferSize);
#else
    err = mAllocDev->alloc(mAllocDev, w, h, format, usage, handle, stride);
#endif

    if (err != NO_ERROR) {
        ALOGW("WOW! gralloc alloc failed, waiting for pending frees!");
        BufferLiberatorThread::waitForLiberation();
#ifdef QCOM_BSP
        err = mAllocDev->allocSize(mAllocDev, w, h,
                               format, usage, handle, stride, bufferSize);
#else
        err = mAllocDev->alloc(mAllocDev, w, h, format, usage, handle, stride);
#endif
    }

#ifdef QCOM_BSP
    ALOGW_IF(err, "alloc(%u, %u, %d, %08x, %d ...) failed %d (%s)",
            w, h, format, usage, bufferSize, err, strerror(-err));
#else
    ALOGW_IF(err, "alloc(%u, %u, %d, %08x, ...) failed %d (%s)",
            w, h, format, usage, err, strerror(-err));
#endif

    if (err == NO_ERROR) {
        Mutex::Autolock _l(sLock);
        KeyedVector<buffer_handle_t, alloc_rec_t>& list(sAllocList);
        int bpp = bytesPerPixel(format);
        if (bpp < 0) {
            // probably a HAL custom format. in any case, we don't know
            // what its pixel size is.
            bpp = 0;
        }
        alloc_rec_t rec;
        rec.w = w;
        rec.h = h;
        rec.s = *stride;
        rec.format = format;
        rec.usage = usage;
        rec.size = h * stride[0] * bpp;
        list.add(*handle, rec);
    }

    return err;
}


status_t GraphicBufferAllocator::free(buffer_handle_t handle)
{
    BufferLiberatorThread::queueCaptiveBuffer(handle);
    return NO_ERROR;
}

// ---------------------------------------------------------------------------
}; // namespace android
