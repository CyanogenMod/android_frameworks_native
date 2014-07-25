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

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <cutils/properties.h>

#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/String16.h>

#include <binder/BinderService.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>

#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>
#include <gui/SensorEventQueue.h>

#include <hardware/sensors.h>
#include <hardware_legacy/power.h>

#include "BatteryService.h"
#include "CorrectedGyroSensor.h"
#include "GravitySensor.h"
#include "LinearAccelerationSensor.h"
#include "OrientationSensor.h"
#include "RotationVectorSensor.h"
#include "SensorFusion.h"
#include "SensorService.h"

namespace android {
// ---------------------------------------------------------------------------

/*
 * Notes:
 *
 * - what about a gyro-corrected magnetic-field sensor?
 * - run mag sensor from time to time to force calibration
 * - gravity sensor length is wrong (=> drift in linear-acc sensor)
 *
 */

const char* SensorService::WAKE_LOCK_NAME = "SensorService";

SensorService::SensorService()
    : mInitCheck(NO_INIT)
{
}

void SensorService::onFirstRef()
{
    ALOGD("nuSensorService starting...");

    SensorDevice& dev(SensorDevice::getInstance());

    if (dev.initCheck() == NO_ERROR) {
        sensor_t const* list;
        ssize_t count = dev.getSensorList(&list);
        if (count > 0) {
            ssize_t orientationIndex = -1;
            bool hasGyro = false;
            uint32_t virtualSensorsNeeds =
                    (1<<SENSOR_TYPE_GRAVITY) |
                    (1<<SENSOR_TYPE_LINEAR_ACCELERATION) |
                    (1<<SENSOR_TYPE_ROTATION_VECTOR);

            mLastEventSeen.setCapacity(count);
            for (ssize_t i=0 ; i<count ; i++) {
                registerSensor( new HardwareSensor(list[i]) );
                switch (list[i].type) {
                    case SENSOR_TYPE_ORIENTATION:
                        orientationIndex = i;
                        break;
                    case SENSOR_TYPE_GYROSCOPE:
                    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
                        hasGyro = true;
                        break;
                    case SENSOR_TYPE_GRAVITY:
                    case SENSOR_TYPE_LINEAR_ACCELERATION:
                    case SENSOR_TYPE_ROTATION_VECTOR:
                        virtualSensorsNeeds &= ~(1<<list[i].type);
                        break;
                }
            }

            // it's safe to instantiate the SensorFusion object here
            // (it wants to be instantiated after h/w sensors have been
            // registered)
            const SensorFusion& fusion(SensorFusion::getInstance());

            // build the sensor list returned to users
            mUserSensorList = mSensorList;

            if (hasGyro) {
                Sensor aSensor;

                // Add Android virtual sensors if they're not already
                // available in the HAL

                aSensor = registerVirtualSensor( new RotationVectorSensor() );
                if (virtualSensorsNeeds & (1<<SENSOR_TYPE_ROTATION_VECTOR)) {
                    mUserSensorList.add(aSensor);
                }

                aSensor = registerVirtualSensor( new GravitySensor(list, count) );
                if (virtualSensorsNeeds & (1<<SENSOR_TYPE_GRAVITY)) {
                    mUserSensorList.add(aSensor);
                }

                aSensor = registerVirtualSensor( new LinearAccelerationSensor(list, count) );
                if (virtualSensorsNeeds & (1<<SENSOR_TYPE_LINEAR_ACCELERATION)) {
                    mUserSensorList.add(aSensor);
                }

                aSensor = registerVirtualSensor( new OrientationSensor() );
                if (virtualSensorsNeeds & (1<<SENSOR_TYPE_ROTATION_VECTOR)) {
                    // if we are doing our own rotation-vector, also add
                    // the orientation sensor and remove the HAL provided one.
                    mUserSensorList.replaceAt(aSensor, orientationIndex);
                }

                // virtual debugging sensors are not added to mUserSensorList
                registerVirtualSensor( new CorrectedGyroSensor(list, count) );
                registerVirtualSensor( new GyroDriftSensor() );
            }

            // debugging sensor list
            mUserSensorListDebug = mSensorList;

            mSocketBufferSize = SOCKET_BUFFER_SIZE_NON_BATCHED;
            FILE *fp = fopen("/proc/sys/net/core/wmem_max", "r");
            char line[128];
            if (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
                line[sizeof(line) - 1] = '\0';
                sscanf(line, "%zu", &mSocketBufferSize);
                if (mSocketBufferSize > MAX_SOCKET_BUFFER_SIZE_BATCHED) {
                    mSocketBufferSize = MAX_SOCKET_BUFFER_SIZE_BATCHED;
                }
            }
            if (fp) {
                fclose(fp);
            }

            mWakeLockAcquired = false;
            run("SensorService", PRIORITY_URGENT_DISPLAY);
            mLooper = new Looper(false);
            mInitCheck = NO_ERROR;
        }
    }
}

Sensor SensorService::registerSensor(SensorInterface* s)
{
    sensors_event_t event;
    memset(&event, 0, sizeof(event));

    const Sensor sensor(s->getSensor());
    // add to the sensor list (returned to clients)
    mSensorList.add(sensor);
    // add to our handle->SensorInterface mapping
    mSensorMap.add(sensor.getHandle(), s);
    // create an entry in the mLastEventSeen array
    mLastEventSeen.add(sensor.getHandle(), event);

    return sensor;
}

Sensor SensorService::registerVirtualSensor(SensorInterface* s)
{
    Sensor sensor = registerSensor(s);
    mVirtualSensorList.add( s );
    return sensor;
}

SensorService::~SensorService()
{
    for (size_t i=0 ; i<mSensorMap.size() ; i++)
        delete mSensorMap.valueAt(i);
}

static const String16 sDump("android.permission.DUMP");

status_t SensorService::dump(int fd, const Vector<String16>& /*args*/)
{
    String8 result;
    if (!PermissionCache::checkCallingPermission(sDump)) {
        result.appendFormat("Permission Denial: "
                "can't dump SensorService from pid=%d, uid=%d\n",
                IPCThreadState::self()->getCallingPid(),
                IPCThreadState::self()->getCallingUid());
    } else {
        Mutex::Autolock _l(mLock);
        result.append("Sensor List:\n");
        for (size_t i=0 ; i<mSensorList.size() ; i++) {
            const Sensor& s(mSensorList[i]);
            const sensors_event_t& e(mLastEventSeen.valueFor(s.getHandle()));
            result.appendFormat(
                    "%-48s| %-32s| %-48s| 0x%08x | \"%s\"\n\t",
                    s.getName().string(),
                    s.getVendor().string(),
                    s.getStringType().string(),
                    s.getHandle(),
                    s.getRequiredPermission().string());

            const int reportingMode = s.getReportingMode();
            if (reportingMode == AREPORTING_MODE_CONTINUOUS) {
                result.append("continuous      |");
            } else if (reportingMode == AREPORTING_MODE_ON_CHANGE) {
                result.append("on-change       | ");
            } else if (reportingMode == AREPORTING_MODE_ONE_SHOT) {
                result.append("one-shot        | ");
            } else {
                result.append("special-trigger | ");
            }

            if (s.getMinDelay() > 0) {
                result.appendFormat("maxRate=%7.2fHz | ", 1e6f / s.getMinDelay());
            } else {
                result.appendFormat("minDelay=%5dus |", s.getMinDelay());
            }

            if (s.getFifoMaxEventCount() > 0) {
                result.appendFormat("FifoMax=%d events | ",
                        s.getFifoMaxEventCount());
            } else {
                result.append("no batching support | ");
            }

            switch (s.getType()) {
                case SENSOR_TYPE_ROTATION_VECTOR:
                case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
                    result.appendFormat(
                            "last=<%5.1f,%5.1f,%5.1f,%5.1f,%5.1f>\n",
                            e.data[0], e.data[1], e.data[2], e.data[3], e.data[4]);
                    break;
                case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
                case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
                    result.appendFormat(
                            "last=<%5.1f,%5.1f,%5.1f,%5.1f,%5.1f,%5.1f>\n",
                            e.data[0], e.data[1], e.data[2], e.data[3], e.data[4], e.data[5]);
                    break;
                case SENSOR_TYPE_GAME_ROTATION_VECTOR:
                    result.appendFormat(
                            "last=<%5.1f,%5.1f,%5.1f,%5.1f>\n",
                            e.data[0], e.data[1], e.data[2], e.data[3]);
                    break;
                case SENSOR_TYPE_SIGNIFICANT_MOTION:
                case SENSOR_TYPE_STEP_DETECTOR:
                    result.appendFormat( "last=<%f>\n", e.data[0]);
                    break;
                case SENSOR_TYPE_STEP_COUNTER:
                    result.appendFormat( "last=<%" PRIu64 ">\n", e.u64.step_counter);
                    break;
                default:
                    // default to 3 values
                    result.appendFormat(
                            "last=<%5.1f,%5.1f,%5.1f>\n",
                            e.data[0], e.data[1], e.data[2]);
                    break;
            }
        }
        SensorFusion::getInstance().dump(result);
        SensorDevice::getInstance().dump(result);

        result.append("Active sensors:\n");
        for (size_t i=0 ; i<mActiveSensors.size() ; i++) {
            int handle = mActiveSensors.keyAt(i);
            result.appendFormat("%s (handle=0x%08x, connections=%zu)\n",
                    getSensorName(handle).string(),
                    handle,
                    mActiveSensors.valueAt(i)->getNumConnections());
        }

        result.appendFormat("%zu Max Socket Buffer size\n", mSocketBufferSize);
        result.appendFormat("WakeLock Status: %s \n", mWakeLockAcquired ? "acquired" : "not held");
        result.appendFormat("%zd active connections\n", mActiveConnections.size());

        for (size_t i=0 ; i < mActiveConnections.size() ; i++) {
            sp<SensorEventConnection> connection(mActiveConnections[i].promote());
            if (connection != 0) {
                result.appendFormat("Connection Number: %zu \n", i);
                connection->dump(result);
            }
        }
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

void SensorService::cleanupAutoDisabledSensorLocked(const sp<SensorEventConnection>& connection,
        sensors_event_t const* buffer, const int count) {
    for (int i=0 ; i<count ; i++) {
        int handle = buffer[i].sensor;
        if (connection->hasSensor(handle)) {
            SensorInterface* sensor = mSensorMap.valueFor(handle);
            // If this buffer has an event from a one_shot sensor and this connection is registered
            // for this particular one_shot sensor, try cleaning up the connection.
            if (sensor != NULL &&
                sensor->getSensor().getReportingMode() == AREPORTING_MODE_ONE_SHOT) {
                sensor->autoDisable(connection.get(), handle);
                cleanupWithoutDisableLocked(connection, handle);
            }
        }
    }
}

bool SensorService::threadLoop()
{
    ALOGD("nuSensorService thread starting...");

    // each virtual sensor could generate an event per "real" event, that's why we need
    // to size numEventMax much smaller than MAX_RECEIVE_BUFFER_EVENT_COUNT.
    // in practice, this is too aggressive, but guaranteed to be enough.
    const size_t minBufferSize = SensorEventQueue::MAX_RECEIVE_BUFFER_EVENT_COUNT;
    const size_t numEventMax = minBufferSize / (1 + mVirtualSensorList.size());

    sensors_event_t buffer[minBufferSize];
    sensors_event_t scratch[minBufferSize];
    SensorDevice& device(SensorDevice::getInstance());
    const size_t vcount = mVirtualSensorList.size();

    SensorEventAckReceiver sender(this);
    sender.run("SensorEventAckReceiver", PRIORITY_URGENT_DISPLAY);
    ssize_t count;
    const int halVersion = device.getHalDeviceVersion();
    do {
        count = device.poll(buffer, numEventMax);
        if (count<0) {
            ALOGE("sensor poll failed (%s)", strerror(-count));
            break;
        }

        // Reset sensors_event_t.flags to zero for all events in the buffer.
        for (int i = 0; i < count; i++) {
             buffer[i].flags = 0;
        }
        Mutex::Autolock _l(mLock);
        // Poll has returned. Hold a wakelock if one of the events is from a wake up sensor. The
        // rest of this loop is under a critical section protected by mLock. Acquiring a wakeLock,
        // sending events to clients (incrementing SensorEventConnection::mWakeLockRefCount) should
        // not be interleaved with decrementing SensorEventConnection::mWakeLockRefCount and
        // releasing the wakelock.
        bool bufferHasWakeUpEvent = false;
        for (int i = 0; i < count; i++) {
            if (isWakeUpSensorEvent(buffer[i])) {
                bufferHasWakeUpEvent = true;
                break;
            }
        }

        if (bufferHasWakeUpEvent && !mWakeLockAcquired) {
            acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_NAME);
            mWakeLockAcquired = true;
            ALOGD_IF(DEBUG_CONNECTIONS, "acquired wakelock %s", WAKE_LOCK_NAME);
        }
        recordLastValueLocked(buffer, count);

        // handle virtual sensors
        if (count && vcount) {
            sensors_event_t const * const event = buffer;
            const size_t activeVirtualSensorCount = mActiveVirtualSensors.size();
            if (activeVirtualSensorCount) {
                size_t k = 0;
                SensorFusion& fusion(SensorFusion::getInstance());
                if (fusion.isEnabled()) {
                    for (size_t i=0 ; i<size_t(count) ; i++) {
                        fusion.process(event[i]);
                    }
                }
                for (size_t i=0 ; i<size_t(count) && k<minBufferSize ; i++) {
                    for (size_t j=0 ; j<activeVirtualSensorCount ; j++) {
                        if (count + k >= minBufferSize) {
                            ALOGE("buffer too small to hold all events: "
                                    "count=%zd, k=%zu, size=%zu",
                                    count, k, minBufferSize);
                            break;
                        }
                        sensors_event_t out;
                        SensorInterface* si = mActiveVirtualSensors.valueAt(j);
                        if (si->process(&out, event[i])) {
                            buffer[count + k] = out;
                            k++;
                        }
                    }
                }
                if (k) {
                    // record the last synthesized values
                    recordLastValueLocked(&buffer[count], k);
                    count += k;
                    // sort the buffer by time-stamps
                    sortEventBuffer(buffer, count);
                }
            }
        }

        // handle backward compatibility for RotationVector sensor
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_0) {
            for (int i = 0; i < count; i++) {
                if (buffer[i].type == SENSOR_TYPE_ROTATION_VECTOR) {
                    // All the 4 components of the quaternion should be available
                    // No heading accuracy. Set it to -1
                    buffer[i].data[4] = -1;
                }
            }
        }

        // Send our events to clients. Check the state of wake lock for each client and release the
        // lock if none of the clients need it.
        bool needsWakeLock = false;
        for (size_t i=0 ; i < mActiveConnections.size(); i++) {
            sp<SensorEventConnection> connection(mActiveConnections[i].promote());
            if (connection != 0) {
                connection->sendEvents(buffer, count, scratch);
                needsWakeLock |= connection->needsWakeLock();
                // Some sensors need to be auto disabled after the trigger
                cleanupAutoDisabledSensorLocked(connection, buffer, count);
            }
        }

        if (mWakeLockAcquired && !needsWakeLock) {
            release_wake_lock(WAKE_LOCK_NAME);
            mWakeLockAcquired = false;
            ALOGD_IF(DEBUG_CONNECTIONS, "released wakelock %s", WAKE_LOCK_NAME);
        }
    } while (count >= 0 || Thread::exitPending());

    ALOGW("Exiting SensorService::threadLoop => aborting...");
    abort();
    return false;
}

sp<Looper> SensorService::getLooper() const {
    return mLooper;
}

bool SensorService::SensorEventAckReceiver::threadLoop() {
    ALOGD("new thread SensorEventAckReceiver");
    do {
        sp<Looper> looper = mService->getLooper();
        looper->pollOnce(-1);
    } while(!Thread::exitPending());
    return false;
}

void SensorService::recordLastValueLocked(
        const sensors_event_t* buffer, size_t count) {
    const sensors_event_t* last = NULL;
    for (size_t i = 0; i < count; i++) {
        const sensors_event_t* event = &buffer[i];
        if (event->type != SENSOR_TYPE_META_DATA) {
            if (last && event->sensor != last->sensor) {
                mLastEventSeen.editValueFor(last->sensor) = *last;
            }
            last = event;
        }
    }
    if (last) {
        mLastEventSeen.editValueFor(last->sensor) = *last;
    }
}

void SensorService::sortEventBuffer(sensors_event_t* buffer, size_t count)
{
    struct compar {
        static int cmp(void const* lhs, void const* rhs) {
            sensors_event_t const* l = static_cast<sensors_event_t const*>(lhs);
            sensors_event_t const* r = static_cast<sensors_event_t const*>(rhs);
            return l->timestamp - r->timestamp;
        }
    };
    qsort(buffer, count, sizeof(sensors_event_t), compar::cmp);
}

String8 SensorService::getSensorName(int handle) const {
    size_t count = mUserSensorList.size();
    for (size_t i=0 ; i<count ; i++) {
        const Sensor& sensor(mUserSensorList[i]);
        if (sensor.getHandle() == handle) {
            return sensor.getName();
        }
    }
    String8 result("unknown");
    return result;
}

bool SensorService::isVirtualSensor(int handle) const {
    SensorInterface* sensor = mSensorMap.valueFor(handle);
    return sensor->isVirtual();
}

bool SensorService::isWakeUpSensorEvent(const sensors_event_t& event) const {
    int handle = event.sensor;
    if (event.type == SENSOR_TYPE_META_DATA) {
        handle = event.meta_data.sensor;
    }
    SensorInterface* sensor = mSensorMap.valueFor(handle);
    return sensor != NULL && sensor->getSensor().isWakeUpSensor();
}

Vector<Sensor> SensorService::getSensorList()
{
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sensors", value, "0");
    const Vector<Sensor>& initialSensorList = (atoi(value)) ?
            mUserSensorListDebug : mUserSensorList;
    Vector<Sensor> accessibleSensorList;
    for (size_t i = 0; i < initialSensorList.size(); i++) {
        Sensor sensor = initialSensorList[i];
        if (canAccessSensor(sensor)) {
            accessibleSensorList.add(sensor);
        } else {
            String8 infoMessage;
            infoMessage.appendFormat(
                    "Skipped sensor %s because it requires permission %s",
                    sensor.getName().string(),
                    sensor.getRequiredPermission().string());
            ALOGI(infoMessage.string());
        }
    }
    return accessibleSensorList;
}

sp<ISensorEventConnection> SensorService::createSensorEventConnection()
{
    uid_t uid = IPCThreadState::self()->getCallingUid();
    sp<SensorEventConnection> result(new SensorEventConnection(this, uid));
    return result;
}

void SensorService::cleanupConnection(SensorEventConnection* c)
{
    Mutex::Autolock _l(mLock);
    const wp<SensorEventConnection> connection(c);
    size_t size = mActiveSensors.size();
    ALOGD_IF(DEBUG_CONNECTIONS, "%zu active sensors", size);
    for (size_t i=0 ; i<size ; ) {
        int handle = mActiveSensors.keyAt(i);
        if (c->hasSensor(handle)) {
            ALOGD_IF(DEBUG_CONNECTIONS, "%zu: disabling handle=0x%08x", i, handle);
            SensorInterface* sensor = mSensorMap.valueFor( handle );
            ALOGE_IF(!sensor, "mSensorMap[handle=0x%08x] is null!", handle);
            if (sensor) {
                sensor->activate(c, false);
            }
        }
        SensorRecord* rec = mActiveSensors.valueAt(i);
        ALOGE_IF(!rec, "mActiveSensors[%zu] is null (handle=0x%08x)!", i, handle);
        ALOGD_IF(DEBUG_CONNECTIONS,
                "removing connection %p for sensor[%zu].handle=0x%08x",
                c, i, handle);

        if (rec && rec->removeConnection(connection)) {
            ALOGD_IF(DEBUG_CONNECTIONS, "... and it was the last connection");
            mActiveSensors.removeItemsAt(i, 1);
            mActiveVirtualSensors.removeItem(handle);
            delete rec;
            size--;
        } else {
            i++;
        }
    }
    mActiveConnections.remove(connection);
    BatteryService::cleanup(c->getUid());
    if (c->needsWakeLock()) {
        checkWakeLockStateLocked();
    }
}

Sensor SensorService::getSensorFromHandle(int handle) const {
    return mSensorMap.valueFor(handle)->getSensor();
}

status_t SensorService::enable(const sp<SensorEventConnection>& connection,
        int handle, nsecs_t samplingPeriodNs,  nsecs_t maxBatchReportLatencyNs, int reservedFlags)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    SensorInterface* sensor = mSensorMap.valueFor(handle);
    if (sensor == NULL) {
        return BAD_VALUE;
    }

    if (!verifyCanAccessSensor(sensor->getSensor(), "Tried enabling")) {
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mLock);
    SensorRecord* rec = mActiveSensors.valueFor(handle);
    if (rec == 0) {
        rec = new SensorRecord(connection);
        mActiveSensors.add(handle, rec);
        if (sensor->isVirtual()) {
            mActiveVirtualSensors.add(handle, sensor);
        }
    } else {
        if (rec->addConnection(connection)) {
            // this sensor is already activated, but we are adding a connection that uses it.
            // Immediately send down the last known value of the requested sensor if it's not a
            // "continuous" sensor.
            if (sensor->getSensor().getReportingMode() == AREPORTING_MODE_ON_CHANGE) {
                // NOTE: The wake_up flag of this event may get set to
                // WAKE_UP_SENSOR_EVENT_NEEDS_ACK if this is a wake_up event.
                sensors_event_t& event(mLastEventSeen.editValueFor(handle));
                if (event.version == sizeof(sensors_event_t)) {
                    if (isWakeUpSensorEvent(event) && !mWakeLockAcquired) {
                        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_NAME);
                        mWakeLockAcquired = true;
                        ALOGD_IF(DEBUG_CONNECTIONS, "acquired wakelock for on_change sensor %s",
                                                        WAKE_LOCK_NAME);
                    }
                    connection->sendEvents(&event, 1, NULL);
                    if (!connection->needsWakeLock() && mWakeLockAcquired) {
                        checkWakeLockStateLocked();
                    }
                }
            }
        }
    }

    if (connection->addSensor(handle)) {
        BatteryService::enableSensor(connection->getUid(), handle);
        // the sensor was added (which means it wasn't already there)
        // so, see if this connection becomes active
        if (mActiveConnections.indexOf(connection) < 0) {
            mActiveConnections.add(connection);
        }
    } else {
        ALOGW("sensor %08x already enabled in connection %p (ignoring)",
            handle, connection.get());
    }

    nsecs_t minDelayNs = sensor->getSensor().getMinDelayNs();
    if (samplingPeriodNs < minDelayNs) {
        samplingPeriodNs = minDelayNs;
    }

    ALOGD_IF(DEBUG_CONNECTIONS, "Calling batch handle==%d flags=%d rate=%" PRId64 " timeout== %" PRId64,
             handle, reservedFlags, samplingPeriodNs, maxBatchReportLatencyNs);

    status_t err = sensor->batch(connection.get(), handle, reservedFlags, samplingPeriodNs,
                                 maxBatchReportLatencyNs);
    if (err == NO_ERROR) {
        connection->setFirstFlushPending(handle, true);
        status_t err_flush = sensor->flush(connection.get(), handle);
        // Flush may return error if the sensor is not activated or the underlying h/w sensor does
        // not support flush.
        if (err_flush != NO_ERROR) {
            connection->setFirstFlushPending(handle, false);
        }
    }

    if (err == NO_ERROR) {
        ALOGD_IF(DEBUG_CONNECTIONS, "Calling activate on %d", handle);
        err = sensor->activate(connection.get(), true);
    }

    if (err == NO_ERROR && sensor->getSensor().isWakeUpSensor()) {
        // Add the file descriptor to the Looper for receiving acknowledgments;
        int ret = mLooper->addFd(connection->getSensorChannel()->getSendFd(), 0,
                                        ALOOPER_EVENT_INPUT, connection.get(), NULL);
    }

    if (err != NO_ERROR) {
        // batch/activate has failed, reset our state.
        cleanupWithoutDisableLocked(connection, handle);
    }
    return err;
}

status_t SensorService::disable(const sp<SensorEventConnection>& connection,
        int handle)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    Mutex::Autolock _l(mLock);
    status_t err = cleanupWithoutDisableLocked(connection, handle);
    if (err == NO_ERROR) {
        SensorInterface* sensor = mSensorMap.valueFor(handle);
        err = sensor ? sensor->activate(connection.get(), false) : status_t(BAD_VALUE);
    }
    return err;
}

status_t SensorService::cleanupWithoutDisable(
        const sp<SensorEventConnection>& connection, int handle) {
    Mutex::Autolock _l(mLock);
    return cleanupWithoutDisableLocked(connection, handle);
}

status_t SensorService::cleanupWithoutDisableLocked(
        const sp<SensorEventConnection>& connection, int handle) {
    SensorRecord* rec = mActiveSensors.valueFor(handle);
    if (rec) {
        // see if this connection becomes inactive
        if (connection->removeSensor(handle)) {
            BatteryService::disableSensor(connection->getUid(), handle);
        }
        if (connection->hasAnySensor() == false) {
            mActiveConnections.remove(connection);
        }
        // see if this sensor becomes inactive
        if (rec->removeConnection(connection)) {
            mActiveSensors.removeItem(handle);
            mActiveVirtualSensors.removeItem(handle);
            delete rec;
        }
        return NO_ERROR;
    }
    return BAD_VALUE;
}

status_t SensorService::setEventRate(const sp<SensorEventConnection>& connection,
        int handle, nsecs_t ns)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    SensorInterface* sensor = mSensorMap.valueFor(handle);
    if (!sensor)
        return BAD_VALUE;

    if (!verifyCanAccessSensor(sensor->getSensor(), "Tried configuring")) {
        return BAD_VALUE;
    }

    if (ns < 0)
        return BAD_VALUE;

    nsecs_t minDelayNs = sensor->getSensor().getMinDelayNs();
    if (ns < minDelayNs) {
        ns = minDelayNs;
    }

    return sensor->setDelay(connection.get(), handle, ns);
}

status_t SensorService::flushSensor(const sp<SensorEventConnection>& connection,
                                    int handle) {
    if (mInitCheck != NO_ERROR) return mInitCheck;
    SensorInterface* sensor = mSensorMap.valueFor(handle);
    if (sensor == NULL) {
        return BAD_VALUE;
    }

    if (!verifyCanAccessSensor(sensor->getSensor(), "Tried flushing")) {
        return BAD_VALUE;
    }

    if (sensor->getSensor().getReportingMode() == AREPORTING_MODE_ONE_SHOT) {
        ALOGE("flush called on a one-shot sensor");
        return INVALID_OPERATION;
    }
    return sensor->flush(connection.get(), handle);
}


bool SensorService::canAccessSensor(const Sensor& sensor) {
    return (sensor.getRequiredPermission().isEmpty()) ||
            PermissionCache::checkCallingPermission(String16(sensor.getRequiredPermission()));
}

bool SensorService::verifyCanAccessSensor(const Sensor& sensor, const char* operation) {
    if (canAccessSensor(sensor)) {
        return true;
    } else {
        String8 errorMessage;
        errorMessage.appendFormat(
                "%s a sensor (%s) without holding its required permission: %s",
                operation,
                sensor.getName().string(),
                sensor.getRequiredPermission().string());
        return false;
    }
}

void SensorService::checkWakeLockState() {
    Mutex::Autolock _l(mLock);
    checkWakeLockStateLocked();
}

void SensorService::checkWakeLockStateLocked() {
    if (!mWakeLockAcquired) {
        return;
    }
    bool releaseLock = true;
    for (size_t i=0 ; i<mActiveConnections.size() ; i++) {
        sp<SensorEventConnection> connection(mActiveConnections[i].promote());
        if (connection != 0) {
            if (connection->needsWakeLock()) {
                releaseLock = false;
                break;
            }
        }
    }
    if (releaseLock) {
        ALOGD_IF(DEBUG_CONNECTIONS, "releasing wakelock %s", WAKE_LOCK_NAME);
        release_wake_lock(WAKE_LOCK_NAME);
        mWakeLockAcquired = false;
    }
}
// ---------------------------------------------------------------------------
SensorService::SensorRecord::SensorRecord(
        const sp<SensorEventConnection>& connection)
{
    mConnections.add(connection);
}

bool SensorService::SensorRecord::addConnection(
        const sp<SensorEventConnection>& connection)
{
    if (mConnections.indexOf(connection) < 0) {
        mConnections.add(connection);
        return true;
    }
    return false;
}

bool SensorService::SensorRecord::removeConnection(
        const wp<SensorEventConnection>& connection)
{
    ssize_t index = mConnections.indexOf(connection);
    if (index >= 0) {
        mConnections.removeItemsAt(index, 1);
    }
    return mConnections.size() ? false : true;
}

// ---------------------------------------------------------------------------

SensorService::SensorEventConnection::SensorEventConnection(
        const sp<SensorService>& service, uid_t uid)
    : mService(service), mUid(uid), mWakeLockRefCount(0), mEventCache(NULL), mCacheSize(0),
      mMaxCacheSize(0) {
    const SensorDevice& device(SensorDevice::getInstance());
    if (device.getHalDeviceVersion() >= SENSORS_DEVICE_API_VERSION_1_1) {
        // Increase socket buffer size to a max of 100 KB for batching capabilities.
        mChannel = new BitTube(mService->mSocketBufferSize);
    } else {
        mChannel = new BitTube(SOCKET_BUFFER_SIZE_NON_BATCHED);
    }
#if DEBUG_CONNECTIONS
    mEventsReceived = mEventsSentFromCache = mEventsSent = 0;
#endif
}

SensorService::SensorEventConnection::~SensorEventConnection() {
    ALOGD_IF(DEBUG_CONNECTIONS, "~SensorEventConnection(%p)", this);
    if (mEventCache != NULL) {
        delete mEventCache;
    }
    mService->cleanupConnection(this);
}

void SensorService::SensorEventConnection::onFirstRef() {
    LooperCallback::onFirstRef();
}

bool SensorService::SensorEventConnection::needsWakeLock() {
    Mutex::Autolock _l(mConnectionLock);
    return mWakeLockRefCount > 0;
}

void SensorService::SensorEventConnection::dump(String8& result) {
    Mutex::Autolock _l(mConnectionLock);
    result.appendFormat("\t %d WakeLockRefCount \n", mWakeLockRefCount);
    for (size_t i = 0; i < mSensorInfo.size(); ++i) {
        const FlushInfo& flushInfo = mSensorInfo.valueAt(i);
        result.appendFormat("\t %s | status: %s | pending flush events %d | flush calls %d| uid %d|"
                            "cache size: %d max cache size %d\n",
                            mService->getSensorName(mSensorInfo.keyAt(i)).string(),
                            flushInfo.mFirstFlushPending ? "First flush pending" :
                                                           "active",
                            flushInfo.mPendingFlushEventsToSend,
                            flushInfo.mNumFlushCalls,
                            mUid,
                            mCacheSize,
                            mMaxCacheSize);
#if DEBUG_CONNECTIONS
        result.appendFormat("\t events recvd: %d | sent %d | cache %d | dropped %d\n",
                                        mEventsReceived,
                                        mEventsSent,
                                        mEventsSentFromCache,
                                        mEventsReceived - (mEventsSentFromCache +
                                                           mEventsSent + mCacheSize));
#endif

    }
}

bool SensorService::SensorEventConnection::addSensor(int32_t handle) {
    Mutex::Autolock _l(mConnectionLock);
    if (!verifyCanAccessSensor(mService->getSensorFromHandle(handle), "Tried adding")) {
        return false;
    }
    if (mSensorInfo.indexOfKey(handle) < 0) {
        mSensorInfo.add(handle, FlushInfo());
        return true;
    }
    return false;
}

bool SensorService::SensorEventConnection::removeSensor(int32_t handle) {
    Mutex::Autolock _l(mConnectionLock);
    if (mSensorInfo.removeItem(handle) >= 0) {
        return true;
    }
    return false;
}

bool SensorService::SensorEventConnection::hasSensor(int32_t handle) const {
    Mutex::Autolock _l(mConnectionLock);
    return mSensorInfo.indexOfKey(handle) >= 0;
}

bool SensorService::SensorEventConnection::hasAnySensor() const {
    Mutex::Autolock _l(mConnectionLock);
    return mSensorInfo.size() ? true : false;
}

void SensorService::SensorEventConnection::setFirstFlushPending(int32_t handle,
                                bool value) {
    Mutex::Autolock _l(mConnectionLock);
    ssize_t index = mSensorInfo.indexOfKey(handle);
    if (index >= 0) {
        FlushInfo& flushInfo = mSensorInfo.editValueAt(index);
        flushInfo.mFirstFlushPending = value;
    }
}

status_t SensorService::SensorEventConnection::sendEvents(
        sensors_event_t const* buffer, size_t numEvents,
        sensors_event_t* scratch) {
    // filter out events not for this connection
    size_t count = 0;
    Mutex::Autolock _l(mConnectionLock);
    if (scratch) {
        size_t i=0;
        while (i<numEvents) {
            int32_t curr = buffer[i].sensor;
            if (buffer[i].type == SENSOR_TYPE_META_DATA) {
                ALOGD_IF(DEBUG_CONNECTIONS, "flush complete event sensor==%d ",
                         buffer[i].meta_data.sensor);
                // Setting curr to the correct sensor to ensure the sensor events per connection are
                // filtered correctly. buffer[i].sensor is zero for meta_data events.
                curr = buffer[i].meta_data.sensor;
            }
            ssize_t index = mSensorInfo.indexOfKey(curr);
            // Check if this connection has registered for this sensor. If not continue to the
            // next sensor_event.
            if (index < 0) {
                ++i;
                continue;
            }

            // Check if there is a pending flush_complete event for this sensor on this connection.
            FlushInfo& flushInfo = mSensorInfo.editValueAt(index);
            if (buffer[i].type == SENSOR_TYPE_META_DATA) {
                if (flushInfo.mFirstFlushPending == true) {
                    // This is the first flush before activate is called. Events can now be sent for
                    // this sensor on this connection.
                    ALOGD_IF(DEBUG_CONNECTIONS, "First flush event for sensor==%d ",
                                                    buffer[i].meta_data.sensor);
                    flushInfo.mFirstFlushPending = false;
                    ++i;
                    continue;
                }
            }

            // If there is a pending flush complete event for this sensor on this connection,
            // ignore the event and proceed to the next.
            if (flushInfo.mFirstFlushPending) {
                ++i;
                continue;
            }

            do {
                if (buffer[i].type == SENSOR_TYPE_META_DATA) {
                    // Send flush complete event only if flush() has been explicitly called by
                    // this app else ignore.
                    if (flushInfo.mNumFlushCalls > 0) {
                        scratch[count++] = buffer[i];
                        flushInfo.mNumFlushCalls--;
                    }
                    ++i;
                } else {
                    // Regular sensor event, just copy it to the scratch buffer.
                    scratch[count++] = buffer[i++];
                }
            } while ((i<numEvents) && ((buffer[i].sensor == curr) ||
                                       (buffer[i].type == SENSOR_TYPE_META_DATA  &&
                                        buffer[i].meta_data.sensor == curr)));
        }
    } else {
        scratch = const_cast<sensors_event_t *>(buffer);
        count = numEvents;
    }

    // Early return if there are no events for this connection.
    if (count == 0) {
        return status_t(NO_ERROR);
    }

#if DEBUG_CONNECTIONS
     mEventsReceived += count;
#endif
    if (mCacheSize != 0) {
        // There are some events in the cache which need to be sent first. Copy this buffer to
        // the end of cache.
        if (mCacheSize + count <= mMaxCacheSize) {
            memcpy(&mEventCache[mCacheSize], scratch, count * sizeof(sensors_event_t));
            mCacheSize += count;
        } else {
            // Some events need to be dropped.
            int remaningCacheSize = mMaxCacheSize - mCacheSize;
            if (remaningCacheSize != 0) {
                memcpy(&mEventCache[mCacheSize], scratch,
                                                remaningCacheSize * sizeof(sensors_event_t));
            }
            int numEventsDropped = count - remaningCacheSize;
            countFlushCompleteEventsLocked(mEventCache, numEventsDropped);
            // Drop the first "numEventsDropped" in the cache.
            memmove(mEventCache, &mEventCache[numEventsDropped],
                    (mCacheSize - numEventsDropped) * sizeof(sensors_event_t));

            // Copy the remainingEvents in scratch buffer to the end of cache.
            memcpy(&mEventCache[mCacheSize - numEventsDropped], scratch + remaningCacheSize,
                                            numEventsDropped * sizeof(sensors_event_t));
        }
        return status_t(NO_ERROR);
    }

    int numWakeUpSensorEvents = countWakeUpSensorEventsLocked(scratch, count);
    mWakeLockRefCount += numWakeUpSensorEvents;

    // NOTE: ASensorEvent and sensors_event_t are the same type.
    ssize_t size = SensorEventQueue::write(mChannel,
                                    reinterpret_cast<ASensorEvent const*>(scratch), count);
    if (size < 0) {
        // Write error, copy events to local cache.
        mWakeLockRefCount -= numWakeUpSensorEvents;
        if (mEventCache == NULL) {
            mMaxCacheSize = computeMaxCacheSizeLocked();
            mEventCache = new sensors_event_t[mMaxCacheSize];
            mCacheSize = 0;
        }
        memcpy(&mEventCache[mCacheSize], scratch, count * sizeof(sensors_event_t));
        mCacheSize += count;

        // Add this file descriptor to the looper to get a callback when this fd is available for
        // writing.
        mService->getLooper()->addFd(mChannel->getSendFd(), 0,
                ALOOPER_EVENT_OUTPUT | ALOOPER_EVENT_INPUT, this, NULL);
        return size;
    }

#if DEBUG_CONNECTIONS
    if (size > 0) {
        mEventsSent += count;
    }
#endif

    return size < 0 ? status_t(size) : status_t(NO_ERROR);
}

void SensorService::SensorEventConnection::writeToSocketFromCacheLocked() {
    // At a time write at most half the size of the receiver buffer in SensorEventQueue.
    const int maxWriteSize = SensorEventQueue::MAX_RECEIVE_BUFFER_EVENT_COUNT/2;
    // Send pending flush events (if any) before sending events from the buffer.
    {
        ASensorEvent flushCompleteEvent;
        flushCompleteEvent.type = SENSOR_TYPE_META_DATA;
        flushCompleteEvent.sensor = 0;
        // Loop through all the sensors for this connection and check if there are any pending
        // flush complete events to be sent.
        for (size_t i = 0; i < mSensorInfo.size(); ++i) {
            FlushInfo& flushInfo = mSensorInfo.editValueAt(i);
            while (flushInfo.mPendingFlushEventsToSend > 0) {
                flushCompleteEvent.meta_data.sensor = mSensorInfo.keyAt(i);
                ssize_t size = SensorEventQueue::write(mChannel, &flushCompleteEvent, 1);
                if (size < 0) {
                    return;
                }
                ALOGD_IF(DEBUG_CONNECTIONS, "sent dropped flush complete event==%d ",
                                                flushCompleteEvent.meta_data.sensor);
                flushInfo.mPendingFlushEventsToSend--;
            }
        }
    }
    // Write "count" events at a time.
    for (int numEventsSent = 0; numEventsSent < mCacheSize;) {
        const int count = (mCacheSize - numEventsSent) < maxWriteSize ?
                                        mCacheSize - numEventsSent : maxWriteSize;
        int numWakeUpSensorEvents =
                  countWakeUpSensorEventsLocked(mEventCache + numEventsSent, count);
        mWakeLockRefCount += numWakeUpSensorEvents;

        ssize_t size = SensorEventQueue::write(mChannel,
                          reinterpret_cast<ASensorEvent const*>(mEventCache + numEventsSent),
                          count);
        if (size < 0) {
            memmove(mEventCache, &mEventCache[numEventsSent],
                                 (mCacheSize - numEventsSent) * sizeof(sensors_event_t));
            ALOGD_IF(DEBUG_CONNECTIONS, "wrote %d events from cache size==%d ",
                                            numEventsSent, mCacheSize);
            mCacheSize -= numEventsSent;
            mWakeLockRefCount -= numWakeUpSensorEvents;
            return;
        }
        numEventsSent += count;
#if DEBUG_CONNECTIONS
        mEventsSentFromCache += count;
#endif
    }
    ALOGD_IF(DEBUG_CONNECTIONS, "wrote all events from cache size=%d ", mCacheSize);
    // All events from the cache have been sent. Reset cache size to zero.
    mCacheSize = 0;
    // Poll only for ALOOPER_EVENT_INPUT(read) on the file descriptor.
    mService->getLooper()->addFd(mChannel->getSendFd(), 0, ALOOPER_EVENT_INPUT, this, NULL);
}

void SensorService::SensorEventConnection::countFlushCompleteEventsLocked(
                sensors_event_t* scratch, const int numEventsDropped) {
    ALOGD_IF(DEBUG_CONNECTIONS, "dropping %d events ", numEventsDropped);
    // Count flushComplete events in the events that are about to the dropped. These will be sent
    // separately before the next batch of events.
    for (int j = 0; j < numEventsDropped; ++j) {
        if (scratch[j].type == SENSOR_TYPE_META_DATA) {
            FlushInfo& flushInfo = mSensorInfo.editValueFor(scratch[j].meta_data.sensor);
            flushInfo.mPendingFlushEventsToSend++;
            ALOGD_IF(DEBUG_CONNECTIONS, "increment pendingFlushCount %d",
                     flushInfo.mPendingFlushEventsToSend);
        }
    }
    return;
}

int SensorService::SensorEventConnection::countWakeUpSensorEventsLocked(
                       sensors_event_t* scratch, const int count) {
    for (int i = 0; i < count; ++i) {
        if (mService->isWakeUpSensorEvent(scratch[i])) {
            scratch[i].flags |= WAKE_UP_SENSOR_EVENT_NEEDS_ACK;
            return 1;
        }
    }
    return 0;
}

sp<BitTube> SensorService::SensorEventConnection::getSensorChannel() const
{
    return mChannel;
}

status_t SensorService::SensorEventConnection::enableDisable(
        int handle, bool enabled, nsecs_t samplingPeriodNs, nsecs_t maxBatchReportLatencyNs,
        int reservedFlags)
{
    status_t err;
    if (enabled) {
        err = mService->enable(this, handle, samplingPeriodNs, maxBatchReportLatencyNs,
                               reservedFlags);

    } else {
        err = mService->disable(this, handle);
    }
    return err;
}

status_t SensorService::SensorEventConnection::setEventRate(
        int handle, nsecs_t samplingPeriodNs)
{
    return mService->setEventRate(this, handle, samplingPeriodNs);
}

status_t  SensorService::SensorEventConnection::flush() {
    SensorDevice& dev(SensorDevice::getInstance());
    const int halVersion = dev.getHalDeviceVersion();
    Mutex::Autolock _l(mConnectionLock);
    status_t err(NO_ERROR);
    // Loop through all sensors for this connection and call flush on each of them.
    for (size_t i = 0; i < mSensorInfo.size(); ++i) {
        const int handle = mSensorInfo.keyAt(i);
        FlushInfo& flushInfo = mSensorInfo.editValueFor(handle);
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_1 || mService->isVirtualSensor(handle)) {
            // For older devices just increment pending flush count which will send a trivial
            // flush complete event.
            flushInfo.mPendingFlushEventsToSend++;
        } else {
            status_t err_flush = mService->flushSensor(this, handle);
            if (err_flush == NO_ERROR) {
                flushInfo.mNumFlushCalls++;
            } else {
                ALOGE("Flush error handle=%d %s", handle, strerror(-err_flush));
            }
            err = (err_flush != NO_ERROR) ? err_flush : err;
        }
    }
    return err;
}

int SensorService::SensorEventConnection::handleEvent(int fd, int events, void* data) {
    if (events & ALOOPER_EVENT_HANGUP || events & ALOOPER_EVENT_ERROR) {
        return 0;
    }

    if (events & ALOOPER_EVENT_INPUT) {
        char buf;
        ssize_t ret = ::recv(fd, &buf, sizeof(buf), MSG_DONTWAIT);

        {
           Mutex::Autolock _l(mConnectionLock);
           --mWakeLockRefCount;
        }
        // Check if wakelock can be released by sensorservice. mConnectionLock needs to be released
        // here as checkWakeLockState() will need it.
        if (mWakeLockRefCount == 0) {
            mService->checkWakeLockState();
        }
        // continue getting callbacks.
        return 1;
    }

    if (events & ALOOPER_EVENT_OUTPUT) {
        // send sensor data that is stored in mEventCache.
        Mutex::Autolock _l(mConnectionLock);
        writeToSocketFromCacheLocked();
    }
    return 1;
}

int SensorService::SensorEventConnection::computeMaxCacheSizeLocked() const {
    int fifoWakeUpSensors = 0;
    int fifoNonWakeUpSensors = 0;
    for (size_t i = 0; i < mSensorInfo.size(); ++i) {
        const Sensor& sensor = mService->getSensorFromHandle(mSensorInfo.keyAt(i));
        if (sensor.getFifoReservedEventCount() == sensor.getFifoMaxEventCount()) {
            // Each sensor has a reserved fifo. Sum up the fifo sizes for all wake up sensors and
            // non wake_up sensors.
            if (sensor.isWakeUpSensor()) {
                fifoWakeUpSensors += sensor.getFifoReservedEventCount();
            } else {
                fifoNonWakeUpSensors += sensor.getFifoReservedEventCount();
            }
        } else {
            // Shared fifo. Compute the max of the fifo sizes for wake_up and non_wake up sensors.
            if (sensor.isWakeUpSensor()) {
                fifoWakeUpSensors = fifoWakeUpSensors > sensor.getFifoMaxEventCount() ?
                                          fifoWakeUpSensors : sensor.getFifoMaxEventCount();

            } else {
                fifoNonWakeUpSensors = fifoNonWakeUpSensors > sensor.getFifoMaxEventCount() ?
                                          fifoNonWakeUpSensors : sensor.getFifoMaxEventCount();

            }
        }
   }
   if (fifoWakeUpSensors + fifoNonWakeUpSensors == 0) {
       // It is extremely unlikely that there is a write failure in non batch mode. Return a cache
       // size of 100.
       ALOGI("Write failure in non-batch mode");
       return 100;
   }
   return fifoWakeUpSensors + fifoNonWakeUpSensors;
}

// ---------------------------------------------------------------------------
}; // namespace android

