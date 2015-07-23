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

#include <stdint.h>
#include <sys/types.h>

#include <utils/Vector.h>
#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/AndroidThreads.h>
#include <utils/RefBase.h>
#include <utils/Looper.h>
#include <utils/String8.h>

#include <binder/BinderService.h>

#include <gui/Sensor.h>
#include <gui/BitTube.h>
#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>

#include "SensorInterface.h"

#if __clang__
// Clang warns about SensorEventConnection::dump hiding BBinder::dump
// The cause isn't fixable without changing the API, so let's tell clang
// this is indeed intentional.
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif

// ---------------------------------------------------------------------------
#define IGNORE_HARDWARE_FUSION  false
#define DEBUG_CONNECTIONS   false
// Max size is 100 KB which is enough to accept a batch of about 1000 events.
#define MAX_SOCKET_BUFFER_SIZE_BATCHED 100 * 1024
// For older HALs which don't support batching, use a smaller socket buffer size.
#define SOCKET_BUFFER_SIZE_NON_BATCHED 4 * 1024

#define CIRCULAR_BUF_SIZE 10
#define SENSOR_REGISTRATIONS_BUF_SIZE 20

struct sensors_poll_device_t;
struct sensors_module_t;

namespace android {
// ---------------------------------------------------------------------------

class SensorService :
        public BinderService<SensorService>,
        public BnSensorServer,
        protected Thread
{
    friend class BinderService<SensorService>;

    enum Mode {
       // The regular operating mode where any application can register/unregister/call flush on
       // sensors.
       NORMAL = 0,
       // This mode is only used for testing purposes. Not all HALs support this mode. In this
       // mode, the HAL ignores the sensor data provided by physical sensors and accepts the data
       // that is injected from the SensorService as if it were the real sensor data. This mode
       // is primarily used for testing various algorithms like vendor provided SensorFusion,
       // Step Counter and Step Detector etc. Typically in this mode, there will be a client
       // (a SensorEventConnection) which will be injecting sensor data into the HAL. Normal apps
       // can unregister and register for any sensor that supports injection. Registering to sensors
       // that do not support injection will give an error.
       // TODO(aakella) : Allow exactly one client to inject sensor data at a time.
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
       // time.
       // NOTE: Non whitelisted app whose sensors were previously deactivated may still receive
       // events if a whitelisted app requests data from the same sensor.
       RESTRICTED = 2

      // State Transitions supported.
      //     RESTRICTED   <---  NORMAL   ---> DATA_INJECTION
      //                  --->           <---

      // Shell commands to switch modes in SensorService.
      // 1) Put SensorService in RESTRICTED mode with packageName .cts. If it is already in
      // restricted mode it is treated as a NO_OP (and packageName is NOT changed).
      // $ adb shell dumpsys sensorservice restrict .cts.
      //
      // 2) Put SensorService in DATA_INJECTION mode with packageName .xts. If it is already in
      // data_injection mode it is treated as a NO_OP (and packageName is NOT changed).
      // $ adb shell dumpsys sensorservice data_injection .xts.
      //
      // 3) Reset sensorservice back to NORMAL mode.
      // $ adb shell dumpsys sensorservice enable
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
    virtual sp<ISensorEventConnection> createSensorEventConnection(const String8& packageName,
             int requestedMode, const String16& opPackageName);
    virtual int isDataInjectionEnabled();
    virtual status_t dump(int fd, const Vector<String16>& args);

    class SensorEventConnection : public BnSensorEventConnection, public LooperCallback {
        friend class SensorService;
        virtual ~SensorEventConnection();
        virtual void onFirstRef();
        virtual sp<BitTube> getSensorChannel() const;
        virtual status_t enableDisable(int handle, bool enabled, nsecs_t samplingPeriodNs,
                                       nsecs_t maxBatchReportLatencyNs, int reservedFlags);
        virtual status_t setEventRate(int handle, nsecs_t samplingPeriodNs);
        virtual status_t flush();
        // Count the number of flush complete events which are about to be dropped in the buffer.
        // Increment mPendingFlushEventsToSend in mSensorInfo. These flush complete events will be
        // sent separately before the next batch of events.
        void countFlushCompleteEventsLocked(sensors_event_t const* scratch, int numEventsDropped);

        // Check if there are any wake up events in the buffer. If yes, return the index of the
        // first wake_up sensor event in the buffer else return -1. This wake_up sensor event will
        // have the flag WAKE_UP_SENSOR_EVENT_NEEDS_ACK set. Exactly one event per packet will have
        // the wake_up flag set. SOCK_SEQPACKET ensures that either the entire packet is read or
        // dropped.
        int findWakeUpSensorEventLocked(sensors_event_t const* scratch, int count);

        // Send pending flush_complete events. There may have been flush_complete_events that are
        // dropped which need to be sent separately before other events. On older HALs (1_0) this
        // method emulates the behavior of flush().
        void sendPendingFlushEventsLocked();

        // Writes events from mEventCache to the socket.
        void writeToSocketFromCache();

        // Compute the approximate cache size from the FIFO sizes of various sensors registered for
        // this connection. Wake up and non-wake up sensors have separate FIFOs but FIFO may be
        // shared amongst wake-up sensors and non-wake up sensors.
        int computeMaxCacheSizeLocked() const;

        // When more sensors register, the maximum cache size desired may change. Compute max cache
        // size, reallocate memory and copy over events from the older cache.
        void reAllocateCacheLocked(sensors_event_t const* scratch, int count);

        // LooperCallback method. If there is data to read on this fd, it is an ack from the
        // app that it has read events from a wake up sensor, decrement mWakeLockRefCount.
        // If this fd is available for writing send the data from the cache.
        virtual int handleEvent(int fd, int events, void* data);

        // Increment mPendingFlushEventsToSend for the given sensor handle.
        void incrementPendingFlushCount(int32_t handle);

        // Add or remove the file descriptor associated with the BitTube to the looper. If mDead is
        // set to true or there are no more sensors for this connection, the file descriptor is
        // removed if it has been previously added to the Looper. Depending on the state of the
        // connection FD may be added to the Looper. The flags to set are determined by the internal
        // state of the connection. FDs are added to the looper when wake-up sensors are registered
        // (to poll for acknowledgements) and when write fails on the socket when there are too many
        // error and the other end hangs up or when this client unregisters for this connection.
        void updateLooperRegistration(const sp<Looper>& looper);
        void updateLooperRegistrationLocked(const sp<Looper>& looper);

        sp<SensorService> const mService;
        sp<BitTube> mChannel;
        uid_t mUid;
        mutable Mutex mConnectionLock;
        // Number of events from wake up sensors which are still pending and haven't been delivered
        // to the corresponding application. It is incremented by one unit for each write to the
        // socket.
        uint32_t mWakeLockRefCount;

        // If this flag is set to true, it means that the file descriptor associated with the
        // BitTube has been added to the Looper in SensorService. This flag is typically set when
        // this connection has wake-up sensors associated with it or when write has failed on this
        // connection and we're storing some events in the cache.
        bool mHasLooperCallbacks;
        // If there are any errors associated with the Looper this flag is set to true and
        // mWakeLockRefCount is reset to zero. needsWakeLock method will always return false, if
        // this flag is set.
        bool mDead;

        bool mDataInjectionMode;
        struct FlushInfo {
            // The number of flush complete events dropped for this sensor is stored here.
            // They are sent separately before the next batch of events.
            int mPendingFlushEventsToSend;
            // Every activate is preceded by a flush. Only after the first flush complete is
            // received, the events for the sensor are sent on that *connection*.
            bool mFirstFlushPending;
            FlushInfo() : mPendingFlushEventsToSend(0), mFirstFlushPending(false) {}
        };
        // protected by SensorService::mLock. Key for this vector is the sensor handle.
        KeyedVector<int, FlushInfo> mSensorInfo;
        sensors_event_t *mEventCache;
        int mCacheSize, mMaxCacheSize;
        String8 mPackageName;
        const String16 mOpPackageName;
#if DEBUG_CONNECTIONS
        int mEventsReceived, mEventsSent, mEventsSentFromCache;
        int mTotalAcksNeeded, mTotalAcksReceived;
#endif

    public:
        SensorEventConnection(const sp<SensorService>& service, uid_t uid, String8 packageName,
                 bool isDataInjectionMode, const String16& opPackageName);

        status_t sendEvents(sensors_event_t const* buffer, size_t count,
                sensors_event_t* scratch,
                SensorEventConnection const * const * mapFlushEventsToConnections = NULL);
        bool hasSensor(int32_t handle) const;
        bool hasAnySensor() const;
        bool hasOneShotSensors() const;
        bool addSensor(int32_t handle);
        bool removeSensor(int32_t handle);
        void setFirstFlushPending(int32_t handle, bool value);
        void dump(String8& result);
        bool needsWakeLock();
        void resetWakeLockRefCount();
        String8 getPackageName() const;

        uid_t getUid() const { return mUid; }
    };

    class SensorRecord {
        SortedVector< wp<SensorEventConnection> > mConnections;
        // A queue of all flush() calls made on this sensor. Flush complete events will be
        // sent in this order.
        Vector< wp<SensorEventConnection> > mPendingFlushConnections;
    public:
        SensorRecord(const sp<SensorEventConnection>& connection);
        bool addConnection(const sp<SensorEventConnection>& connection);
        bool removeConnection(const wp<SensorEventConnection>& connection);
        size_t getNumConnections() const { return mConnections.size(); }

        void addPendingFlushConnection(const sp<SensorEventConnection>& connection);
        void removeFirstPendingFlushConnection();
        SensorEventConnection * getFirstPendingFlushConnection();
        void clearAllPendingFlushConnections();
    };

    class SensorEventAckReceiver : public Thread {
        sp<SensorService> const mService;
    public:
        virtual bool threadLoop();
        SensorEventAckReceiver(const sp<SensorService>& service): mService(service) {}
    };

    // sensor_event_t with only the data and the timestamp.
    struct TrimmedSensorEvent {
        union {
            float *mData;
            uint64_t mStepCounter;
        };
        // Timestamp from the sensor_event.
        int64_t mTimestamp;
        // HH:MM:SS local time at which this sensor event is read at SensorService. Useful
        // for debugging.
        int32_t mHour, mMin, mSec;

        TrimmedSensorEvent(int sensorType);
        static bool isSentinel(const TrimmedSensorEvent& event);

        ~TrimmedSensorEvent() {
            delete [] mData;
        }
    };

    // A circular buffer of TrimmedSensorEvents. The size of this buffer is typically 10. The
    // last N events generated from the sensor are stored in this buffer. The buffer is NOT
    // cleared when the sensor unregisters and as a result one may see very old data in the
    // dumpsys output but this is WAI.
    class CircularBuffer {
        int mNextInd;
        int mSensorType;
        int mBufSize;
        TrimmedSensorEvent ** mTrimmedSensorEventArr;
    public:
        CircularBuffer(int sensor_event_type);
        void addEvent(const sensors_event_t& sensor_event);
        void printBuffer(String8& buffer) const;
        bool populateLastEvent(sensors_event_t *event);
        ~CircularBuffer();
    };

    struct SensorRegistrationInfo {
        int32_t mSensorHandle;
        String8 mPackageName;
        bool mActivated;
        int32_t mSamplingRateUs;
        int32_t mMaxReportLatencyUs;
        int32_t mHour, mMin, mSec;

        SensorRegistrationInfo() : mPackageName() {
            mSensorHandle = mSamplingRateUs = mMaxReportLatencyUs = INT32_MIN;
            mHour = mMin = mSec = INT32_MIN;
            mActivated = false;
        }

        static bool isSentinel(const SensorRegistrationInfo& info) {
           return (info.mHour == INT32_MIN && info.mMin == INT32_MIN && info.mSec == INT32_MIN);
        }
    };

    static int getNumEventsForSensorType(int sensor_event_type);
    String8 getSensorName(int handle) const;
    bool isVirtualSensor(int handle) const;
    Sensor getSensorFromHandle(int handle) const;
    bool isWakeUpSensor(int type) const;
    void recordLastValueLocked(sensors_event_t const* buffer, size_t count);
    static void sortEventBuffer(sensors_event_t* buffer, size_t count);
    Sensor registerSensor(SensorInterface* sensor);
    Sensor registerVirtualSensor(SensorInterface* sensor);
    status_t cleanupWithoutDisable(
            const sp<SensorEventConnection>& connection, int handle);
    status_t cleanupWithoutDisableLocked(
            const sp<SensorEventConnection>& connection, int handle);
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

    SensorRecord * getSensorRecord(int handle);

    sp<Looper> getLooper() const;

    // Reset mWakeLockRefCounts for all SensorEventConnections to zero. This may happen if
    // SensorService did not receive any acknowledgements from apps which have registered for
    // wake_up sensors.
    void resetAllWakeLockRefCounts();

    // Acquire or release wake_lock. If wake_lock is acquired, set the timeout in the looper to
    // 5 seconds and wake the looper.
    void setWakeLockAcquiredLocked(bool acquire);

    // Send events from the event cache for this particular connection.
    void sendEventsFromCache(const sp<SensorEventConnection>& connection);

    // Promote all weak referecences in mActiveConnections vector to strong references and add them
    // to the output vector.
    void populateActiveConnections(SortedVector< sp<SensorEventConnection> >* activeConnections);

    // If SensorService is operating in RESTRICTED mode, only select whitelisted packages are
    // allowed to register for or call flush on sensors. Typically only cts test packages are
    // allowed.
    bool isWhiteListedPackage(const String8& packageName);

    // Reset the state of SensorService to NORMAL mode.
    status_t resetToNormalMode();
    status_t resetToNormalModeLocked();

    // constants
    Vector<Sensor> mSensorList;
    Vector<Sensor> mUserSensorListDebug;
    Vector<Sensor> mUserSensorList;
    DefaultKeyedVector<int, SensorInterface*> mSensorMap;
    Vector<SensorInterface *> mVirtualSensorList;
    status_t mInitCheck;
    // Socket buffersize used to initialize BitTube. This size depends on whether batching is
    // supported or not.
    uint32_t mSocketBufferSize;
    sp<Looper> mLooper;
    sp<SensorEventAckReceiver> mAckReceiver;

    // protected by mLock
    mutable Mutex mLock;
    DefaultKeyedVector<int, SensorRecord*> mActiveSensors;
    DefaultKeyedVector<int, SensorInterface*> mActiveVirtualSensors;
    SortedVector< wp<SensorEventConnection> > mActiveConnections;
    bool mWakeLockAcquired;
    sensors_event_t *mSensorEventBuffer, *mSensorEventScratch;
    SensorEventConnection const **mMapFlushEventsToConnections;
    Mode mCurrentOperatingMode;
    // This packagaName is set when SensorService is in RESTRICTED or DATA_INJECTION mode. Only
    // applications with this packageName are allowed to activate/deactivate or call flush on
    // sensors. To run CTS this is can be set to ".cts." and only CTS tests will get access to
    // sensors.
    String8 mWhiteListedPackage;

    // The size of this vector is constant, only the items are mutable
    KeyedVector<int32_t, CircularBuffer *> mLastEventSeen;

    int mNextSensorRegIndex;
    Vector<SensorRegistrationInfo> mLastNSensorRegistrations;
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
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_SENSOR_SERVICE_H
