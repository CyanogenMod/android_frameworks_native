/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Not a Contribution
 *
 *
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
#include <GLES2/gl2ext.h>

#include <ui/Rect.h>

#include <utils/String8.h>
#include <utils/Trace.h>

#include <cutils/compiler.h>
#include <gui/ISurfaceComposer.h>
#include <math.h>

#include "GLES20RenderEngine.h"
#include "Program.h"
#include "ProgramCache.h"
#include "Description.h"
#include "Mesh.h"
#include "Texture.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

GLES20RenderEngine::GLES20RenderEngine() :
        mVpWidth(0), mVpHeight(0), mProjectionRotation(Transform::ROT_0) {

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    const uint16_t protTexData[] = { 0 };
    glGenTextures(1, &mProtectedTexName);
    glBindTexture(GL_TEXTURE_2D, mProtectedTexName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, protTexData);

    //mColorBlindnessCorrection = M;
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
        size_t vpw, size_t vph, Rect sourceCrop, size_t hwh, bool yswap,
        Transform::orientation_flags rotation) {

    size_t l = sourceCrop.left;
    size_t r = sourceCrop.right;

    // In GL, (0, 0) is the bottom-left corner, so flip y coordinates
    size_t t = hwh - sourceCrop.top;
    size_t b = hwh - sourceCrop.bottom;

    mat4 m;
    if (yswap) {
        m = mat4::ortho(l, r, t, b, 0, 1);
    } else {
        m = mat4::ortho(l, r, b, t, 0, 1);
    }

    // Apply custom rotation to the projection.
    float rot90InRadians = 2.0f * static_cast<float>(M_PI) / 4.0f;
    switch (rotation) {
        case Transform::ROT_0:
            break;
        case Transform::ROT_90:
            m = mat4::rotate(rot90InRadians, vec3(0,0,1)) * m;
            break;
        case Transform::ROT_180:
            m = mat4::rotate(rot90InRadians * 2.0f, vec3(0,0,1)) * m;
            break;
        case Transform::ROT_270:
            m = mat4::rotate(rot90InRadians * 3.0f, vec3(0,0,1)) * m;
            break;
        case Transform::FLIP_H:
            m = mat4::scale(vec4(-1, 1, 1, 1)) * m;
            break;
        case Transform::FLIP_V:
            m = mat4::scale(vec4(1, -1, 1, 1)) * m;
            break;
        default:
            break;
    }

    glViewport(0, 0, vpw, vph);
    mState.setProjectionMatrix(m);
    mVpWidth = vpw;
    mVpHeight = vph;
    mProjectionSourceCrop = sourceCrop;
    mProjectionYSwap = yswap;
    mProjectionRotation = rotation;
}

#ifdef USE_HWC2
void GLES20RenderEngine::setupLayerBlending(bool premultipliedAlpha,
        bool opaque, float alpha) {
#else
void GLES20RenderEngine::setupLayerBlending(
    bool premultipliedAlpha, bool opaque, int alpha) {
#endif

    mState.setPremultipliedAlpha(premultipliedAlpha);
    mState.setOpaque(opaque);
#ifdef USE_HWC2
    mState.setPlaneAlpha(alpha);

    if (alpha < 1.0f || !opaque) {
#else
    mState.setPlaneAlpha(alpha / 255.0f);

    if (alpha < 0xFF || !opaque) {
#endif
        glEnable(GL_BLEND);
        glBlendFunc(premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

#ifdef USE_HWC2
void GLES20RenderEngine::setupDimLayerBlending(float alpha) {
#else
void GLES20RenderEngine::setupDimLayerBlending(int alpha) {
#endif
    mState.setPlaneAlpha(1.0f);
    mState.setPremultipliedAlpha(true);
    mState.setOpaque(false);
#ifdef USE_HWC2
    mState.setColor(0, 0, 0, alpha);
#else
    mState.setColor(0, 0, 0, alpha/255.0f);
#endif
    mState.disableTexture();

#ifdef USE_HWC2
    if (alpha == 1.0f) {
#else
    if (alpha == 0xFF) {
#endif
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
}

#ifdef USE_HWC2
void GLES20RenderEngine::setupDimLayerBlendingWithColor(uint32_t color, float alpha) {
#else
void GLES20RenderEngine::setupDimLayerBlendingWithColor(uint32_t color, int alpha) {
#endif
    // SF Client sets the color on Dim Layer in RGBA format
    float r = float((color & 0xFF000000) >> 24);
    float g = float((color & 0x00FF0000) >> 16);
    float b = float((color & 0x0000FF00) >> 8);
    float a = float(color & 0x000000FF);

    mState.setPlaneAlpha(1.0f);
    mState.setPremultipliedAlpha(true);
    mState.setOpaque(false);
#ifdef USE_HWC2
    mState.setColor(r, g, b, (a / 255.0f) * alpha);
#else
    mState.setColor(r, g, b, (a / 255.0f) * (alpha / 255.0f));
#endif
    mState.disableTexture();

#ifdef USE_HWC2
    if (alpha == 1.0f) {
#else
    if (alpha == 0xFF) {
#endif
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void GLES20RenderEngine::setupLayerTexturing(const Texture& texture) {
    GLuint target = texture.getTextureTarget();
    glBindTexture(target, texture.getTextureName());
    GLenum filter = GL_NEAREST;
    if (texture.getFiltering()) {
        filter = GL_LINEAR;
    }
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);

    mState.setTexture(texture);
}

void GLES20RenderEngine::setupLayerBlackedOut() {
    glBindTexture(GL_TEXTURE_2D, mProtectedTexName);
    Texture texture(Texture::TEXTURE_2D, mProtectedTexName);
    texture.setDimensions(1, 1); // FIXME: we should get that from somewhere
    mState.setTexture(texture);
}

mat4 GLES20RenderEngine::setupColorTransform(const mat4& colorTransform) {
    mat4 oldTransform = mState.getColorMatrix();
    mState.setColorMatrix(colorTransform);
    return oldTransform;
}

void GLES20RenderEngine::disableTexturing() {
    mState.disableTexture();
}

void GLES20RenderEngine::disableBlending() {
    glDisable(GL_BLEND);
}


void GLES20RenderEngine::bindImageAsFramebuffer(EGLImageKHR image,
        uint32_t* texName, uint32_t* fbName, uint32_t* status,
        bool useReadPixels, int reqWidth, int reqHeight) {
    GLuint tname, name;
    if (!useReadPixels) {
        // turn our EGLImage into a texture
        glGenTextures(1, &tname);
        glBindTexture(GL_TEXTURE_2D, tname);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);

        // create a Framebuffer Object to render into
        glGenFramebuffers(1, &name);
        glBindFramebuffer(GL_FRAMEBUFFER, name);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tname, 0);
    } else {
        // since we're going to use glReadPixels() anyways,
        // use an intermediate renderbuffer instead
        glGenRenderbuffers(1, &tname);
        glBindRenderbuffer(GL_RENDERBUFFER, tname);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, reqWidth, reqHeight);
        // create a FBO to render into
        glGenFramebuffers(1, &name);
        glBindFramebuffer(GL_FRAMEBUFFER, name);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, tname);
    }

    *status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    *texName = tname;
    *fbName = name;
}

void GLES20RenderEngine::unbindFramebuffer(uint32_t texName, uint32_t fbName,
        bool useReadPixels) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbName);
    if (!useReadPixels)
        glDeleteTextures(1, &texName);
    else
        glDeleteRenderbuffers(1, &texName);
}

void GLES20RenderEngine::setupFillWithColor(float r, float g, float b, float a) {
    mState.setPlaneAlpha(1.0f);
    mState.setPremultipliedAlpha(true);
    mState.setOpaque(false);
    mState.setColor(r, g, b, a);
    mState.disableTexture();
    glDisable(GL_BLEND);
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
            mesh.getPositions());

    glDrawArrays(mesh.getPrimitive(), 0, mesh.getVertexCount());

    if (mesh.getTexCoordsSize()) {
        glDisableVertexAttribArray(Program::texCoords);
    }
}

void GLES20RenderEngine::dump(String8& result) {
    RenderEngine::dump(result);
}

void GLES20RenderEngine::setupLayerMasking(const Texture& maskTexture, float alphaThreshold) {
    glActiveTexture(GL_TEXTURE0 + 1);
    GLuint target = maskTexture.getTextureTarget();
    glBindTexture(target, maskTexture.getTextureName());
    GLenum filter = GL_NEAREST;
    if (maskTexture.getFiltering()) {
        filter = GL_LINEAR;
    }
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);

    if (alphaThreshold < 0) alphaThreshold = 0;
    if (alphaThreshold > 1.0f) alphaThreshold = 1.0f;

    mState.setMasking(maskTexture, alphaThreshold);
    glActiveTexture(GL_TEXTURE0);
}

void GLES20RenderEngine::disableLayerMasking() {
    mState.disableMasking();
}

// ---------------------------------------------------------------------------
}; // namespace android
// ---------------------------------------------------------------------------

#if defined(__gl_h_)
#error "don't include gl/gl.h in this file"
#endif
