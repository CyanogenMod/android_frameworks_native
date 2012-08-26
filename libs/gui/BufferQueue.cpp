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

#define LOG_TAG "BufferQueue"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <gui/BufferQueue.h>
#include <gui/ISurfaceComposer.h>
#include <private/gui/ComposerService.h>

#include <utils/Log.h>
#include <gui/SurfaceTexture.h>
#include <utils/Trace.h>

// This compile option causes SurfaceTexture to return the buffer that is currently
// attached to the GL texture from dequeueBuffer when no other buffers are
// available.  It requires the drivers (Gralloc, GL, OMX IL, and Camera) to do
// implicit cross-process synchronization to prevent the buffer from being
// written to before the buffer has (a) been detached from the GL texture and
// (b) all GL reads from the buffer have completed.

// During refactoring, do not support dequeuing the current buffer
#undef ALLOW_DEQUEUE_CURRENT_BUFFER

#ifdef ALLOW_DEQUEUE_CURRENT_BUFFER
#define FLAG_ALLOW_DEQUEUE_CURRENT_BUFFER    true
#warning "ALLOW_DEQUEUE_CURRENT_BUFFER enabled"
#else
#define FLAG_ALLOW_DEQUEUE_CURRENT_BUFFER    false
#endif

// Macros for including the BufferQueue name in log messages
#define ST_LOGV(x, ...) ALOGV("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define ST_LOGD(x, ...) ALOGD("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define ST_LOGI(x, ...) ALOGI("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define ST_LOGW(x, ...) ALOGW("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)
#define ST_LOGE(x, ...) ALOGE("[%s] "x, mConsumerName.string(), ##__VA_ARGS__)

#define ATRACE_BUFFER_INDEX(index)                                            \
    if (ATRACE_ENABLED()) {                                                   \
        char ___traceBuf[1024];                                               \
        snprintf(___traceBuf, 1024, "%s: %d", mConsumerName.string(),         \
                (index));                                                     \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);           \
    }

namespace android {

// Get an ID that's unique within this process.
static int32_t createProcessUniqueId() {
    static volatile int32_t globalCounter = 0;
    return android_atomic_inc(&globalCounter);
}

static const char* scalingModeName(int scalingMode) {
    switch (scalingMode) {
        case NATIVE_WINDOW_SCALING_MODE_FREEZE: return "FREEZE";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW: return "SCALE_TO_WINDOW";
        case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP: return "SCALE_CROP";
        default: return "Unknown";
    }
}

BufferQueue::BufferQueue(  bool allowSynchronousMode, int bufferCount ) :
    mDefaultWidth(1),
    mDefaultHeight(1),
    mPixelFormat(PIXEL_FORMAT_RGBA_8888),
    mMinUndequeuedBuffers(bufferCount),
    mMinAsyncBufferSlots(bufferCount + 1),
    mMinSyncBufferSlots(bufferCount),
    mBufferCount(mMinAsyncBufferSlots),
    mClientBufferCount(0),
    mServerBufferCount(mMinAsyncBufferSlots),
    mSynchronousMode(false),
    mAllowSynchronousMode(allowSynchronousMode),
    mConnectedApi(NO_CONNECTED_API),
    mAbandoned(false),
    mFrameCounter(0),
    mBufferHasBeenQueued(false),
    mDefaultBufferFormat(0),
    mConsumerUsageBits(0),
    mTransformHint(0)
{
    // Choose a name using the PID and a process-unique ID.
    mConsumerName = String8::format("unnamed-%d-%d", getpid(), createProcessUniqueId());

    ST_LOGV("BufferQueue");

    ALOGI("BufferQueue: minUndequeued=%d", mMinUndequeuedBuffers);
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    mGraphicBufferAlloc = composer->createGraphicBufferAlloc();
    if (mGraphicBufferAlloc == 0) {
        ST_LOGE("createGraphicBufferAlloc() failed in BufferQueue()");
    }
}

BufferQueue::~BufferQueue() {
    ST_LOGV("~BufferQueue");
}

status_t BufferQueue::setBufferCountServerLocked(int bufferCount) {
    if (bufferCount > NUM_BUFFER_SLOTS)
        return BAD_VALUE;

    // special-case, nothing to do
    if (bufferCount == mBufferCount)
        return OK;

    if (!mClientBufferCount &&
        bufferCount >= mBufferCount) {
        // easy, we just have more buffers
        mBufferCount = bufferCount;
        mServerBufferCount = bufferCount;
        mDequeueCondition.broadcast();
    } else {
        // we're here because we're either
        // - reducing the number of available buffers
        // - or there is a client-buffer-count in effect

        // less than 2 buffers is never allowed
        if (bufferCount < 2)
            return BAD_VALUE;

        // when there is non client-buffer-count in effect, the client is not
        // allowed to dequeue more than one buffer at a time,
        // so the next time they dequeue a buffer, we know that they don't
        // own one. the actual resizing will happen during the next
        // dequeueBuffer.

        mServerBufferCount = bufferCount;
        mDequeueCondition.broadcast();
    }
    return OK;
}

bool BufferQueue::isSynchronousMode() const {
    Mutex::Autolock lock(mMutex);
    return mSynchronousMode;
}

void BufferQueue::setConsumerName(const String8& name) {
    Mutex::Autolock lock(mMutex);
    mConsumerName = name;
}

status_t BufferQueue::setDefaultBufferFormat(uint32_t defaultFormat) {
    Mutex::Autolock lock(mMutex);
    mDefaultBufferFormat = defaultFormat;
    return OK;
}

status_t BufferQueue::setConsumerUsageBits(uint32_t usage) {
    Mutex::Autolock lock(mMutex);
    mConsumerUsageBits = usage;
    return OK;
}

status_t BufferQueue::setTransformHint(uint32_t hint) {
    Mutex::Autolock lock(mMutex);
    mTransformHint = hint;
    return OK;
}

status_t BufferQueue::setBufferCount(int bufferCount) {
    ALOGI("setBufferCount: count=%d", bufferCount);

    sp<ConsumerListener> listener;
    {
        Mutex::Autolock lock(mMutex);

        if (mAbandoned) {
            ST_LOGE("setBufferCount: SurfaceTexture has been abandoned!");
            return NO_INIT;
        }
        if (bufferCount > NUM_BUFFER_SLOTS) {
            ST_LOGE("setBufferCount: bufferCount larger than slots available");
            return BAD_VALUE;
        }

        // Error out if the user has dequeued buffers
        for (int i=0 ; i<mBufferCount ; i++) {
            if (mSlots[i].mBufferState == BufferSlot::DEQUEUED) {
                ST_LOGE("setBufferCount: client owns some buffers");
                return -EINVAL;
            }
        }

        const int minBufferSlots = mSynchronousMode ?
            mMinSyncBufferSlots : mMinAsyncBufferSlots;
        if (bufferCount == 0) {
            mClientBufferCount = 0;
            bufferCount = (mServerBufferCount >= minBufferSlots) ?
                    mServerBufferCount : minBufferSlots;
            return setBufferCountServerLocked(bufferCount);
        }

        if (bufferCount < minBufferSlots) {
            ST_LOGE("setBufferCount: requested buffer count (%d) is less than "
                    "minimum (%d)", bufferCount, minBufferSlots);
            return BAD_VALUE;
        }

        // here we're guaranteed that the client doesn't have dequeued buffers
        // and will release all of its buffer references.
        freeAllBuffersLocked();
        mBufferCount = bufferCount;
        mClientBufferCount = bufferCount;
        mBufferHasBeenQueued = false;
        mQueue.clear();
        mDequeueCondition.broadcast();
        listener = mConsumerListener;
    } // scope for lock

    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return OK;
}

#ifdef QCOM_HARDWARE
status_t BufferQueue::setBuffersSize(int size) {
    ST_LOGV("setBuffersSize: size=%d", size);
    Mutex::Autolock lock(mMutex);
    mGraphicBufferAlloc->setGraphicBufferSize(size);
    return NO_ERROR;
}

status_t BufferQueue::setMinUndequeuedBufferCount(int count) {
    ALOGI("setMinUndequeuedBufferCount: count=%d", count);
    Mutex::Autolock lock(mMutex);
    mMinUndequeuedBuffers = count;
    return NO_ERROR;
}
#endif

int BufferQueue::query(int what, int* outValue)
{
    ATRACE_CALL();
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("query: SurfaceTexture has been abandoned!");
        return NO_INIT;
    }

    int value;
    switch (what) {
    case NATIVE_WINDOW_WIDTH:
        value = mDefaultWidth;
        break;
    case NATIVE_WINDOW_HEIGHT:
        value = mDefaultHeight;
        break;
    case NATIVE_WINDOW_FORMAT:
        value = mPixelFormat;
        break;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        value = mSynchronousMode ?
                (mMinUndequeuedBuffers-1) : mMinUndequeuedBuffers;
        break;
    case NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND:
        value = (mQueue.size() >= 2);
        break;
    default:
        return BAD_VALUE;
    }
    outValue[0] = value;
    return NO_ERROR;
}

status_t BufferQueue::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ATRACE_CALL();
    ST_LOGV("requestBuffer: slot=%d", slot);
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        ST_LOGE("requestBuffer: SurfaceTexture has been abandoned!");
        return NO_INIT;
    }
    if (slot < 0 || mBufferCount <= slot) {
        ST_LOGE("requestBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, slot);
        return BAD_VALUE;
    }
    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}

status_t BufferQueue::dequeueBuffer(int *outBuf, uint32_t w, uint32_t h,
        uint32_t format, uint32_t usage) {
    ATRACE_CALL();
    ST_LOGV("dequeueBuffer: w=%d h=%d fmt=%#x usage=%#x", w, h, format, usage);

    if ((w && !h) || (!w && h)) {
        ST_LOGE("dequeueBuffer: invalid size: w=%u, h=%u", w, h);
        return BAD_VALUE;
    }

    status_t returnFlags(OK);
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLSyncKHR fence = EGL_NO_SYNC_KHR;

    { // Scope for the lock
        Mutex::Autolock lock(mMutex);

        if (format == 0) {
            format = mDefaultBufferFormat;
        }
        // turn on usage bits the consumer requested
        usage |= mConsumerUsageBits;

        int found = -1;
        int foundSync = -1;
        int dequeuedCount = 0;
        bool tryAgain = true;
        while (tryAgain) {
            if (mAbandoned) {
                ST_LOGE("dequeueBuffer: SurfaceTexture has been abandoned!");
                return NO_INIT;
            }

            // We need to wait for the FIFO to drain if the number of buffer
            // needs to change.
            //
            // The condition "number of buffers needs to change" is true if
            // - the client doesn't care about how many buffers there are
            // - AND the actual number of buffer is different from what was
            //   set in the last setBufferCountServer()
            //                         - OR -
            //   setBufferCountServer() was set to a value incompatible with
            //   the synchronization mode (for instance because the sync mode
            //   changed since)
            //
            // As long as this condition is true AND the FIFO is not empty, we
            // wait on mDequeueCondition.

            const int minBufferCountNeeded = mSynchronousMode ?
                    mMinSyncBufferSlots : mMinAsyncBufferSlots;

            const bool numberOfBuffersNeedsToChange = !mClientBufferCount &&
                    ((mServerBufferCount != mBufferCount) ||
                            (mServerBufferCount < minBufferCountNeeded));

            if (!mQueue.isEmpty() && numberOfBuffersNeedsToChange) {
                // wait for the FIFO to drain
                mDequeueCondition.wait(mMutex);
                // NOTE: we continue here because we need to reevaluate our
                // whole state (eg: we could be abandoned or disconnected)
                continue;
            }

            if (numberOfBuffersNeedsToChange) {
                // here we're guaranteed that mQueue is empty
                freeAllBuffersLocked();
                mBufferCount = mServerBufferCount;
                if (mBufferCount < minBufferCountNeeded)
                    mBufferCount = minBufferCountNeeded;
                mBufferHasBeenQueued = false;
                returnFlags |= ISurfaceTexture::RELEASE_ALL_BUFFERS;
            }

            // look for a free buffer to give to the client
            found = INVALID_BUFFER_SLOT;
            foundSync = INVALID_BUFFER_SLOT;
            dequeuedCount = 0;
            for (int i = 0; i < mBufferCount; i++) {
                const int state = mSlots[i].mBufferState;
                if (state == BufferSlot::DEQUEUED) {
                    dequeuedCount++;
                }

                // this logic used to be if (FLAG_ALLOW_DEQUEUE_CURRENT_BUFFER)
                // but dequeuing the current buffer is disabled.
                if (false) {
                    // This functionality has been temporarily removed so
                    // BufferQueue and SurfaceTexture can be refactored into
                    // separate objects
                } else {
                    if (state == BufferSlot::FREE) {
                        /* We return the oldest of the free buffers to avoid
                         * stalling the producer if possible.  This is because
                         * the consumer may still have pending reads of the
                         * buffers in flight.
                         */
                        bool isOlder = mSlots[i].mFrameNumber <
                                mSlots[found].mFrameNumber;
                        if (found < 0 || isOlder) {
                            foundSync = i;
                            found = i;
                        }
                    }
                }
            }

            // clients are not allowed to dequeue more than one buffer
            // if they didn't set a buffer count.
            if (!mClientBufferCount && dequeuedCount) {
                ST_LOGE("dequeueBuffer: can't dequeue multiple buffers without "
                        "setting the buffer count");
                return -EINVAL;
            }

            // See whether a buffer has been queued since the last
            // setBufferCount so we know whether to perform the
            // mMinUndequeuedBuffers check below.
            if (mBufferHasBeenQueued) {
                // make sure the client is not trying to dequeue more buffers
                // than allowed.
                const int avail = mBufferCount - (dequeuedCount+1);
                if (avail < (mMinUndequeuedBuffers-int(mSynchronousMode))) {
                    ST_LOGE("dequeueBuffer: mMinUndequeuedBuffers=%d exceeded "
                            "(dequeued=%d)",
                            mMinUndequeuedBuffers-int(mSynchronousMode),
                            dequeuedCount);
                    return -EBUSY;
                }
            }

            // if no buffer is found, wait for a buffer to be released
            tryAgain = found == INVALID_BUFFER_SLOT;
            if (tryAgain) {
                mDequeueCondition.wait(mMutex);
            }
        }


        if (found == INVALID_BUFFER_SLOT) {
            // This should not happen.
            ST_LOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        const int buf = found;
        *outBuf = found;

        ATRACE_BUFFER_INDEX(buf);

        const bool useDefaultSize = !w && !h;
        if (useDefaultSize) {
            // use the default size
            w = mDefaultWidth;
            h = mDefaultHeight;
        }

        const bool updateFormat = (format != 0);
        if (!updateFormat) {
            // keep the current (or default) format
            format = mPixelFormat;
        }

        // buffer is now in DEQUEUED (but can also be current at the same time,
        // if we're in synchronous mode)
        mSlots[buf].mBufferState = BufferSlot::DEQUEUED;

        const sp<GraphicBuffer>& buffer(mSlots[buf].mGraphicBuffer);
        if ((buffer == NULL) ||
            (uint32_t(buffer->width)  != w) ||
            (uint32_t(buffer->height) != h) ||
            (uint32_t(buffer->format) != format) ||
            ((uint32_t(buffer->usage) & usage) != usage))
        {
            status_t error;
            sp<GraphicBuffer> graphicBuffer(
                    mGraphicBufferAlloc->createGraphicBuffer(
                            w, h, format, usage, &error));
            if (graphicBuffer == 0) {
                ST_LOGE("dequeueBuffer: SurfaceComposer::createGraphicBuffer "
                        "failed");
                return error;
            }
            if (updateFormat) {
                mPixelFormat = format;
            }

            mSlots[buf].mAcquireCalled = false;
            mSlots[buf].mGraphicBuffer = graphicBuffer;
            mSlots[buf].mRequestBufferCalled = false;
            mSlots[buf].mFence = EGL_NO_SYNC_KHR;
            mSlots[buf].mEglDisplay = EGL_NO_DISPLAY;

            returnFlags |= ISurfaceTexture::BUFFER_NEEDS_REALLOCATION;
        }

        dpy = mSlots[buf].mEglDisplay;
        fence = mSlots[buf].mFence;
        mSlots[buf].mFence = EGL_NO_SYNC_KHR;
    }  // end lock scope

    if (fence != EGL_NO_SYNC_KHR) {
        EGLint result = eglClientWaitSyncKHR(dpy, fence, 0, 1000000000);
        // If something goes wrong, log the error, but return the buffer without
        // synchronizing access to it.  It's too late at this point to abort the
        // dequeue operation.
        if (result == EGL_FALSE) {
            ST_LOGE("dequeueBuffer: error waiting for fence: %#x", eglGetError());
        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
            ST_LOGE("dequeueBuffer: timeout waiting for fence");
        }
        eglDestroySyncKHR(dpy, fence);
    }

    ST_LOGV("dequeueBuffer: returning slot=%d buf=%p flags=%#x", *outBuf,
            mSlots[*outBuf].mGraphicBuffer->handle, returnFlags);

    return returnFlags;
}

status_t BufferQueue::setSynchronousMode(bool enabled) {
    ATRACE_CALL();
    ST_LOGV("setSynchronousMode: enabled=%d", enabled);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("setSynchronousMode: SurfaceTexture has been abandoned!");
        return NO_INIT;
    }

    status_t err = OK;
    if (!mAllowSynchronousMode && enabled)
        return err;

    if (!enabled) {
        // going to asynchronous mode, drain the queue
        err = drainQueueLocked();
        if (err != NO_ERROR)
            return err;
    }

    if (mSynchronousMode != enabled) {
        // - if we're going to asynchronous mode, the queue is guaranteed to be
        // empty here
        // - if the client set the number of buffers, we're guaranteed that
        // we have at least 3 (because we don't allow less)
        mSynchronousMode = enabled;
        mDequeueCondition.broadcast();
    }
    return err;
}

status_t BufferQueue::queueBuffer(int buf,
        const QueueBufferInput& input, QueueBufferOutput* output) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(buf);

    Rect crop;
    uint32_t transform;
    int scalingMode;
    int64_t timestamp;

    input.deflate(&timestamp, &crop, &scalingMode, &transform);

    ST_LOGV("queueBuffer: slot=%d time=%#llx crop=[%d,%d,%d,%d] tr=%#x "
            "scale=%s",
            buf, timestamp, crop.left, crop.top, crop.right, crop.bottom,
            transform, scalingModeName(scalingMode));

    sp<ConsumerListener> listener;

    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        if (mAbandoned) {
            ST_LOGE("queueBuffer: SurfaceTexture has been abandoned!");
            return NO_INIT;
        }
        if (buf < 0 || buf >= mBufferCount) {
            ST_LOGE("queueBuffer: slot index out of range [0, %d]: %d",
                    mBufferCount, buf);
            return -EINVAL;
        } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
            ST_LOGE("queueBuffer: slot %d is not owned by the client "
                    "(state=%d)", buf, mSlots[buf].mBufferState);
            return -EINVAL;
        } else if (!mSlots[buf].mRequestBufferCalled) {
            ST_LOGE("queueBuffer: slot %d was enqueued without requesting a "
                    "buffer", buf);
            return -EINVAL;
        }

        const sp<GraphicBuffer>& graphicBuffer(mSlots[buf].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedCrop;
        crop.intersect(bufferRect, &croppedCrop);
        if (croppedCrop != crop) {
            ST_LOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", buf);
            return -EINVAL;
        }

        if (mSynchronousMode) {
            // In synchronous mode we queue all buffers in a FIFO.
            mQueue.push_back(buf);

            // Synchronous mode always signals that an additional frame should
            // be consumed.
            listener = mConsumerListener;
        } else {
            // In asynchronous mode we only keep the most recent buffer.
            if (mQueue.empty()) {
                mQueue.push_back(buf);

                // Asynchronous mode only signals that a frame should be
                // consumed if no previous frame was pending. If a frame were
                // pending then the consumer would have already been notified.
                listener = mConsumerListener;
            } else {
                Fifo::iterator front(mQueue.begin());
                // buffer currently queued is freed
                mSlots[*front].mBufferState = BufferSlot::FREE;
                // and we record the new buffer index in the queued list
                *front = buf;
            }
        }

        mSlots[buf].mTimestamp = timestamp;
        mSlots[buf].mCrop = crop;
        mSlots[buf].mTransform = transform;

        switch (scalingMode) {
            case NATIVE_WINDOW_SCALING_MODE_FREEZE:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
                break;
            default:
                ST_LOGE("unknown scaling mode: %d (ignoring)", scalingMode);
                scalingMode = mSlots[buf].mScalingMode;
                break;
        }

        mSlots[buf].mBufferState = BufferSlot::QUEUED;
        mSlots[buf].mScalingMode = scalingMode;
        mFrameCounter++;
        mSlots[buf].mFrameNumber = mFrameCounter;

        mBufferHasBeenQueued = true;
        mDequeueCondition.broadcast();

        output->inflate(mDefaultWidth, mDefaultHeight, mTransformHint,
                mQueue.size());

        ATRACE_INT(mConsumerName.string(), mQueue.size());
    } // scope for the lock

    // call back without lock held
    if (listener != 0) {
        listener->onFrameAvailable();
    }
    return OK;
}

void BufferQueue::cancelBuffer(int buf) {
    ATRACE_CALL();
    ST_LOGV("cancelBuffer: slot=%d", buf);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGW("cancelBuffer: BufferQueue has been abandoned!");
        return;
    }

    if (buf < 0 || buf >= mBufferCount) {
        ST_LOGE("cancelBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, buf);
        return;
    } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
        ST_LOGE("cancelBuffer: slot %d is not owned by the client (state=%d)",
                buf, mSlots[buf].mBufferState);
        return;
    }
    mSlots[buf].mBufferState = BufferSlot::FREE;
    mSlots[buf].mFrameNumber = 0;
    mDequeueCondition.broadcast();
}

status_t BufferQueue::connect(int api, QueueBufferOutput* output) {
    ATRACE_CALL();
    ST_LOGV("connect: api=%d", api);
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("connect: BufferQueue has been abandoned!");
        return NO_INIT;
    }

    if (mConsumerListener == NULL) {
        ST_LOGE("connect: BufferQueue has no consumer!");
        return NO_INIT;
    }

    int err = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            if (mConnectedApi != NO_CONNECTED_API) {
                ST_LOGE("connect: already connected (cur=%d, req=%d)",
                        mConnectedApi, api);
                err = -EINVAL;
            } else {
                mConnectedApi = api;
                output->inflate(mDefaultWidth, mDefaultHeight, mTransformHint,
                        mQueue.size());
            }
            break;
        default:
            err = -EINVAL;
            break;
    }

    mBufferHasBeenQueued = false;

    return err;
}

status_t BufferQueue::disconnect(int api) {
    ATRACE_CALL();
    ST_LOGV("disconnect: api=%d", api);

    int err = NO_ERROR;
    sp<ConsumerListener> listener;

    { // Scope for the lock
        Mutex::Autolock lock(mMutex);

        if (mAbandoned) {
            // it is not really an error to disconnect after the surface
            // has been abandoned, it should just be a no-op.
            return NO_ERROR;
        }

        switch (api) {
            case NATIVE_WINDOW_API_EGL:
            case NATIVE_WINDOW_API_CPU:
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                if (mConnectedApi == api) {
                    drainQueueAndFreeBuffersLocked();
                    mConnectedApi = NO_CONNECTED_API;
                    mDequeueCondition.broadcast();
                    listener = mConsumerListener;
                } else {
                    ST_LOGE("disconnect: connected to another api (cur=%d, req=%d)",
                            mConnectedApi, api);
                    err = -EINVAL;
                }
                break;
            default:
                ST_LOGE("disconnect: unknown API %d", api);
                err = -EINVAL;
                break;
        }
    }

    if (listener != NULL) {
        listener->onBuffersReleased();
    }

    return err;
}

void BufferQueue::dump(String8& result) const
{
    char buffer[1024];
    BufferQueue::dump(result, "", buffer, 1024);
}

void BufferQueue::dump(String8& result, const char* prefix,
        char* buffer, size_t SIZE) const
{
    Mutex::Autolock _l(mMutex);

    String8 fifo;
    int fifoSize = 0;
    Fifo::const_iterator i(mQueue.begin());
    while (i != mQueue.end()) {
       snprintf(buffer, SIZE, "%02d ", *i++);
       fifoSize++;
       fifo.append(buffer);
    }

    snprintf(buffer, SIZE,
            "%s-BufferQueue mBufferCount=%d, mSynchronousMode=%d, default-size=[%dx%d], "
            "mPixelFormat=%d, FIFO(%d)={%s}\n",
            prefix, mBufferCount, mSynchronousMode, mDefaultWidth,
            mDefaultHeight, mPixelFormat, fifoSize, fifo.string());
    result.append(buffer);


    struct {
        const char * operator()(int state) const {
            switch (state) {
                case BufferSlot::DEQUEUED: return "DEQUEUED";
                case BufferSlot::QUEUED: return "QUEUED";
                case BufferSlot::FREE: return "FREE";
                case BufferSlot::ACQUIRED: return "ACQUIRED";
                default: return "Unknown";
            }
        }
    } stateName;

    for (int i=0 ; i<mBufferCount ; i++) {
        const BufferSlot& slot(mSlots[i]);
        snprintf(buffer, SIZE,
                "%s%s[%02d] "
                "state=%-8s, crop=[%d,%d,%d,%d], "
                "xform=0x%02x, time=%#llx, scale=%s",
                prefix, (slot.mBufferState == BufferSlot::ACQUIRED)?">":" ", i,
                stateName(slot.mBufferState),
                slot.mCrop.left, slot.mCrop.top, slot.mCrop.right,
                slot.mCrop.bottom, slot.mTransform, slot.mTimestamp,
                scalingModeName(slot.mScalingMode)
        );
        result.append(buffer);

        const sp<GraphicBuffer>& buf(slot.mGraphicBuffer);
        if (buf != NULL) {
            snprintf(buffer, SIZE,
                    ", %p [%4ux%4u:%4u,%3X]",
                    buf->handle, buf->width, buf->height, buf->stride,
                    buf->format);
            result.append(buffer);
        }
        result.append("\n");
    }
}

void BufferQueue::freeBufferLocked(int i) {
    mSlots[i].mGraphicBuffer = 0;
    if (mSlots[i].mBufferState == BufferSlot::ACQUIRED) {
        mSlots[i].mNeedsCleanupOnRelease = true;
    }
    mSlots[i].mBufferState = BufferSlot::FREE;
    mSlots[i].mFrameNumber = 0;
    mSlots[i].mAcquireCalled = false;

    // destroy fence as BufferQueue now takes ownership
    if (mSlots[i].mFence != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(mSlots[i].mEglDisplay, mSlots[i].mFence);
        mSlots[i].mFence = EGL_NO_SYNC_KHR;
    }
}

void BufferQueue::freeAllBuffersLocked() {
    ALOGW_IF(!mQueue.isEmpty(),
            "freeAllBuffersLocked called but mQueue is not empty");
    mQueue.clear();
    mBufferHasBeenQueued = false;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        freeBufferLocked(i);
    }
}

status_t BufferQueue::acquireBuffer(BufferItem *buffer) {
    ATRACE_CALL();
    Mutex::Autolock _l(mMutex);
    // check if queue is empty
    // In asynchronous mode the list is guaranteed to be one buffer
    // deep, while in synchronous mode we use the oldest buffer.
    if (!mQueue.empty()) {
        Fifo::iterator front(mQueue.begin());
        int buf = *front;

        ATRACE_BUFFER_INDEX(buf);

        if (mSlots[buf].mAcquireCalled) {
            buffer->mGraphicBuffer = NULL;
        } else {
            buffer->mGraphicBuffer = mSlots[buf].mGraphicBuffer;
        }
        buffer->mCrop = mSlots[buf].mCrop;
        buffer->mTransform = mSlots[buf].mTransform;
        buffer->mScalingMode = mSlots[buf].mScalingMode;
        buffer->mFrameNumber = mSlots[buf].mFrameNumber;
        buffer->mTimestamp = mSlots[buf].mTimestamp;
        buffer->mBuf = buf;
        mSlots[buf].mAcquireCalled = true;

        mSlots[buf].mBufferState = BufferSlot::ACQUIRED;
        mQueue.erase(front);
        mDequeueCondition.broadcast();

        ATRACE_INT(mConsumerName.string(), mQueue.size());
    } else {
        return NO_BUFFER_AVAILABLE;
    }

    return OK;
}

status_t BufferQueue::releaseBuffer(int buf, EGLDisplay display,
        EGLSyncKHR fence) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(buf);

    Mutex::Autolock _l(mMutex);

    if (buf == INVALID_BUFFER_SLOT) {
        return -EINVAL;
    }

    mSlots[buf].mEglDisplay = display;
    mSlots[buf].mFence = fence;

    // The buffer can now only be released if its in the acquired state
    if (mSlots[buf].mBufferState == BufferSlot::ACQUIRED) {
        mSlots[buf].mBufferState = BufferSlot::FREE;
    } else if (mSlots[buf].mNeedsCleanupOnRelease) {
        ST_LOGV("releasing a stale buf %d its state was %d", buf, mSlots[buf].mBufferState);
        mSlots[buf].mNeedsCleanupOnRelease = false;
        return STALE_BUFFER_SLOT;
    } else {
        ST_LOGE("attempted to release buf %d but its state was %d", buf, mSlots[buf].mBufferState);
        return -EINVAL;
    }

    mDequeueCondition.broadcast();
    return OK;
}

status_t BufferQueue::consumerConnect(const sp<ConsumerListener>& consumerListener) {
    ST_LOGV("consumerConnect");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("consumerConnect: BufferQueue has been abandoned!");
        return NO_INIT;
    }

    mConsumerListener = consumerListener;

    return OK;
}

status_t BufferQueue::consumerDisconnect() {
    ST_LOGV("consumerDisconnect");
    Mutex::Autolock lock(mMutex);

    if (mConsumerListener == NULL) {
        ST_LOGE("consumerDisconnect: No consumer is connected!");
        return -EINVAL;
    }

    mAbandoned = true;
    mConsumerListener = NULL;
    mQueue.clear();
    freeAllBuffersLocked();
    mDequeueCondition.broadcast();
    return OK;
}

status_t BufferQueue::getReleasedBuffers(uint32_t* slotMask) {
    ST_LOGV("getReleasedBuffers");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        ST_LOGE("getReleasedBuffers: BufferQueue has been abandoned!");
        return NO_INIT;
    }

    uint32_t mask = 0;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (!mSlots[i].mAcquireCalled) {
            mask |= 1 << i;
        }
    }
    *slotMask = mask;

    ST_LOGV("getReleasedBuffers: returning mask %#x", mask);
    return NO_ERROR;
}

status_t BufferQueue::setDefaultBufferSize(uint32_t w, uint32_t h)
{
    ST_LOGV("setDefaultBufferSize: w=%d, h=%d", w, h);
    if (!w || !h) {
        ST_LOGE("setDefaultBufferSize: dimensions cannot be 0 (w=%d, h=%d)",
                w, h);
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    mDefaultWidth = w;
    mDefaultHeight = h;
    return OK;
}

status_t BufferQueue::setBufferCountServer(int bufferCount) {
    ATRACE_CALL();
    Mutex::Autolock lock(mMutex);
    return setBufferCountServerLocked(bufferCount);
}

void BufferQueue::freeAllBuffersExceptHeadLocked() {
    int head = -1;
    if (!mQueue.empty()) {
        Fifo::iterator front(mQueue.begin());
        head = *front;
    }
    mBufferHasBeenQueued = false;
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        if (i != head) {
            freeBufferLocked(i);
        }
    }
}

status_t BufferQueue::drainQueueLocked() {
    while (mSynchronousMode && !mQueue.isEmpty()) {
        mDequeueCondition.wait(mMutex);
        if (mAbandoned) {
            ST_LOGE("drainQueueLocked: BufferQueue has been abandoned!");
            return NO_INIT;
        }
        if (mConnectedApi == NO_CONNECTED_API) {
            ST_LOGE("drainQueueLocked: BufferQueue is not connected!");
            return NO_INIT;
        }
    }
    return NO_ERROR;
}

status_t BufferQueue::drainQueueAndFreeBuffersLocked() {
    status_t err = drainQueueLocked();
    if (err == NO_ERROR) {
        if (mSynchronousMode) {
            freeAllBuffersLocked();
        } else {
            freeAllBuffersExceptHeadLocked();
        }
    }
    return err;
}

BufferQueue::ProxyConsumerListener::ProxyConsumerListener(
        const wp<BufferQueue::ConsumerListener>& consumerListener):
        mConsumerListener(consumerListener) {}

BufferQueue::ProxyConsumerListener::~ProxyConsumerListener() {}

void BufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<BufferQueue::ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}

void BufferQueue::ProxyConsumerListener::onBuffersReleased() {
    sp<BufferQueue::ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onBuffersReleased();
    }
}

}; // namespace android
