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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <GLES2/gl2.h>

#include <utils/String8.h>
#include <utils/Trace.h>

#include <cutils/compiler.h>

#include "GLES20RenderEngine.h"
#include "GLExtensions.h"
#include "Program.h"
#include "ProgramCache.h"
#include "Description.h"
#include "Mesh.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

GLES20RenderEngine::GLES20RenderEngine() {

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    struct pack565 {
        inline uint16_t operator() (int r, int g, int b) const {
            return (r<<11)|(g<<5)|b;
        }
    } pack565;

    const uint16_t protTexData[] = { pack565(0x03, 0x03, 0x03) };
    glGenTextures(1, &mProtectedTexName);
    glBindTexture(GL_TEXTURE_2D, mProtectedTexName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, protTexData);
}

GLES20RenderEngine::~GLES20RenderEngine() {
}


size_t GLES20RenderEngine::getMaxTextureSize() const {
    return mMaxTextureSize;
}

size_t GLES20RenderEngine::getMaxViewportDims() const {
    return
        mMaxViewportDims[0] < mMaxViewportDims[1] ?
            mMaxViewportDims[0] : mMaxViewportDims[1];
}

void GLES20RenderEngine::setViewportAndProjection(
        size_t vpw, size_t vph, size_t w, size_t h, bool yswap) {

    struct ortho {
        inline void operator() (GLfloat *m,
                GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
                GLfloat near, GLfloat far) const {
            memset(m, 0, 16*sizeof(GLfloat));
            m[ 0] = 2.0f / (right - left);
            m[ 5] = 2.0f / (top   - bottom);
            m[10] =-2.0f / (far   - near);
            m[15] = 1.0f;
            m[12] = -(right + left) / (right - left);
            m[13] = -(top + bottom) / (top - bottom);
            m[14] = -(far + near) / (far - near);
        }
    } ortho;

    GLfloat m[16];
    if (yswap)  ortho(m, 0, w, h, 0, 0, 1);
    else        ortho(m, 0, w, 0, h, 0, 1);

    glViewport(0, 0, vpw, vph);
    mState.setProjectionMatrix(m);
}

void GLES20RenderEngine::setupLayerBlending(
    bool premultipliedAlpha, bool opaque, int alpha) {

    mState.setPremultipliedAlpha(premultipliedAlpha);
    mState.setOpaque(opaque);
    mState.setPlaneAlpha(alpha / 255.0f);

    if (alpha < 0xFF || !opaque) {
        glEnable(GL_BLEND);
        glBlendFunc(premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void GLES20RenderEngine::setupDimLayerBlending(int alpha) {
    mState.setPlaneAlpha(alpha / 255.0f);

    if (alpha == 0xFF) {
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    disableTexturing();
}

void GLES20RenderEngine::setupLayerTexturing(size_t textureName,
    bool useFiltering, const float* textureMatrix) {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureName);
    GLenum filter = GL_NEAREST;
    if (useFiltering) {
        filter = GL_LINEAR;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, filter);

    mState.setTextureName(GL_TEXTURE_EXTERNAL_OES, textureName);
    mState.setTextureMatrix(textureMatrix);
}

void GLES20RenderEngine::setupLayerBlackedOut() {
    const GLfloat m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    glBindTexture(GL_TEXTURE_2D, mProtectedTexName);
    mState.setTextureName(GL_TEXTURE_2D, mProtectedTexName);
    mState.setTextureMatrix(m);
}

void GLES20RenderEngine::disableTexturing() {
    mState.disableTexture();
}

void GLES20RenderEngine::disableBlending() {
    glDisable(GL_BLEND);
}

void GLES20RenderEngine::fillWithColor(const Mesh& mesh, float r, float g, float b, float a) {
    mState.setColor(r, g, b, a);
    disableTexturing();
    glDisable(GL_BLEND);

    ProgramCache::getInstance().useProgram(mState);

    glVertexAttribPointer(Program::position,
            mesh.getVertexSize(),
            GL_FLOAT, GL_FALSE,
            mesh.getByteStride(),
            mesh.getVertices());

    glDrawArrays(mesh.getPrimitive(), 0, mesh.getVertexCount());
}

void GLES20RenderEngine::drawMesh(const Mesh& mesh) {

    ProgramCache::getInstance().useProgram(mState);

    if (mesh.getTexCoordsSize()) {
        glEnableVertexAttribArray(Program::texCoords);
        glVertexAttribPointer(Program::texCoords,
                mesh.getTexCoordsSize(),
                GL_FLOAT, GL_FALSE,
                mesh.getByteStride(),
                mesh.getTexCoords());
    }

    glVertexAttribPointer(Program::position,
            mesh.getVertexSize(),
            GL_FLOAT, GL_FALSE,
            mesh.getByteStride(),
            mesh.getVertices());

    glDrawArrays(mesh.getPrimitive(), 0, mesh.getVertexCount());

    if (mesh.getTexCoordsSize()) {
        glDisableVertexAttribArray(Program::texCoords);
    }
}

void GLES20RenderEngine::dump(String8& result) {
    const GLExtensions& extensions(GLExtensions::getInstance());
    result.appendFormat("GLES: %s, %s, %s\n",
            extensions.getVendor(),
            extensions.getRenderer(),
            extensions.getVersion());
    result.appendFormat("%s\n", extensions.getExtension());
}

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------
