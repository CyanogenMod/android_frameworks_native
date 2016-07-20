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

#include <system/graphics.h>

#include <ui/DisplayInfo.h>
#include <ui/DisplayStatInfo.h>
#include <ui/HdrCapabilities.h>

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

    virtual ~BpSurfaceComposer();

    virtual sp<ISurfaceComposerClient> createConnection()
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::CREATE_CONNECTION, data, &reply);
        return interface_cast<ISurfaceComposerClient>(reply.readStrongBinder());
    }

    virtual sp<IGraphicBufferAlloc> createGraphicBufferAlloc()
    {
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

        data.writeUint32(static_cast<uint32_t>(state.size()));
        for (const auto& s : state) {
            s.write(data);
        }

        data.writeUint32(static_cast<uint32_t>(displays.size()));
        for (const auto& d : displays) {
            d.write(data);
        }

        data.writeUint32(flags);
        remote()->transact(BnSurfaceComposer::SET_TRANSACTION_STATE, data, &reply);
    }

    virtual void bootFinished()
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        remote()->transact(BnSurfaceComposer::BOOT_FINISHED, data, &reply);
    }

    virtual status_t captureScreen(const sp<IBinder>& display,
            const sp<IGraphicBufferProducer>& producer,
            Rect sourceCrop, uint32_t reqWidth, uint32_t reqHeight,
            uint32_t minLayerZ, uint32_t maxLayerZ,
            bool useIdentityTransform,
            ISurfaceComposer::Rotation rotation)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        data.writeStrongBinder(display);
        data.writeStrongBinder(IInterface::asBinder(producer));
        data.write(sourceCrop);
        data.writeUint32(reqWidth);
        data.writeUint32(reqHeight);
        data.writeUint32(minLayerZ);
        data.writeUint32(maxLayerZ);
        data.writeInt32(static_cast<int32_t>(useIdentityTransform));
        data.writeInt32(static_cast<int32_t>(rotation));
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
        err = data.writeStrongBinder(IInterface::asBinder(bufferProducer));
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
            size_t numConfigs = reply.readUint32();
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

    virtual status_t getDisplayColorModes(const sp<IBinder>& display,
            Vector<android_color_mode_t>* outColorModes) {
        Parcel data, reply;
        status_t result = data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        if (result != NO_ERROR) {
            ALOGE("getDisplayColorModes failed to writeInterfaceToken: %d", result);
            return result;
        }
        result = data.writeStrongBinder(display);
        if (result != NO_ERROR) {
            ALOGE("getDisplayColorModes failed to writeStrongBinder: %d", result);
            return result;
        }
        result = remote()->transact(BnSurfaceComposer::GET_DISPLAY_COLOR_MODES, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getDisplayColorModes failed to transact: %d", result);
            return result;
        }
        result = static_cast<status_t>(reply.readInt32());
        if (result == NO_ERROR) {
            size_t numModes = reply.readUint32();
            outColorModes->clear();
            outColorModes->resize(numModes);
            for (size_t i = 0; i < numModes; ++i) {
                outColorModes->replaceAt(static_cast<android_color_mode_t>(reply.readInt32()), i);
            }
        }
        return result;
    }

    virtual android_color_mode_t getActiveColorMode(const sp<IBinder>& display) {
        Parcel data, reply;
        status_t result = data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        if (result != NO_ERROR) {
            ALOGE("getActiveColorMode failed to writeInterfaceToken: %d", result);
            return static_cast<android_color_mode_t>(result);
        }
        result = data.writeStrongBinder(display);
        if (result != NO_ERROR) {
            ALOGE("getActiveColorMode failed to writeStrongBinder: %d", result);
            return static_cast<android_color_mode_t>(result);
        }
        result = remote()->transact(BnSurfaceComposer::GET_ACTIVE_COLOR_MODE, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getActiveColorMode failed to transact: %d", result);
            return static_cast<android_color_mode_t>(result);
        }
        return static_cast<android_color_mode_t>(reply.readInt32());
    }

    virtual status_t setActiveColorMode(const sp<IBinder>& display,
            android_color_mode_t colorMode) {
        Parcel data, reply;
        status_t result = data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        if (result != NO_ERROR) {
            ALOGE("setActiveColorMode failed to writeInterfaceToken: %d", result);
            return result;
        }
        result = data.writeStrongBinder(display);
        if (result != NO_ERROR) {
            ALOGE("setActiveColorMode failed to writeStrongBinder: %d", result);
            return result;
        }
        result = data.writeInt32(colorMode);
        if (result != NO_ERROR) {
            ALOGE("setActiveColorMode failed to writeInt32: %d", result);
            return result;
        }
        result = remote()->transact(BnSurfaceComposer::SET_ACTIVE_COLOR_MODE, data, &reply);
        if (result != NO_ERROR) {
            ALOGE("setActiveColorMode failed to transact: %d", result);
            return result;
        }
        return static_cast<status_t>(reply.readInt32());
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

    virtual status_t getHdrCapabilities(const sp<IBinder>& display,
            HdrCapabilities* outCapabilities) const {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposer::getInterfaceDescriptor());
        status_t result = data.writeStrongBinder(display);
        if (result != NO_ERROR) {
            ALOGE("getHdrCapabilities failed to writeStrongBinder: %d", result);
            return result;
        }
        result = remote()->transact(BnSurfaceComposer::GET_HDR_CAPABILITIES,
                data, &reply);
        if (result != NO_ERROR) {
            ALOGE("getHdrCapabilities failed to transact: %d", result);
            return result;
        }
        result = reply.readInt32();
        if (result == NO_ERROR) {
            result = reply.readParcelable(outCapabilities);
        }
        return result;
    }
};

// Out-of-line virtual method definition to trigger vtable emission in this
// translation unit (see clang warning -Wweak-vtables)
BpSurfaceComposer::~BpSurfaceComposer() {}

IMPLEMENT_META_INTERFACE(SurfaceComposer, "android.ui.ISurfaceComposer");

// ----------------------------------------------------------------------

status_t BnSurfaceComposer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case CREATE_CONNECTION: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> b = IInterface::asBinder(createConnection());
            reply->writeStrongBinder(b);
            return NO_ERROR;
        }
        case CREATE_GRAPHIC_BUFFER_ALLOC: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> b = IInterface::asBinder(createGraphicBufferAlloc());
            reply->writeStrongBinder(b);
            return NO_ERROR;
        }
        case SET_TRANSACTION_STATE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);

            size_t count = data.readUint32();
            if (count > data.dataSize()) {
                return BAD_VALUE;
            }
            ComposerState s;
            Vector<ComposerState> state;
            state.setCapacity(count);
            for (size_t i = 0; i < count; i++) {
                if (s.read(data) == BAD_VALUE) {
                    return BAD_VALUE;
                }
                state.add(s);
            }

            count = data.readUint32();
            if (count > data.dataSize()) {
                return BAD_VALUE;
            }
            DisplayState d;
            Vector<DisplayState> displays;
            displays.setCapacity(count);
            for (size_t i = 0; i < count; i++) {
                if (d.read(data) == BAD_VALUE) {
                    return BAD_VALUE;
                }
                displays.add(d);
            }

            uint32_t stateFlags = data.readUint32();
            setTransactionState(state, displays, stateFlags);
            return NO_ERROR;
        }
        case BOOT_FINISHED: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            bootFinished();
            return NO_ERROR;
        }
        case CAPTURE_SCREEN: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = data.readStrongBinder();
            sp<IGraphicBufferProducer> producer =
                    interface_cast<IGraphicBufferProducer>(data.readStrongBinder());
            Rect sourceCrop(Rect::EMPTY_RECT);
            data.read(sourceCrop);
            uint32_t reqWidth = data.readUint32();
            uint32_t reqHeight = data.readUint32();
            uint32_t minLayerZ = data.readUint32();
            uint32_t maxLayerZ = data.readUint32();
            bool useIdentityTransform = static_cast<bool>(data.readInt32());
            int32_t rotation = data.readInt32();

            status_t res = captureScreen(display, producer,
                    sourceCrop, reqWidth, reqHeight, minLayerZ, maxLayerZ,
                    useIdentityTransform,
                    static_cast<ISurfaceComposer::Rotation>(rotation));
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
            reply->writeStrongBinder(IInterface::asBinder(connection));
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
                reply->writeUint32(static_cast<uint32_t>(configs.size()));
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
        case GET_DISPLAY_COLOR_MODES: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            Vector<android_color_mode_t> colorModes;
            sp<IBinder> display = nullptr;
            status_t result = data.readStrongBinder(&display);
            if (result != NO_ERROR) {
                ALOGE("getDisplayColorModes failed to readStrongBinder: %d", result);
                return result;
            }
            result = getDisplayColorModes(display, &colorModes);
            reply->writeInt32(result);
            if (result == NO_ERROR) {
                reply->writeUint32(static_cast<uint32_t>(colorModes.size()));
                for (size_t i = 0; i < colorModes.size(); ++i) {
                    reply->writeInt32(colorModes[i]);
                }
            }
            return NO_ERROR;
        }
        case GET_ACTIVE_COLOR_MODE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = nullptr;
            status_t result = data.readStrongBinder(&display);
            if (result != NO_ERROR) {
                ALOGE("getActiveColorMode failed to readStrongBinder: %d", result);
                return result;
            }
            android_color_mode_t colorMode = getActiveColorMode(display);
            result = reply->writeInt32(static_cast<int32_t>(colorMode));
            return result;
        }
        case SET_ACTIVE_COLOR_MODE: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = nullptr;
            status_t result = data.readStrongBinder(&display);
            if (result != NO_ERROR) {
                ALOGE("getActiveColorMode failed to readStrongBinder: %d", result);
                return result;
            }
            int32_t colorModeInt = 0;
            result = data.readInt32(&colorModeInt);
            if (result != NO_ERROR) {
                ALOGE("setActiveColorMode failed to readInt32: %d", result);
                return result;
            }
            result = setActiveColorMode(display,
                    static_cast<android_color_mode_t>(colorModeInt));
            result = reply->writeInt32(result);
            return result;
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
        case GET_HDR_CAPABILITIES: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            sp<IBinder> display = nullptr;
            status_t result = data.readStrongBinder(&display);
            if (result != NO_ERROR) {
                ALOGE("getHdrCapabilities failed to readStrongBinder: %d",
                        result);
                return result;
            }
            HdrCapabilities capabilities;
            result = getHdrCapabilities(display, &capabilities);
            reply->writeInt32(result);
            if (result == NO_ERROR) {
                reply->writeParcelable(capabilities);
            }
            return NO_ERROR;
        }
        default: {
            return BBinder::onTransact(code, data, reply, flags);
        }
    }
}

// ----------------------------------------------------------------------------

};
