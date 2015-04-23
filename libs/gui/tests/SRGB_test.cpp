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

#define LOG_TAG "SRGB_test"
//#define LOG_NDEBUG 0

// Ignore for this file because it flags every instance of
// ASSERT_EQ(GL_NO_ERROR, glGetError());
#pragma clang diagnostic ignored "-Wsign-compare"

#include "GLTest.h"

#include <math.h>

#include <gui/CpuConsumer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <android/native_window.h>

#include <gtest/gtest.h>

namespace android {

class SRGBTest : public ::testing::Test {
protected:
    // Class constants
    enum {
        DISPLAY_WIDTH = 512,
        DISPLAY_HEIGHT = 512,
        PIXEL_SIZE = 4, // bytes or components
        DISPLAY_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT * PIXEL_SIZE,
        ALPHA_VALUE = 223, // should be in [0, 255]
        TOLERANCE = 1,
    };
    static const char SHOW_DEBUG_STRING[];

    SRGBTest() :
            mInputSurface(), mCpuConsumer(), mLockedBuffer(),
            mEglDisplay(EGL_NO_DISPLAY), mEglConfig(),
            mEglContext(EGL_NO_CONTEXT), mEglSurface(EGL_NO_SURFACE),
            mComposerClient(), mSurfaceControl(), mOutputSurface() {
    }

    virtual ~SRGBTest() {
        if (mEglDisplay != EGL_NO_DISPLAY) {
            if (mEglSurface != EGL_NO_SURFACE) {
                eglDestroySurface(mEglDisplay, mEglSurface);
            }
            if (mEglContext != EGL_NO_CONTEXT) {
                eglDestroyContext(mEglDisplay, mEglContext);
            }
            eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
            eglTerminate(mEglDisplay);
        }
    }

    virtual void SetUp() {
        sp<IGraphicBufferProducer> producer;
        sp<IGraphicBufferConsumer> consumer;
        BufferQueue::createBufferQueue(&producer, &consumer);
        ASSERT_EQ(NO_ERROR, consumer->setDefaultBufferSize(
                DISPLAY_WIDTH, DISPLAY_HEIGHT));
        mCpuConsumer = new CpuConsumer(consumer, 1);
        String8 name("CpuConsumer_for_SRGBTest");
        mCpuConsumer->setName(name);
        mInputSurface = new Surface(producer);

        ASSERT_NO_FATAL_FAILURE(createEGLSurface(mInputSurface.get()));
        ASSERT_NO_FATAL_FAILURE(createDebugSurface());
    }

    virtual void TearDown() {
        ASSERT_NO_FATAL_FAILURE(copyToDebugSurface());
        ASSERT_TRUE(mLockedBuffer.data != NULL);
        ASSERT_EQ(NO_ERROR, mCpuConsumer->unlockBuffer(mLockedBuffer));
    }

    static float linearToSRGB(float l) {
        if (l <= 0.0031308f) {
            return l * 12.92f;
        } else {
            return 1.055f * pow(l, (1 / 2.4f)) - 0.055f;
        }
    }

    static float srgbToLinear(float s) {
        if (s <= 0.04045) {
            return s / 12.92f;
        } else {
            return pow(((s + 0.055f) / 1.055f), 2.4f);
        }
    }

    static uint8_t srgbToLinear(uint8_t u) {
        float f = u / 255.0f;
        return static_cast<uint8_t>(srgbToLinear(f) * 255.0f + 0.5f);
    }

    void fillTexture(bool writeAsSRGB) {
        uint8_t* textureData = new uint8_t[DISPLAY_SIZE];

        for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
            for (int x = 0; x < DISPLAY_WIDTH; ++x) {
                float realValue = static_cast<float>(x) / (DISPLAY_WIDTH - 1);
                realValue *= ALPHA_VALUE / 255.0f; // Premultiply by alpha
                if (writeAsSRGB) {
                    realValue = linearToSRGB(realValue);
                }

                int offset = (y * DISPLAY_WIDTH + x) * PIXEL_SIZE;
                for (int c = 0; c < 3; ++c) {
                    uint8_t intValue = static_cast<uint8_t>(
                            realValue * 255.0f + 0.5f);
                    textureData[offset + c] = intValue;
                }
                textureData[offset + 3] = ALPHA_VALUE;
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, writeAsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                textureData);
        ASSERT_EQ(GL_NO_ERROR, glGetError());

        delete[] textureData;
    }

    void initShaders() {
        static const char vertexSource[] =
            "attribute vec4 vPosition;\n"
            "varying vec2 texCoords;\n"
            "void main() {\n"
            "  texCoords = 0.5 * (vPosition.xy + vec2(1.0, 1.0));\n"
            "  gl_Position = vPosition;\n"
            "}\n";

        static const char fragmentSource[] =
            "precision mediump float;\n"
            "uniform sampler2D texSampler;\n"
            "varying vec2 texCoords;\n"
            "void main() {\n"
            "  gl_FragColor = texture2D(texSampler, texCoords);\n"
            "}\n";

        GLuint program;
        {
            SCOPED_TRACE("Creating shader program");
            ASSERT_NO_FATAL_FAILURE(GLTest::createProgram(
                    vertexSource, fragmentSource, &program));
        }

        GLint positionHandle = glGetAttribLocation(program, "vPosition");
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        ASSERT_NE(-1, positionHandle);

        GLint samplerHandle = glGetUniformLocation(program, "texSampler");
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        ASSERT_NE(-1, samplerHandle);

        static const GLfloat vertices[] = {
            -1.0f, 1.0f,
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
        };

        glVertexAttribPointer(positionHandle, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glEnableVertexAttribArray(positionHandle);
        ASSERT_EQ(GL_NO_ERROR, glGetError());

        glUseProgram(program);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glUniform1i(samplerHandle, 0);
        ASSERT_EQ(GL_NO_ERROR, glGetError());

        GLuint textureHandle;
        glGenTextures(1, &textureHandle);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glBindTexture(GL_TEXTURE_2D, textureHandle);
        ASSERT_EQ(GL_NO_ERROR, glGetError());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
    }

    void drawTexture(bool asSRGB, GLint x, GLint y, GLsizei width,
            GLsizei height) {
        ASSERT_NO_FATAL_FAILURE(fillTexture(asSRGB));
        glViewport(x, y, width, height);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        ASSERT_EQ(GL_NO_ERROR, glGetError());
    }

    void checkLockedBuffer(PixelFormat format, android_dataspace dataSpace) {
        ASSERT_EQ(mLockedBuffer.format, format);
        ASSERT_EQ(mLockedBuffer.width, DISPLAY_WIDTH);
        ASSERT_EQ(mLockedBuffer.height, DISPLAY_HEIGHT);
        ASSERT_EQ(mLockedBuffer.dataSpace, dataSpace);
    }

    static bool withinTolerance(int a, int b) {
        int diff = a - b;
        return diff >= 0 ? diff <= TOLERANCE : -diff <= TOLERANCE;
    }

    // Primary producer and consumer
    sp<Surface> mInputSurface;
    sp<CpuConsumer> mCpuConsumer;
    CpuConsumer::LockedBuffer mLockedBuffer;

    EGLDisplay mEglDisplay;
    EGLConfig mEglConfig;
    EGLContext mEglContext;
    EGLSurface mEglSurface;

    // Auxiliary display output
    sp<SurfaceComposerClient> mComposerClient;
    sp<SurfaceControl> mSurfaceControl;
    sp<Surface> mOutputSurface;

private:
    void createEGLSurface(Surface* inputSurface) {
        mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        ASSERT_EQ(EGL_SUCCESS, eglGetError());
        ASSERT_NE(EGL_NO_DISPLAY, mEglDisplay);

        EXPECT_TRUE(eglInitialize(mEglDisplay, NULL, NULL));
        ASSERT_EQ(EGL_SUCCESS, eglGetError());

        static const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE };

        EGLint numConfigs = 0;
        EXPECT_TRUE(eglChooseConfig(mEglDisplay, configAttribs, &mEglConfig, 1,
                &numConfigs));
        ASSERT_EQ(EGL_SUCCESS, eglGetError());
        ASSERT_GT(numConfigs, 0);

        static const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE } ;

        mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT,
                contextAttribs);
        ASSERT_EQ(EGL_SUCCESS, eglGetError());
        ASSERT_NE(EGL_NO_CONTEXT, mEglContext);

        mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig,
                inputSurface, NULL);
        ASSERT_EQ(EGL_SUCCESS, eglGetError());
        ASSERT_NE(EGL_NO_SURFACE, mEglSurface);

        EXPECT_TRUE(eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface,
                mEglContext));
        ASSERT_EQ(EGL_SUCCESS, eglGetError());
    }

    void createDebugSurface() {
        if (getenv(SHOW_DEBUG_STRING) == NULL) return;

        mComposerClient = new SurfaceComposerClient;
        ASSERT_EQ(NO_ERROR, mComposerClient->initCheck());

        mSurfaceControl = mComposerClient->createSurface(
                String8("SRGBTest Surface"), DISPLAY_WIDTH, DISPLAY_HEIGHT,
                PIXEL_FORMAT_RGBA_8888);

        ASSERT_TRUE(mSurfaceControl != NULL);
        ASSERT_TRUE(mSurfaceControl->isValid());

        SurfaceComposerClient::openGlobalTransaction();
        ASSERT_EQ(NO_ERROR, mSurfaceControl->setLayer(0x7FFFFFFF));
        ASSERT_EQ(NO_ERROR, mSurfaceControl->show());
        SurfaceComposerClient::closeGlobalTransaction();

        ANativeWindow_Buffer outBuffer;
        ARect inOutDirtyBounds;
        mOutputSurface = mSurfaceControl->getSurface();
        mOutputSurface->lock(&outBuffer, &inOutDirtyBounds);
        uint8_t* bytePointer = reinterpret_cast<uint8_t*>(outBuffer.bits);
        for (int y = 0; y < outBuffer.height; ++y) {
            int rowOffset = y * outBuffer.stride; // pixels
            for (int x = 0; x < outBuffer.width; ++x) {
                int colOffset = (rowOffset + x) * PIXEL_SIZE; // bytes
                for (int c = 0; c < PIXEL_SIZE; ++c) {
                    int offset = colOffset + c;
                    bytePointer[offset] = ((c + 1) * 56) - 1;
                }
            }
        }
        mOutputSurface->unlockAndPost();
    }

    void copyToDebugSurface() {
        if (!mOutputSurface.get()) return;

        size_t bufferSize = mLockedBuffer.height * mLockedBuffer.stride *
                PIXEL_SIZE;

        ANativeWindow_Buffer outBuffer;
        ARect outBufferBounds;
        mOutputSurface->lock(&outBuffer, &outBufferBounds);
        ASSERT_EQ(mLockedBuffer.width, static_cast<uint32_t>(outBuffer.width));
        ASSERT_EQ(mLockedBuffer.height, static_cast<uint32_t>(outBuffer.height));
        ASSERT_EQ(mLockedBuffer.stride, static_cast<uint32_t>(outBuffer.stride));

        if (mLockedBuffer.format == outBuffer.format) {
            memcpy(outBuffer.bits, mLockedBuffer.data, bufferSize);
        } else {
            ASSERT_EQ(mLockedBuffer.format, PIXEL_FORMAT_RGBA_8888);
            ASSERT_EQ(mLockedBuffer.dataSpace, HAL_DATASPACE_SRGB);
            ASSERT_EQ(outBuffer.format, PIXEL_FORMAT_RGBA_8888);
            uint8_t* outPointer = reinterpret_cast<uint8_t*>(outBuffer.bits);
            for (int y = 0; y < outBuffer.height; ++y) {
                int rowOffset = y * outBuffer.stride; // pixels
                for (int x = 0; x < outBuffer.width; ++x) {
                    int colOffset = (rowOffset + x) * PIXEL_SIZE; // bytes

                    // RGB are converted
                    for (int c = 0; c < (PIXEL_SIZE - 1); ++c) {
                        outPointer[colOffset + c] = srgbToLinear(
                                mLockedBuffer.data[colOffset + c]);
                    }

                    // Alpha isn't converted
                    outPointer[colOffset + 3] =
                            mLockedBuffer.data[colOffset + 3];
                }
            }
        }
        mOutputSurface->unlockAndPost();

        int sleepSeconds = atoi(getenv(SHOW_DEBUG_STRING));
        sleep(sleepSeconds);
    }
};

const char SRGBTest::SHOW_DEBUG_STRING[] = "DEBUG_OUTPUT_SECONDS";

TEST_F(SRGBTest, GLRenderFromSRGBTexture) {
    ASSERT_NO_FATAL_FAILURE(initShaders());

    // The RGB texture is displayed in the top half
    ASSERT_NO_FATAL_FAILURE(drawTexture(false, 0, DISPLAY_HEIGHT / 2,
            DISPLAY_WIDTH, DISPLAY_HEIGHT / 2));

    // The SRGB texture is displayed in the bottom half
    ASSERT_NO_FATAL_FAILURE(drawTexture(true, 0, 0,
            DISPLAY_WIDTH, DISPLAY_HEIGHT / 2));

    eglSwapBuffers(mEglDisplay, mEglSurface);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    // Lock
    ASSERT_EQ(NO_ERROR, mCpuConsumer->lockNextBuffer(&mLockedBuffer));
    ASSERT_NO_FATAL_FAILURE(
        checkLockedBuffer(PIXEL_FORMAT_RGBA_8888, HAL_DATASPACE_UNKNOWN));

    // Compare a pixel in the middle of each texture
    int midSRGBOffset = (DISPLAY_HEIGHT / 4) * mLockedBuffer.stride *
            PIXEL_SIZE;
    int midRGBOffset = midSRGBOffset * 3;
    midRGBOffset += (DISPLAY_WIDTH / 2) * PIXEL_SIZE;
    midSRGBOffset += (DISPLAY_WIDTH / 2) * PIXEL_SIZE;
    for (int c = 0; c < PIXEL_SIZE; ++c) {
        int expectedValue = mLockedBuffer.data[midRGBOffset + c];
        int actualValue = mLockedBuffer.data[midSRGBOffset + c];
        ASSERT_PRED2(withinTolerance, expectedValue, actualValue);
    }

    // mLockedBuffer is unlocked in TearDown so we can copy data from it to
    // the debug surface if necessary
}

// XXX: Disabled since we don't currently expect this to work
TEST_F(SRGBTest, DISABLED_RenderToSRGBSurface) {
    ASSERT_NO_FATAL_FAILURE(initShaders());

    // By default, the first buffer we write into will be RGB

    // Render an RGB texture across the whole surface
    ASSERT_NO_FATAL_FAILURE(drawTexture(false, 0, 0,
            DISPLAY_WIDTH, DISPLAY_HEIGHT));
    eglSwapBuffers(mEglDisplay, mEglSurface);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    // Lock
    ASSERT_EQ(NO_ERROR, mCpuConsumer->lockNextBuffer(&mLockedBuffer));
    ASSERT_NO_FATAL_FAILURE(
        checkLockedBuffer(PIXEL_FORMAT_RGBA_8888, HAL_DATASPACE_UNKNOWN));

    // Save the values of the middle pixel for later comparison against SRGB
    uint8_t values[PIXEL_SIZE] = {};
    int middleOffset = (DISPLAY_HEIGHT / 2) * mLockedBuffer.stride *
            PIXEL_SIZE;
    middleOffset += (DISPLAY_WIDTH / 2) * PIXEL_SIZE;
    for (int c = 0; c < PIXEL_SIZE; ++c) {
        values[c] = mLockedBuffer.data[middleOffset + c];
    }

    // Unlock
    ASSERT_EQ(NO_ERROR, mCpuConsumer->unlockBuffer(mLockedBuffer));

    // Switch to SRGB window surface
#define EGL_GL_COLORSPACE_KHR      EGL_VG_COLORSPACE
#define EGL_GL_COLORSPACE_SRGB_KHR EGL_VG_COLORSPACE_sRGB

    static const int srgbAttribs[] = {
        EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR,
        EGL_NONE,
    };

    EXPECT_TRUE(eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
            mEglContext));
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    EXPECT_TRUE(eglDestroySurface(mEglDisplay, mEglSurface));
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    mEglSurface = eglCreateWindowSurface(mEglDisplay, mEglConfig,
            mInputSurface.get(), srgbAttribs);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_SURFACE, mEglSurface);

    EXPECT_TRUE(eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface,
            mEglContext));
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    // Render the texture again
    ASSERT_NO_FATAL_FAILURE(drawTexture(false, 0, 0,
            DISPLAY_WIDTH, DISPLAY_HEIGHT));
    eglSwapBuffers(mEglDisplay, mEglSurface);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());

    // Lock
    ASSERT_EQ(NO_ERROR, mCpuConsumer->lockNextBuffer(&mLockedBuffer));

    // Make sure we actually got the SRGB buffer on the consumer side
    ASSERT_NO_FATAL_FAILURE(
        checkLockedBuffer(PIXEL_FORMAT_RGBA_8888, HAL_DATASPACE_SRGB));

    // Verify that the stored value is the same, accounting for RGB/SRGB
    for (int c = 0; c < PIXEL_SIZE; ++c) {
        // The alpha value should be equivalent before linear->SRGB
        float rgbAsSRGB = (c == 3) ? values[c] / 255.0f :
                linearToSRGB(values[c] / 255.0f);
        int expectedValue = rgbAsSRGB * 255.0f + 0.5f;
        int actualValue = mLockedBuffer.data[middleOffset + c];
        ASSERT_PRED2(withinTolerance, expectedValue, actualValue);
    }

    // mLockedBuffer is unlocked in TearDown so we can copy data from it to
    // the debug surface if necessary
}

} // namespace android
