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

#include <gui/Surface.h>
#include <ui/GraphicBuffer.h>

#include "RenderEngine/RenderEngine.h"
#include "DisplayHardware/FramebufferSurface.h"
#include "DisplayUtils.h"
#include <ExSurfaceFlinger/ExSurfaceFlinger.h>
#include <ExSurfaceFlinger/ExLayer.h>
#include <ExSurfaceFlinger/ExHWComposer.h>
#include <ExSurfaceFlinger/ExVirtualDisplaySurface.h>
#include <dlfcn.h>
#if QTI_BSP
#include <gralloc_priv.h>
#endif

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

void DisplayUtils::initVDSInstance(HWComposer* hwc, int32_t hwcDisplayId,
        sp<IGraphicBufferProducer> currentStateSurface, sp<DisplaySurface> &dispSurface,
        sp<IGraphicBufferProducer> &producer, sp<IGraphicBufferProducer> bqProducer,
        sp<IGraphicBufferConsumer> bqConsumer, String8 currentStateDisplayName,
        bool currentStateIsSecure, int currentStateType)
{
    if(sUseExtendedImpls) {
        if(hwc->isVDSEnabled()) {
            VirtualDisplaySurface* vds = new ExVirtualDisplaySurface(*hwc, hwcDisplayId,
                    currentStateSurface, bqProducer, bqConsumer, currentStateDisplayName,
                    currentStateIsSecure);
            dispSurface = vds;
            producer = vds;
        } else if(!createV4L2BasedVirtualDisplay(hwc, hwcDisplayId, dispSurface, producer,
                          currentStateSurface, bqProducer, bqConsumer, currentStateType)) {
            VirtualDisplaySurface* vds = new VirtualDisplaySurface(*hwc, hwcDisplayId,
                    currentStateSurface, bqProducer, bqConsumer, currentStateDisplayName);
            dispSurface = vds;
            producer = vds;
        }
    } else {
        VirtualDisplaySurface* vds = new VirtualDisplaySurface(*hwc, hwcDisplayId,
                currentStateSurface, bqProducer, bqConsumer, currentStateDisplayName);
        dispSurface = vds;
        producer = vds;
    }
}

bool DisplayUtils::createV4L2BasedVirtualDisplay(HWComposer* hwc, int32_t &hwcDisplayId,
                   sp<DisplaySurface> &dispSurface, sp<IGraphicBufferProducer> &producer,
                   sp<IGraphicBufferProducer> currentStateSurface,
                   sp<IGraphicBufferProducer> bqProducer, sp<IGraphicBufferConsumer> bqConsumer,
                   int currentStateType) {
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.sys.wfd.virtual", value, "0");
    int wfdVirtual = atoi(value);
    if(wfdVirtual && hwcDisplayId > 0) {
        //Read virtual display properties and create a
        //rendering surface for it inorder to be handled
        //by hwc.

        sp<ANativeWindow> mNativeWindow = new Surface(currentStateSurface);
        ANativeWindow* const window = mNativeWindow.get();

        int format;
        window->query(window, NATIVE_WINDOW_FORMAT, &format);
        EGLSurface surface;
        EGLint w, h;
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        // In M AOSP getEGLConfig() always returns EGL_NO_CONFIG as
        // EGL_ANDROIDX_no_config_context active now.
        EGLConfig config = RenderEngine::chooseEglConfig(display, format);

        surface = eglCreateWindowSurface(display, config, window, NULL);
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);
        if(hwc->setVirtualDisplayProperties(hwcDisplayId, w, h, format) != NO_ERROR)
            return false;

        dispSurface = new FramebufferSurface(*hwc, currentStateType, bqConsumer);
        producer = bqProducer;
        return true;
    }
    return false;
}

bool DisplayUtils::canAllocateHwcDisplayIdForVDS(int usage) {
    // on AOSP builds with QTI_BSP disabled, we should allocate hwc display id for virtual display
    int flag_mask = 0xffffffff;
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.vds.allow_hwc", value, "0");
    int allowHwcForVDS = atoi(value);

#if QTI_BSP
    // Reserve hardware acceleration for WFD use-case
    flag_mask = GRALLOC_USAGE_PRIVATE_WFD;
#endif

    return allowHwcForVDS || (usage & flag_mask);
}

}; // namespace android

