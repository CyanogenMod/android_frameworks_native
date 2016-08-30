/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "Surface"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <android/native_window.h>

#include <binder/Parcel.h>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <utils/NativeHandle.h>

#include <ui/Fence.h>
#include <ui/Region.h>

#include <gui/IProducerListener.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/GLConsumer.h>
#include <gui/Surface.h>

#include <private/gui/ComposerService.h>

namespace android {

Surface::Surface(
        const sp<IGraphicBufferProducer>& bufferProducer,
        bool controlledByApp)
    : mGraphicBufferProducer(bufferProducer),
      mCrop(Rect::EMPTY_RECT),
      mGenerationNumber(0),
      mSharedBufferMode(false),
      mAutoRefresh(false),
      mSharedBufferSlot(BufferItem::INVALID_BUFFER_SLOT),
      mSharedBufferHasBeenQueued(false),
      mNextFrameNumber(1)
{
    // Initialize the ANativeWindow function pointers.
    ANativeWindow::setSwapInterval  = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer    = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer     = hook_cancelBuffer;
    ANativeWindow::queueBuffer      = hook_queueBuffer;
    ANativeWindow::query            = hook_query;
    ANativeWindow::perform          = hook_perform;

    ANativeWindow::dequeueBuffer_DEPRECATED = hook_dequeueBuffer_DEPRECATED;
    ANativeWindow::cancelBuffer_DEPRECATED  = hook_cancelBuffer_DEPRECATED;
    ANativeWindow::lockBuffer_DEPRECATED    = hook_lockBuffer_DEPRECATED;
    ANativeWindow::queueBuffer_DEPRECATED   = hook_queueBuffer_DEPRECATED;

    const_cast<int&>(ANativeWindow::minSwapInterval) = 0;
    const_cast<int&>(ANativeWindow::maxSwapInterval) = 1;

    mReqWidth = 0;
    mReqHeight = 0;
    mReqFormat = 0;
    mReqUsage = 0;
    mTimestamp = NATIVE_WINDOW_TIMESTAMP_AUTO;
    mDataSpace = HAL_DATASPACE_UNKNOWN;
    mScalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
    mTransform = 0;
    mStickyTransform = 0;
    mDefaultWidth = 0;
    mDefaultHeight = 0;
    mUserWidth = 0;
    mUserHeight = 0;
    mTransformHint = 0;
    mConsumerRunningBehind = false;
    mConnectedToCpu = false;
    mProducerControlledByApp = controlledByApp;
    mSwapIntervalZero = false;
}

Surface::~Surface() {
    if (mConnectedToCpu) {
        Surface::disconnect(NATIVE_WINDOW_API_CPU);
    }
}

sp<IGraphicBufferProducer> Surface::getIGraphicBufferProducer() const {
    return mGraphicBufferProducer;
}

void Surface::setSidebandStream(const sp<NativeHandle>& stream) {
    mGraphicBufferProducer->setSidebandStream(stream);
}

void Surface::allocateBuffers() {
    uint32_t reqWidth = mReqWidth ? mReqWidth : mUserWidth;
    uint32_t reqHeight = mReqHeight ? mReqHeight : mUserHeight;
    mGraphicBufferProducer->allocateBuffers(reqWidth, reqHeight,
            mReqFormat, mReqUsage);
}

status_t Surface::setGenerationNumber(uint32_t generation) {
    status_t result = mGraphicBufferProducer->setGenerationNumber(generation);
    if (result == NO_ERROR) {
        mGenerationNumber = generation;
    }
    return result;
}

uint64_t Surface::getNextFrameNumber() const {
    Mutex::Autolock lock(mMutex);
    return mNextFrameNumber;
}

String8 Surface::getConsumerName() const {
    return mGraphicBufferProducer->getConsumerName();
}

status_t Surface::setDequeueTimeout(nsecs_t timeout) {
    return mGraphicBufferProducer->setDequeueTimeout(timeout);
}

status_t Surface::getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence, float outTransformMatrix[16]) {
    return mGraphicBufferProducer->getLastQueuedBuffer(outBuffer, outFence,
            outTransformMatrix);
}

bool Surface::getFrameTimestamps(uint64_t frameNumber, nsecs_t* outPostedTime,
        nsecs_t* outAcquireTime, nsecs_t* outRefreshStartTime,
        nsecs_t* outGlCompositionDoneTime, nsecs_t* outDisplayRetireTime,
        nsecs_t* outReleaseTime) {
    ATRACE_CALL();

    FrameTimestamps timestamps;
    bool found = mGraphicBufferProducer->getFrameTimestamps(frameNumber,
            &timestamps);
    if (found) {
        if (outPostedTime) {
            *outPostedTime = timestamps.postedTime;
        }
        if (outAcquireTime) {
            *outAcquireTime = timestamps.acquireTime;
        }
        if (outRefreshStartTime) {
            *outRefreshStartTime = timestamps.refreshStartTime;
        }
        if (outGlCompositionDoneTime) {
            *outGlCompositionDoneTime = timestamps.glCompositionDoneTime;
        }
        if (outDisplayRetireTime) {
            *outDisplayRetireTime = timestamps.displayRetireTime;
        }
        if (outReleaseTime) {
            *outReleaseTime = timestamps.releaseTime;
        }
        return true;
    }
    return false;
}

int Surface::hook_setSwapInterval(ANativeWindow* window, int interval) {
    Surface* c = getSelf(window);
    return c->setSwapInterval(interval);
}

int Surface::hook_dequeueBuffer(ANativeWindow* window,
        ANativeWindowBuffer** buffer, int* fenceFd) {
    Surface* c = getSelf(window);
    return c->dequeueBuffer(buffer, fenceFd);
}

int Surface::hook_cancelBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer, int fenceFd) {
    Surface* c = getSelf(window);
    return c->cancelBuffer(buffer, fenceFd);
}

int Surface::hook_queueBuffer(ANativeWindow* window,
        ANativeWindowBuffer* buffer, int fenceFd) {
    Surface* c = getSelf(window);
    return c->queueBuffer(buffer, fenceFd);
}

int Surface::hook_dequeueBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer** buffer) {
    Surface* c = getSelf(window);
    ANativeWindowBuffer* buf;
    int fenceFd = -1;
    int result = c->dequeueBuffer(&buf, &fenceFd);
    if (result != OK) {
        return result;
    }
    sp<Fence> fence(new Fence(fenceFd));
    int waitResult = fence->waitForever("dequeueBuffer_DEPRECATED");
    if (waitResult != OK) {
        ALOGE("dequeueBuffer_DEPRECATED: Fence::wait returned an error: %d",
                waitResult);
        c->cancelBuffer(buf, -1);
        return waitResult;
    }
    *buffer = buf;
    return result;
}

int Surface::hook_cancelBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    Surface* c = getSelf(window);
    return c->cancelBuffer(buffer, -1);
}

int Surface::hook_lockBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    Surface* c = getSelf(window);
    return c->lockBuffer_DEPRECATED(buffer);
}

int Surface::hook_queueBuffer_DEPRECATED(ANativeWindow* window,
        ANativeWindowBuffer* buffer) {
    Surface* c = getSelf(window);
    return c->queueBuffer(buffer, -1);
}

int Surface::hook_query(const ANativeWindow* window,
                                int what, int* value) {
    const Surface* c = getSelf(window);
    return c->query(what, value);
}

int Surface::hook_perform(ANativeWindow* window, int operation, ...) {
    va_list args;
    va_start(args, operation);
    Surface* c = getSelf(window);
    int result = c->perform(operation, args);
    va_end(args);
    return result;
}

int Surface::setSwapInterval(int interval) {
    ATRACE_CALL();
    // EGL specification states:
    //  interval is silently clamped to minimum and maximum implementation
    //  dependent values before being stored.

    if (interval < minSwapInterval)
        interval = minSwapInterval;

    if (interval > maxSwapInterval)
        interval = maxSwapInterval;

    mSwapIntervalZero = (interval == 0);
    mGraphicBufferProducer->setAsyncMode(mSwapIntervalZero);

    return NO_ERROR;
}

int Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
    ATRACE_CALL();
    ALOGV("Surface::dequeueBuffer");

    uint32_t reqWidth;
    uint32_t reqHeight;
    PixelFormat reqFormat;
    uint32_t reqUsage;

    {
        Mutex::Autolock lock(mMutex);

        reqWidth = mReqWidth ? mReqWidth : mUserWidth;
        reqHeight = mReqHeight ? mReqHeight : mUserHeight;

        reqFormat = mReqFormat;
        reqUsage = mReqUsage;

        if (mSharedBufferMode && mAutoRefresh && mSharedBufferSlot !=
                BufferItem::INVALID_BUFFER_SLOT) {
            sp<GraphicBuffer>& gbuf(mSlots[mSharedBufferSlot].buffer);
            if (gbuf != NULL) {
                *buffer = gbuf.get();
                *fenceFd = -1;
                return OK;
            }
        }
    } // Drop the lock so that we can still touch the Surface while blocking in IGBP::dequeueBuffer

    int buf = -1;
    sp<Fence> fence;
    nsecs_t now = systemTime();
    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence,
            reqWidth, reqHeight, reqFormat, reqUsage);
    mLastDequeueDuration = systemTime() - now;

    if (result < 0) {
        ALOGV("dequeueBuffer: IGraphicBufferProducer::dequeueBuffer"
                "(%d, %d, %d, %d) failed: %d", reqWidth, reqHeight, reqFormat,
                reqUsage, result);
        return result;
    }

    Mutex::Autolock lock(mMutex);

    sp<GraphicBuffer>& gbuf(mSlots[buf].buffer);

    // this should never happen
    ALOGE_IF(fence == NULL, "Surface::dequeueBuffer: received null Fence! buf=%d", buf);

    if (result & IGraphicBufferProducer::RELEASE_ALL_BUFFERS) {
        freeAllBuffers();
    }

    if ((result & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {
        result = mGraphicBufferProducer->requestBuffer(buf, &gbuf);
        if (result != NO_ERROR) {
            ALOGE("dequeueBuffer: IGraphicBufferProducer::requestBuffer failed: %d", result);
            mGraphicBufferProducer->cancelBuffer(buf, fence);
            return result;
        }
    }

    if (fence->isValid()) {
        *fenceFd = fence->dup();
        if (*fenceFd == -1) {
            ALOGE("dequeueBuffer: error duping fence: %d", errno);
            // dup() should never fail; something is badly wrong. Soldier on
            // and hope for the best; the worst that should happen is some
            // visible corruption that lasts until the next frame.
        }
    } else {
        *fenceFd = -1;
    }

    *buffer = gbuf.get();

    if (mSharedBufferMode && mAutoRefresh) {
        mSharedBufferSlot = buf;
        mSharedBufferHasBeenQueued = false;
    } else if (mSharedBufferSlot == buf) {
        mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
        mSharedBufferHasBeenQueued = false;
    }

    return OK;
}

int Surface::cancelBuffer(android_native_buffer_t* buffer,
        int fenceFd) {
    ATRACE_CALL();
    ALOGV("Surface::cancelBuffer");
    Mutex::Autolock lock(mMutex);
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        if (fenceFd >= 0) {
            close(fenceFd);
        }
        return i;
    }
    if (mSharedBufferSlot == i && mSharedBufferHasBeenQueued) {
        if (fenceFd >= 0) {
            close(fenceFd);
        }
        return OK;
    }
    sp<Fence> fence(fenceFd >= 0 ? new Fence(fenceFd) : Fence::NO_FENCE);
    mGraphicBufferProducer->cancelBuffer(i, fence);

    if (mSharedBufferMode && mAutoRefresh && mSharedBufferSlot == i) {
        mSharedBufferHasBeenQueued = true;
    }

    return OK;
}

int Surface::getSlotFromBufferLocked(
        android_native_buffer_t* buffer) const {
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].buffer != NULL &&
                mSlots[i].buffer->handle == buffer->handle) {
            return i;
        }
    }
    ALOGE("getSlotFromBufferLocked: unknown buffer: %p", buffer->handle);
    return BAD_VALUE;
}

int Surface::lockBuffer_DEPRECATED(android_native_buffer_t* buffer __attribute__((unused))) {
    ALOGV("Surface::lockBuffer");
    Mutex::Autolock lock(mMutex);
    return OK;
}

int Surface::queueBuffer(android_native_buffer_t* buffer, int fenceFd) {
    ATRACE_CALL();
    ALOGV("Surface::queueBuffer");
    Mutex::Autolock lock(mMutex);
    int64_t timestamp;
    bool isAutoTimestamp = false;

    if (mTimestamp == NATIVE_WINDOW_TIMESTAMP_AUTO) {
        timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        isAutoTimestamp = true;
        ALOGV("Surface::queueBuffer making up timestamp: %.2f ms",
            timestamp / 1000000.f);
    } else {
        timestamp = mTimestamp;
    }
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        if (fenceFd >= 0) {
            close(fenceFd);
        }
        return i;
    }
    if (mSharedBufferSlot == i && mSharedBufferHasBeenQueued) {
        if (fenceFd >= 0) {
            close(fenceFd);
        }
        return OK;
    }


    // Make sure the crop rectangle is entirely inside the buffer.
    Rect crop(Rect::EMPTY_RECT);
    mCrop.intersect(Rect(buffer->width, buffer->height), &crop);

    sp<Fence> fence(fenceFd >= 0 ? new Fence(fenceFd) : Fence::NO_FENCE);
    IGraphicBufferProducer::QueueBufferOutput output;
    IGraphicBufferProducer::QueueBufferInput input(timestamp, isAutoTimestamp,
            mDataSpace, crop, mScalingMode, mTransform ^ mStickyTransform,
            fence, mStickyTransform);

    if (mConnectedToCpu || mDirtyRegion.bounds() == Rect::INVALID_RECT) {
        input.setSurfaceDamage(Region::INVALID_REGION);
    } else {
        // Here we do two things:
        // 1) The surface damage was specified using the OpenGL ES convention of
        //    the origin being in the bottom-left corner. Here we flip to the
        //    convention that the rest of the system uses (top-left corner) by
        //    subtracting all top/bottom coordinates from the buffer height.
        // 2) If the buffer is coming in rotated (for example, because the EGL
        //    implementation is reacting to the transform hint coming back from
        //    SurfaceFlinger), the surface damage needs to be rotated the
        //    opposite direction, since it was generated assuming an unrotated
        //    buffer (the app doesn't know that the EGL implementation is
        //    reacting to the transform hint behind its back). The
        //    transformations in the switch statement below apply those
        //    complementary rotations (e.g., if 90 degrees, rotate 270 degrees).

        int width = buffer->width;
        int height = buffer->height;
        bool rotated90 = (mTransform ^ mStickyTransform) &
                NATIVE_WINDOW_TRANSFORM_ROT_90;
        if (rotated90) {
            std::swap(width, height);
        }

        Region flippedRegion;
        for (auto rect : mDirtyRegion) {
            int left = rect.left;
            int right = rect.right;
            int top = height - rect.bottom; // Flip from OpenGL convention
            int bottom = height - rect.top; // Flip from OpenGL convention
            switch (mTransform ^ mStickyTransform) {
                case NATIVE_WINDOW_TRANSFORM_ROT_90: {
                    // Rotate 270 degrees
                    Rect flippedRect{top, width - right, bottom, width - left};
                    flippedRegion.orSelf(flippedRect);
                    break;
                }
                case NATIVE_WINDOW_TRANSFORM_ROT_180: {
                    // Rotate 180 degrees
                    Rect flippedRect{width - right, height - bottom,
                            width - left, height - top};
                    flippedRegion.orSelf(flippedRect);
                    break;
                }
                case NATIVE_WINDOW_TRANSFORM_ROT_270: {
                    // Rotate 90 degrees
                    Rect flippedRect{height - bottom, left,
                            height - top, right};
                    flippedRegion.orSelf(flippedRect);
                    break;
                }
                default: {
                    Rect flippedRect{left, top, right, bottom};
                    flippedRegion.orSelf(flippedRect);
                    break;
                }
            }
        }

        input.setSurfaceDamage(flippedRegion);
    }

    nsecs_t now = systemTime();
    status_t err = mGraphicBufferProducer->queueBuffer(i, input, &output);
    mLastQueueDuration = systemTime() - now;
    if (err != OK)  {
        ALOGE("queueBuffer: error queuing buffer to SurfaceTexture, %d", err);
    }

    uint32_t numPendingBuffers = 0;
    uint32_t hint = 0;
    output.deflate(&mDefaultWidth, &mDefaultHeight, &hint,
            &numPendingBuffers, &mNextFrameNumber);

    // Disable transform hint if sticky transform is set.
    if (mStickyTransform == 0) {
        mTransformHint = hint;
    }

    mConsumerRunningBehind = (numPendingBuffers >= 2);

    if (!mConnectedToCpu) {
        // Clear surface damage back to full-buffer
        mDirtyRegion = Region::INVALID_REGION;
    }

    if (mSharedBufferMode && mAutoRefresh && mSharedBufferSlot == i) {
        mSharedBufferHasBeenQueued = true;
    }

    mQueueBufferCondition.broadcast();

    return err;
}

int Surface::query(int what, int* value) const {
    ATRACE_CALL();
    ALOGV("Surface::query");
    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        switch (what) {
            case NATIVE_WINDOW_FORMAT:
                if (mReqFormat) {
                    *value = static_cast<int>(mReqFormat);
                    return NO_ERROR;
                }
                break;
            case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER: {
                sp<ISurfaceComposer> composer(
                        ComposerService::getComposerService());
                if (composer->authenticateSurfaceTexture(mGraphicBufferProducer)) {
                    *value = 1;
                } else {
                    *value = 0;
                }
                return NO_ERROR;
            }
            case NATIVE_WINDOW_CONCRETE_TYPE:
                *value = NATIVE_WINDOW_SURFACE;
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_WIDTH:
                *value = static_cast<int>(
                        mUserWidth ? mUserWidth : mDefaultWidth);
                return NO_ERROR;
            case NATIVE_WINDOW_DEFAULT_HEIGHT:
                *value = static_cast<int>(
                        mUserHeight ? mUserHeight : mDefaultHeight);
                return NO_ERROR;
            case NATIVE_WINDOW_TRANSFORM_HINT:
                *value = static_cast<int>(mTransformHint);
                return NO_ERROR;
            case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND: {
                status_t err = NO_ERROR;
                if (!mConsumerRunningBehind) {
                    *value = 0;
                } else {
                    err = mGraphicBufferProducer->query(what, value);
                    if (err == NO_ERROR) {
                        mConsumerRunningBehind = *value;
                    }
                }
                return err;
            }
            case NATIVE_WINDOW_LAST_DEQUEUE_DURATION: {
                int64_t durationUs = mLastDequeueDuration / 1000;
                *value = durationUs > std::numeric_limits<int>::max() ?
                        std::numeric_limits<int>::max() :
                        static_cast<int>(durationUs);
                return NO_ERROR;
            }
            case NATIVE_WINDOW_LAST_QUEUE_DURATION: {
                int64_t durationUs = mLastQueueDuration / 1000;
                *value = durationUs > std::numeric_limits<int>::max() ?
                        std::numeric_limits<int>::max() :
                        static_cast<int>(durationUs);
                return NO_ERROR;
            }
        }
    }
    return mGraphicBufferProducer->query(what, value);
}

int Surface::perform(int operation, va_list args)
{
    int res = NO_ERROR;
    switch (operation) {
    case NATIVE_WINDOW_CONNECT:
        // deprecated. must return NO_ERROR.
        break;
    case NATIVE_WINDOW_DISCONNECT:
        // deprecated. must return NO_ERROR.
        break;
    case NATIVE_WINDOW_SET_USAGE:
        res = dispatchSetUsage(args);
        break;
    case NATIVE_WINDOW_SET_CROP:
        res = dispatchSetCrop(args);
        break;
    case NATIVE_WINDOW_SET_BUFFER_COUNT:
        res = dispatchSetBufferCount(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_GEOMETRY:
        res = dispatchSetBuffersGeometry(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TRANSFORM:
        res = dispatchSetBuffersTransform(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_STICKY_TRANSFORM:
        res = dispatchSetBuffersStickyTransform(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP:
        res = dispatchSetBuffersTimestamp(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS:
        res = dispatchSetBuffersDimensions(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS:
        res = dispatchSetBuffersUserDimensions(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_FORMAT:
        res = dispatchSetBuffersFormat(args);
        break;
    case NATIVE_WINDOW_LOCK:
        res = dispatchLock(args);
        break;
    case NATIVE_WINDOW_UNLOCK_AND_POST:
        res = dispatchUnlockAndPost(args);
        break;
    case NATIVE_WINDOW_SET_SCALING_MODE:
        res = dispatchSetScalingMode(args);
        break;
    case NATIVE_WINDOW_API_CONNECT:
        res = dispatchConnect(args);
        break;
    case NATIVE_WINDOW_API_DISCONNECT:
        res = dispatchDisconnect(args);
        break;
    case NATIVE_WINDOW_SET_SIDEBAND_STREAM:
        res = dispatchSetSidebandStream(args);
        break;
    case NATIVE_WINDOW_SET_BUFFERS_DATASPACE:
        res = dispatchSetBuffersDataSpace(args);
        break;
    case NATIVE_WINDOW_SET_SURFACE_DAMAGE:
        res = dispatchSetSurfaceDamage(args);
        break;
    case NATIVE_WINDOW_SET_SHARED_BUFFER_MODE:
        res = dispatchSetSharedBufferMode(args);
        break;
    case NATIVE_WINDOW_SET_AUTO_REFRESH:
        res = dispatchSetAutoRefresh(args);
        break;
    case NATIVE_WINDOW_GET_FRAME_TIMESTAMPS:
        res = dispatchGetFrameTimestamps(args);
        break;
    default:
        res = NAME_NOT_FOUND;
        break;
    }
    return res;
}

int Surface::dispatchConnect(va_list args) {
    int api = va_arg(args, int);
    return connect(api);
}

int Surface::dispatchDisconnect(va_list args) {
    int api = va_arg(args, int);
    return disconnect(api);
}

int Surface::dispatchSetUsage(va_list args) {
    int usage = va_arg(args, int);
    return setUsage(static_cast<uint32_t>(usage));
}

int Surface::dispatchSetCrop(va_list args) {
    android_native_rect_t const* rect = va_arg(args, android_native_rect_t*);
    return setCrop(reinterpret_cast<Rect const*>(rect));
}

int Surface::dispatchSetBufferCount(va_list args) {
    size_t bufferCount = va_arg(args, size_t);
    return setBufferCount(static_cast<int32_t>(bufferCount));
}

int Surface::dispatchSetBuffersGeometry(va_list args) {
    uint32_t width = va_arg(args, uint32_t);
    uint32_t height = va_arg(args, uint32_t);
    PixelFormat format = va_arg(args, PixelFormat);
    int err = setBuffersDimensions(width, height);
    if (err != 0) {
        return err;
    }
    return setBuffersFormat(format);
}

int Surface::dispatchSetBuffersDimensions(va_list args) {
    uint32_t width = va_arg(args, uint32_t);
    uint32_t height = va_arg(args, uint32_t);
    return setBuffersDimensions(width, height);
}

int Surface::dispatchSetBuffersUserDimensions(va_list args) {
    uint32_t width = va_arg(args, uint32_t);
    uint32_t height = va_arg(args, uint32_t);
    return setBuffersUserDimensions(width, height);
}

int Surface::dispatchSetBuffersFormat(va_list args) {
    PixelFormat format = va_arg(args, PixelFormat);
    return setBuffersFormat(format);
}

int Surface::dispatchSetScalingMode(va_list args) {
    int mode = va_arg(args, int);
    return setScalingMode(mode);
}

int Surface::dispatchSetBuffersTransform(va_list args) {
    uint32_t transform = va_arg(args, uint32_t);
    return setBuffersTransform(transform);
}

int Surface::dispatchSetBuffersStickyTransform(va_list args) {
    uint32_t transform = va_arg(args, uint32_t);
    return setBuffersStickyTransform(transform);
}

int Surface::dispatchSetBuffersTimestamp(va_list args) {
    int64_t timestamp = va_arg(args, int64_t);
    return setBuffersTimestamp(timestamp);
}

int Surface::dispatchLock(va_list args) {
    ANativeWindow_Buffer* outBuffer = va_arg(args, ANativeWindow_Buffer*);
    ARect* inOutDirtyBounds = va_arg(args, ARect*);
    return lock(outBuffer, inOutDirtyBounds);
}

int Surface::dispatchUnlockAndPost(va_list args __attribute__((unused))) {
    return unlockAndPost();
}

int Surface::dispatchSetSidebandStream(va_list args) {
    native_handle_t* sH = va_arg(args, native_handle_t*);
    sp<NativeHandle> sidebandHandle = NativeHandle::create(sH, false);
    setSidebandStream(sidebandHandle);
    return OK;
}

int Surface::dispatchSetBuffersDataSpace(va_list args) {
    android_dataspace dataspace =
            static_cast<android_dataspace>(va_arg(args, int));
    return setBuffersDataSpace(dataspace);
}

int Surface::dispatchSetSurfaceDamage(va_list args) {
    android_native_rect_t* rects = va_arg(args, android_native_rect_t*);
    size_t numRects = va_arg(args, size_t);
    setSurfaceDamage(rects, numRects);
    return NO_ERROR;
}

int Surface::dispatchSetSharedBufferMode(va_list args) {
    bool sharedBufferMode = va_arg(args, int);
    return setSharedBufferMode(sharedBufferMode);
}

int Surface::dispatchSetAutoRefresh(va_list args) {
    bool autoRefresh = va_arg(args, int);
    return setAutoRefresh(autoRefresh);
}

int Surface::dispatchGetFrameTimestamps(va_list args) {
    uint32_t framesAgo = va_arg(args, uint32_t);
    nsecs_t* outPostedTime = va_arg(args, int64_t*);
    nsecs_t* outAcquireTime = va_arg(args, int64_t*);
    nsecs_t* outRefreshStartTime = va_arg(args, int64_t*);
    nsecs_t* outGlCompositionDoneTime = va_arg(args, int64_t*);
    nsecs_t* outDisplayRetireTime = va_arg(args, int64_t*);
    nsecs_t* outReleaseTime = va_arg(args, int64_t*);
    bool ret = getFrameTimestamps(getNextFrameNumber() - 1 - framesAgo,
            outPostedTime, outAcquireTime, outRefreshStartTime,
            outGlCompositionDoneTime, outDisplayRetireTime, outReleaseTime);
    return ret ? NO_ERROR : BAD_VALUE;
}

int Surface::connect(int api) {
    static sp<IProducerListener> listener = new DummyProducerListener();
    return connect(api, listener);
}

int Surface::connect(int api, const sp<IProducerListener>& listener) {
    ATRACE_CALL();
    ALOGV("Surface::connect");
    Mutex::Autolock lock(mMutex);
    IGraphicBufferProducer::QueueBufferOutput output;
    int err = mGraphicBufferProducer->connect(listener, api, mProducerControlledByApp, &output);
    if (err == NO_ERROR) {
        uint32_t numPendingBuffers = 0;
        uint32_t hint = 0;
        output.deflate(&mDefaultWidth, &mDefaultHeight, &hint,
                &numPendingBuffers, &mNextFrameNumber);

        // Disable transform hint if sticky transform is set.
        if (mStickyTransform == 0) {
            mTransformHint = hint;
        }

        mConsumerRunningBehind = (numPendingBuffers >= 2);
    }
    if (!err && api == NATIVE_WINDOW_API_CPU) {
        mConnectedToCpu = true;
        // Clear the dirty region in case we're switching from a non-CPU API
        mDirtyRegion.clear();
    } else if (!err) {
        // Initialize the dirty region for tracking surface damage
        mDirtyRegion = Region::INVALID_REGION;
    }

    return err;
}


int Surface::disconnect(int api) {
    ATRACE_CALL();
    ALOGV("Surface::disconnect");
    Mutex::Autolock lock(mMutex);
    mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
    mSharedBufferHasBeenQueued = false;
    freeAllBuffers();
    int err = mGraphicBufferProducer->disconnect(api);
    if (!err) {
        mReqFormat = 0;
        mReqWidth = 0;
        mReqHeight = 0;
        mReqUsage = 0;
        mCrop.clear();
        mScalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
        mTransform = 0;
        mStickyTransform = 0;

        if (api == NATIVE_WINDOW_API_CPU) {
            mConnectedToCpu = false;
        }
    }
    return err;
}

int Surface::detachNextBuffer(sp<GraphicBuffer>* outBuffer,
        sp<Fence>* outFence) {
    ATRACE_CALL();
    ALOGV("Surface::detachNextBuffer");

    if (outBuffer == NULL || outFence == NULL) {
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);

    sp<GraphicBuffer> buffer(NULL);
    sp<Fence> fence(NULL);
    status_t result = mGraphicBufferProducer->detachNextBuffer(
            &buffer, &fence);
    if (result != NO_ERROR) {
        return result;
    }

    *outBuffer = buffer;
    if (fence != NULL && fence->isValid()) {
        *outFence = fence;
    } else {
        *outFence = Fence::NO_FENCE;
    }

    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (mSlots[i].buffer != NULL &&
                mSlots[i].buffer->handle == buffer->handle) {
            mSlots[i].buffer = NULL;
        }
    }

    return NO_ERROR;
}

int Surface::attachBuffer(ANativeWindowBuffer* buffer)
{
    ATRACE_CALL();
    ALOGV("Surface::attachBuffer");

    Mutex::Autolock lock(mMutex);

    sp<GraphicBuffer> graphicBuffer(static_cast<GraphicBuffer*>(buffer));
    uint32_t priorGeneration = graphicBuffer->mGenerationNumber;
    graphicBuffer->mGenerationNumber = mGenerationNumber;
    int32_t attachedSlot = -1;
    status_t result = mGraphicBufferProducer->attachBuffer(
            &attachedSlot, graphicBuffer);
    if (result != NO_ERROR) {
        ALOGE("attachBuffer: IGraphicBufferProducer call failed (%d)", result);
        graphicBuffer->mGenerationNumber = priorGeneration;
        return result;
    }
    mSlots[attachedSlot].buffer = graphicBuffer;

    return NO_ERROR;
}

int Surface::setUsage(uint32_t reqUsage)
{
    ALOGV("Surface::setUsage");
    Mutex::Autolock lock(mMutex);
    if (reqUsage != mReqUsage) {
        mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
    }
    mReqUsage = reqUsage;
    return OK;
}

int Surface::setCrop(Rect const* rect)
{
    ATRACE_CALL();

    Rect realRect(Rect::EMPTY_RECT);
    if (rect == NULL || rect->isEmpty()) {
        realRect.clear();
    } else {
        realRect = *rect;
    }

    ALOGV("Surface::setCrop rect=[%d %d %d %d]",
            realRect.left, realRect.top, realRect.right, realRect.bottom);

    Mutex::Autolock lock(mMutex);
    mCrop = realRect;
    return NO_ERROR;
}

int Surface::setBufferCount(int bufferCount)
{
    ATRACE_CALL();
    ALOGV("Surface::setBufferCount");
    Mutex::Autolock lock(mMutex);

    status_t err = NO_ERROR;
    if (bufferCount == 0) {
        err = mGraphicBufferProducer->setMaxDequeuedBufferCount(1);
    } else {
        int minUndequeuedBuffers = 0;
        err = mGraphicBufferProducer->query(
                NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBuffers);
        if (err == NO_ERROR) {
            err = mGraphicBufferProducer->setMaxDequeuedBufferCount(
                    bufferCount - minUndequeuedBuffers);
        }
    }

    ALOGE_IF(err, "IGraphicBufferProducer::setBufferCount(%d) returned %s",
             bufferCount, strerror(-err));

    return err;
}

int Surface::setMaxDequeuedBufferCount(int maxDequeuedBuffers) {
    ATRACE_CALL();
    ALOGV("Surface::setMaxDequeuedBufferCount");
    Mutex::Autolock lock(mMutex);

    status_t err = mGraphicBufferProducer->setMaxDequeuedBufferCount(
            maxDequeuedBuffers);
    ALOGE_IF(err, "IGraphicBufferProducer::setMaxDequeuedBufferCount(%d) "
            "returned %s", maxDequeuedBuffers, strerror(-err));

    return err;
}

int Surface::setAsyncMode(bool async) {
    ATRACE_CALL();
    ALOGV("Surface::setAsyncMode");
    Mutex::Autolock lock(mMutex);

    status_t err = mGraphicBufferProducer->setAsyncMode(async);
    ALOGE_IF(err, "IGraphicBufferProducer::setAsyncMode(%d) returned %s",
            async, strerror(-err));

    return err;
}

int Surface::setSharedBufferMode(bool sharedBufferMode) {
    ATRACE_CALL();
    ALOGV("Surface::setSharedBufferMode (%d)", sharedBufferMode);
    Mutex::Autolock lock(mMutex);

    status_t err = mGraphicBufferProducer->setSharedBufferMode(
            sharedBufferMode);
    if (err == NO_ERROR) {
        mSharedBufferMode = sharedBufferMode;
    }
    ALOGE_IF(err, "IGraphicBufferProducer::setSharedBufferMode(%d) returned"
            "%s", sharedBufferMode, strerror(-err));

    return err;
}

int Surface::setAutoRefresh(bool autoRefresh) {
    ATRACE_CALL();
    ALOGV("Surface::setAutoRefresh (%d)", autoRefresh);
    Mutex::Autolock lock(mMutex);

    status_t err = mGraphicBufferProducer->setAutoRefresh(autoRefresh);
    if (err == NO_ERROR) {
        mAutoRefresh = autoRefresh;
    }
    ALOGE_IF(err, "IGraphicBufferProducer::setAutoRefresh(%d) returned %s",
            autoRefresh, strerror(-err));
    return err;
}

int Surface::setBuffersDimensions(uint32_t width, uint32_t height)
{
    ATRACE_CALL();
    ALOGV("Surface::setBuffersDimensions");

    if ((width && !height) || (!width && height))
        return BAD_VALUE;

    Mutex::Autolock lock(mMutex);
    if (width != mReqWidth || height != mReqHeight) {
        mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
    }
    mReqWidth = width;
    mReqHeight = height;
    return NO_ERROR;
}

int Surface::setBuffersUserDimensions(uint32_t width, uint32_t height)
{
    ATRACE_CALL();
    ALOGV("Surface::setBuffersUserDimensions");

    if ((width && !height) || (!width && height))
        return BAD_VALUE;

    Mutex::Autolock lock(mMutex);
    if (width != mUserWidth || height != mUserHeight) {
        mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
    }
    mUserWidth = width;
    mUserHeight = height;
    return NO_ERROR;
}

int Surface::setBuffersFormat(PixelFormat format)
{
    ALOGV("Surface::setBuffersFormat");

    Mutex::Autolock lock(mMutex);
    if (format != mReqFormat) {
        mSharedBufferSlot = BufferItem::INVALID_BUFFER_SLOT;
    }
    mReqFormat = format;
    return NO_ERROR;
}

int Surface::setScalingMode(int mode)
{
    ATRACE_CALL();
    ALOGV("Surface::setScalingMode(%d)", mode);

    switch (mode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
        case NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP:
            break;
        default:
            ALOGE("unknown scaling mode: %d", mode);
            return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    mScalingMode = mode;
    return NO_ERROR;
}

int Surface::setBuffersTransform(uint32_t transform)
{
    ATRACE_CALL();
    ALOGV("Surface::setBuffersTransform");
    Mutex::Autolock lock(mMutex);
    mTransform = transform;
    return NO_ERROR;
}

int Surface::setBuffersStickyTransform(uint32_t transform)
{
    ATRACE_CALL();
    ALOGV("Surface::setBuffersStickyTransform");
    Mutex::Autolock lock(mMutex);
    mStickyTransform = transform;
    return NO_ERROR;
}

int Surface::setBuffersTimestamp(int64_t timestamp)
{
    ALOGV("Surface::setBuffersTimestamp");
    Mutex::Autolock lock(mMutex);
    mTimestamp = timestamp;
    return NO_ERROR;
}

int Surface::setBuffersDataSpace(android_dataspace dataSpace)
{
    ALOGV("Surface::setBuffersDataSpace");
    Mutex::Autolock lock(mMutex);
    mDataSpace = dataSpace;
    return NO_ERROR;
}

void Surface::freeAllBuffers() {
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        mSlots[i].buffer = 0;
    }
}

void Surface::setSurfaceDamage(android_native_rect_t* rects, size_t numRects) {
    ATRACE_CALL();
    ALOGV("Surface::setSurfaceDamage");
    Mutex::Autolock lock(mMutex);

    if (mConnectedToCpu || numRects == 0) {
        mDirtyRegion = Region::INVALID_REGION;
        return;
    }

    mDirtyRegion.clear();
    for (size_t r = 0; r < numRects; ++r) {
        // We intentionally flip top and bottom here, since because they're
        // specified with a bottom-left origin, top > bottom, which fails
        // validation in the Region class. We will fix this up when we flip to a
        // top-left origin in queueBuffer.
        Rect rect(rects[r].left, rects[r].bottom, rects[r].right, rects[r].top);
        mDirtyRegion.orSelf(rect);
    }
}

// ----------------------------------------------------------------------
// the lock/unlock APIs must be used from the same thread

static status_t copyBlt(
        const sp<GraphicBuffer>& dst,
        const sp<GraphicBuffer>& src,
        const Region& reg)
{
    // src and dst with, height and format must be identical. no verification
    // is done here.
    status_t err;
    uint8_t* src_bits = NULL;
    err = src->lock(GRALLOC_USAGE_SW_READ_OFTEN, reg.bounds(),
            reinterpret_cast<void**>(&src_bits));
    ALOGE_IF(err, "error locking src buffer %s", strerror(-err));

    uint8_t* dst_bits = NULL;
    err = dst->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, reg.bounds(),
            reinterpret_cast<void**>(&dst_bits));
    ALOGE_IF(err, "error locking dst buffer %s", strerror(-err));

    Region::const_iterator head(reg.begin());
    Region::const_iterator tail(reg.end());
    if (head != tail && src_bits && dst_bits) {
        const size_t bpp = bytesPerPixel(src->format);
        const size_t dbpr = static_cast<uint32_t>(dst->stride) * bpp;
        const size_t sbpr = static_cast<uint32_t>(src->stride) * bpp;

        while (head != tail) {
            const Rect& r(*head++);
            int32_t h = r.height();
            if (h <= 0) continue;
            size_t size = static_cast<uint32_t>(r.width()) * bpp;
            uint8_t const * s = src_bits +
                    static_cast<uint32_t>(r.left + src->stride * r.top) * bpp;
            uint8_t       * d = dst_bits +
                    static_cast<uint32_t>(r.left + dst->stride * r.top) * bpp;
            if (dbpr==sbpr && size==sbpr) {
                size *= static_cast<size_t>(h);
                h = 1;
            }
            do {
                memcpy(d, s, size);
                d += dbpr;
                s += sbpr;
            } while (--h > 0);
        }
    }

    if (src_bits)
        src->unlock();

    if (dst_bits)
        dst->unlock();

    return err;
}

// ----------------------------------------------------------------------------

status_t Surface::lock(
        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds)
{
    if (mLockedBuffer != 0) {
        ALOGE("Surface::lock failed, already locked");
        return INVALID_OPERATION;
    }

    if (!mConnectedToCpu) {
        int err = Surface::connect(NATIVE_WINDOW_API_CPU);
        if (err) {
            return err;
        }
        // we're intending to do software rendering from this point
        setUsage(GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
    }

    ANativeWindowBuffer* out;
    int fenceFd = -1;
    status_t err = dequeueBuffer(&out, &fenceFd);
    ALOGE_IF(err, "dequeueBuffer failed (%s)", strerror(-err));
    if (err == NO_ERROR) {
        sp<GraphicBuffer> backBuffer(GraphicBuffer::getSelf(out));
        const Rect bounds(backBuffer->width, backBuffer->height);

        Region newDirtyRegion;
        if (inOutDirtyBounds) {
            newDirtyRegion.set(static_cast<Rect const&>(*inOutDirtyBounds));
            newDirtyRegion.andSelf(bounds);
        } else {
            newDirtyRegion.set(bounds);
        }

        // figure out if we can copy the frontbuffer back
        const sp<GraphicBuffer>& frontBuffer(mPostedBuffer);
        const bool canCopyBack = (frontBuffer != 0 &&
                backBuffer->width  == frontBuffer->width &&
                backBuffer->height == frontBuffer->height &&
                backBuffer->format == frontBuffer->format);

        if (canCopyBack) {
            // copy the area that is invalid and not repainted this round
            const Region copyback(mDirtyRegion.subtract(newDirtyRegion));
            if (!copyback.isEmpty())
                copyBlt(backBuffer, frontBuffer, copyback);
        } else {
            // if we can't copy-back anything, modify the user's dirty
            // region to make sure they redraw the whole buffer
            newDirtyRegion.set(bounds);
            mDirtyRegion.clear();
            Mutex::Autolock lock(mMutex);
            for (size_t i=0 ; i<NUM_BUFFER_SLOTS ; i++) {
                mSlots[i].dirtyRegion.clear();
            }
        }


        { // scope for the lock
            Mutex::Autolock lock(mMutex);
            int backBufferSlot(getSlotFromBufferLocked(backBuffer.get()));
            if (backBufferSlot >= 0) {
                Region& dirtyRegion(mSlots[backBufferSlot].dirtyRegion);
                mDirtyRegion.subtract(dirtyRegion);
                dirtyRegion = newDirtyRegion;
            }
        }

        mDirtyRegion.orSelf(newDirtyRegion);
        if (inOutDirtyBounds) {
            *inOutDirtyBounds = newDirtyRegion.getBounds();
        }

        void* vaddr;
        status_t res = backBuffer->lockAsync(
                GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
                newDirtyRegion.bounds(), &vaddr, fenceFd);

        ALOGW_IF(res, "failed locking buffer (handle = %p)",
                backBuffer->handle);

        if (res != 0) {
            err = INVALID_OPERATION;
        } else {
            mLockedBuffer = backBuffer;
            outBuffer->width  = backBuffer->width;
            outBuffer->height = backBuffer->height;
            outBuffer->stride = backBuffer->stride;
            outBuffer->format = backBuffer->format;
            outBuffer->bits   = vaddr;
        }
    }
    return err;
}

status_t Surface::unlockAndPost()
{
    if (mLockedBuffer == 0) {
        ALOGE("Surface::unlockAndPost failed, no locked buffer");
        return INVALID_OPERATION;
    }

    int fd = -1;
    status_t err = mLockedBuffer->unlockAsync(&fd);
    ALOGE_IF(err, "failed unlocking buffer (%p)", mLockedBuffer->handle);

    err = queueBuffer(mLockedBuffer.get(), fd);
    ALOGE_IF(err, "queueBuffer (handle=%p) failed (%s)",
            mLockedBuffer->handle, strerror(-err));

    mPostedBuffer = mLockedBuffer;
    mLockedBuffer = 0;
    return err;
}

bool Surface::waitForNextFrame(uint64_t lastFrame, nsecs_t timeout) {
    Mutex::Autolock lock(mMutex);
    if (mNextFrameNumber > lastFrame) {
      return true;
    }
    return mQueueBufferCondition.waitRelative(mMutex, timeout) == OK;
}

status_t Surface::getUniqueId(uint64_t* outId) const {
    Mutex::Autolock lock(mMutex);
    return mGraphicBufferProducer->getUniqueId(outId);
}

namespace view {

status_t Surface::writeToParcel(Parcel* parcel) const {
    return writeToParcel(parcel, false);
}

status_t Surface::writeToParcel(Parcel* parcel, bool nameAlreadyWritten) const {
    if (parcel == nullptr) return BAD_VALUE;

    status_t res = OK;

    if (!nameAlreadyWritten) {
        res = parcel->writeString16(name);
        if (res != OK) return res;

        /* isSingleBuffered defaults to no */
        res = parcel->writeInt32(0);
        if (res != OK) return res;
    }

    res = parcel->writeStrongBinder(
            IGraphicBufferProducer::asBinder(graphicBufferProducer));

    return res;
}

status_t Surface::readFromParcel(const Parcel* parcel) {
    return readFromParcel(parcel, false);
}

status_t Surface::readFromParcel(const Parcel* parcel, bool nameAlreadyRead) {
    if (parcel == nullptr) return BAD_VALUE;

    status_t res = OK;
    if (!nameAlreadyRead) {
        name = readMaybeEmptyString16(parcel);
        // Discard this for now
        int isSingleBuffered;
        res = parcel->readInt32(&isSingleBuffered);
        if (res != OK) {
            return res;
        }
    }

    sp<IBinder> binder;

    res = parcel->readStrongBinder(&binder);
    if (res != OK) return res;

    graphicBufferProducer = interface_cast<IGraphicBufferProducer>(binder);

    return OK;
}

String16 Surface::readMaybeEmptyString16(const Parcel* parcel) {
    size_t len;
    const char16_t* str = parcel->readString16Inplace(&len);
    if (str != nullptr) {
        return String16(str, len);
    } else {
        return String16();
    }
}

} // namespace view

}; // namespace android
