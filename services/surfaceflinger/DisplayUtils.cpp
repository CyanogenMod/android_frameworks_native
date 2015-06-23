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

#include <dlfcn.h>

#include "DisplayUtils.h"
#include <ExSurfaceFlinger/ExSurfaceFlinger.h>
#include <ExSurfaceFlinger/ExLayer.h>
#include <ExSurfaceFlinger/ExHWComposer.h>
#include <ExSurfaceFlinger/ExVirtualDisplaySurface.h>
#if QTI_BSP
#include <gralloc_priv.h>
#endif

namespace android {

DisplayUtils* DisplayUtils::sDisplayUtils = NULL;
bool DisplayUtils::sUseExtendedImpls = false;

DisplayUtils::DisplayUtils() {
#ifdef QTI_BSP
    sUseExtendedImpls = true;
#endif
}

DisplayUtils* DisplayUtils::getInstance() {
    if(sDisplayUtils == NULL) {
        sDisplayUtils = new DisplayUtils();
    }
    return sDisplayUtils;
}

SurfaceFlinger* DisplayUtils::getSFInstance() {
    if(sUseExtendedImpls) {
        return new ExSurfaceFlinger();
    } else {
        return new SurfaceFlinger();
    }
}

Layer* DisplayUtils::getLayerInstance(SurfaceFlinger* flinger,
                            const sp<Client>& client, const String8& name,
                            uint32_t w, uint32_t h, uint32_t flags) {
    if(sUseExtendedImpls) {
        return new ExLayer(flinger, client, name, w, h, flags);
    } else {
        return new Layer(flinger, client, name, w, h, flags);
    }
}

HWComposer* DisplayUtils::getHWCInstance(
                        const sp<SurfaceFlinger>& flinger,
                        HWComposer::EventHandler& handler) {
    if(sUseExtendedImpls) {
        return new ExHWComposer(flinger, handler);
    } else {
        return new HWComposer(flinger,handler);
    }
}

VirtualDisplaySurface* DisplayUtils::getVDSInstance(HWComposer* hwc, int32_t hwcDisplayId,
        sp<IGraphicBufferProducer> currentStateSurface, sp<IGraphicBufferProducer> bqProducer,
        sp<IGraphicBufferConsumer> bqConsumer, String8 currentStateDisplayName,
        bool currentStateIsSecure)
{
    if(sUseExtendedImpls) {
        return new ExVirtualDisplaySurface(*hwc, hwcDisplayId, currentStateSurface, bqProducer,
                bqConsumer, currentStateDisplayName, currentStateIsSecure);
    } else {
        return new VirtualDisplaySurface(*hwc, hwcDisplayId, currentStateSurface, bqProducer,
                bqConsumer, currentStateDisplayName);
    }
}

}; // namespace android

