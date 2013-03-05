/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <stdint.h>
#include <sys/types.h>

#include <binder/PermissionCache.h>

#include <private/android_filesystem_config.h>

#include "Client.h"
#include "Layer.h"
#include "LayerBase.h"
#include "SurfaceFlinger.h"

namespace android {

// ---------------------------------------------------------------------------

const String16 sAccessSurfaceFlinger("android.permission.ACCESS_SURFACE_FLINGER");

// ---------------------------------------------------------------------------

Client::Client(const sp<SurfaceFlinger>& flinger)
    : mFlinger(flinger)
{
}

Client::~Client()
{
    const size_t count = mLayers.size();
    for (size_t i=0 ; i<count ; i++) {
        sp<LayerBase> layer(mLayers.valueAt(i).promote());
        if (layer != 0) {
            mFlinger->removeLayer(layer);
        }
    }
}

status_t Client::initCheck() const {
    return NO_ERROR;
}

void Client::attachLayer(const sp<IBinder>& handle, const sp<LayerBase>& layer)
{
    Mutex::Autolock _l(mLock);
    mLayers.add(handle, layer);
}

void Client::detachLayer(const LayerBase* layer)
{
    Mutex::Autolock _l(mLock);
    // we do a linear search here, because this doesn't happen often
    const size_t count = mLayers.size();
    for (size_t i=0 ; i<count ; i++) {
        if (mLayers.valueAt(i) == layer) {
            mLayers.removeItemsAt(i, 1);
            break;
        }
    }
}
sp<LayerBase> Client::getLayerUser(const sp<IBinder>& handle) const
{
    Mutex::Autolock _l(mLock);
    sp<LayerBase> lbc;
    wp<LayerBase> layer(mLayers.valueFor(handle));
    if (layer != 0) {
        lbc = layer.promote();
        ALOGE_IF(lbc==0, "getLayerUser(name=%p) is dead", handle.get());
    }
    return lbc;
}


status_t Client::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    // these must be checked
     IPCThreadState* ipc = IPCThreadState::self();
     const int pid = ipc->getCallingPid();
     const int uid = ipc->getCallingUid();
     const int self_pid = getpid();
     if (CC_UNLIKELY(pid != self_pid && uid != AID_GRAPHICS && uid != 0)) {
         // we're called from a different process, do the real check
         if (!PermissionCache::checkCallingPermission(sAccessSurfaceFlinger))
         {
             ALOGE("Permission Denial: "
                     "can't openGlobalTransaction pid=%d, uid=%d", pid, uid);
             return PERMISSION_DENIED;
         }
     }
     return BnSurfaceComposerClient::onTransact(code, data, reply, flags);
}


sp<ISurface> Client::createSurface(
        const String8& name,
        uint32_t w, uint32_t h, PixelFormat format,
        uint32_t flags)
{
    /*
     * createSurface must be called from the GL thread so that it can
     * have access to the GL context.
     */

    class MessageCreateLayer : public MessageBase {
        sp<ISurface> result;
        SurfaceFlinger* flinger;
        Client* client;
        const String8& name;
        uint32_t w, h;
        PixelFormat format;
        uint32_t flags;
    public:
        MessageCreateLayer(SurfaceFlinger* flinger,
                const String8& name, Client* client,
                uint32_t w, uint32_t h, PixelFormat format,
                uint32_t flags)
            : flinger(flinger), client(client), name(name),
              w(w), h(h), format(format), flags(flags)
        {
        }
        sp<ISurface> getResult() const { return result; }
        virtual bool handler() {
            result = flinger->createLayer(name, client, w, h, format, flags);
            return true;
        }
    };

    sp<MessageBase> msg = new MessageCreateLayer(mFlinger.get(),
            name, this, w, h, format, flags);
    mFlinger->postMessageSync(msg);
    return static_cast<MessageCreateLayer*>( msg.get() )->getResult();
}

status_t Client::destroySurface(const sp<IBinder>& handle) {
    return mFlinger->onLayerRemoved(this, handle);
}

// ---------------------------------------------------------------------------
}; // namespace android
