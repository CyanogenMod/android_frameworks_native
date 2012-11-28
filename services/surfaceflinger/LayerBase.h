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

#ifndef ANDROID_LAYER_BASE_H
#define ANDROID_LAYER_BASE_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>

#include <utils/RefBase.h>
#include <utils/String8.h>

#include <ui/Region.h>

#include <gui/ISurfaceComposerClient.h>

#include <private/gui/LayerState.h>

#include "Transform.h"
#include "DisplayHardware/HWComposer.h"

namespace android {

// ---------------------------------------------------------------------------

class Client;
class DisplayDevice;
class GraphicBuffer;
class Layer;
class LayerBaseClient;
class SurfaceFlinger;

// ---------------------------------------------------------------------------

class LayerBase : public RefBase
{
    static int32_t sSequence;

public:
            LayerBase(SurfaceFlinger* flinger);

    mutable bool        contentDirty;
            // regions below are in window-manager space
            Region      visibleRegion;
            Region      coveredRegion;
            Region      visibleNonTransparentRegion;
            int32_t     sequence;
            
            struct Geometry {
                uint32_t w;
                uint32_t h;
                Rect crop;
                inline bool operator == (const Geometry& rhs) const {
                    return (w==rhs.w && h==rhs.h && crop==rhs.crop);
                }
                inline bool operator != (const Geometry& rhs) const {
                    return !operator == (rhs);
                }
            };

            struct State {
                Geometry        active;
                Geometry        requested;
                uint32_t        z;
                uint32_t        layerStack;
                uint8_t         alpha;
                uint8_t         flags;
                uint8_t         reserved[2];
                int32_t         sequence;   // changes when visible regions can change
                Transform       transform;
                Region          transparentRegion;
            };

            class LayerMesh {
                friend class LayerBase;
                GLfloat mVertices[4][2];
                size_t mNumVertices;
            public:
                LayerMesh() : mNumVertices(4) { }
                GLfloat const* getVertices() const {
                    return &mVertices[0][0];
                }
                size_t getVertexCount() const {
                    return mNumVertices;
                }
            };

    virtual void setName(const String8& name);
            String8 getName() const;

            // modify current state
            bool setPosition(float x, float y);
            bool setLayer(uint32_t z);
            bool setSize(uint32_t w, uint32_t h);
            bool setAlpha(uint8_t alpha);
            bool setMatrix(const layer_state_t::matrix22_t& matrix);
            bool setTransparentRegionHint(const Region& transparent);
            bool setFlags(uint8_t flags, uint8_t mask);
            bool setCrop(const Rect& crop);
            bool setLayerStack(uint32_t layerStack);

            void commitTransaction();
            bool requestTransaction();
            void forceVisibilityTransaction();
            
            uint32_t getTransactionFlags(uint32_t flags);
            uint32_t setTransactionFlags(uint32_t flags);

            void computeGeometry(const sp<const DisplayDevice>& hw, LayerMesh* mesh) const;
            Rect computeBounds() const;


    virtual sp<LayerBaseClient> getLayerBaseClient() const;
    virtual sp<Layer> getLayer() const;

    virtual const char* getTypeId() const { return "LayerBase"; }

    virtual void setGeometry(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);
    virtual void setPerFrameData(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);
    virtual void setAcquireFence(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface& layer);

    /**
     * draw - performs some global clipping optimizations
     * and calls onDraw().
     * Typically this method is not overridden, instead implement onDraw()
     * to perform the actual drawing.  
     */
    virtual void draw(const sp<const DisplayDevice>& hw, const Region& clip) const;
    virtual void draw(const sp<const DisplayDevice>& hw);
    
    /**
     * onDraw - draws the surface.
     */
    virtual void onDraw(const sp<const DisplayDevice>& hw, const Region& clip) const = 0;
    
    /**
     * initStates - called just after construction
     */
    virtual void initStates(uint32_t w, uint32_t h, uint32_t flags);
    
    /**
     * doTransaction - process the transaction. This is a good place to figure
     * out which attributes of the surface have changed.
     */
    virtual uint32_t doTransaction(uint32_t transactionFlags);
    
    /**
     * setVisibleRegion - called to set the new visible region. This gives
     * a chance to update the new visible region or record the fact it changed.
     */
    virtual void setVisibleRegion(const Region& visibleRegion);
    
    /**
     * setCoveredRegion - called when the covered region changes. The covered
     * region corresponds to any area of the surface that is covered
     * (transparently or not) by another surface.
     */
    virtual void setCoveredRegion(const Region& coveredRegion);

    /**
     * setVisibleNonTransparentRegion - called when the visible and
     * non-transparent region changes.
     */
    virtual void setVisibleNonTransparentRegion(const Region&
            visibleNonTransparentRegion);

    /**
     * latchBuffer - called each time the screen is redrawn and returns whether
     * the visible regions need to be recomputed (this is a fairly heavy
     * operation, so this should be set only if needed). Typically this is used
     * to figure out if the content or size of a surface has changed.
     */
    virtual Region latchBuffer(bool& recomputeVisibleRegions);

    /**
     * isOpaque - true if this surface is opaque
     */
    virtual bool isOpaque() const  { return true; }

    /**
     * needsDithering - true if this surface needs dithering
     */
    virtual bool needsDithering() const { return false; }

    /**
     * needsLinearFiltering - true if this surface's state requires filtering
     */
    virtual bool needsFiltering(const sp<const DisplayDevice>& hw) const;

    /**
     * isSecure - true if this surface is secure, that is if it prevents
     * screenshots or VNC servers.
     */
    virtual bool isSecure() const       { return false; }

    /**
     * isProtected - true if the layer may contain protected content in the
     * GRALLOC_USAGE_PROTECTED sense.
     */
    virtual bool isProtected() const   { return false; }

    /*
     * isVisible - true if this layer is visibile, false otherwise
     */
    virtual bool isVisible() const;

    /** called with the state lock when the surface is removed from the
     *  current list */
    virtual void onRemoved() { }

    /** called after page-flip
     */
    virtual void onLayerDisplayed(const sp<const DisplayDevice>& hw,
            HWComposer::HWCLayerInterface* layer);

    /** called before composition.
     * returns true if the layer has pending updates.
     */
    virtual bool onPreComposition() { return false; }

    /** called before composition.
     */
    virtual void onPostComposition() { }

    /**
     * Updates the SurfaceTexture's transform hint, for layers that have
     * a SurfaceTexture.
     */
    virtual void updateTransformHint(const sp<const DisplayDevice>& hw) const { }

    /** always call base class first */
    virtual void dump(String8& result, char* scratch, size_t size) const;
    virtual void shortDump(String8& result, char* scratch, size_t size) const;
    virtual void dumpStats(String8& result, char* buffer, size_t SIZE) const;
    virtual void clearStats();


    enum { // flags for doTransaction()
        eDontUpdateGeometryState = 0x00000001,
        eVisibleRegion           = 0x00000002,
    };


    inline  const State&    drawingState() const    { return mDrawingState; }
    inline  const State&    currentState() const    { return mCurrentState; }
    inline  State&          currentState()          { return mCurrentState; }

    void clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip) const;

    void setFiltering(bool filtering);
    bool getFiltering() const;

protected:
          void clearWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip,
                  GLclampf r, GLclampf g, GLclampf b, GLclampf alpha) const;
          void drawWithOpenGL(const sp<const DisplayDevice>& hw, const Region& clip) const;

                sp<SurfaceFlinger> mFlinger;

private:
                // accessed only in the main thread
                // Whether filtering is forced on or not
                bool            mFiltering;

                // Whether filtering is needed b/c of the drawingstate
                bool            mNeedsFiltering;

protected:
                // these are protected by an external lock
                State           mCurrentState;
                State           mDrawingState;
    volatile    int32_t         mTransactionFlags;

                // don't change, don't need a lock
                bool            mPremultipliedAlpha;
                String8         mName;
    mutable     bool            mDebug;


public:
    // called from class SurfaceFlinger
    virtual ~LayerBase();

private:
    LayerBase(const LayerBase& rhs);
};


// ---------------------------------------------------------------------------

class LayerBaseClient : public LayerBase
{
public:
            LayerBaseClient(SurfaceFlinger* flinger, const sp<Client>& client);

            virtual ~LayerBaseClient();

            sp<ISurface> getSurface();
            wp<IBinder> getSurfaceBinder() const;
            virtual wp<IBinder> getSurfaceTextureBinder() const;

    virtual sp<LayerBaseClient> getLayerBaseClient() const {
        return const_cast<LayerBaseClient*>(this); }

    virtual const char* getTypeId() const { return "LayerBaseClient"; }

    uint32_t getIdentity() const { return mIdentity; }

protected:
    virtual void dump(String8& result, char* scratch, size_t size) const;
    virtual void shortDump(String8& result, char* scratch, size_t size) const;

    class LayerCleaner {
        sp<SurfaceFlinger> mFlinger;
        wp<LayerBaseClient> mLayer;
    protected:
        ~LayerCleaner();
    public:
        LayerCleaner(const sp<SurfaceFlinger>& flinger,
                const sp<LayerBaseClient>& layer);
    };

private:
    virtual sp<ISurface> createSurface();

    mutable Mutex mLock;
    mutable bool mHasSurface;
    wp<IBinder> mClientSurfaceBinder;
    const wp<Client> mClientRef;
    // only read
    const uint32_t mIdentity;
    static int32_t sIdentity;
};

// ---------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_LAYER_BASE_H
