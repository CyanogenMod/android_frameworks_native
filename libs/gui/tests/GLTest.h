#ifndef ANDROID_GL_TEST_H
#define ANDROID_GL_TEST_H

#include <gtest/gtest.h>

#include <gui/SurfaceComposerClient.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

namespace android {

class GLTest : public ::testing::Test {
protected:
    GLTest() :
            mEglDisplay(EGL_NO_DISPLAY),
            mEglSurface(EGL_NO_SURFACE),
            mEglContext(EGL_NO_CONTEXT) {
    }

    virtual void SetUp();
    virtual void TearDown();

    virtual EGLint const* getConfigAttribs();
    virtual EGLint const* getContextAttribs();
    virtual EGLint getSurfaceWidth();
    virtual EGLint getSurfaceHeight();
    virtual EGLSurface createWindowSurface(EGLDisplay display, EGLConfig config,
                                           sp<ANativeWindow>& window) const;

    ::testing::AssertionResult checkPixel(int x, int y,
            int r, int g, int b, int a, int tolerance = 2);
    ::testing::AssertionResult assertRectEq(const Rect &r1, const Rect &r2,
            int tolerance = 1);

    static void loadShader(GLenum shaderType, const char* pSource,
            GLuint* outShader);
    static void createProgram(const char* pVertexSource,
            const char* pFragmentSource, GLuint* outPgm);

    int mDisplaySecs;
    sp<SurfaceComposerClient> mComposerClient;
    sp<SurfaceControl> mSurfaceControl;

    EGLDisplay mEglDisplay;
    EGLSurface mEglSurface;
    EGLContext mEglContext;
    EGLConfig  mGlConfig;
};

} // namespace android

#endif
