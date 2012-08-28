/*
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

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>

#include "LayerDim.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"

namespace android {
// ---------------------------------------------------------------------------

LayerDim::LayerDim(SurfaceFlinger* flinger, const sp<Client>& client)
    : LayerBaseClient(flinger, client)
{
}

LayerDim::~LayerDim()
{
}

void LayerDim::onDraw(const sp<const DisplayDevice>& hw, const Region& clip) const
{
    const State& s(drawingState());
    if (s.alpha>0) {
        const GLfloat alpha = s.alpha/255.0f;
        const uint32_t fbHeight = hw->getHeight();
        glDisable(GL_TEXTURE_EXTERNAL_OES);
        glDisable(GL_TEXTURE_2D);

        if (s.alpha == 0xFF) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        glColor4f(0, 0, 0, alpha);

        LayerMesh mesh;
        computeGeometry(hw, &mesh);

        glVertexPointer(2, GL_FLOAT, 0, mesh.getVertices());
        glDrawArrays(GL_TRIANGLE_FAN, 0, mesh.getVertexCount());

        glDisable(GL_BLEND);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
}

// ---------------------------------------------------------------------------

}; // namespace android
