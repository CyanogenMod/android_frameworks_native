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

#ifndef ANDROID_SURFACEFLINGERCONSUMER_H
#define ANDROID_SURFACEFLINGERCONSUMER_H

#include "DispSync.h"
#include <gui/GLConsumer.h>

namespace android {
// ----------------------------------------------------------------------------

class Layer;

/*
 * This is a thin wrapper around GLConsumer.
 */
class SurfaceFlingerConsumer : public GLConsumer {
public:
    static const status_t BUFFER_REJECTED = UNKNOWN_ERROR + 8;

    struct ContentsChangedListener: public FrameAvailableListener {
        virtual void onSidebandStreamChanged() = 0;
    };

    SurfaceFlingerConsumer(const sp<IGraphicBufferConsumer>& consumer,
            uint32_t tex, const Layer* layer)
        : GLConsumer(consumer, tex, GLConsumer::TEXTURE_EXTERNAL, false, false),
          mTransformToDisplayInverse(false), mSurfaceDamage(),
          mPrevReleaseFence(Fence::NO_FENCE), mLayer(layer)
    {}

    class BufferRejecter {
        friend class SurfaceFlingerConsumer;
        virtual bool reject(const sp<GraphicBuffer>& buf,
                const BufferItem& item) = 0;

    protected:
        virtual ~BufferRejecter() { }
    };

    virtual status_t acquireBufferLocked(BufferItem *item, nsecs_t presentWhen,
            uint64_t maxFrameNumber = 0) override;

    // This version of updateTexImage() takes a functor that may be used to
    // reject the newly acquired buffer.  Unlike the GLConsumer version,
    // this does not guarantee that the buffer has been bound to the GL
    // texture.
    status_t updateTexImage(BufferRejecter* rejecter, const DispSync& dispSync,
            bool* autoRefresh, bool* queuedBuffer,
            uint64_t maxFrameNumber = 0);

    // See GLConsumer::bindTextureImageLocked().
    status_t bindTextureImage();

    bool getTransformToDisplayInverse() const;

    // must be called from SF main thread
    const Region& getSurfaceDamage() const;

    // Sets the contents changed listener. This should be used instead of
    // ConsumerBase::setFrameAvailableListener().
    void setContentsChangedListener(const wp<ContentsChangedListener>& listener);

    sp<NativeHandle> getSidebandStream() const;

    nsecs_t computeExpectedPresent(const DispSync& dispSync);

    virtual void setReleaseFence(const sp<Fence>& fence) override;
    sp<Fence> getPrevReleaseFence() const;
#ifdef USE_HWC2
    void releasePendingBuffer();
#endif

    virtual bool getFrameTimestamps(uint64_t frameNumber,
            FrameTimestamps* outTimestamps) const override;

private:
    virtual void onSidebandStreamChanged();

    wp<ContentsChangedListener> mContentsChangedListener;

    // Indicates this buffer must be transformed by the inverse transform of the screen
    // it is displayed onto. This is applied after GLConsumer::mCurrentTransform.
    // This must be set/read from SurfaceFlinger's main thread.
    bool mTransformToDisplayInverse;

    // The portion of this surface that has changed since the previous frame
    Region mSurfaceDamage;

#ifdef USE_HWC2
    // A release that is pending on the receipt of a new release fence from
    // presentDisplay
    PendingRelease mPendingRelease;
#endif

    // The release fence of the already displayed buffer (previous frame).
    sp<Fence> mPrevReleaseFence;

    // The layer for this SurfaceFlingerConsumer
    wp<const Layer> mLayer;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SURFACEFLINGERCONSUMER_H
