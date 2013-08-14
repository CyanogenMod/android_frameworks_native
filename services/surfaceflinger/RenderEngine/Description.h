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

#include <GLES2/gl2.h>

#ifndef SF_RENDER_ENGINE_DESCRIPTION_H_
#define SF_RENDER_ENGINE_DESCRIPTION_H_

namespace android {

class Program;

/*
 * This holds the state of the rendering engine. This class is used
 * to generate a corresponding GLSL program and set the appropriate
 * uniform.
 *
 * Program and ProgramCache are friends and access the state directly
 */
class Description {
    friend class Program;
    friend class ProgramCache;

    // value of the plane-alpha, between 0 and 1
    GLclampf mPlaneAlpha;
    // whether textures are premultiplied
    bool mPremultipliedAlpha;
    // whether this layer is marked as opaque
    bool mOpaque;
    // texture target, TEXTURE_2D or TEXTURE_EXTERNAL
    GLenum mTextureTarget;

    // name of the texture
    GLuint mTextureName;
    // color used when texturing is disabled
    GLclampf mColor[4];
    // projection matrix
    GLfloat mProjectionMatrix[16];
    // texture matrix
    GLfloat mTextureMatrix[16];

public:
    Description();
    ~Description();

    void setPlaneAlpha(GLclampf planeAlpha);
    void setPremultipliedAlpha(bool premultipliedAlpha);
    void setOpaque(bool opaque);
    void setTextureName(GLenum target, GLuint tname);
    void disableTexture();
    void setColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void setProjectionMatrix(GLfloat const* mtx);
    void setTextureMatrix(GLfloat const* mtx);

private:
    bool mUniformsDirty;
};

} /* namespace android */

#endif /* SF_RENDER_ENGINE_DESCRIPTION_H_ */
