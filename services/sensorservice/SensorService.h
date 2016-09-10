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

#ifndef ANDROID_SENSOR_SERVICE_H
#define ANDROID_SENSOR_SERVICE_H

#include "SensorList.h"
#include "RecentEventLogger.h"

#include <binder/BinderService.h>
#include <cutils/compiler.h>
#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>
#include <gui/Sensor.h>

#include <utils/AndroidThreads.h>
#include <utils/KeyedVector.h>
#include <utils/Looper.h>
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/threads.h>

#include <stdint.h>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>

#if __clang__
// Clang warns about SensorEventConnection::dump hiding BBinder::dump. The cause isn't fixable
// without changing the API, so let's tell clang this is indeed intentional.
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif

// ---------------------------------------------------------------------------
#define IGNORE_HARDWARE_FUSION  false
#define DEBUG_CONNECTIONS   false
// Max size is 100 KB which is enough to accept a batch of about 1000 events.
#define MAX_SOCKET_BUFFER_SIZE_BATCHED 100 * 1024
// For older HALs which don't support batching, use a smaller socket buffer size.
#define SOCKET_BUFFER_SIZE_NON_BATCHED 4 * 1024

#define SENSOR_REGISTRATIONS_BUF_SIZE 200

namespace android {
// ---------------------------------------------------------------------------
class SensorInterface;
using namespace SensorServiceUtil;

class SensorService :
        public BinderService<SensorService>,
        public BnSensorServer,
        protected Thread
{
    // nested class/struct for internal use
    class SensorEventConnection;

public:
    void cleanupConnection(SensorEventConnection* connection);

    status_t enable(const sp<SensorEventConnection>& connection, int handle,
                    nsecs_t samplingPeriodNs,  nsecs_t maxBatchReportLatencyNs, int reservedFlags,
                    const String16& opPackageName);

    status_t disable(const sp<SensorEventConnection>& connection, int handle);

    status_t setEventRate(const sp<SensorEventConnection>& connection, int handle, nsecs_t ns,
                          const String16& opPackageName);

    status_t flushSensor(const sp<SensorEventConnection>& connection,
                         const String16& opPackageName);

private:
    friend class BinderService<SensorService>;

    // nested class/struct for internal use
    class SensorRecord;
    class SensorEventAckReceiver;
    struct SensorRegistrationInfo;

    enum Mode {
       // The regular operating mode where any application can register/unregister/call flush on
       // sensors.
       NORMAL = 0,
       // This mode is only used for testing purposes. Not all HALs support this mode. In this mode,
       // the HAL ignores the sensor data provided by physical sensors and accepts the data that is
       // injected from the SensorService as if it were the real sensor data. This mode is primarily
       // used for testing various algorithms like vendor provided SensorFusion, Step Counter and
       // Step Detector etc. Typically in this mode, there will be a client (a
       // SensorEventConnection) which will be injecting sensor data into the HAL. Normal apps can
       // unregister and register for any sensor that supports injection. Registering to sensors
       // that do not support injection will give an error.  TODO(aakella) : Allow exactly one
       // client to inject sensor data at a time.
       DATA_INJECTION = 1,
       // This mode is used only for testing sensors. Each sensor can be tested in isolation with
       // the required sampling_rate and maxReportLatency parameters without having to think about
       // the data rates requested by other applications. End user devices are always expected to be
       // in NORMAL mode. When this mode is first activated, all active sensors from all connections
       // are disabled. Calling flush() will return an error. In this mode, only the requests from
       // selected apps whose package names are whitelisted are allowed (typically CTS apps).  Only
       // these apps can register/unregister/call flush() on sensors. If SensorService switches to
       // NORMAL mode again, all sensors that were previously registered to are activated with the
       // corresponding paramaters if the application hasn't unregistered for sensors in the mean
       // time.  NOTE: Non whitelisted app whose sensors were previously deactivated may still
       // receive events if a whitelisted app requests data from the same sensor.
       RESTRICTED = 2

      // State Transitions supported.
      //     RESTRICTED   <---  NORMAL   ---> DATA_INJECTION
      //                  --->           <---

      // Shell commands to switch modes in SensorService.
      // 1) Put SensorService in RESTRICTED mode with packageName .cts. If it is already in
      // restricted mode it is treated as a NO_OP (and packageName is NOT changed).
      //
      //     $ adb shell dumpsys sensorservice restrict .cts.
      //
      // 2) Put SensorService in DATA_INJECTION mode with packageName .xts. If it is already in
      // data_injection mode it is treated as a NO_OP (and packageName is NOT changed).
      //
      //     $ adb shell dumpsys sensorservice data_injection .xts.
      //
      // 3) Reset sensorservice back to NORMAL mode.
      //     $ adb shell dumpsys sensorservice enable
    };

    static const char* WAKE_LOCK_NAME;
    static char const* getServiceName() ANDROID_API { return "sensorservice"; }
    SensorService() ANDROID_API;
    virtual ~SensorService();

    virtual void onFirstRef();

    // Thread interface
    virtual bool threadLoop();

    // ISensorServer interface
    virtual Vector<Sensor> getSensorList(const String16& opPackageName);
    virtual Vector<Sensor> getDynamicSensorList(const String16& opPackageName);
    virtual sp<ISensorEventConnection> createSensorEventConnection(
            const String8& packageName,
            int requestedMode, const String16& opPackageName);
    virtual int isDataInjectionEnabled();
    virtual status_t dump(int fd, const Vector<String16>& args);

    String8 getSensorName(int handle) const;
    bool isVirtualSensor(int handle) const;
    sp<SensorInterface> getSensorInterfaceFromHandle(int handle) const;
    bool isWakeUpSensor(int type) const;
    void recordLastValueLocked(sensors_event_t const* buffer, size_t count);
    static void sortEventBuffer(sensors_event_t* buffer, size_t count);
    const Sensor& registerSensor(SensorInterface* sensor,
                                 bool isDebug = false, bool isVirtual = false);
    const Sensor& registerVirtualSensor(SensorInterface* sensor, bool isDebug = false);
    const Sensor& registerDynamicSensorLocked(SensorInterface* sensor, bool isDebug = false);
    bool unregisterDynamicSensorLocked(int handle);
    status_t cleanupWithoutDisable(const sp<SensorEventConnection>& connection, int handle);
    status_t cleanupWithoutDisableLocked(const sp<SensorEventConnection>& connection, int handle);
    void cleanupAutoDisabledSensorLocked(const sp<SensorEventConnection>& connection,
            sensors_event_t const* buffer, const int count);
    static bool canAccessSensor(const Sensor& sensor, const char* operation,
            const String16& opPackageName);
    // SensorService acquires a partial wakelock for delivering events from wake up sensors. This
    // method checks whether all the events from these wake up sensors have been delivered to the
    // corresponding applications, if yes the wakelock is released.
    void checkWakeLockState();
    void checkWakeLockStateLocked();
    bool isWakeLockAcquired();
    bool isWakeUpSensorEvent(const sensors_event_t& event) const;

    sp<Looper> getLooper() const;

    // Reset mWakeLockRefCounts for all SensorEventConnections to zero. This may happen if
    // SensorService did not receive any acknowledgements from apps which have registered for
    // wake_up sensors.
    void resetAllWakeLockRefCounts();

    // Acquire or release wake_lock. If wake_lock is acquired, set the timeout in the looper to 5
    // seconds and wake the looper.
    void setWakeLockAcquiredLocked(bool acquire);

    // Send events from the event cache for this particular connection.
    void sendEventsFromCache(const sp<SensorEventConnection>& connection);

    // Promote all weak referecences in mActiveConnections vector to strong references and add them
    // to the output vector.
    void populateActiveConnections( SortedVector< sp<SensorEventConnection> >* activeConnections);

    // If SensorService is operating in RESTRICTED mode, only select whitelisted packages are
    // allowed to register for or call flush on sensors. Typically only cts test packages are
    // allowed.
    bool isWhiteListedPackage(const String8& packageName);

    // Reset the state of SensorService to NORMAL mode.
    status_t resetToNormalMode();
    status_t resetToNormalModeLocked();

    // Transforms the UUIDs for all the sensors into proper IDs.
    void makeUuidsIntoIdsForSensorList(Vector<Sensor> &sensorList) const;
    // Gets the appropriate ID from the given UUID.
    int32_t getIdFromUuid(const Sensor::uuid_t &uuid) const;
    // Either read from storage or create a new one.
    static bool initializeHmacKey();

    // Enable SCHED_FIFO priority for thread
    void enableSchedFifoMode();

    static uint8_t sHmacGlobalKey[128];
    static bool sHmacGlobalKeyIsValid;

    SensorList mSensors;
    status_t mInitCheck;

    // Socket buffersize used to initialize BitTube. This size depends on whether batching is
    // supported or not.
    uint32_t mSocketBufferSize;
    sp<Looper> mLooper;
    sp<SensorEventAckReceiver> mAckReceiver;

    // protected by mLock
    mutable Mutex mLock;
    DefaultKeyedVector<int, SensorRecord*> mActiveSensors;
    std::unordered_set<int> mActiveVirtualSensors;
    SortedVector< wp<SensorEventConnection> > mActiveConnections;
    bool mWakeLockAcquired;
    sensors_event_t *mSensorEventBuffer, *mSensorEventScratch;
    wp<const SensorEventConnection> * mMapFlushEventsToConnections;
    std::unordered_map<int, RecentEventLogger*> mRecentEvent;
    Mode mCurrentOperatingMode;

    // This packagaName is set when SensorService is in RESTRICTED or DATA_INJECTION mode. Only
    // applications with this packageName are allowed to activate/deactivate or call flush on
    // sensors. To run CTS this is can be set to ".cts." and only CTS tests will get access to
    // sensors.
    String8 mWhiteListedPackage;

    int mNextSensorRegIndex;
    Vector<SensorRegistrationInfo> mLastNSensorRegistrations;
};

} // namespace android
#endif // ANDROID_SENSOR_SERVICE_H
