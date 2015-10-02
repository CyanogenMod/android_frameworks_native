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

#ifndef ANDROID_DISPLAY_UTILS_H
#define ANDROID_DISPLAY_UTILS_H

#include <stdint.h>
#include <sys/types.h>

#include "Layer.h"
#include "SurfaceFlinger.h"
#include "DisplayHardware/HWComposer.h"
#include "DisplayHardware/VirtualDisplaySurface.h"

// ---------------------------------------------------------------------------

namespace android {

class IGraphicBufferProducer;
class IGraphicBufferConsumer;
class DisplaySurface;

/* Factory Classes */

class DisplayUtils {
  public:
    static DisplayUtils* getInstance() ANDROID_API;
    SurfaceFlinger* getSFInstance() ANDROID_API;
    Layer* getLayerInstance(SurfaceFlinger*, const sp<Client>&,
                            const String8&, uint32_t,
                            uint32_t, uint32_t);
    HWComposer* getHWCInstance(const sp<SurfaceFlinger>& flinger,
                            HWComposer::EventHandler& handler);
    void initVDSInstance(HWComposer* hwc, int32_t hwcDisplayId,
            sp<IGraphicBufferProducer> currentStateSurface, sp<DisplaySurface> &dispSurface,
            sp<IGraphicBufferProducer> &producer, sp<IGraphicBufferProducer> bqProducer,
            sp<IGraphicBufferConsumer> bqConsumer, String8 currentStateDisplayName,
            bool currentStateIsSecure, int currentStateType);
    bool canAllocateHwcDisplayIdForVDS(int usage);
    DisplayUtils();
  private:
    static DisplayUtils* sDisplayUtils;
    static bool sUseExtendedImpls;

    bool createV4L2BasedVirtualDisplay(HWComposer* hwc, int32_t &hwcDisplayId,
                   sp<DisplaySurface> &dispSurface, sp<IGraphicBufferProducer> &producer,
                   sp<IGraphicBufferProducer> currentStateSurface,
                   sp<IGraphicBufferProducer> bqProducer,
                   sp<IGraphicBufferConsumer> bqConsumer, int currentStateType);
};

}; // namespace android

#endif // ANDROID_DISPLAY_UTILS_H
