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
    : mFlinger(flinger), mNameGenerator(1)
{
}

Client::~Client()
{
    const size_t count = mLayers.size();
    for (size_t i=0 ; i<count ; i++) {
        sp<LayerBaseClient> layer(mLayers.valueAt(i).promote());
        if (layer != 0) {
            mFlinger->removeLayer(layer);
        }
    }
}

status_t Client::initCheck() const {
    return NO_ERROR;
}

size_t Client::attachLayer(const sp<LayerBaseClient>& layer)
{
    Mutex::Autolock _l(mLock);
    size_t name = mNameGenerator++;
    mLayers.add(name, layer);
    return name;
}

void Client::detachLayer(const LayerBaseClient* layer)
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
sp<LayerBaseClient> Client::getLayerUser(int32_t i) const
{
    Mutex::Autolock _l(mLock);
    sp<LayerBaseClient> lbc;
    wp<LayerBaseClient> layer(mLayers.valueFor(i));
    if (layer != 0) {
        lbc = layer.promote();
        ALOGE_IF(lbc==0, "getLayerUser(name=%d) is dead", int(i));
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
        ISurfaceComposerClient::surface_data_t* params,
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
        ISurfaceComposerClient::surface_data_t* params;
        Client* client;
        const String8& name;
        uint32_t w, h;
        PixelFormat format;
        uint32_t flags;
    public:
        MessageCreateLayer(SurfaceFlinger* flinger,
                ISurfaceComposerClient::surface_data_t* params,
                const String8& name, Client* client,
                uint32_t w, uint32_t h, PixelFormat format,
                uint32_t flags)
            : flinger(flinger), params(params), client(client), name(name),
              w(w), h(h), format(format), flags(flags)
        {
        }
        sp<ISurface> getResult() const { return result; }
        virtual bool handler() {
            result = flinger->createLayer(params, name, client,
                    w, h, format, flags);
            return true;
        }
    };

    sp<MessageBase> msg = new MessageCreateLayer(mFlinger.get(),
            params, name, this, w, h, format, flags);
    mFlinger->postMessageSync(msg);
    return static_cast<MessageCreateLayer*>( msg.get() )->getResult();
}
status_t Client::destroySurface(SurfaceID sid) {
    return mFlinger->onLayerRemoved(this, sid);
}

// ---------------------------------------------------------------------------
}; // namespace android
