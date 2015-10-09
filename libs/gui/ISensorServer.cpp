/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/Timers.h>

#include <binder/Parcel.h>
#include <binder/IInterface.h>

#include <gui/Sensor.h>
#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>

namespace android {
// ----------------------------------------------------------------------------

enum {
    GET_SENSOR_LIST = IBinder::FIRST_CALL_TRANSACTION,
    CREATE_SENSOR_EVENT_CONNECTION,
    ENABLE_DATA_INJECTION,
    SET_SENSOR_PHYSICAL_DATA,
};

class BpSensorServer : public BpInterface<ISensorServer>
{
public:
    BpSensorServer(const sp<IBinder>& impl)
        : BpInterface<ISensorServer>(impl)
    {
    }

    virtual ~BpSensorServer();

    virtual Vector<Sensor> getSensorList(const String16& opPackageName)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISensorServer::getInterfaceDescriptor());
        data.writeString16(opPackageName);
        remote()->transact(GET_SENSOR_LIST, data, &reply);
        Sensor s;
        Vector<Sensor> v;
        uint32_t n = reply.readUint32();
        v.setCapacity(n);
        while (n--) {
            reply.read(s);
            v.add(s);
        }
        return v;
    }

    virtual sp<ISensorEventConnection> createSensorEventConnection(const String8& packageName,
             int mode, const String16& opPackageName)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISensorServer::getInterfaceDescriptor());
        data.writeString8(packageName);
        data.writeInt32(mode);
        data.writeString16(opPackageName);
        remote()->transact(CREATE_SENSOR_EVENT_CONNECTION, data, &reply);
        return interface_cast<ISensorEventConnection>(reply.readStrongBinder());
    }

    virtual int isDataInjectionEnabled() {
        Parcel data, reply;
        data.writeInterfaceToken(ISensorServer::getInterfaceDescriptor());
        remote()->transact(ENABLE_DATA_INJECTION, data, &reply);
        return reply.readInt32();
    }

    virtual status_t setSensorPhysicalData(const char* physicaldata)
    {
        Parcel data, reply;
        //ALOGD("ISensorManager::SetSensorPhysicalData(%s)",physicaldata);
        data.writeInterfaceToken(ISensorServer::getInterfaceDescriptor());
        data.writeCString(physicaldata);
        remote()->transact(SET_SENSOR_PHYSICAL_DATA, data, &reply);
        return reply.readInt32();
     }
};

// Out-of-line virtual method definition to trigger vtable emission in this
// translation unit (see clang warning -Wweak-vtables)
BpSensorServer::~BpSensorServer() {}

IMPLEMENT_META_INTERFACE(SensorServer, "android.gui.SensorServer");

// ----------------------------------------------------------------------

status_t BnSensorServer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case GET_SENSOR_LIST: {
            CHECK_INTERFACE(ISensorServer, data, reply);
            const String16& opPackageName = data.readString16();
            Vector<Sensor> v(getSensorList(opPackageName));
            size_t n = v.size();
            reply->writeUint32(static_cast<uint32_t>(n));
            for (size_t i = 0; i < n; i++) {
                reply->write(v[i]);
            }
            return NO_ERROR;
        }
        case CREATE_SENSOR_EVENT_CONNECTION: {
            CHECK_INTERFACE(ISensorServer, data, reply);
            String8 packageName = data.readString8();
            int32_t mode = data.readInt32();
            const String16& opPackageName = data.readString16();
            sp<ISensorEventConnection> connection(createSensorEventConnection(packageName, mode,
                    opPackageName));
            reply->writeStrongBinder(IInterface::asBinder(connection));
            return NO_ERROR;
        }
        case ENABLE_DATA_INJECTION: {
            CHECK_INTERFACE(ISensorServer, data, reply);
            int32_t ret = isDataInjectionEnabled();
            reply->writeInt32(static_cast<int32_t>(ret));
            return NO_ERROR;
        }
        case SET_SENSOR_PHYSICAL_DATA: {
            CHECK_INTERFACE(ISensorServer, data, reply);
            const char* physicaldata = data.readCString();
            //ALOGD("ISensorManager, BnSensorServer::onTransact, physicaldata is: (%s)",physicaldata);
            status_t result = setSensorPhysicalData(physicaldata);
            reply->writeInt32(result);
            return NO_ERROR;
        }
    }
    return BBinder::onTransact(code, data, reply, flags);
}

// ----------------------------------------------------------------------------
}; // namespace android
