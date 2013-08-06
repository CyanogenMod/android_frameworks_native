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

// tag as surfaceflinger
#define LOG_TAG "SurfaceFlinger"

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IMemory.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <gui/BitTube.h>
#include <gui/IDisplayEventConnection.h>
#include <gui/ISurfaceComposer.h>
#include <gui/IGraphicBufferProducer.h>

#include <private/gui/LayerState.h>

#include <ui/DisplayInfo.h>
#include <ui/DisplayStatInfo.h>

#include <utils/Log.h>

// ---------------------------------------------------------------------------

namespace android {

class IDisplayEventConnection;

class BpSurfaceComposer : public BpInterface<ISurfaceComposer>
{
public:
    BpSurfaceComposer(const sp<IBinder>& impl)
        : BpInterface<ISurfaceComposer>(impl)
    {
    }

    virtual sp<ISurfaceComposerClient> createConnection()
    {
        uint32_t n;
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::CREATE_CONNECTION, data, &reply);
        return interface_cast<ISurfaceComposerClient>(reply.readStrongBinder());
    }

    virtual sp<IGraphicBufferAlloc> createGraphicBufferAlloc()
    {
        uint32_t n;
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::CREATE_GRAPHIC_BUFFER_ALLOC, data, &reply);
        return interface_cast<IGraphicBufferAlloc>(reply.readStrongBinder());
    }

    virtual void setTransactionState(
            const Vector<ComposerState>& state,
            const Vector<DisplayState>& displays,
            uint32_t flags)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        {
            Vector<ComposerState>::const_iterator b(state.begin());
            Vector<ComposerState>::const_iterator e(state.end());
            data.writeInt32(state.size());
            for ( ; b != e ; ++b ) {
                b->write(data);
            }
        }
        {
            Vector<DisplayState>::const_iterator b(displays.begin());
            Vector<DisplayState>::const_iterator e(displays.end());
            data.writeInt32(displays.size());
            for ( ; b != e ; ++b ) {
                b->write(data);
            }
        }
        data.writeInt32(flags);
        remote()->transact(BnSurfaceComposer::SET_TRANSACTION_STATE, data, &reply);
    }

    virtual void bootFinished()
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::BOOT_FINISHED, data, &reply);
    }

#ifdef USE_MHEAP_SCREENSHOT
    virtual status_t captureScreen(
            const sp<IBinder>& display, sp<IMemoryHeap>* heap,
            uint32_t* width, uint32_t* height,
            Rect sourceCrop, uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ,
            bool useIdentityTransform,
            ISurfaceComposer::Rotation rotation)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        data.write(sourceCrop);
        data.writeInt32(reqWidth);
        data.writeInt32(reqHeight);
        data.writeInt32(minLayerZ);
        data.writeInt32(maxLayerZ);
        data.writeInt32(static_cast<int32_t>(useIdentityTransform));
        data.writeInt32(static_cast<int32_t>(rotation));
        remote()->transact(BnSurfaceComposer::CAPTURE_SCREEN_DEPRECATED, data, &reply);
        *heap = interface_cast<IMemoryHeap>(reply.readStrongBinder());
        *width = reply.readInt32();
        *height = reply.readInt32();
        return reply.readInt32();
    }
#endif

    virtual status_t captureScreen(const sp<IBinder>& display,
            const sp<IGraphicBufferProducer>& producer,
            Rect sourceCrop, uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ,
            bool useIdentityTransform,
            ISurfaceComposer::Rotation rotation,
            bool isCpuConsumer)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        data.writeStrongBinder(producer->asBinder());
        data.write(sourceCrop);
        data.writeInt32(reqWidth);
        data.writeInt32(reqHeight);
        data.writeInt32(minLayerZ);
        data.writeInt32(maxLayerZ);
        data.writeInt32(static_cast<int32_t>(useIdentityTransform));
        data.writeInt32(static_cast<int32_t>(rotation));
        data.writeInt32(isCpuConsumer);
        remote()->transact(BnSurfaceComposer::CAPTURE_SCREEN, data, &reply);
        return reply.readInt32();
    }

    virtual bool authenticateSurfaceTexture(
            const sp<IGraphicBufferProducer>& bufferProducer) const
    {
        Parcel data, reply;
        int err = NO_ERROR;
        err = data.writeInterfaceToken(
                ISurfaceComposer::getInterfaceDescriptor());
        if (err != NO_ERROR) {
            ALOGE("ISurfaceComposer::authenticateSurfaceTexture: error writing "
                    "interface descriptor: %s (%d)", strerror(-err), -err);
            return false;
        }
        err = data.writeStrongBinder(bufferProducer->asBinder());
        if (err != NO_ERROR) {
            ALOGE("ISurfaceComposer::authenticateSurfaceTexture: error writing "
                    "strong binder to parcel: %s (%d)", strerror(-err), -err);
            return false;
        }
        err = remote()->transact(BnSurfaceComposer::AUTHENTICATE_SURFACE, data,
                &reply);
        if (err != NO_ERROR) {
            ALOGE("ISurfaceComposer::authenticateSurfaceTexture: error "
                    "performing transaction: %s (%d)", strerror(-err), -err);
            return false;
        }
        int32_t result = 0;
        err = reply.readInt32(&result);
        if (err != NO_ERROR) {
            ALOGE("ISurfaceComposer::authenticateSurfaceTexture: error "
                    "retrieving result: %s (%d)", strerror(-err), -err);
            return false;
        }
        return result != 0;
    }

    virtual sp<IDisplayEventConnection> createDisplayEventConnection()
    {
        Parcel data, reply;
        sp<IDisplayEventConnection> result;
        int err = data.writeInterfaceToken(
                ISurfaceComposer::getInterfaceDescriptor());
        if (err != NO_ERROR) {
            return result;
        }
        err = remote()->transact(
                BnSurfaceComposer::CREATE_DISPLAY_EVENT_CONNECTION,
                data, &reply);
        if (err != NO_ERROR) {
            ALOGE("ISurfaceComposer::createDisplayEventConnection: error performing "
                    "transaction: %s (%d)", strerror(-err), -err);
            return result;
        }
        result = interface_cast<IDisplayEventConnection>(reply.readStrongBinder());
        return result;
    }

    virtual sp<IBinder> createDisplay(const String8& displayName, bool secure)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeString8(displayName);
        data.writeInt32(secure ? 1 : 0);
        remote()->transact(BnSurfaceComposer::CREATE_DISPLAY, data, &reply);
        return reply.readStrongBinder();
    }

    virtual void destroyDisplay(const sp<IBinder>& display)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        remote()->transact(BnSurfaceComposer::DESTROY_DISPLAY, data, &reply);
    }

    virtual sp<IBinder> getBuiltInDisplay(int32_t id)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeInt32(id);
        remote()->transact(BnSurfaceComposer::GET_BUILT_IN_DISPLAY, data, &reply);
        return reply.readStrongBinder();
    }

    virtual void setPowerMode(const sp<IBinder>& display, int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        data.writeInt32(mode);
        remote()->transact(BnSurfaceComposer::SET_POWER_MODE, data, &reply);
    }

    virtual status_t getDisplayConfigs(const sp<IBinder>& display,
            Vector<DisplayInfo>* configs)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        remote()->transact(BnSurfaceComposer::GET_DISPLAY_CONFIGS, data, &reply);
        status_t result = reply.readInt32();
        if (result == NO_ERROR) {
            size_t numConfigs = static_cast<size_t>(reply.readInt32());
            configs->clear();
            configs->resize(numConfigs);
            for (size_t c = 0; c < numConfigs; ++c) {
                memcpy(&(configs->editItemAt(c)),
                        reply.readInplace(sizeof(DisplayInfo)),
                        sizeof(DisplayInfo));
            }
        }
        return result;
    }

    virtual status_t getDisplayStats(const sp<IBinder>& display,
            DisplayStatInfo* stats)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        remote()->transact(BnSurfaceComposer::GET_DISPLAY_STATS, data, &reply);
        status_t result = reply.readInt32();
        if (result == NO_ERROR) {
            memcpy(stats,
                    reply.readInplace(sizeof(DisplayStatInfo)),
                    sizeof(DisplayStatInfo));
        }
        return result;
    }

    virtual int getActiveConfig(const sp<IBinder>& display)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        remote()->transact(BnSurfaceComposer::GET_ACTIVE_CONFIG, data, &reply);
        return reply.readInt32();
    }

    virtual status_t setActiveConfig(const sp<IBinder>& display, int id)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        data.writeInt32(id);
        remote()->transact(BnSurfaceComposer::SET_ACTIVE_CONFIG, data, &reply);
        return reply.readInt32();
    }

    virtual status_t clearAnimationFrameStats() {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::CLEAR_ANIMATION_FRAME_STATS, data, &reply);
        return reply.readInt32();
    }

    virtual status_t getAnimationFrameStats(FrameStats* outStats) const {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::GET_ANIMATION_FRAME_STATS, data, &reply);
        reply.read(*outStats);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(SurfaceComposer, "android.ui.ISurfaceComposer");

// ----------------------------------------------------------------------

status_t BnSurfaceComposer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case CREATE_CONNECTION: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> b = createConnection()->asBinder();
            reply->writeStrongBinder(b);
            return NO_ERROR;
        }
        case CREATE_GRAPHIC_BUFFER_ALLOC: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> b = createGraphicBufferAlloc()->asBinder();
            reply->writeStrongBinder(b);
            return NO_ERROR;
        }
        case SET_TRANSACTION_STATE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            size_t count = data.readInt32();
            if (count > data.dataSize()) {
                return BAD_VALUE;
            }
            ComposerState s;
            Vector<ComposerState> state;
            state.setCapacity(count);
            for (size_t i=0 ; i<count ; i++) {
                if (s.read(data) == BAD_VALUE) {
                    return BAD_VALUE;
                }
                state.add(s);
            }
            count = data.readInt32();
            if (count > data.dataSize()) {
                return BAD_VALUE;
            }
            DisplayState d;
            Vector<DisplayState> displays;
            displays.setCapacity(count);
            for (size_t i=0 ; i<count ; i++) {
                if (d.read(data) == BAD_VALUE) {
                    return BAD_VALUE;
                }
                displays.add(d);
            }
            uint32_t flags = data.readInt32();
            setTransactionState(state, displays, flags);
            return NO_ERROR;
        }
        case BOOT_FINISHED: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            bootFinished();
            return NO_ERROR;
        }
#ifdef USE_MHEAP_SCREENSHOT
        case CAPTURE_SCREEN_DEPRECATED: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            Rect sourceCrop;
            data.read(sourceCrop);
            uint32_t reqWidth = data.readInt32();
            uint32_t reqHeight = data.readInt32();
            uint32_t minLayerZ = data.readInt32();
            uint32_t maxLayerZ = data.readInt32();
            bool useIdentityTransform = static_cast<bool>(data.readInt32());
            uint32_t rotation = data.readInt32();
            sp<IMemoryHeap> heap;
            uint32_t w, h;
            status_t res = captureScreen(display, &heap, &w, &h,
                    sourceCrop, reqWidth, reqHeight, minLayerZ, maxLayerZ,
                    useIdentityTransform,
                    static_cast<ISurfaceComposer::Rotation>(rotation));
            reply->writeStrongBinder(heap->asBinder());
            reply->writeInt32(w);
            reply->writeInt32(h);
            reply->writeInt32(res);
            return NO_ERROR;
        }
#endif
        case CAPTURE_SCREEN: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            sp<IGraphicBufferProducer> producer =
                    interface_cast<IGraphicBufferProducer>(data.readStrongBinder());
            Rect sourceCrop;
            data.read(sourceCrop);
            uint32_t reqWidth = data.readInt32();
            uint32_t reqHeight = data.readInt32();
            uint32_t minLayerZ = data.readInt32();
            uint32_t maxLayerZ = data.readInt32();
            bool useIdentityTransform = static_cast<bool>(data.readInt32());
            uint32_t rotation = data.readInt32();
            bool isCpuConsumer = data.readInt32();

            status_t res = captureScreen(display, producer,
                    sourceCrop, reqWidth, reqHeight, minLayerZ, maxLayerZ,
                    useIdentityTransform,
                    static_cast<ISurfaceComposer::Rotation>(rotation),
                    isCpuConsumer);
            reply->writeInt32(res);
            return NO_ERROR;
        }
        case AUTHENTICATE_SURFACE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IGraphicBufferProducer> bufferProducer =
                    interface_cast<IGraphicBufferProducer>(data.readStrongBinder());
            int32_t result = authenticateSurfaceTexture(bufferProducer) ? 1 : 0;
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case CREATE_DISPLAY_EVENT_CONNECTION: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IDisplayEventConnection> connection(createDisplayEventConnection());
            reply->writeStrongBinder(connection->asBinder());
            return NO_ERROR;
        }
        case CREATE_DISPLAY: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            String8 displayName = data.readString8();
            bool secure = bool(data.readInt32());
            sp<IBinder> display(createDisplay(displayName, secure));
            reply->writeStrongBinder(display);
            return NO_ERROR;
        }
        case DESTROY_DISPLAY: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            destroyDisplay(display);
            return NO_ERROR;
        }
        case GET_BUILT_IN_DISPLAY: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            int32_t id = data.readInt32();
            sp<IBinder> display(getBuiltInDisplay(id));
            reply->writeStrongBinder(display);
            return NO_ERROR;
        }
        case GET_DISPLAY_CONFIGS: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            Vector<DisplayInfo> configs;
            sp<IBinder> display = data.readStrongBinder();
            status_t result = getDisplayConfigs(display, &configs);
            reply->writeInt32(result);
            if (result == NO_ERROR) {
                reply->writeInt32(static_cast<int32_t>(configs.size()));
                for (size_t c = 0; c < configs.size(); ++c) {
                    memcpy(reply->writeInplace(sizeof(DisplayInfo)),
                            &configs[c], sizeof(DisplayInfo));
                }
            }
            return NO_ERROR;
        }
        case GET_DISPLAY_STATS: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            DisplayStatInfo stats;
            sp<IBinder> display = data.readStrongBinder();
            status_t result = getDisplayStats(display, &stats);
            reply->writeInt32(result);
            if (result == NO_ERROR) {
                memcpy(reply->writeInplace(sizeof(DisplayStatInfo)),
                        &stats, sizeof(DisplayStatInfo));
            }
            return NO_ERROR;
        }
        case GET_ACTIVE_CONFIG: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            int id = getActiveConfig(display);
            reply->writeInt32(id);
            return NO_ERROR;
        }
        case SET_ACTIVE_CONFIG: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            int id = data.readInt32();
            status_t result = setActiveConfig(display, id);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case CLEAR_ANIMATION_FRAME_STATS: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            status_t result = clearAnimationFrameStats();
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case GET_ANIMATION_FRAME_STATS: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            FrameStats stats;
            status_t result = getAnimationFrameStats(&stats);
            reply->write(stats);
            reply->writeInt32(result);
            return NO_ERROR;
        }
        case SET_POWER_MODE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            int32_t mode = data.readInt32();
            setPowerMode(display, mode);
            return NO_ERROR;
        }
        default: {
            return BBinder::onTransact(code, data, reply, flags);
        }
    }
    // should be unreachable
    return NO_ERROR;
}

// ----------------------------------------------------------------------------

};
