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

#include "ExVirtualDisplaySurface.h"
#ifdef QTI_BSP
#include <gralloc_priv.h>
#endif

namespace android {

#define VDS_LOGE(msg, ...) ALOGE("[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)
#define VDS_LOGW_IF(cond, msg, ...) ALOGW_IF(cond, "[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)
#define VDS_LOGV(msg, ...) ALOGV("[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)

ExVirtualDisplaySurface::ExVirtualDisplaySurface(HWComposer& hwc, int32_t dispId,
        const sp<IGraphicBufferProducer>& sink,
        const sp<IGraphicBufferProducer>& bqProducer,
        const sp<IGraphicBufferConsumer>& bqConsumer,
        const String8& name,
        bool secure)
:   VirtualDisplaySurface(hwc, dispId, sink, bqProducer, bqConsumer, name),
   mSecure(secure) {
   sink->query(NATIVE_WINDOW_CONSUMER_USAGE_BITS, &mSinkUsage);
   mSinkUsage |= GRALLOC_USAGE_HW_COMPOSER;
   setOutputUsage(mSinkUsage);
}

status_t ExVirtualDisplaySurface::beginFrame(bool mustRecompose) {
    if (mDisplayId < 0)
        return NO_ERROR;

    mMustRecompose = mustRecompose;
    /* For WFD use cases we must always set the recompose flag in order
     * to support pause/resume functionality
     */
    if (mOutputUsage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
        mMustRecompose = true;
    }

    VDS_LOGW_IF(mDbgState != DBG_STATE_IDLE,
            "Unexpected beginFrame() in %s state", dbgStateStr());
    mDbgState = DBG_STATE_BEGUN;

    return refreshOutputBuffer();

}

/* Helper to update the output usage when the display is secure */
void ExVirtualDisplaySurface::setOutputUsage(uint32_t /*flag*/) {
    mOutputUsage = mSinkUsage;
    if (mSecure && (mOutputUsage & GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
        /* TODO: Currently, the framework can only say whether the display
         * and its subsequent session are secure or not. However, there is
         * no mechanism to distinguish the different levels of security.
         * The current solution assumes WV L3 protection.
         */
        mOutputUsage |= GRALLOC_USAGE_PROTECTED;
#ifdef QTI_BSP
        mOutputUsage |= GRALLOC_USAGE_PRIVATE_MM_HEAP |
                        GRALLOC_USAGE_PRIVATE_UNCACHED;
#endif
    }
}

}; // namespace android
