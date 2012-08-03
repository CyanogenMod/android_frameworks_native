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

#ifndef ANDROID_DISPLAY_DEVICE_H
#define ANDROID_DISPLAY_DEVICE_H

#include <stdlib.h>

#include <ui/PixelFormat.h>
#include <ui/Region.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <utils/Mutex.h>
#include <utils/Timers.h>

#include "Transform.h"

struct ANativeWindow;

namespace android {

class DisplayInfo;
class FramebufferSurface;
class LayerBase;
class SurfaceFlinger;

class DisplayDevice : public LightRefBase<DisplayDevice>
{
public:
    // region in layer-stack space
    mutable Region dirtyRegion;
    // region in screen space
    mutable Region swapRegion;
    // region in screen space
    Region undefinedRegion;

    enum {
        DISPLAY_ID_MAIN = 0,
        DISPLAY_ID_HDMI = 1
    };

    enum {
        PARTIAL_UPDATES = 0x00020000, // video driver feature
        SWAP_RECTANGLE  = 0x00080000,
    };

    DisplayDevice(
            const sp<SurfaceFlinger>& flinger,
            int dpy,
            const sp<ANativeWindow>& nativeWindow,
            const sp<FramebufferSurface>& framebufferSurface,
            EGLConfig config);

    ~DisplayDevice();

    // whether this is a valid object. An invalid DisplayDevice is returned
    // when an non existing id is requested
    bool isValid() const;

    // Flip the front and back buffers if the back buffer is "dirty".  Might
    // be instantaneous, might involve copying the frame buffer around.
    void flip(const Region& dirty) const;

    float       getDpiX() const;
    float       getDpiY() const;
    float       getDensity() const;
    int         getWidth() const;
    int         getHeight() const;
    PixelFormat getFormat() const;
    uint32_t    getFlags() const;

    EGLSurface  getEGLSurface() const;

    void                    setVisibleLayersSortedByZ(const Vector< sp<LayerBase> >& layers);
    Vector< sp<LayerBase> > getVisibleLayersSortedByZ() const;
    bool                    getSecureLayerVisible() const;

    status_t                setOrientation(int orientation);
    int                     getOrientation() const { return mOrientation; }
    const Transform&        getTransform() const { return mGlobalTransform; }
    uint32_t                getLayerStack() const { return mLayerStack; }

    status_t compositionComplete() const;
    
    Rect getBounds() const {
        return Rect(mDisplayWidth, mDisplayHeight);
    }
    inline Rect bounds() const { return getBounds(); }

    static void makeCurrent(const sp<const DisplayDevice>& hw, EGLContext ctx);

    /* ------------------------------------------------------------------------
     * blank / unplank management
     */
    void releaseScreen() const;
    void acquireScreen() const;
    bool isScreenAcquired() const;
    bool canDraw() const;

    /* ------------------------------------------------------------------------
     * Debugging
     */
    uint32_t getPageFlipCount() const;
    void dump(String8& res) const;

    inline bool operator < (const DisplayDevice& rhs) const {
        return mId < rhs.mId;
    }

private:
    void init(EGLConfig config);

    /*
     *  Constants, set during initialization
     */
    sp<SurfaceFlinger> mFlinger;
    int32_t mId;

    // ANativeWindow this display is rendering into
    sp<ANativeWindow> mNativeWindow;

    // set if mNativeWindow is a FramebufferSurface
    sp<FramebufferSurface> mFramebufferSurface;

    EGLDisplay      mDisplay;
    EGLSurface      mSurface;
    EGLContext      mContext;
    float           mDpiX;
    float           mDpiY;
    float           mDensity;
    int             mDisplayWidth;
    int             mDisplayHeight;
    PixelFormat     mFormat;
    uint32_t        mFlags;
    mutable uint32_t mPageFlipCount;

    /*
     * Can only accessed from the main thread, these members
     * don't need synchronization.
     */

    // list of visible layers on that display
    Vector< sp<LayerBase> > mVisibleLayersSortedByZ;

    // Whether we have a visible secure layer on this display
    bool mSecureLayerVisible;

    // Whether the screen is blanked;
    mutable int mScreenAcquired;


    /*
     * Transaction state
     */
    static status_t orientationToTransfrom(int orientation, int w, int h,
            Transform* tr);
    Transform mGlobalTransform;
    int mOrientation;
    uint32_t mLayerStack;
};

}; // namespace android

#endif // ANDROID_DISPLAY_DEVICE_H
