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

#include "ExSurfaceFlinger.h"
#include "ExLayer.h"
#include <cutils/properties.h>
#ifdef QTI_BSP
#include <hardware/display_defs.h>
#endif
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

namespace android {

bool ExSurfaceFlinger::sExtendedMode = false;

ExSurfaceFlinger::ExSurfaceFlinger() {
    char property[PROPERTY_VALUE_MAX] = {0};

    mDebugLogs = false;
    if((property_get("persist.debug.qdframework.logs", property, NULL) > 0) &&
       (!strncmp(property, "1", PROPERTY_VALUE_MAX ) ||
        (!strncasecmp(property,"true", PROPERTY_VALUE_MAX )))) {
        mDebugLogs = true;
    }

    ALOGD_IF(isDebug(),"Creating custom SurfaceFlinger %s",__FUNCTION__);

    mDisableExtAnimation = false;
    if((property_get("sys.disable_ext_animation", property, "0") > 0) &&
       (!strncmp(property, "1", PROPERTY_VALUE_MAX ) ||
        (!strncasecmp(property,"true", PROPERTY_VALUE_MAX )))) {
        mDisableExtAnimation = true;
    }

    ALOGD_IF(isDebug(),"Animation on external is %s in %s",
             mDisableExtAnimation ? "disabled" : "not disabled", __FUNCTION__);
}

ExSurfaceFlinger::~ExSurfaceFlinger() { }

void ExSurfaceFlinger::updateExtendedMode() {
    char prop[PROPERTY_VALUE_MAX];
    property_get("sys.extended_mode", prop, "0");
    sExtendedMode = atoi(prop) ? true : false;
}

void ExSurfaceFlinger::getIndexLOI(size_t dpy,
                               const LayerVector& currentLayers,
                               bool& bIgnoreLayers,
                               int& indexLOI ) {
    size_t i = currentLayers.size();
    while(i--) {
        const sp<Layer>& layer = currentLayers[i];
        /* iterate through the layer list to find ext_only layers and store
         * the index
         */
        if (layer->isSecureDisplay()) {
            bIgnoreLayers = true;
            indexLOI = -1;
            if(!dpy)
                indexLOI = i;
            break;
        }
        /* iterate through the layer list to find ext_only layers or yuv
         * layer(extended_mode) and store the index
         */
        if ((dpy && (layer->isExtOnly() ||
                     (isExtendedMode() && layer->isYuvLayer())))) {
            bIgnoreLayers= true;
            indexLOI = i;
        }
    }
    return;
}

bool ExSurfaceFlinger::updateLayerVisibleNonTransparentRegion(
        const int& dpy, const sp<Layer>& layer,
        bool& bIgnoreLayers, int& indexLOI,
        uint32_t layerStack, const int& i) {

    const Layer::State& s(layer->getDrawingState());

    /* Only add the layer marked as "external_only" or yuvLayer
     * (extended_mode) to external list and
     * only remove the layer marked as "external_only" or yuvLayer in
     * extended_mode from primary list
     * and do not add the layer marked as "internal_only" to external list
     * Add secure UI layers to primary and remove other layers from internal
     * and external list
     */
    if(((bIgnoreLayers && indexLOI != (int)i) ||
        (!dpy && layer->isExtOnly()) ||
        (!dpy && isExtendedMode() && layer->isYuvLayer()))||
       (dpy && layer->isIntOnly())) {
        /* Ignore all other layers except the layers marked as ext_only
         * by setting visible non transparent region empty
         */
        Region visibleNonTransRegion;
        visibleNonTransRegion.set(Rect(0,0));
        layer->setVisibleNonTransparentRegion(visibleNonTransRegion);
        return true;
    }
    /* only consider the layers on the given later stack
     * Override layers created using presentation class by the layers having
     * ext_only flag enabled
     */
    if(s.layerStack != layerStack && !bIgnoreLayers) {
        /* set the visible region as empty since we have removed the
         * layerstack check in rebuildLayerStack() function
         */
        Region visibleNonTransRegion;
        visibleNonTransRegion.set(Rect(0,0));
        layer->setVisibleNonTransparentRegion(visibleNonTransRegion);
        return true;
    }
    return false;
}

void ExSurfaceFlinger::delayDPTransactionIfNeeded(
        const Vector<DisplayState>& displays) {
    /* Delay the display projection transaction by 50ms only when the disable
     * external rotation animation feature is enabled
     */
    if(mDisableExtAnimation) {
        size_t count = displays.size();
        for (size_t i=0 ; i<count ; i++) {
            const DisplayState& s(displays[i]);
            if((mDisplays.indexOfKey(s.token) >= 0) && (s.token !=
                    mBuiltinDisplays[DisplayDevice::DISPLAY_PRIMARY])) {
                const uint32_t what = s.what;
                /* Invalidate and Delay the binder thread by 50 ms on
                 * eDisplayProjectionChanged to trigger a draw cycle so that
                 * it can fix one incorrect frame on the External, when we
                 * disable external animation
                 */
                if (what & DisplayState::eDisplayProjectionChanged) {
                    invalidateHwcGeometry();
                    repaintEverything();
                    usleep(50000);
                }
            }
        }
    }
}

bool ExSurfaceFlinger::canDrawLayerinScreenShot(
                             const sp<const DisplayDevice>& hw,
                             const sp<Layer>& layer) {
    int dispType = hw->getDisplayType();
    /* a) Don't draw SecureDisplayLayer or ProtectedLayer.
     * b) Don't let ext_only and extended_mode to be captured
     * If not, we would see incorrect image during rotation
     * on primary.
     */
    if(!layer->isSecureDisplay()
       && !layer->isProtected()
       && !(!dispType && (layer->isExtOnly() ||
          (isExtendedMode() && layer->isYuvLayer())))
       && layer->isVisible() ){
        return true;
    }
    return false;
}

void ExSurfaceFlinger::isfreezeSurfacePresent(bool& freezeSurfacePresent,
                             const sp<const DisplayDevice>& hw,
                             const int32_t& id) {
    freezeSurfacePresent = false;
    /* Get the layers in the current drawing state */
    const LayerVector& layers(mDrawingState.layersSortedByZ);
    const size_t layerCount = layers.size();
    /* Look for ScreenShotSurface in external layer list, only when
     * disable external rotation animation feature is enabled
     */
    if(mDisableExtAnimation && (id != HWC_DISPLAY_PRIMARY)) {
        for (size_t i = 0 ; i < layerCount ; ++i) {
            static int screenShotLen = strlen("ScreenshotSurface");
            const sp<Layer>& layer(layers[i]);
            const Layer::State& s(layer->getDrawingState());
            /* check the layers associated with external display */
            if(s.layerStack == hw->getLayerStack()) {
                if(!strncmp(layer->getName(), "ScreenshotSurface",
                            screenShotLen)) {
                    /* Screenshot layer is present, and animation in
                     * progress
                     */
                    freezeSurfacePresent = true;
                    break;
                }
            }
        }
    }
}

void ExSurfaceFlinger::setOrientationEventControl(bool& freezeSurfacePresent,
                             const int32_t& id) {
    HWComposer& hwc(getHwComposer());
    HWComposer::LayerListIterator cur = hwc.begin(id);

    if(freezeSurfacePresent) {
        /* If freezeSurfacePresent, set ANIMATING flag
         * which is used to support disable animation on external
         */
        cur->setAnimating(true);
    }
}

void ExSurfaceFlinger::updateVisibleRegionsDirty() {
    /* If extended_mode is set, and set mVisibleRegionsDirty
     * as we need to rebuildLayerStack
     */
    if(isExtendedMode()) {
        mVisibleRegionsDirty = true;
    }
}

void ExSurfaceFlinger::drawWormHoleIfRequired(HWComposer::LayerListIterator& cur,
        const HWComposer::LayerListIterator& end,
        const sp<const DisplayDevice>& hw,
        const Region& region) {
    if (cur != end) {
#ifdef QTI_BSP
        if (cur->getCompositionType() != HWC_BLIT)
            drawWormhole(hw, region);
#endif
    } else {
           drawWormhole(hw, region);
    }
}

}; // namespace android
