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

#define LOG_TAG "GLConsumer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cutils/compiler.h>

#include <hardware/hardware.h>

#include <gui/BufferItem.h>
#include <gui/GLConsumer.h>
#include <gui/IGraphicBufferAlloc.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <private/gui/ComposerService.h>
#include <private/gui/SyncFeatures.h>

#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/Trace.h>

EGLAPI const char* eglQueryStringImplementationANDROID(EGLDisplay dpy, EGLint name);
#define CROP_EXT_STR "EGL_ANDROID_image_crop"
#define PROT_CONTENT_EXT_STR "EGL_EXT_protected_content"
#define EGL_PROTECTED_CONTENT_EXT 0x32C0

namespace android {

// Macros for including the GLConsumer name in log messages
#define GLC_LOGV(x, ...) ALOGV("[%s] " x, mName.string(), ##__VA_ARGS__)
#define GLC_LOGD(x, ...) ALOGD("[%s] " x, mName.string(), ##__VA_ARGS__)
//#define GLC_LOGI(x, ...) ALOGI("[%s] " x, mName.string(), ##__VA_ARGS__)
#define GLC_LOGW(x, ...) ALOGW("[%s] " x, mName.string(), ##__VA_ARGS__)
#define GLC_LOGE(x, ...) ALOGE("[%s] " x, mName.string(), ##__VA_ARGS__)

static const struct {
    uint32_t width, height;
    char const* bits;
} kDebugData = { 15, 12,
    "_______________"
    "_______________"
    "_____XX_XX_____"
    "__X_X_____X_X__"
    "__X_XXXXXXX_X__"
    "__XXXXXXXXXXX__"
    "___XX_XXX_XX___"
    "____XXXXXXX____"
    "_____X___X_____"
    "____X_____X____"
    "_______________"
    "_______________"
};

// Transform matrices
static float mtxIdentity[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};
static float mtxFlipH[16] = {
    -1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    1, 0, 0, 1,
};
static float mtxFlipV[16] = {
    1, 0, 0, 0,
    0, -1, 0, 0,
    0, 0, 1, 0,
    0, 1, 0, 1,
};
static float mtxRot90[16] = {
    0, 1, 0, 0,
    -1, 0, 0, 0,
    0, 0, 1, 0,
    1, 0, 0, 1,
};

static void mtxMul(float out[16], const float a[16], const float b[16]);

Mutex GLConsumer::sStaticInitLock;
sp<GraphicBuffer> GLConsumer::sReleasedTexImageBuffer;

static bool hasEglAndroidImageCropImpl() {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    const char* exts = eglQueryStringImplementationANDROID(dpy, EGL_EXTENSIONS);
    size_t cropExtLen = strlen(CROP_EXT_STR);
    size_t extsLen = strlen(exts);
    bool equal = !strcmp(CROP_EXT_STR, exts);
    bool atStart = !strncmp(CROP_EXT_STR " ", exts, cropExtLen+1);
    bool atEnd = (cropExtLen+1) < extsLen &&
            !strcmp(" " CROP_EXT_STR, exts + extsLen - (cropExtLen+1));
    bool inMiddle = strstr(exts, " " CROP_EXT_STR " ");
    return equal || atStart || atEnd || inMiddle;
}

static bool hasEglAndroidImageCrop() {
    // Only compute whether the extension is present once the first time this
    // function is called.
    static bool hasIt = hasEglAndroidImageCropImpl();
    return hasIt;
}

static bool hasEglProtectedContentImpl() {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    const char* exts = eglQueryString(dpy, EGL_EXTENSIONS);
    size_t cropExtLen = strlen(PROT_CONTENT_EXT_STR);
    size_t extsLen = strlen(exts);
    bool equal = !strcmp(PROT_CONTENT_EXT_STR, exts);
    bool atStart = !strncmp(PROT_CONTENT_EXT_STR " ", exts, cropExtLen+1);
    bool atEnd = (cropExtLen+1) < extsLen &&
            !strcmp(" " PROT_CONTENT_EXT_STR, exts + extsLen - (cropExtLen+1));
    bool inMiddle = strstr(exts, " " PROT_CONTENT_EXT_STR " ");
    return equal || atStart || atEnd || inMiddle;
}

static bool hasEglProtectedContent() {
    // Only compute whether the extension is present once the first time this
    // function is called.
    static bool hasIt = hasEglProtectedContentImpl();
    return hasIt;
}

static bool isEglImageCroppable(const Rect& crop) {
    return hasEglAndroidImageCrop() && (crop.left == 0 && crop.top == 0);
}

GLConsumer::GLConsumer(const sp<IGraphicBufferConsumer>& bq, uint32_t tex,
        uint32_t texTarget, bool useFenceSync, bool isControlledByApp) :
    ConsumerBase(bq, isControlledByApp),
    mCurrentCrop(Rect::EMPTY_RECT),
    mCurrentTransform(0),
    mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
    mCurrentFence(Fence::NO_FENCE),
    mCurrentTimestamp(0),
    mCurrentFrameNumber(0),
    mDefaultWidth(1),
    mDefaultHeight(1),
    mFilteringEnabled(true),
    mTexName(tex),
    mUseFenceSync(useFenceSync),
    mTexTarget(texTarget),
    mEglDisplay(EGL_NO_DISPLAY),
    mEglContext(EGL_NO_CONTEXT),
    mCurrentTexture(BufferQueue::INVALID_BUFFER_SLOT),
    mAttached(true)
{
    GLC_LOGV("GLConsumer");

    memcpy(mCurrentTransformMatrix, mtxIdentity,
            sizeof(mCurrentTransformMatrix));

    mConsumer->setConsumerUsageBits(DEFAULT_USAGE_FLAGS);
}

GLConsumer::GLConsumer(const sp<IGraphicBufferConsumer>& bq, uint32_t texTarget,
        bool useFenceSync, bool isControlledByApp) :
    ConsumerBase(bq, isControlledByApp),
    mCurrentCrop(Rect::EMPTY_RECT),
    mCurrentTransform(0),
    mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
    mCurrentFence(Fence::NO_FENCE),
    mCurrentTimestamp(0),
    mCurrentFrameNumber(0),
    mDefaultWidth(1),
    mDefaultHeight(1),
    mFilteringEnabled(true),
    mTexName(0),
    mUseFenceSync(useFenceSync),
    mTexTarget(texTarget),
    mEglDisplay(EGL_NO_DISPLAY),
    mEglContext(EGL_NO_CONTEXT),
    mCurrentTexture(BufferQueue::INVALID_BUFFER_SLOT),
    mAttached(false)
{
    GLC_LOGV("GLConsumer");

    memcpy(mCurrentTransformMatrix, mtxIdentity,
            sizeof(mCurrentTransformMatrix));

    mConsumer->setConsumerUsageBits(DEFAULT_USAGE_FLAGS);
}

status_t GLConsumer::setDefaultBufferSize(uint32_t w, uint32_t h)
{
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setDefaultBufferSize: GLConsumer is abandoned!");
        return NO_INIT;
    }
    mDefaultWidth = w;
    mDefaultHeight = h;
    return mConsumer->setDefaultBufferSize(w, h);
}

status_t GLConsumer::updateTexImage() {
    ATRACE_CALL();
    GLC_LOGV("updateTexImage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("updateTexImage: GLConsumer is abandoned!");
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
    err = acquireBufferLocked(&item, 0);
    if (err != NO_ERROR) {
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            // We always bind the texture even if we don't update its contents.
            GLC_LOGV("updateTexImage: no buffers were available");
            glBindTexture(mTexTarget, mTexName);
            err = NO_ERROR;
        } else {
            GLC_LOGE("updateTexImage: acquire failed: %s (%d)",
                strerror(-err), err);
        }
        return err;
    }

    // Release the previous buffer.
    err = updateAndReleaseLocked(item);
    if (err != NO_ERROR) {
        // We always bind the texture.
        glBindTexture(mTexTarget, mTexName);
        return err;
    }

    // Bind the new buffer to the GL texture, and wait until it's ready.
    return bindTextureImageLocked();
}


status_t GLConsumer::releaseTexImage() {
    ATRACE_CALL();
    GLC_LOGV("releaseTexImage");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("releaseTexImage: GLConsumer is abandoned!");
        return NO_INIT;
    }

    // Make sure the EGL state is the same as in previous calls.
    status_t err = NO_ERROR;

    if (mAttached) {
        err = checkAndUpdateEglStateLocked(true);
        if (err != NO_ERROR) {
            return err;
        }
    } else {
        // if we're detached, no need to validate EGL's state -- we won't use it.
    }

    // Update the GLConsumer state.
    int buf = mCurrentTexture;
    if (buf != BufferQueue::INVALID_BUFFER_SLOT) {

        GLC_LOGV("releaseTexImage: (slot=%d, mAttached=%d)", buf, mAttached);

        if (mAttached) {
            // Do whatever sync ops we need to do before releasing the slot.
            err = syncForReleaseLocked(mEglDisplay);
            if (err != NO_ERROR) {
                GLC_LOGE("syncForReleaseLocked failed (slot=%d), err=%d", buf, err);
                return err;
            }
        } else {
            // if we're detached, we just use the fence that was created in detachFromContext()
            // so... basically, nothing more to do here.
        }

        err = releaseBufferLocked(buf, mSlots[buf].mGraphicBuffer, mEglDisplay, EGL_NO_SYNC_KHR);
        if (err < NO_ERROR) {
            GLC_LOGE("releaseTexImage: failed to release buffer: %s (%d)",
                    strerror(-err), err);
            return err;
        }

        if (mReleasedTexImage == NULL) {
            mReleasedTexImage = new EglImage(getDebugTexImageBuffer());
        }

        mCurrentTexture = BufferQueue::INVALID_BUFFER_SLOT;
        mCurrentTextureImage = mReleasedTexImage;
        mCurrentCrop.makeInvalid();
        mCurrentTransform = 0;
        mCurrentTimestamp = 0;
        mCurrentFence = Fence::NO_FENCE;

        if (mAttached) {
            // This binds a dummy buffer (mReleasedTexImage).
            status_t result = bindTextureImageLocked();
            if (result != NO_ERROR) {
                return result;
            }
        } else {
            // detached, don't touch the texture (and we may not even have an
            // EGLDisplay here.
        }
    }

    return NO_ERROR;
}

sp<GraphicBuffer> GLConsumer::getDebugTexImageBuffer() {
    Mutex::Autolock _l(sStaticInitLock);
    if (CC_UNLIKELY(sReleasedTexImageBuffer == NULL)) {
        // The first time, create the debug texture in case the application
        // continues to use it.
        sp<GraphicBuffer> buffer = new GraphicBuffer(
                kDebugData.width, kDebugData.height, PIXEL_FORMAT_RGBA_8888,
                GraphicBuffer::USAGE_SW_WRITE_RARELY,
                "[GLConsumer debug texture]");
        uint32_t* bits;
        buffer->lock(GraphicBuffer::USAGE_SW_WRITE_RARELY, reinterpret_cast<void**>(&bits));
        uint32_t stride = buffer->getStride();
        uint32_t height = buffer->getHeight();
        memset(bits, 0, stride * height * 4);
        for (uint32_t y = 0; y < kDebugData.height; y++) {
            for (uint32_t x = 0; x < kDebugData.width; x++) {
                bits[x] = (kDebugData.bits[y + kDebugData.width + x] == 'X') ?
                    0xFF000000 : 0xFFFFFFFF;
            }
            bits += stride;
        }
        buffer->unlock();
        sReleasedTexImageBuffer = buffer;
    }
    return sReleasedTexImageBuffer;
}

status_t GLConsumer::acquireBufferLocked(BufferItem *item,
        nsecs_t presentWhen, uint64_t maxFrameNumber) {
    status_t err = ConsumerBase::acquireBufferLocked(item, presentWhen,
            maxFrameNumber);
    if (err != NO_ERROR) {
        return err;
    }

    // If item->mGraphicBuffer is not null, this buffer has not been acquired
    // before, so any prior EglImage created is using a stale buffer. This
    // replaces any old EglImage with a new one (using the new buffer).
    if (item->mGraphicBuffer != NULL) {
        int slot = item->mSlot;
        mEglSlots[slot].mEglImage = new EglImage(item->mGraphicBuffer);
    }

    return NO_ERROR;
}

status_t GLConsumer::releaseBufferLocked(int buf,
        sp<GraphicBuffer> graphicBuffer,
        EGLDisplay display, EGLSyncKHR eglFence) {
    // release the buffer if it hasn't already been discarded by the
    // BufferQueue. This can happen, for example, when the producer of this
    // buffer has reallocated the original buffer slot after this buffer
    // was acquired.
    status_t err = ConsumerBase::releaseBufferLocked(
            buf, graphicBuffer, display, eglFence);
    mEglSlots[buf].mEglFence = EGL_NO_SYNC_KHR;
    return err;
}

status_t GLConsumer::updateAndReleaseLocked(const BufferItem& item,
        PendingRelease* pendingRelease)
{
    status_t err = NO_ERROR;

    int slot = item.mSlot;

    if (!mAttached) {
        GLC_LOGE("updateAndRelease: GLConsumer is not attached to an OpenGL "
                "ES context");
        releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return INVALID_OPERATION;
    }

    // Confirm state.
    err = checkAndUpdateEglStateLocked();
    if (err != NO_ERROR) {
        releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return err;
    }

    // Ensure we have a valid EglImageKHR for the slot, creating an EglImage
    // if nessessary, for the gralloc buffer currently in the slot in
    // ConsumerBase.
    // We may have to do this even when item.mGraphicBuffer == NULL (which
    // means the buffer was previously acquired).
    err = mEglSlots[slot].mEglImage->createIfNeeded(mEglDisplay, item.mCrop);
    if (err != NO_ERROR) {
        GLC_LOGW("updateAndRelease: unable to createImage on display=%p slot=%d",
                mEglDisplay, slot);
        releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer,
                mEglDisplay, EGL_NO_SYNC_KHR);
        return UNKNOWN_ERROR;
    }

    // Do whatever sync ops we need to do before releasing the old slot.
    if (slot != mCurrentTexture) {
        err = syncForReleaseLocked(mEglDisplay);
        if (err != NO_ERROR) {
            // Release the buffer we just acquired.  It's not safe to
            // release the old buffer, so instead we just drop the new frame.
            // As we are still under lock since acquireBuffer, it is safe to
            // release by slot.
            releaseBufferLocked(slot, mSlots[slot].mGraphicBuffer,
                    mEglDisplay, EGL_NO_SYNC_KHR);
            return err;
        }
    }

    GLC_LOGV("updateAndRelease: (slot=%d buf=%p) -> (slot=%d buf=%p)",
            mCurrentTexture, mCurrentTextureImage != NULL ?
                    mCurrentTextureImage->graphicBufferHandle() : 0,
            slot, mSlots[slot].mGraphicBuffer->handle);

    // Hang onto the pointer so that it isn't freed in the call to
    // releaseBufferLocked() if we're in shared buffer mode and both buffers are
    // the same.
    sp<EglImage> nextTextureImage = mEglSlots[slot].mEglImage;

    // release old buffer
    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        if (pendingRelease == nullptr) {
            status_t status = releaseBufferLocked(
                    mCurrentTexture, mCurrentTextureImage->graphicBuffer(),
                    mEglDisplay, mEglSlots[mCurrentTexture].mEglFence);
            if (status < NO_ERROR) {
                GLC_LOGE("updateAndRelease: failed to release buffer: %s (%d)",
                        strerror(-status), status);
                err = status;
                // keep going, with error raised [?]
            }
        } else {
            pendingRelease->currentTexture = mCurrentTexture;
            pendingRelease->graphicBuffer =
                    mCurrentTextureImage->graphicBuffer();
            pendingRelease->display = mEglDisplay;
            pendingRelease->fence = mEglSlots[mCurrentTexture].mEglFence;
            pendingRelease->isPending = true;
        }
    }

    // Update the GLConsumer state.
    mCurrentTexture = slot;
    mCurrentTextureImage = nextTextureImage;
    mCurrentCrop = item.mCrop;
    mCurrentTransform = item.mTransform;
    mCurrentScalingMode = item.mScalingMode;
    mCurrentTimestamp = item.mTimestamp;
    mCurrentFence = item.mFence;
    mCurrentFrameNumber = item.mFrameNumber;

    computeCurrentTransformMatrixLocked();

    return err;
}

status_t GLConsumer::bindTextureImageLocked() {
    if (mEglDisplay == EGL_NO_DISPLAY) {
        ALOGE("bindTextureImage: invalid display");
        return INVALID_OPERATION;
    }

    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR) {
        GLC_LOGW("bindTextureImage: clearing GL error: %#04x", error);
    }

    glBindTexture(mTexTarget, mTexName);
    if (mCurrentTexture == BufferQueue::INVALID_BUFFER_SLOT &&
            mCurrentTextureImage == NULL) {
        GLC_LOGE("bindTextureImage: no currently-bound texture");
        return NO_INIT;
    }

    status_t err = mCurrentTextureImage->createIfNeeded(mEglDisplay,
                                                        mCurrentCrop);
    if (err != NO_ERROR) {
        GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
                mEglDisplay, mCurrentTexture);
        return UNKNOWN_ERROR;
    }
    mCurrentTextureImage->bindToTextureTarget(mTexTarget);

    // In the rare case that the display is terminated and then initialized
    // again, we can't detect that the display changed (it didn't), but the
    // image is invalid. In this case, repeat the exact same steps while
    // forcing the creation of a new image.
    if ((error = glGetError()) != GL_NO_ERROR) {
        glBindTexture(mTexTarget, mTexName);
        status_t result = mCurrentTextureImage->createIfNeeded(mEglDisplay,
                                                               mCurrentCrop,
                                                               true);
        if (result != NO_ERROR) {
            GLC_LOGW("bindTextureImage: can't create image on display=%p slot=%d",
                    mEglDisplay, mCurrentTexture);
            return UNKNOWN_ERROR;
        }
        mCurrentTextureImage->bindToTextureTarget(mTexTarget);
        if ((error = glGetError()) != GL_NO_ERROR) {
            GLC_LOGE("bindTextureImage: error binding external image: %#04x", error);
            return UNKNOWN_ERROR;
        }
    }

    // Wait for the new buffer to be ready.
    return doGLFenceWaitLocked();
}

status_t GLConsumer::checkAndUpdateEglStateLocked(bool contextCheck) {
    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLContext ctx = eglGetCurrentContext();

    if (!contextCheck) {
        // if this is the first time we're called, mEglDisplay/mEglContext have
        // never been set, so don't error out (below).
        if (mEglDisplay == EGL_NO_DISPLAY) {
            mEglDisplay = dpy;
        }
        if (mEglContext == EGL_NO_CONTEXT) {
            mEglContext = ctx;
        }
    }

    if (mEglDisplay != dpy || dpy == EGL_NO_DISPLAY) {
        GLC_LOGE("checkAndUpdateEglState: invalid current EGLDisplay");
        return INVALID_OPERATION;
    }

    if (mEglContext != ctx || ctx == EGL_NO_CONTEXT) {
        GLC_LOGE("checkAndUpdateEglState: invalid current EGLContext");
        return INVALID_OPERATION;
    }

    mEglDisplay = dpy;
    mEglContext = ctx;
    return NO_ERROR;
}

void GLConsumer::setReleaseFence(const sp<Fence>& fence) {
    if (fence->isValid() &&
            mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        status_t err = addReleaseFence(mCurrentTexture,
                mCurrentTextureImage->graphicBuffer(), fence);
        if (err != OK) {
            GLC_LOGE("setReleaseFence: failed to add the fence: %s (%d)",
                    strerror(-err), err);
        }
    }
}

status_t GLConsumer::detachFromContext() {
    ATRACE_CALL();
    GLC_LOGV("detachFromContext");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("detachFromContext: abandoned GLConsumer");
        return NO_INIT;
    }

    if (!mAttached) {
        GLC_LOGE("detachFromContext: GLConsumer is not attached to a "
                "context");
        return INVALID_OPERATION;
    }

    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLContext ctx = eglGetCurrentContext();

    if (mEglDisplay != dpy && mEglDisplay != EGL_NO_DISPLAY) {
        GLC_LOGE("detachFromContext: invalid current EGLDisplay");
        return INVALID_OPERATION;
    }

    if (mEglContext != ctx && mEglContext != EGL_NO_CONTEXT) {
        GLC_LOGE("detachFromContext: invalid current EGLContext");
        return INVALID_OPERATION;
    }

    if (dpy != EGL_NO_DISPLAY && ctx != EGL_NO_CONTEXT) {
        status_t err = syncForReleaseLocked(dpy);
        if (err != OK) {
            return err;
        }

        glDeleteTextures(1, &mTexName);
    }

    mEglDisplay = EGL_NO_DISPLAY;
    mEglContext = EGL_NO_CONTEXT;
    mAttached = false;

    return OK;
}

status_t GLConsumer::attachToContext(uint32_t tex) {
    ATRACE_CALL();
    GLC_LOGV("attachToContext");
    Mutex::Autolock lock(mMutex);

    if (mAbandoned) {
        GLC_LOGE("attachToContext: abandoned GLConsumer");
        return NO_INIT;
    }

    if (mAttached) {
        GLC_LOGE("attachToContext: GLConsumer is already attached to a "
                "context");
        return INVALID_OPERATION;
    }

    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLContext ctx = eglGetCurrentContext();

    if (dpy == EGL_NO_DISPLAY) {
        GLC_LOGE("attachToContext: invalid current EGLDisplay");
        return INVALID_OPERATION;
    }

    if (ctx == EGL_NO_CONTEXT) {
        GLC_LOGE("attachToContext: invalid current EGLContext");
        return INVALID_OPERATION;
    }

    // We need to bind the texture regardless of whether there's a current
    // buffer.
    glBindTexture(mTexTarget, GLuint(tex));

    mEglDisplay = dpy;
    mEglContext = ctx;
    mTexName = tex;
    mAttached = true;

    if (mCurrentTextureImage != NULL) {
        // This may wait for a buffer a second time. This is likely required if
        // this is a different context, since otherwise the wait could be skipped
        // by bouncing through another context. For the same context the extra
        // wait is redundant.
        status_t err =  bindTextureImageLocked();
        if (err != NO_ERROR) {
            return err;
        }
    }

    return OK;
}


status_t GLConsumer::syncForReleaseLocked(EGLDisplay dpy) {
    GLC_LOGV("syncForReleaseLocked");

    if (mCurrentTexture != BufferQueue::INVALID_BUFFER_SLOT) {
        if (SyncFeatures::getInstance().useNativeFenceSync()) {
            EGLSyncKHR sync = eglCreateSyncKHR(dpy,
                    EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
            if (sync == EGL_NO_SYNC_KHR) {
                GLC_LOGE("syncForReleaseLocked: error creating EGL fence: %#x",
                        eglGetError());
                return UNKNOWN_ERROR;
            }
            glFlush();
            int fenceFd = eglDupNativeFenceFDANDROID(dpy, sync);
            eglDestroySyncKHR(dpy, sync);
            if (fenceFd == EGL_NO_NATIVE_FENCE_FD_ANDROID) {
                GLC_LOGE("syncForReleaseLocked: error dup'ing native fence "
                        "fd: %#x", eglGetError());
                return UNKNOWN_ERROR;
            }
            sp<Fence> fence(new Fence(fenceFd));
            status_t err = addReleaseFenceLocked(mCurrentTexture,
                    mCurrentTextureImage->graphicBuffer(), fence);
            if (err != OK) {
                GLC_LOGE("syncForReleaseLocked: error adding release fence: "
                        "%s (%d)", strerror(-err), err);
                return err;
            }
        } else if (mUseFenceSync && SyncFeatures::getInstance().useFenceSync()) {
            EGLSyncKHR fence = mEglSlots[mCurrentTexture].mEglFence;
            if (fence != EGL_NO_SYNC_KHR) {
                // There is already a fence for the current slot.  We need to
                // wait on that before replacing it with another fence to
                // ensure that all outstanding buffer accesses have completed
                // before the producer accesses it.
                EGLint result = eglClientWaitSyncKHR(dpy, fence, 0, 1000000000);
                if (result == EGL_FALSE) {
                    GLC_LOGE("syncForReleaseLocked: error waiting for previous "
                            "fence: %#x", eglGetError());
                    return UNKNOWN_ERROR;
                } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
                    GLC_LOGE("syncForReleaseLocked: timeout waiting for previous "
                            "fence");
                    return TIMED_OUT;
                }
                eglDestroySyncKHR(dpy, fence);
            }

            // Create a fence for the outstanding accesses in the current
            // OpenGL ES context.
            fence = eglCreateSyncKHR(dpy, EGL_SYNC_FENCE_KHR, NULL);
            if (fence == EGL_NO_SYNC_KHR) {
                GLC_LOGE("syncForReleaseLocked: error creating fence: %#x",
                        eglGetError());
                return UNKNOWN_ERROR;
            }
            glFlush();
            mEglSlots[mCurrentTexture].mEglFence = fence;
        }
    }

    return OK;
}

bool GLConsumer::isExternalFormat(PixelFormat format)
{
    switch (format) {
    // supported YUV formats
    case HAL_PIXEL_FORMAT_YV12:
    // Legacy/deprecated YUV formats
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        return true;
    }

    // Any OEM format needs to be considered
    if (format>=0x100 && format<=0x1FF)
        return true;

    return false;
}

uint32_t GLConsumer::getCurrentTextureTarget() const {
    return mTexTarget;
}

void GLConsumer::getTransformMatrix(float mtx[16]) {
    Mutex::Autolock lock(mMutex);
    memcpy(mtx, mCurrentTransformMatrix, sizeof(mCurrentTransformMatrix));
}

void GLConsumer::setFilteringEnabled(bool enabled) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setFilteringEnabled: GLConsumer is abandoned!");
        return;
    }
    bool needsRecompute = mFilteringEnabled != enabled;
    mFilteringEnabled = enabled;

    if (needsRecompute && mCurrentTextureImage==NULL) {
        GLC_LOGD("setFilteringEnabled called with mCurrentTextureImage == NULL");
    }

    if (needsRecompute && mCurrentTextureImage != NULL) {
        computeCurrentTransformMatrixLocked();
    }
}

void GLConsumer::computeCurrentTransformMatrixLocked() {
    GLC_LOGV("computeCurrentTransformMatrixLocked");
    sp<GraphicBuffer> buf = (mCurrentTextureImage == nullptr) ?
            nullptr : mCurrentTextureImage->graphicBuffer();
    if (buf == nullptr) {
        GLC_LOGD("computeCurrentTransformMatrixLocked: "
                "mCurrentTextureImage is NULL");
    }
    computeTransformMatrix(mCurrentTransformMatrix, buf,
        isEglImageCroppable(mCurrentCrop) ? Rect::EMPTY_RECT : mCurrentCrop,
        mCurrentTransform, mFilteringEnabled);
}

void GLConsumer::computeTransformMatrix(float outTransform[16],
        const sp<GraphicBuffer>& buf, const Rect& cropRect, uint32_t transform,
        bool filtering) {

    float xform[16];
    for (int i = 0; i < 16; i++) {
        xform[i] = mtxIdentity[i];
    }
    if (transform & NATIVE_WINDOW_TRANSFORM_FLIP_H) {
        float result[16];
        mtxMul(result, xform, mtxFlipH);
        for (int i = 0; i < 16; i++) {
            xform[i] = result[i];
        }
    }
    if (transform & NATIVE_WINDOW_TRANSFORM_FLIP_V) {
        float result[16];
        mtxMul(result, xform, mtxFlipV);
        for (int i = 0; i < 16; i++) {
            xform[i] = result[i];
        }
    }
    if (transform & NATIVE_WINDOW_TRANSFORM_ROT_90) {
        float result[16];
        mtxMul(result, xform, mtxRot90);
        for (int i = 0; i < 16; i++) {
            xform[i] = result[i];
        }
    }

    float mtxBeforeFlipV[16];
    if (!cropRect.isEmpty()) {
        float tx = 0.0f, ty = 0.0f, sx = 1.0f, sy = 1.0f;
        float bufferWidth = buf->getWidth();
        float bufferHeight = buf->getHeight();
        float shrinkAmount = 0.0f;
        if (filtering) {
            // In order to prevent bilinear sampling beyond the edge of the
            // crop rectangle we may need to shrink it by 2 texels in each
            // dimension.  Normally this would just need to take 1/2 a texel
            // off each end, but because the chroma channels of YUV420 images
            // are subsampled we may need to shrink the crop region by a whole
            // texel on each side.
            switch (buf->getPixelFormat()) {
                case PIXEL_FORMAT_RGBA_8888:
                case PIXEL_FORMAT_RGBX_8888:
                case PIXEL_FORMAT_RGB_888:
                case PIXEL_FORMAT_RGB_565:
                case PIXEL_FORMAT_BGRA_8888:
                    // We know there's no subsampling of any channels, so we
                    // only need to shrink by a half a pixel.
                    shrinkAmount = 0.5;
                    break;

                default:
                    // If we don't recognize the format, we must assume the
                    // worst case (that we care about), which is YUV420.
                    shrinkAmount = 1.0;
                    break;
            }
        }

        // Only shrink the dimensions that are not the size of the buffer.
        if (cropRect.width() < bufferWidth) {
            tx = (float(cropRect.left) + shrinkAmount) / bufferWidth;
            sx = (float(cropRect.width()) - (2.0f * shrinkAmount)) /
                    bufferWidth;
        }
        if (cropRect.height() < bufferHeight) {
            ty = (float(bufferHeight - cropRect.bottom) + shrinkAmount) /
                    bufferHeight;
            sy = (float(cropRect.height()) - (2.0f * shrinkAmount)) /
                    bufferHeight;
        }
        float crop[16] = {
            sx, 0, 0, 0,
            0, sy, 0, 0,
            0, 0, 1, 0,
            tx, ty, 0, 1,
        };

        mtxMul(mtxBeforeFlipV, crop, xform);
    } else {
        for (int i = 0; i < 16; i++) {
            mtxBeforeFlipV[i] = xform[i];
        }
    }

    // SurfaceFlinger expects the top of its window textures to be at a Y
    // coordinate of 0, so GLConsumer must behave the same way.  We don't
    // want to expose this to applications, however, so we must add an
    // additional vertical flip to the transform after all the other transforms.
    mtxMul(outTransform, mtxFlipV, mtxBeforeFlipV);
}

nsecs_t GLConsumer::getTimestamp() {
    GLC_LOGV("getTimestamp");
    Mutex::Autolock lock(mMutex);
    return mCurrentTimestamp;
}

uint64_t GLConsumer::getFrameNumber() {
    GLC_LOGV("getFrameNumber");
    Mutex::Autolock lock(mMutex);
    return mCurrentFrameNumber;
}

sp<GraphicBuffer> GLConsumer::getCurrentBuffer() const {
    Mutex::Autolock lock(mMutex);
    return (mCurrentTextureImage == NULL) ?
            NULL : mCurrentTextureImage->graphicBuffer();
}

Rect GLConsumer::getCurrentCrop() const {
    Mutex::Autolock lock(mMutex);

    Rect outCrop = mCurrentCrop;
    if (mCurrentScalingMode == NATIVE_WINDOW_SCALING_MODE_SCALE_CROP) {
        uint32_t newWidth = static_cast<uint32_t>(mCurrentCrop.width());
        uint32_t newHeight = static_cast<uint32_t>(mCurrentCrop.height());

        if (newWidth * mDefaultHeight > newHeight * mDefaultWidth) {
            newWidth = newHeight * mDefaultWidth / mDefaultHeight;
            GLC_LOGV("too wide: newWidth = %d", newWidth);
        } else if (newWidth * mDefaultHeight < newHeight * mDefaultWidth) {
            newHeight = newWidth * mDefaultHeight / mDefaultWidth;
            GLC_LOGV("too tall: newHeight = %d", newHeight);
        }

        uint32_t currentWidth = static_cast<uint32_t>(mCurrentCrop.width());
        uint32_t currentHeight = static_cast<uint32_t>(mCurrentCrop.height());

        // The crop is too wide
        if (newWidth < currentWidth) {
            uint32_t dw = currentWidth - newWidth;
            auto halfdw = dw / 2;
            outCrop.left += halfdw;
            // Not halfdw because it would subtract 1 too few when dw is odd
            outCrop.right -= (dw - halfdw);
        // The crop is too tall
        } else if (newHeight < currentHeight) {
            uint32_t dh = currentHeight - newHeight;
            auto halfdh = dh / 2;
            outCrop.top += halfdh;
            // Not halfdh because it would subtract 1 too few when dh is odd
            outCrop.bottom -= (dh - halfdh);
        }

        GLC_LOGV("getCurrentCrop final crop [%d,%d,%d,%d]",
            outCrop.left, outCrop.top,
            outCrop.right,outCrop.bottom);
    }

    return outCrop;
}

uint32_t GLConsumer::getCurrentTransform() const {
    Mutex::Autolock lock(mMutex);
    return mCurrentTransform;
}

uint32_t GLConsumer::getCurrentScalingMode() const {
    Mutex::Autolock lock(mMutex);
    return mCurrentScalingMode;
}

sp<Fence> GLConsumer::getCurrentFence() const {
    Mutex::Autolock lock(mMutex);
    return mCurrentFence;
}

status_t GLConsumer::doGLFenceWait() const {
    Mutex::Autolock lock(mMutex);
    return doGLFenceWaitLocked();
}

status_t GLConsumer::doGLFenceWaitLocked() const {

    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLContext ctx = eglGetCurrentContext();

    if (mEglDisplay != dpy || mEglDisplay == EGL_NO_DISPLAY) {
        GLC_LOGE("doGLFenceWait: invalid current EGLDisplay");
        return INVALID_OPERATION;
    }

    if (mEglContext != ctx || mEglContext == EGL_NO_CONTEXT) {
        GLC_LOGE("doGLFenceWait: invalid current EGLContext");
        return INVALID_OPERATION;
    }

    if (mCurrentFence->isValid()) {
        if (SyncFeatures::getInstance().useWaitSync()) {
            // Create an EGLSyncKHR from the current fence.
            int fenceFd = mCurrentFence->dup();
            if (fenceFd == -1) {
                GLC_LOGE("doGLFenceWait: error dup'ing fence fd: %d", errno);
                return -errno;
            }
            EGLint attribs[] = {
                EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fenceFd,
                EGL_NONE
            };
            EGLSyncKHR sync = eglCreateSyncKHR(dpy,
                    EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
            if (sync == EGL_NO_SYNC_KHR) {
                close(fenceFd);
                GLC_LOGE("doGLFenceWait: error creating EGL fence: %#x",
                        eglGetError());
                return UNKNOWN_ERROR;
            }

            // XXX: The spec draft is inconsistent as to whether this should
            // return an EGLint or void.  Ignore the return value for now, as
            // it's not strictly needed.
            eglWaitSyncKHR(dpy, sync, 0);
            EGLint eglErr = eglGetError();
            eglDestroySyncKHR(dpy, sync);
            if (eglErr != EGL_SUCCESS) {
                GLC_LOGE("doGLFenceWait: error waiting for EGL fence: %#x",
                        eglErr);
                return UNKNOWN_ERROR;
            }
        } else {
            status_t err = mCurrentFence->waitForever(
                    "GLConsumer::doGLFenceWaitLocked");
            if (err != NO_ERROR) {
                GLC_LOGE("doGLFenceWait: error waiting for fence: %d", err);
                return err;
            }
        }
    }

    return NO_ERROR;
}

void GLConsumer::freeBufferLocked(int slotIndex) {
    GLC_LOGV("freeBufferLocked: slotIndex=%d", slotIndex);
    if (slotIndex == mCurrentTexture) {
        mCurrentTexture = BufferQueue::INVALID_BUFFER_SLOT;
    }
    mEglSlots[slotIndex].mEglImage.clear();
    ConsumerBase::freeBufferLocked(slotIndex);
}

void GLConsumer::abandonLocked() {
    GLC_LOGV("abandonLocked");
    mCurrentTextureImage.clear();
    ConsumerBase::abandonLocked();
}

void GLConsumer::setName(const String8& name) {
    Mutex::Autolock _l(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setName: GLConsumer is abandoned!");
        return;
    }
    mName = name;
    mConsumer->setConsumerName(name);
}

status_t GLConsumer::setDefaultBufferFormat(PixelFormat defaultFormat) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setDefaultBufferFormat: GLConsumer is abandoned!");
        return NO_INIT;
    }
    return mConsumer->setDefaultBufferFormat(defaultFormat);
}

status_t GLConsumer::setDefaultBufferDataSpace(
        android_dataspace defaultDataSpace) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setDefaultBufferDataSpace: GLConsumer is abandoned!");
        return NO_INIT;
    }
    return mConsumer->setDefaultBufferDataSpace(defaultDataSpace);
}

status_t GLConsumer::setConsumerUsageBits(uint32_t usage) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setConsumerUsageBits: GLConsumer is abandoned!");
        return NO_INIT;
    }
    usage |= DEFAULT_USAGE_FLAGS;
    return mConsumer->setConsumerUsageBits(usage);
}

status_t GLConsumer::setTransformHint(uint32_t hint) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setTransformHint: GLConsumer is abandoned!");
        return NO_INIT;
    }
    return mConsumer->setTransformHint(hint);
}

status_t GLConsumer::setMaxAcquiredBufferCount(int maxAcquiredBuffers) {
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        GLC_LOGE("setMaxAcquiredBufferCount: GLConsumer is abandoned!");
        return NO_INIT;
    }
    return mConsumer->setMaxAcquiredBufferCount(maxAcquiredBuffers);
}

void GLConsumer::dumpLocked(String8& result, const char* prefix) const
{
    result.appendFormat(
       "%smTexName=%d mCurrentTexture=%d\n"
       "%smCurrentCrop=[%d,%d,%d,%d] mCurrentTransform=%#x\n",
       prefix, mTexName, mCurrentTexture, prefix, mCurrentCrop.left,
       mCurrentCrop.top, mCurrentCrop.right, mCurrentCrop.bottom,
       mCurrentTransform);

    ConsumerBase::dumpLocked(result, prefix);
}

static void mtxMul(float out[16], const float a[16], const float b[16]) {
    out[0] = a[0]*b[0] + a[4]*b[1] + a[8]*b[2] + a[12]*b[3];
    out[1] = a[1]*b[0] + a[5]*b[1] + a[9]*b[2] + a[13]*b[3];
    out[2] = a[2]*b[0] + a[6]*b[1] + a[10]*b[2] + a[14]*b[3];
    out[3] = a[3]*b[0] + a[7]*b[1] + a[11]*b[2] + a[15]*b[3];

    out[4] = a[0]*b[4] + a[4]*b[5] + a[8]*b[6] + a[12]*b[7];
    out[5] = a[1]*b[4] + a[5]*b[5] + a[9]*b[6] + a[13]*b[7];
    out[6] = a[2]*b[4] + a[6]*b[5] + a[10]*b[6] + a[14]*b[7];
    out[7] = a[3]*b[4] + a[7]*b[5] + a[11]*b[6] + a[15]*b[7];

    out[8] = a[0]*b[8] + a[4]*b[9] + a[8]*b[10] + a[12]*b[11];
    out[9] = a[1]*b[8] + a[5]*b[9] + a[9]*b[10] + a[13]*b[11];
    out[10] = a[2]*b[8] + a[6]*b[9] + a[10]*b[10] + a[14]*b[11];
    out[11] = a[3]*b[8] + a[7]*b[9] + a[11]*b[10] + a[15]*b[11];

    out[12] = a[0]*b[12] + a[4]*b[13] + a[8]*b[14] + a[12]*b[15];
    out[13] = a[1]*b[12] + a[5]*b[13] + a[9]*b[14] + a[13]*b[15];
    out[14] = a[2]*b[12] + a[6]*b[13] + a[10]*b[14] + a[14]*b[15];
    out[15] = a[3]*b[12] + a[7]*b[13] + a[11]*b[14] + a[15]*b[15];
}

GLConsumer::EglImage::EglImage(sp<GraphicBuffer> graphicBuffer) :
    mGraphicBuffer(graphicBuffer),
    mEglImage(EGL_NO_IMAGE_KHR),
    mEglDisplay(EGL_NO_DISPLAY),
    mCropRect(Rect::EMPTY_RECT) {
}

GLConsumer::EglImage::~EglImage() {
    if (mEglImage != EGL_NO_IMAGE_KHR) {
        if (!eglDestroyImageKHR(mEglDisplay, mEglImage)) {
           ALOGE("~EglImage: eglDestroyImageKHR failed");
        }
        eglTerminate(mEglDisplay);
    }
}

status_t GLConsumer::EglImage::createIfNeeded(EGLDisplay eglDisplay,
                                              const Rect& cropRect,
                                              bool forceCreation) {
    // If there's an image and it's no longer valid, destroy it.
    bool haveImage = mEglImage != EGL_NO_IMAGE_KHR;
    bool displayInvalid = mEglDisplay != eglDisplay;
    bool cropInvalid = hasEglAndroidImageCrop() && mCropRect != cropRect;
    if (haveImage && (displayInvalid || cropInvalid || forceCreation)) {
        if (!eglDestroyImageKHR(mEglDisplay, mEglImage)) {
           ALOGE("createIfNeeded: eglDestroyImageKHR failed");
        }
        eglTerminate(mEglDisplay);
        mEglImage = EGL_NO_IMAGE_KHR;
        mEglDisplay = EGL_NO_DISPLAY;
    }

    // If there's no image, create one.
    if (mEglImage == EGL_NO_IMAGE_KHR) {
        mEglDisplay = eglDisplay;
        mCropRect = cropRect;
        mEglImage = createImage(mEglDisplay, mGraphicBuffer, mCropRect);
    }

    // Fail if we can't create a valid image.
    if (mEglImage == EGL_NO_IMAGE_KHR) {
        mEglDisplay = EGL_NO_DISPLAY;
        mCropRect.makeInvalid();
        const sp<GraphicBuffer>& buffer = mGraphicBuffer;
        ALOGE("Failed to create image. size=%ux%u st=%u usage=0x%x fmt=%d",
            buffer->getWidth(), buffer->getHeight(), buffer->getStride(),
            buffer->getUsage(), buffer->getPixelFormat());
        return UNKNOWN_ERROR;
    }

    return OK;
}

void GLConsumer::EglImage::bindToTextureTarget(uint32_t texTarget) {
    glEGLImageTargetTexture2DOES(texTarget,
            static_cast<GLeglImageOES>(mEglImage));
}

EGLImageKHR GLConsumer::EglImage::createImage(EGLDisplay dpy,
        const sp<GraphicBuffer>& graphicBuffer, const Rect& crop) {
    EGLClientBuffer cbuf =
            static_cast<EGLClientBuffer>(graphicBuffer->getNativeBuffer());
    const bool createProtectedImage =
            (graphicBuffer->getUsage() & GRALLOC_USAGE_PROTECTED) &&
            hasEglProtectedContent();
    EGLint attrs[] = {
        EGL_IMAGE_PRESERVED_KHR,        EGL_TRUE,
        EGL_IMAGE_CROP_LEFT_ANDROID,    crop.left,
        EGL_IMAGE_CROP_TOP_ANDROID,     crop.top,
        EGL_IMAGE_CROP_RIGHT_ANDROID,   crop.right,
        EGL_IMAGE_CROP_BOTTOM_ANDROID,  crop.bottom,
        createProtectedImage ? EGL_PROTECTED_CONTENT_EXT : EGL_NONE,
        createProtectedImage ? EGL_TRUE : EGL_NONE,
        EGL_NONE,
    };
    if (!crop.isValid()) {
        // No crop rect to set, so terminate the attrib array before the crop.
        attrs[2] = EGL_NONE;
    } else if (!isEglImageCroppable(crop)) {
        // The crop rect is not at the origin, so we can't set the crop on the
        // EGLImage because that's not allowed by the EGL_ANDROID_image_crop
        // extension.  In the future we can add a layered extension that
        // removes this restriction if there is hardware that can support it.
        attrs[2] = EGL_NONE;
    }
    eglInitialize(dpy, 0, 0);
    EGLImageKHR image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID, cbuf, attrs);
    if (image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        ALOGE("error creating EGLImage: %#x", error);
        eglTerminate(dpy);
    }
    return image;
}

}; // namespace android
