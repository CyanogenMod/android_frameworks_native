/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Not a Contribution.
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <dlfcn.h>

#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/Trace.h>
#include <GLES/gl.h>
#include <GLES2/gl2.h>

#include "LayerBlur.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"
#include "RenderEngine/RenderEngine.h"

namespace android {
// ---------------------------------------------------------------------------

// Automatically disables scissor test and restores it when destroyed
class ScopedScissorDisabler {
    bool scissorEnabled;
public:
    ScopedScissorDisabler(bool enabled) : scissorEnabled(enabled) {
        if(scissorEnabled) {
            glDisable(GL_SCISSOR_TEST);
        }
    }
    ~ScopedScissorDisabler() {
        if(scissorEnabled) {
            glEnable(GL_SCISSOR_TEST);
        }
    };
};

static void setupMeshPartial(Mesh& mesh, Rect rcDraw, Rect rcTexture, int texWidth, int texHeight, int viewportHeight) {
    Mesh::VertexArray<vec2> position(mesh.getPositionArray<vec2>());
    position[0] = vec2(rcDraw.left, rcDraw.top);
    position[1] = vec2(rcDraw.left, rcDraw.bottom);
    position[2] = vec2(rcDraw.right, rcDraw.bottom);
    position[3] = vec2(rcDraw.right, rcDraw.top);
    for(size_t i=0; i<4; ++i) {
        position[i].y = viewportHeight - position[i].y;
    }

    Mesh::VertexArray<vec2> texCoords(mesh.getTexCoordArray<vec2>());
    texCoords[0] = vec2(rcTexture.left/(float)texWidth, 1.0f - rcTexture.top/(float)texHeight);
    texCoords[1] = vec2(rcTexture.left/(float)texWidth, 1.0f - rcTexture.bottom/(float)texHeight);
    texCoords[2] = vec2(rcTexture.right/(float)texWidth, 1.0f - rcTexture.bottom/(float)texHeight);
    texCoords[3] = vec2(rcTexture.right/(float)texWidth, 1.0f - rcTexture.top/(float)texHeight);
}

static void setupMesh(Mesh& mesh, int width, int height, int viewportHeight) {
    Mesh::VertexArray<vec2> position(mesh.getPositionArray<vec2>());
    position[0] = vec2(0, 0);
    position[1] = vec2(0, height);
    position[2] = vec2(width, height);
    position[3] = vec2(width, 0);
    for(size_t i=0; i<4; ++i) {
        position[i].y = viewportHeight - position[i].y;
    }

    Mesh::VertexArray<vec2> texCoords(mesh.getTexCoordArray<vec2>());
    texCoords[0] = vec2(0, 1.0f);
    texCoords[1] = vec2(0, 0);
    texCoords[2] = vec2(1.0f, 0);
    texCoords[3] = vec2(1.0f, 1.0f);
}

LayerBlur::LayerBlur(SurfaceFlinger* flinger, const sp<Client>& client,
        const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    : Layer(flinger, client, name, w, h, flags), mBlurMaskSampling(1),
    mBlurMaskAlphaThreshold(0.0f) ,mLastFrameSequence(0)
{
    GLuint texnames[3];
    mFlinger->getRenderEngine().genTextures(3, texnames);
    mTextureCapture.init(Texture::TEXTURE_2D, texnames[0]);
    mTextureBlur.init(Texture::TEXTURE_2D, texnames[1]);
    mTextureMasking.init(Texture::TEXTURE_2D, texnames[2]);
}

LayerBlur::~LayerBlur() {

    releaseFbo(mFboCapture);
    releaseFbo(mFboMasking);
    mFlinger->deleteTextureAsync(mTextureCapture.getTextureName());
    mFlinger->deleteTextureAsync(mTextureBlur.getTextureName());
    mFlinger->deleteTextureAsync(mTextureMasking.getTextureName());
}

void LayerBlur::onDraw(const sp<const DisplayDevice>& hw, const Region& /*clip*/,
        bool useIdentityTransform)
{
    clock_t t1 = clock();
    const ScopedTrace traceTotal(ATRACE_TAG, "Blur.onDraw");

    const Layer::State& s(getDrawingState());

    if (s.alpha==0) {
        return;
    }

    /////
    // NOTE:
    //
    // Scissor test has been turned on by SurfaceFlinger for NON-primary display
    // We need to turn off the scissor test during our fbo drawing
    GLboolean isScissorEnabled = false;
    glGetBooleanv(GL_SCISSOR_TEST, &isScissorEnabled);
    ScopedScissorDisabler _(isScissorEnabled);
    //
    /////


    int hwWidth = hw->getWidth();
    int hwHeight = hw->getHeight();

    RenderEngine& engine(mFlinger->getRenderEngine());

    bool savedProjectionYSwap = engine.getProjectionYSwap();
    Rect savedProjectionSourceCrop = engine.getProjectionSourceCrop();
    Transform::orientation_flags savedProjectionRotation = engine.getProjectionRotation();
    size_t savedViewportWidth = engine.getViewportWidth();
    size_t savedViewportHeight = engine.getViewportHeight();


    if (mLastFrameSequence != mFlinger->mActiveFrameSequence ||
            mTextureBlur.getWidth() == 0 || mTextureBlur.getHeight() == 0) {
        // full drawing needed.


        // capture
        if (!captureScreen(hw, mFboCapture, mTextureCapture, hwWidth, hwHeight)) {
            return;
        }

        // blur
        size_t outTexWidth = mTextureBlur.getWidth();
        size_t outTexHeight = mTextureBlur.getHeight();
        if (mBlurImpl.blur(s.blur,
                mTextureCapture.getTextureName(),
                mTextureCapture.getWidth(),
                mTextureCapture.getHeight(),
                mTextureBlur.getTextureName(),
                &outTexWidth,
                &outTexHeight) != OK) {
            return;
        }

        // mTextureBlur now has "Blurred image"
        mTextureBlur.setDimensions(outTexWidth, outTexHeight);

    } else {
        // We can just re-use mTextureBlur.
        // SurfaceFlinger or other LayerBlur object called my draw() multiple times
        // while making one frame.
        //
        // Fall through
    }

    // masking
    bool masking = false;
    sp<Layer> maskLayer = mBlurMaskLayer.promote();
    if (maskLayer != 0) {
        // The larger sampling, the faster drawing.
        // The smaller sampling, the prettier out line.
        int sampling = mBlurMaskSampling >= 1 ? mBlurMaskSampling : 1;
        //ALOGV("maskLayer available, sampling:%d", sampling);
        masking = drawMaskLayer(maskLayer, hw, mFboMasking, hwWidth, hwHeight, sampling, mTextureMasking);
    }


    // final draw
    doDrawFinal(hw,
            savedViewportWidth, savedViewportHeight,
            savedProjectionSourceCrop,
            savedProjectionYSwap,
            savedProjectionRotation,
            useIdentityTransform,
            masking ? &mTextureMasking : 0
            );

    mLastFrameSequence = mFlinger->mActiveFrameSequence;

    clock_t t2 = clock();
    ALOGV("onDraw took %d ms", (int)(1000*(t2-t1)/CLOCKS_PER_SEC));
}


bool LayerBlur::captureScreen(const sp<const DisplayDevice>& hw, FBO& fbo, Texture& texture, int width, int height) {
    ATRACE_CALL();
    ensureFbo(fbo, width, height, texture.getTextureName());
    Transform::orientation_flags rotation = Transform::ROT_0;
    if(fbo.fbo == 0) {
        ALOGE("captureScreen(). fbo.fbo == 0");
        return false;
    }

    GLint savedFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            texture.getTextureTarget(),
            texture.getTextureName(), 0);

    mFlinger->getRenderEngine().clearWithColor(0.0f, 0.0f, 0.0f, 1.0f);
    rotation = (Transform::orientation_flags)(rotation ^ hw->getPanelMountFlip());
    mFlinger->renderScreenImplLocked(
                hw,
                Rect(0,0,width,height),
                width, height,
                0, getDrawingState().z-1,
                false,
                false,
                rotation);

    glBindFramebuffer(GL_FRAMEBUFFER, savedFramebuffer);

    texture.setDimensions(width, height);

    return true;
}

bool LayerBlur::drawMaskLayer(sp<Layer>& maskLayer, const sp<const DisplayDevice>& hw,
        FBO& fbo, int width, int height, int sampling, Texture& texture) {
    // Draw maskLayer into fbo
    ATRACE_CALL();

    int maskWidth = width/sampling;
    int maskHeight = height/sampling;

    ensureFbo(fbo, maskWidth, maskHeight, texture.getTextureName());
    if(fbo.fbo == 0) {
        ALOGE("drawMaskLayer(). fbo.fbo == 0");
        return false;
    }

    GLint savedFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            texture.getTextureTarget(),
            texture.getTextureName(), 0);

    mFlinger->getRenderEngine().setViewportAndProjection(
            maskWidth, maskHeight,
            Rect(0,0,width,height),
            height,
            false,
            Transform::ROT_0
            );
    setupMesh(mMesh, width, height, height);
    mFlinger->getRenderEngine().clearWithColor(0.0f, 0.0f, 0.0f, 0.0f); // alpha must be ZERO
    maskLayer->draw(hw);
    glBindFramebuffer(GL_FRAMEBUFFER, savedFramebuffer);

    texture.setDimensions(maskWidth, maskHeight);

    return true;
}

/*
 * draw final texture into outer framebuffer
 */
void LayerBlur::doDrawFinal(const sp<const DisplayDevice>& hw,
        int savedViewportWidth, int savedViewportHeight,
        Rect savedProjectionSourceCrop,
        bool savedProjectionYSwap,
        Transform::orientation_flags savedRotation,
        bool useIdentityTransform,
        Texture* maskTexture
        ) {
    ATRACE_CALL();

    int hwWidth = hw->getWidth();
    int hwHeight = hw->getHeight();

    RenderEngine& engine(mFlinger->getRenderEngine());
    const Layer::State& s(getDrawingState());

    Transform trToDraw(useIdentityTransform ? hw->getTransform() : hw->getTransform() * s.active.transform);
    Transform trToMapTexture(hw->getTransform() * s.active.transform);

    Rect frameToDraw(trToDraw.transform(Rect(s.active.w, s.active.h)));
    Rect frameToMapTexture(trToMapTexture.transform(Rect(s.active.w, s.active.h)));

    engine.setViewportAndProjection(
            savedViewportWidth, savedViewportHeight,
            savedProjectionSourceCrop,
            hwHeight,
            savedProjectionYSwap,
            savedRotation
            );


    const mat4 identity;
    float textureMatrix[16];
    memcpy(textureMatrix, identity.asArray(), sizeof(textureMatrix));

    //mTextureBlur.setDimensions(hwWidth, hwHeight);
    mTextureBlur.setFiltering(true);
    mTextureBlur.setMatrix(textureMatrix);

    if (maskTexture != 0) {
        maskTexture->setFiltering(false);
        maskTexture->setMatrix(textureMatrix);
    }

    setupMeshPartial(mMesh, frameToDraw, frameToMapTexture, hwWidth, hwHeight,
            savedProjectionSourceCrop.height());

    engine.setupLayerTexturing(mTextureBlur);
    engine.setupLayerBlending(mPremultipliedAlpha, isOpaque(s), s.alpha);
    if (maskTexture) {
        engine.setupLayerMasking(*maskTexture, mBlurMaskAlphaThreshold);
    }
    engine.drawMesh(mMesh);
    engine.disableLayerMasking();
    engine.disableBlending();
    engine.disableTexturing();

}

bool LayerBlur::isVisible() const {
    const Layer::State& s(getDrawingState());
    return !(s.flags & layer_state_t::eLayerHidden) && s.alpha;
}

bool LayerBlur::setBlurMaskLayer(sp<Layer>& maskLayer) {
    if (maskLayer == mBlurMaskLayer) {
        return false;
    }
    mBlurMaskLayer = maskLayer;
    return true;
}


void LayerBlur::initFbo(FBO& fbobj, int width, int height, int textureName) {
    GLuint fbo=0;

    glGenFramebuffers(1, &fbo);
    glBindTexture(GL_TEXTURE_2D, textureName);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLint savedFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFramebuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, textureName, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, savedFramebuffer);

    fbobj.fbo = fbo;
    fbobj.width = width;
    fbobj.height = height;
}

void LayerBlur::releaseFbo(FBO& fbo) {
    if(fbo.fbo != 0) {
        glDeleteFramebuffers(1, (GLuint*)&fbo.fbo);
    }
    fbo.fbo = 0;
    fbo.width = 0;
    fbo.height = 0;
}

void LayerBlur::ensureFbo(FBO& fbo, int width, int height, int textureName) {
    if(fbo.fbo != 0) {
        if(fbo.width != width || fbo.height != height) {
            releaseFbo(fbo);
        }
    }
    if(fbo.fbo == 0) {
        initFbo(fbo, width, height, textureName);
    }
}

// ---------------------------------------------------------------------------

void* LayerBlur::BlurImpl::sLibHandle = NULL;
bool LayerBlur::BlurImpl::sUnsupported = false;

LayerBlur::BlurImpl::initBlurTokenFn LayerBlur::BlurImpl::initBlurToken = NULL;
LayerBlur::BlurImpl::releaseBlurTokenFn LayerBlur::BlurImpl::releaseBlurToken = NULL;
LayerBlur::BlurImpl::blurFn LayerBlur::BlurImpl::doBlur = NULL;
Mutex LayerBlur::BlurImpl::sLock;

void LayerBlur::BlurImpl::closeBlurImpl() {
    if (sLibHandle != NULL) {
        dlclose(sLibHandle);
        sLibHandle = NULL;
    }
}

status_t LayerBlur::BlurImpl::initBlurImpl() {
    if (sLibHandle != NULL) {
        return OK;
    }
    if (sUnsupported) {
        return NO_INIT;
    }

    sLibHandle = dlopen("libuiblur.so", RTLD_NOW);
    if (sLibHandle == NULL) {
        sUnsupported = true;
        return NO_INIT;
    }

    // happy happy joy joy!

    initBlurToken = (initBlurTokenFn)dlsym(sLibHandle,
            "_ZN7qtiblur13initBlurTokenEv");
    releaseBlurToken = (releaseBlurTokenFn)dlsym(sLibHandle,
            "_ZN7qtiblur16releaseBlurTokenEPv");

    if (sizeof(size_t) == 4) {
        doBlur = (blurFn)dlsym(sLibHandle,
                     "_ZN7qtiblur4blurEPvijjjjPjS1_");
    } else if (sizeof(size_t) == 8) {
        doBlur = (blurFn)dlsym(sLibHandle,
                     "_ZN7qtiblur4blurEPvijmmjPmS1_");
    }

    if (!initBlurToken || !releaseBlurToken || !doBlur) {
        ALOGE("dlsym failed for blur impl!: %s", dlerror());
        closeBlurImpl();
        sUnsupported = true;
        return NO_INIT;
    }

    return OK;
}

LayerBlur::BlurImpl::BlurImpl() : mToken(NULL) {
    Mutex::Autolock _l(sLock);
    if (initBlurImpl() == OK) {
        mToken = initBlurToken();
    }
}

LayerBlur::BlurImpl::~BlurImpl() {
    Mutex::Autolock _l(sLock);
    if (mToken != NULL) {
        releaseBlurToken(mToken);
    }
}

status_t LayerBlur::BlurImpl::blur(int level, uint32_t inId, size_t inWidth, size_t inHeight,
        uint32_t outId, size_t* outWidth, size_t* outHeight) {
    Mutex::Autolock _l(sLock);
    if (mToken == NULL) {
        return NO_INIT;
    }
    return doBlur(mToken, level, inId, inWidth, inHeight,
                  outId, outWidth, outHeight) ? OK : NO_INIT;
}


// ---------------------------------------------------------------------------

}; // namespace android
