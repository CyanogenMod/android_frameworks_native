/*
 * Copyright 2013 The Android Open Source Project
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


#ifndef SF_GLES11RENDERENGINE_H_
#define SF_GLES11RENDERENGINE_H_

#include <stdint.h>
#include <sys/types.h>

#include <GLES/gl.h>

#include "RenderEngine.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class String8;

class GLES11RenderEngine : public RenderEngine {
    GLuint mProtectedTexName;
    GLint mMaxViewportDims[2];
    GLint mMaxTextureSize;

public:
    GLES11RenderEngine();

protected:
    virtual ~GLES11RenderEngine();

    virtual void dump(String8& result);
    virtual void setViewportAndProjection(size_t w, size_t h);
    virtual void setupLayerBlending(bool premultipliedAlpha, bool opaque, int alpha);
    virtual void setupDimLayerBlending(int alpha);
    virtual void setupLayerTexturing(size_t textureName, bool useFiltering, const float* textureMatrix);
    virtual void setupLayerBlackedOut();
    virtual void disableTexturing();
    virtual void disableBlending();

    virtual void clearWithColor(const float vertices[][2], size_t count,
        float red, float green, float blue, float alpha);

    virtual void drawMesh2D(const float vertices[][2], const float texCoords[][2], size_t count);

    virtual size_t getMaxTextureSize() const;
    virtual size_t getMaxViewportDims() const;
};

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------

#endif /* SF_GLES11RENDERENGINE_H_ */
