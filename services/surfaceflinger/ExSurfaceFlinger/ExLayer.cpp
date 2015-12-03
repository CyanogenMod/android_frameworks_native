/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>
#ifdef QTI_BSP
#include <gralloc_priv.h>
#include <hardware/display_defs.h>
#endif

#include "ExLayer.h"

namespace android {

/* Calculates the aspect ratio for external display based on the video w/h */
static Rect getAspectRatio(const sp<const DisplayDevice>& hw,
                            const int& srcWidth, const int& srcHeight) {
    Rect outRect;
    int fbWidth  = hw->getWidth();
    int fbHeight = hw->getHeight();
    int x , y = 0;
    int w = fbWidth, h = fbHeight;
    if (srcWidth * fbHeight > fbWidth * srcHeight) {
        h = fbWidth * srcHeight / srcWidth;
        w = fbWidth;
    } else if (srcWidth * fbHeight < fbWidth * srcHeight) {
        w = fbHeight * srcWidth / srcHeight;
        h = fbHeight;
    }
    x = (fbWidth - w) / 2;
    y = (fbHeight - h) / 2;
    outRect.left = x;
    outRect.top = y;
    outRect.right = x + w;
    outRect.bottom = y + h;

    return outRect;
}

ExLayer::ExLayer(SurfaceFlinger* flinger, const sp<Client>& client,
                 const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    : Layer(flinger, client, name, w, h, flags) {

    char property[PROPERTY_VALUE_MAX] = {0};

    mDebugLogs = false;
    mIsGPUAllowedForProtected = false;
    if((property_get("persist.debug.qdframework.logs", property, NULL) > 0) &&
       (!strncmp(property, "1", PROPERTY_VALUE_MAX ) ||
        (!strncasecmp(property,"true", PROPERTY_VALUE_MAX )))) {
        mDebugLogs = true;
    }

    ALOGD_IF(isDebug(),"Creating custom Layer %s",__FUNCTION__);

    if ((property_get("persist.gralloc.cp.level3", property, NULL) > 0) &&
           (atoi(property) == 1)) {
        mIsGPUAllowedForProtected = true;
    }
}

ExLayer::~ExLayer() {
}

bool ExLayer::isExtOnly() const {
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if (activeBuffer != 0) {
#ifdef QTI_BSP
        ANativeWindowBuffer* buffer = activeBuffer->getNativeBuffer();
        if(buffer) {
            private_handle_t* hnd = static_cast<private_handle_t*>
                (const_cast<native_handle_t*>(buffer->handle));
            /* return true if layer is EXT_ONLY */
            return (hnd && (hnd->flags & private_handle_t::PRIV_FLAGS_EXTERNAL_ONLY));
        }
#endif
    }
    return false;
}

bool ExLayer::isIntOnly() const {
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if (activeBuffer != 0) {
#ifdef QTI_BSP
        ANativeWindowBuffer* buffer = activeBuffer->getNativeBuffer();
        if(buffer) {
            private_handle_t* hnd = static_cast<private_handle_t*>
                  (const_cast<native_handle_t*>(buffer->handle));
            /* return true if layer is INT_ONLY */
            return (hnd && (hnd->flags & private_handle_t::PRIV_FLAGS_INTERNAL_ONLY));
        }
#endif
    }
    return false;
}

bool ExLayer::isSecureDisplay() const {
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if (activeBuffer != 0) {
#ifdef QTI_BSP
        ANativeWindowBuffer* buffer = activeBuffer->getNativeBuffer();
        if(buffer) {
            private_handle_t* hnd = static_cast<private_handle_t*>
                (const_cast<native_handle_t*>(buffer->handle));
            /* return true if layer is SECURE_DISPLAY */
            return (hnd && (hnd->flags & private_handle_t::PRIV_FLAGS_SECURE_DISPLAY));
        }
#endif
    }
    return false;
}

bool ExLayer::isYuvLayer() const {
    const sp<GraphicBuffer>& activeBuffer(mActiveBuffer);
    if(activeBuffer != 0) {
#ifdef QTI_BSP
        ANativeWindowBuffer* buffer = activeBuffer->getNativeBuffer();
        if(buffer) {
            private_handle_t* hnd = static_cast<private_handle_t*>
                (const_cast<native_handle_t*>(buffer->handle));
            /* return true if layer is YUV */
            return (hnd && (hnd->bufferType == BUFFER_TYPE_VIDEO));
        }
#endif
    }
    return false;
}

void ExLayer::setPosition(const sp<const DisplayDevice>& hw,
                          HWComposer::HWCLayerInterface& layer, const State& state) {
    /* Set dest_rect to display width and height, if external_only flag
     * for the layer is enabled or if its yuvLayer in extended mode.
     */
    uint32_t w = hw->getWidth();
    uint32_t h = hw->getHeight();
    bool extendedMode = ExSurfaceFlinger::isExtendedMode();
    if(isExtOnly()) {
        /* Position: fullscreen for ext_only */
        Rect r(0, 0, w, h);
        layer.setFrame(r);
    } else if(hw->getDisplayType() > 0 && (extendedMode && isYuvLayer())) {
        /* Need to position the video full screen on external with aspect ratio */
        Rect r = getAspectRatio(hw, state.active.w, state.active.h);
        layer.setFrame(r);
    }
    return;
}

void ExLayer::setAcquiredFenceIfBlit(int &fenceFd,
                                     HWComposer::HWCLayerInterface& layer) {
#ifdef QTI_BSP
    if (layer.getCompositionType() == HWC_BLIT) {
        sp<Fence> fence = mSurfaceFlingerConsumer->getCurrentFence();
        if (fence->isValid()) {
            fenceFd = fence->dup();
            if (fenceFd == -1) {
                ALOGW("%s: failed to dup layer fence, skipping sync: %d",
                      __FUNCTION__,errno);
            }
        }
    }
#else
    ALOGD_IF(isDebug(),"Not a BLIT Layer, compType = %d fencefd = %d",
            layer.getCompositionType(), fenceFd);
#endif
}

bool ExLayer::canAllowGPUForProtected() const {
    if(isProtected()) {
        return mIsGPUAllowedForProtected;
    } else {
        return false;
    }
}

}; // namespace android
