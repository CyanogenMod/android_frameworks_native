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

#ifndef ANDROID_EX_LAYER_H
#define ANDROID_EX_LAYER_H

#include <stdint.h>
#include <sys/types.h>
#include <cutils/properties.h>

#include <Layer.h>
#include "ExSurfaceFlinger.h"

namespace android {

class ExSurfaceFlinger;

class ExLayer : public Layer
{
public:
    ExLayer(SurfaceFlinger* flinger, const sp<Client>& client,
            const String8& name, uint32_t w, uint32_t h, uint32_t flags);
    virtual ~ExLayer();

    virtual bool isExtOnly() const;
    virtual bool isIntOnly() const;
    virtual bool isSecureDisplay() const;
    virtual bool isYuvLayer() const;
    virtual void setPosition(const sp<const DisplayDevice>& hw,
                             HWComposer::HWCLayerInterface& layer, const State& state);
    virtual void setAcquiredFenceIfBlit(int &fenceFd,
                             HWComposer::HWCLayerInterface& layer);
    virtual bool canAllowGPUForProtected() const;

protected:
    bool mDebugLogs;
    bool isDebug() { return mDebugLogs; }
    bool mIsGPUAllowedForProtected;
};

}; // namespace android

#endif // ANDROID_EX_LAYER_H
