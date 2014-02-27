/*
 * Copyright (C) 2014 The Android Open Source Project
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
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>

#include "LayerVideoPlane.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"
#include "RenderEngine/RenderEngine.h"

#define DEBUG_BLUE_SURFACE 1

namespace android {
// ---------------------------------------------------------------------------

LayerVideoPlane::LayerVideoPlane(SurfaceFlinger* flinger, const sp<Client>& client,
        const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    : Layer(flinger, client, name, w, h, flags) {
}

LayerVideoPlane::~LayerVideoPlane() {
}

void LayerVideoPlane::onDraw(const sp<const DisplayDevice>& hw,
        const Region& /* clip */, bool useIdentityTransform) const
{
#if DEBUG_BLUE_SURFACE
    Mesh mesh(Mesh::TRIANGLE_FAN, 4, 2);
    computeGeometry(hw, mesh, useIdentityTransform);
    RenderEngine& engine(mFlinger->getRenderEngine());
    engine.setupFillWithColor(0.0f, 0.0f, 1.0f, 1.0f);
    engine.drawMesh(mesh);
#else
    // TODO
#endif
}

bool LayerVideoPlane::isVisible() const {
    const Layer::State& s(getDrawingState());
    return !(s.flags & layer_state_t::eLayerHidden);
}


// ---------------------------------------------------------------------------

}; // namespace android
