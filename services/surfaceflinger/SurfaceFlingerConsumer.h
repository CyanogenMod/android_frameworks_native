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

#include <gui/GLConsumer.h>

namespace android {
// ----------------------------------------------------------------------------

/*
 * This is a thin wrapper around GLConsumer.
 */
class SurfaceFlingerConsumer : public GLConsumer {
public:
    SurfaceFlingerConsumer(const sp<BufferQueue>& bq, uint32_t tex)
        : GLConsumer(bq, tex, GLConsumer::TEXTURE_EXTERNAL, false)
    {}

    class BufferRejecter {
        friend class SurfaceFlingerConsumer;
        virtual bool reject(const sp<GraphicBuffer>& buf,
                const IGraphicBufferConsumer::BufferItem& item) = 0;

    protected:
        virtual ~BufferRejecter() { }
    };

    // This version of updateTexImage() takes a functor that may be used to
    // reject the newly acquired buffer.  Unlike the GLConsumer version,
    // this does not guarantee that the buffer has been bound to the GL
    // texture.
    status_t updateTexImage(BufferRejecter* rejecter);

    // See GLConsumer::bindTextureImageLocked().
    status_t bindTextureImage();

private:
    nsecs_t computeExpectedPresent();
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SURFACEFLINGERCONSUMER_H
