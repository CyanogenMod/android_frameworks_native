/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>

#include "Layer.h"
#include "SurfaceTextureLayer.h"

namespace android {
// ---------------------------------------------------------------------------


SurfaceTextureLayer::SurfaceTextureLayer()
#ifdef QCOM_HARDWARE
    : BufferQueue(true, 3)
#else
    : BufferQueue(true)
#endif
{
}

SurfaceTextureLayer::~SurfaceTextureLayer() {
}

status_t SurfaceTextureLayer::connect(int api, QueueBufferOutput* output) {
    status_t err = BufferQueue::connect(api, output);
    if (err == NO_ERROR) {
        switch(api) {
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                // Camera preview and videos are rate-limited on the producer
                // side.  If enabled for this build, we use async mode to always
                // show the most recent frame at the cost of requiring an
                // additional buffer.
#ifndef NEVER_DEFAULT_TO_ASYNC_MODE
                err = setSynchronousMode(false);
                break;
#endif
                // fall through to set synchronous mode when not defaulting to
                // async mode.
            default:
                err = setSynchronousMode(true);
                break;
        }
        if (err != NO_ERROR) {
            disconnect(api);
        }
    }
    return err;
}

// ---------------------------------------------------------------------------
}; // namespace android
