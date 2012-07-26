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

#ifndef ANDROID_DISPLAY_HARDWARE_H
#define ANDROID_DISPLAY_HARDWARE_H

#include <stdlib.h>

#include <ui/PixelFormat.h>
#include <ui/Region.h>

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <utils/Mutex.h>
#include <utils/Timers.h>

#include "Transform.h"

#include "DisplayHardware/DisplayHardwareBase.h"

namespace android {

class DisplayInfo;
class FramebufferSurface;
class LayerBase;
class SurfaceFlinger;
class SurfaceTextureClient;

class DisplayHardware : public DisplayHardwareBase
{
public:

    enum {
        PARTIAL_UPDATES             = 0x00020000,   // video driver feature
        SWAP_RECTANGLE              = 0x00080000,
    };

    DisplayHardware(
            const sp<SurfaceFlinger>& flinger,
            int dpy,
            const sp<SurfaceTextureClient>& surface,
            EGLConfig config);

    virtual ~DisplayHardware();

    // Flip the front and back buffers if the back buffer is "dirty".  Might
    // be instantaneous, might involve copying the frame buffer around.
    void flip(const Region& dirty) const;

    void onVSyncReceived(nsecs_t timestamp);

    float       getDpiX() const;
    float       getDpiY() const;
    float       getRefreshRate() const;
    float       getDensity() const;
    int         getWidth() const;
    int         getHeight() const;
    PixelFormat getFormat() const;
    uint32_t    getFlags() const;
    nsecs_t     getRefreshPeriod() const;
    nsecs_t     getRefreshTimestamp() const;
    status_t    getInfo(DisplayInfo* info) const;

    EGLSurface  getEGLSurface() const;

    void                    setVisibleLayersSortedByZ(const Vector< sp<LayerBase> >& layers);
    Vector< sp<LayerBase> > getVisibleLayersSortedByZ() const;
    bool                    getSecureLayerVisible() const;

    status_t                setOrientation(int orientation);
    int                     getOrientation() const { return mOrientation; }
    const Transform&        getTransform() const { return mGlobalTransform; }

    uint32_t getPageFlipCount() const;
    EGLDisplay getEGLDisplay() const { return mDisplay; }

    void dump(String8& res) const;

    status_t compositionComplete() const;
    
    Rect getBounds() const {
        return Rect(mDisplayWidth, mDisplayHeight);
    }
    inline Rect bounds() const { return getBounds(); }

private:
    void init(EGLConfig config);

    /*
     *  Constants, set during initialization
     */
    sp<SurfaceFlinger> mFlinger;
    int mDisplayId;
    // ANativeWindow this display is rendering into
    sp<SurfaceTextureClient> mNativeWindow;
    // set if mNativeWindow is a FramebufferSurface
    sp<FramebufferSurface> mFramebufferSurface;


    EGLDisplay      mDisplay;
    EGLSurface      mSurface;
    EGLContext      mContext;
    float           mDpiX;
    float           mDpiY;
    float           mRefreshRate;
    float           mDensity;
    int             mDisplayWidth;
    int             mDisplayHeight;
    PixelFormat     mFormat;
    uint32_t        mFlags;
    mutable uint32_t mPageFlipCount;

    nsecs_t         mRefreshPeriod;

    /*
     * Can only accessed from the main thread, these members
     * don't need synchronization.
     */
    // list of visible layers on that display
    Vector< sp<LayerBase> > mVisibleLayersSortedByZ;
    // Whether we have a visible secure layer on this display
    bool mSecureLayerVisible;


    // this used to be in GraphicPlane
    static status_t orientationToTransfrom(int orientation, int w, int h,
            Transform* tr);
    Transform mGlobalTransform;
    int mOrientation;

    /*
     *  protected by mLock
     */
    mutable Mutex mLock;
    mutable nsecs_t mLastHwVSync;
};

}; // namespace android

#endif // ANDROID_DISPLAY_HARDWARE_H
