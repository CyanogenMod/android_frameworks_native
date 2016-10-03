/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Not a Contribution
 *
 *
 * Copyright (C) 2007 The Android Open Source Project
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

// #define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "LayerDim"

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>

#include "LayerDim.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"
#include "RenderEngine/RenderEngine.h"

namespace android {
// ---------------------------------------------------------------------------

LayerDim::LayerDim(SurfaceFlinger* flinger, const sp<Client>& client,
        const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    : Layer(flinger, client, name, w, h, flags) {
}

LayerDim::~LayerDim() {
}

void LayerDim::onDraw(const sp<const DisplayDevice>& hw,
        const Region& /* clip */, bool useIdentityTransform)
{
    const State& s(getDrawingState());
    if (s.alpha>0) {
        Mesh mesh(Mesh::TRIANGLE_FAN, 4, 2);
        computeGeometry(hw, mesh, useIdentityTransform);
        RenderEngine& engine(mFlinger->getRenderEngine());
        if (!s.color) {
          engine.setupDimLayerBlending(s.alpha);
        } else {
          engine.setupDimLayerBlendingWithColor(s.color, s.alpha);
        }
        engine.drawMesh(mesh);
        engine.disableBlending();
    }
}

bool LayerDim::isVisible() const {
    const Layer::State& s(getDrawingState());
    return !(s.flags & layer_state_t::eLayerHidden) && s.alpha;
}

#ifndef USE_HWC2
void LayerDim::setPerFrameData(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
  HWComposer& hwc = mFlinger->getHwComposer();

  Layer::setPerFrameData(hw, layer);
  if (hwc.hasDimComposition()) {
    // SF Client can set RGBA color on Dim layer. Solid Black is default.
    uint32_t color = getDrawingState().color;
    uint32_t rgba_color = !color ? 0x000000FF : color;
    layer.setDim(rgba_color);
  }
}
#endif

// ---------------------------------------------------------------------------

}; // namespace android
