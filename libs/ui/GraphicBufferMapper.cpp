/*
 * Copyright (C) 2007 The Android Open Source Project
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

#define LOG_TAG "GraphicBufferMapper"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <stdint.h>
#include <errno.h>

// We would eliminate the non-conforming zero-length array, but we can't since
// this is effectively included from the Linux kernel
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#include <sync/sync.h>
#pragma clang diagnostic pop

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/Trace.h>

#include <ui/Gralloc1On0Adapter.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include <system/graphics.h>

namespace android {
// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE( GraphicBufferMapper )

GraphicBufferMapper::GraphicBufferMapper()
  : mLoader(std::make_unique<Gralloc1::Loader>()),
    mDevice(mLoader->getDevice()) {}



status_t GraphicBufferMapper::registerBuffer(buffer_handle_t handle)
{
    ATRACE_CALL();

    gralloc1_error_t error = mDevice->retain(handle);
    ALOGW_IF(error != GRALLOC1_ERROR_NONE, "registerBuffer(%p) failed: %d",
            handle, error);

    return error;
}

status_t GraphicBufferMapper::registerBuffer(const GraphicBuffer* buffer)
{
    ATRACE_CALL();

    gralloc1_error_t error = mDevice->retain(buffer);
    ALOGW_IF(error != GRALLOC1_ERROR_NONE, "registerBuffer(%p) failed: %d",
            buffer->getNativeBuffer()->handle, error);

    return error;
}

status_t GraphicBufferMapper::unregisterBuffer(buffer_handle_t handle)
{
    ATRACE_CALL();

    gralloc1_error_t error = mDevice->release(handle);
    ALOGW_IF(error != GRALLOC1_ERROR_NONE, "unregisterBuffer(%p): failed %d",
            handle, error);

    return error;
}

static inline gralloc1_rect_t asGralloc1Rect(const Rect& rect) {
    gralloc1_rect_t outRect{};
    outRect.left = rect.left;
    outRect.top = rect.top;
    outRect.width = rect.width();
    outRect.height = rect.height();
    return outRect;
}

status_t GraphicBufferMapper::lock(buffer_handle_t handle, uint32_t usage,
        const Rect& bounds, void** vaddr)
{
    return lockAsync(handle, usage, bounds, vaddr, -1);
}

status_t GraphicBufferMapper::lockYCbCr(buffer_handle_t handle, uint32_t usage,
        const Rect& bounds, android_ycbcr *ycbcr)
{
    return lockAsyncYCbCr(handle, usage, bounds, ycbcr, -1);
}

status_t GraphicBufferMapper::unlock(buffer_handle_t handle)
{
    int32_t fenceFd = -1;
    status_t error = unlockAsync(handle, &fenceFd);
    if (error == NO_ERROR) {
        sync_wait(fenceFd, -1);
        close(fenceFd);
    }
    return error;
}

status_t GraphicBufferMapper::lockAsync(buffer_handle_t handle,
        uint32_t usage, const Rect& bounds, void** vaddr, int fenceFd)
{
    ATRACE_CALL();

    gralloc1_rect_t accessRegion = asGralloc1Rect(bounds);
    sp<Fence> fence = new Fence(fenceFd);
    gralloc1_error_t error = mDevice->lock(handle,
            static_cast<gralloc1_producer_usage_t>(usage),
            static_cast<gralloc1_consumer_usage_t>(usage),
            &accessRegion, vaddr, fence);
    ALOGW_IF(error != GRALLOC1_ERROR_NONE, "lock(%p, ...) failed: %d", handle,
            error);

    return error;
}

static inline bool isValidYCbCrPlane(const android_flex_plane_t& plane) {
    if (plane.bits_per_component != 8) {
        ALOGV("Invalid number of bits per component: %d",
                plane.bits_per_component);
        return false;
    }
    if (plane.bits_used != 8) {
        ALOGV("Invalid number of bits used: %d", plane.bits_used);
        return false;
    }

    bool hasValidIncrement = plane.h_increment == 1 ||
            (plane.component != FLEX_COMPONENT_Y && plane.h_increment == 2);
    hasValidIncrement = hasValidIncrement && plane.v_increment > 0;
    if (!hasValidIncrement) {
        ALOGV("Invalid increment: h %d v %d", plane.h_increment,
                plane.v_increment);
        return false;
    }

    return true;
}

status_t GraphicBufferMapper::lockAsyncYCbCr(buffer_handle_t handle,
        uint32_t usage, const Rect& bounds, android_ycbcr *ycbcr, int fenceFd)
{
    ATRACE_CALL();

    gralloc1_rect_t accessRegion = asGralloc1Rect(bounds);
    sp<Fence> fence = new Fence(fenceFd);

    if (mDevice->hasCapability(GRALLOC1_CAPABILITY_ON_ADAPTER)) {
        gralloc1_error_t error = mDevice->lockYCbCr(handle,
                static_cast<gralloc1_producer_usage_t>(usage),
                static_cast<gralloc1_consumer_usage_t>(usage),
                &accessRegion, ycbcr, fence);
        ALOGW_IF(error != GRALLOC1_ERROR_NONE, "lockYCbCr(%p, ...) failed: %d",
                handle, error);
        return error;
    }

    uint32_t numPlanes = 0;
    gralloc1_error_t error = mDevice->getNumFlexPlanes(handle, &numPlanes);
    if (error != GRALLOC1_ERROR_NONE) {
        ALOGV("Failed to retrieve number of flex planes: %d", error);
        return error;
    }
    if (numPlanes < 3) {
        ALOGV("Not enough planes for YCbCr (%u found)", numPlanes);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    std::vector<android_flex_plane_t> planes(numPlanes);
    android_flex_layout_t flexLayout{};
    flexLayout.num_planes = numPlanes;
    flexLayout.planes = planes.data();

    error = mDevice->lockFlex(handle,
            static_cast<gralloc1_producer_usage_t>(usage),
            static_cast<gralloc1_consumer_usage_t>(usage),
            &accessRegion, &flexLayout, fence);
    if (error != GRALLOC1_ERROR_NONE) {
        ALOGW("lockFlex(%p, ...) failed: %d", handle, error);
        return error;
    }
    if (flexLayout.format != FLEX_FORMAT_YCbCr) {
        ALOGV("Unable to convert flex-format buffer to YCbCr");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    // Find planes
    auto yPlane = planes.cend();
    auto cbPlane = planes.cend();
    auto crPlane = planes.cend();
    for (auto planeIter = planes.cbegin(); planeIter != planes.cend();
            ++planeIter) {
        if (planeIter->component == FLEX_COMPONENT_Y) {
            yPlane = planeIter;
        } else if (planeIter->component == FLEX_COMPONENT_Cb) {
            cbPlane = planeIter;
        } else if (planeIter->component == FLEX_COMPONENT_Cr) {
            crPlane = planeIter;
        }
    }
    if (yPlane == planes.cend()) {
        ALOGV("Unable to find Y plane");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (cbPlane == planes.cend()) {
        ALOGV("Unable to find Cb plane");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (crPlane == planes.cend()) {
        ALOGV("Unable to find Cr plane");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    // Validate planes
    if (!isValidYCbCrPlane(*yPlane)) {
        ALOGV("Y plane is invalid");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (!isValidYCbCrPlane(*cbPlane)) {
        ALOGV("Cb plane is invalid");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (!isValidYCbCrPlane(*crPlane)) {
        ALOGV("Cr plane is invalid");
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (cbPlane->v_increment != crPlane->v_increment) {
        ALOGV("Cb and Cr planes have different step (%d vs. %d)",
                cbPlane->v_increment, crPlane->v_increment);
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }
    if (cbPlane->h_increment != crPlane->h_increment) {
        ALOGV("Cb and Cr planes have different stride (%d vs. %d)",
                cbPlane->h_increment, crPlane->h_increment);
        unlock(handle);
        return GRALLOC1_ERROR_UNSUPPORTED;
    }

    // Pack plane data into android_ycbcr struct
    ycbcr->y = yPlane->top_left;
    ycbcr->cb = cbPlane->top_left;
    ycbcr->cr = crPlane->top_left;
    ycbcr->ystride = static_cast<size_t>(yPlane->v_increment);
    ycbcr->cstride = static_cast<size_t>(cbPlane->v_increment);
    ycbcr->chroma_step = static_cast<size_t>(cbPlane->h_increment);

    return error;
}

status_t GraphicBufferMapper::unlockAsync(buffer_handle_t handle, int *fenceFd)
{
    ATRACE_CALL();

    sp<Fence> fence = Fence::NO_FENCE;
    gralloc1_error_t error = mDevice->unlock(handle, &fence);
    if (error != GRALLOC1_ERROR_NONE) {
        ALOGE("unlock(%p) failed: %d", handle, error);
        return error;
    }

    *fenceFd = fence->dup();
    return error;
}

// ---------------------------------------------------------------------------
}; // namespace android
