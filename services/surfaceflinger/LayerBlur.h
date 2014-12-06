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

#ifndef ANDROID_LAYER_BLUR_H
#define ANDROID_LAYER_BLUR_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include "Layer.h"

// ---------------------------------------------------------------------------

namespace android {

/**
 * Blur layer object.
 * Actual blurring logics are capsulated in libuiblur.so
 */
class LayerBlur : public Layer
{
public:
    LayerBlur(SurfaceFlinger* flinger, const sp<Client>& client,
            const String8& name, uint32_t w, uint32_t h, uint32_t flags);
    virtual ~LayerBlur();

    virtual const char* getTypeId() const { return "LayerBlur"; }
    virtual void onDraw(const sp<const DisplayDevice>& hw, const Region& clip,
            bool useIdentityTransform);
    virtual bool isOpaque(const Layer::State& /*s*/) const { return false; }
    virtual bool isSecure() const         { return false; }
    virtual bool isFixedSize() const      { return true; }
    virtual bool isVisible() const;

    virtual bool isBlurLayer() const      { return true; }
    virtual bool setBlurMaskLayer(sp<Layer>& maskLayer);
    virtual bool setBlurMaskSampling(int32_t sampling) { mBlurMaskSampling = sampling; return true; }
    virtual bool setBlurMaskAlphaThreshold(float alpha) { mBlurMaskAlphaThreshold = alpha; return true; }

private:
    class BlurImpl {
    public:

        BlurImpl();
        ~BlurImpl();

        status_t blur(int level, uint32_t inId, size_t inWidth, size_t inheight,
                uint32_t outId, size_t* outWidth, size_t* outHeight);

    protected:
        static status_t initBlurImpl();
        static void closeBlurImpl();
        static void* sLibHandle;
        static bool sUnsupported;

        typedef void* (*initBlurTokenFn)();
        typedef void* (*releaseBlurTokenFn)(void*);
        typedef void* (*blurFn)(void*, int, uint32_t, size_t, size_t, uint32_t, size_t*, size_t*);

        static initBlurTokenFn initBlurToken;
        static releaseBlurTokenFn releaseBlurToken;
        static blurFn doBlur;

        static Mutex sLock;

    private:
        void* mToken;
    };

    BlurImpl mBlurImpl;

    wp<Layer> mBlurMaskLayer;
    int32_t mBlurMaskSampling;
    float mBlurMaskAlphaThreshold;
    uint32_t mLastFrameSequence;

    class FBO {
    public:
        FBO() : fbo(0), width(0), height(0) {}
        int fbo;
        int width;
        int height;
    };

    void initFbo(FBO& fbo, int width, int height, int textureName);
    void releaseFbo(FBO& fbo);
    void ensureFbo(FBO& fbo, int width, int height, int textureName);


    FBO mFboCapture;
    Texture mTextureCapture;

    Texture mTextureBlur;

    FBO mFboMasking;
    Texture mTextureMasking;

    bool captureScreen(const sp<const DisplayDevice>& hw,
            FBO& fbo, Texture& texture, int width, int height);
    void doDrawFinal(const sp<const DisplayDevice>& hw,
        int savedViewportWidth, int savedViewportHeight,
        Rect savedProjectionSourceCrop, bool savedProjectionYSwap,
        Transform::orientation_flags savedRotation, bool useIdentityTransform,
        Texture* maskTexture);
    bool drawMaskLayer(sp<Layer>& maskLayer, const sp<const DisplayDevice>& hw,
        FBO& fbo, int width, int height, int sampling, Texture& texture);

};

// ---------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_LAYER_BLUR_H
