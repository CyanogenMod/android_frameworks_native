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
                    rec.width, rec.stride, rec.height, rec.format, rec.usage);
        } else {
            snprintf(buffer, SIZE, "%10p: unknown     | %4u (%4u) x %4u | %8X | 0x%08x\n",
                    list.keyAt(i),
                    rec.width, rec.stride, rec.height, rec.format, rec.usage);
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

status_t GraphicBufferAllocator::alloc(uint32_t width, uint32_t height,
        PixelFormat format, uint32_t usage, buffer_handle_t* handle,
        uint32_t* stride)
{
    ATRACE_CALL();

    // make sure to not allocate a N x 0 or 0 x N buffer, since this is
    // allowed from an API stand-point allocate a 1x1 buffer instead.
    if (!width || !height)
        width = height = 1;

    // we have a h/w allocator and h/w buffer is requested
    status_t err;

    // Filter out any usage bits that should not be passed to the gralloc module
    usage &= GRALLOC_USAGE_ALLOC_MASK;

    int outStride = 0;
    err = mAllocDev->alloc(mAllocDev, static_cast<int>(width),
            static_cast<int>(height), format, static_cast<int>(usage), handle,
            &outStride);
    *stride = static_cast<uint32_t>(outStride);

    ALOGW_IF(err, "alloc(%u, %u, %d, %08x, ...) failed %d (%s)",
            width, height, format, usage, err, strerror(-err));

    if (err == NO_ERROR) {
        Mutex::Autolock _l(sLock);
        KeyedVector<buffer_handle_t, alloc_rec_t>& list(sAllocList);
        uint32_t bpp = bytesPerPixel(format);
        alloc_rec_t rec;
        rec.width = width;
        rec.height = height;
        rec.stride = *stride;
        rec.format = format;
        rec.usage = usage;
        rec.size = static_cast<size_t>(height * (*stride) * bpp);
        list.add(*handle, rec);
    }

    return err;
}

status_t GraphicBufferAllocator::free(buffer_handle_t handle)
{
    ATRACE_CALL();
    status_t err;

    err = mAllocDev->free(mAllocDev, handle);

    ALOGW_IF(err, "free(...) failed %d (%s)", err, strerror(-err));
    if (err == NO_ERROR) {
        Mutex::Autolock _l(sLock);
        KeyedVector<buffer_handle_t, alloc_rec_t>& list(sAllocList);
        list.removeItem(handle);
    }

    return err;
}

// ---------------------------------------------------------------------------
}; // namespace android
