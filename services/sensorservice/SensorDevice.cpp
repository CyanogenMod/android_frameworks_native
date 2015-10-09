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

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/Singleton.h>

#include <binder/BinderService.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>

#include <hardware/sensors.h>

#include "SensorDevice.h"
#include "SensorService.h"

namespace android {
// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(SensorDevice)

SensorDevice::SensorDevice()
    :  mSensorDevice(0),
       mSensorModule(0)
{
    status_t err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
            (hw_module_t const**)&mSensorModule);

    ALOGE_IF(err, "couldn't load %s module (%s)",
            SENSORS_HARDWARE_MODULE_ID, strerror(-err));

    if (mSensorModule) {
        err = sensors_open_1(&mSensorModule->common, &mSensorDevice);

        ALOGE_IF(err, "couldn't open device for module %s (%s)",
                SENSORS_HARDWARE_MODULE_ID, strerror(-err));

        if (mSensorDevice) {
            if (mSensorDevice->common.version == SENSORS_DEVICE_API_VERSION_1_1 ||
                mSensorDevice->common.version == SENSORS_DEVICE_API_VERSION_1_2) {
                ALOGE(">>>> WARNING <<< Upgrade sensor HAL to version 1_3");
            }

            sensor_t const* list;
            ssize_t count = mSensorModule->get_sensors_list(mSensorModule, &list);
            mActivationCount.setCapacity(count);
            Info model;
            for (size_t i=0 ; i<size_t(count) ; i++) {
                mActivationCount.add(list[i].handle, model);
                mSensorDevice->activate(
                        reinterpret_cast<struct sensors_poll_device_t *>(mSensorDevice),
                        list[i].handle, 0);
            }
        }
    }
}

void SensorDevice::dump(String8& result)
{
    if (!mSensorModule) return;
    sensor_t const* list;
    ssize_t count = mSensorModule->get_sensors_list(mSensorModule, &list);

    result.appendFormat("halVersion 0x%08x\n", getHalDeviceVersion());
    result.appendFormat("%d h/w sensors:\n", int(count));

    Mutex::Autolock _l(mLock);
    for (size_t i=0 ; i<size_t(count) ; i++) {
        const Info& info = mActivationCount.valueFor(list[i].handle);
        if (info.batchParams.isEmpty()) continue;
        result.appendFormat("handle=0x%08x, active-count=%zu, batch_period(ms)={ ", list[i].handle,
                            info.batchParams.size());
        for (size_t j = 0; j < info.batchParams.size(); j++) {
            const BatchParams& params = info.batchParams.valueAt(j);
            result.appendFormat("%4.1f%s", params.batchDelay / 1e6f,
                                j < info.batchParams.size() - 1 ? ", " : "");
        }
        result.appendFormat(" }, selected=%4.1f ms\n", info.bestBatchParams.batchDelay / 1e6f);

        result.appendFormat("handle=0x%08x, active-count=%zu, batch_timeout(ms)={ ", list[i].handle,
                            info.batchParams.size());
        for (size_t j = 0; j < info.batchParams.size(); j++) {
            BatchParams params = info.batchParams.valueAt(j);
            result.appendFormat("%4.1f%s", params.batchTimeout / 1e6f,
                                j < info.batchParams.size() - 1 ? ", " : "");
        }
        result.appendFormat(" }, selected=%4.1f ms\n", info.bestBatchParams.batchTimeout / 1e6f);
    }
}

ssize_t SensorDevice::getSensorList(sensor_t const** list) {
    if (!mSensorModule) return NO_INIT;
    ssize_t count = mSensorModule->get_sensors_list(mSensorModule, list);
    return count;
}

status_t SensorDevice::initCheck() const {
    return mSensorDevice && mSensorModule ? NO_ERROR : NO_INIT;
}

ssize_t SensorDevice::poll(sensors_event_t* buffer, size_t count) {
    if (!mSensorDevice) return NO_INIT;
    ssize_t c;
    do {
        c = mSensorDevice->poll(reinterpret_cast<struct sensors_poll_device_t *> (mSensorDevice),
                                buffer, count);
    } while (c == -EINTR);
    return c;
}

void SensorDevice::autoDisable(void *ident, int handle) {
    Info& info( mActivationCount.editValueFor(handle) );
    Mutex::Autolock _l(mLock);
    info.removeBatchParamsForIdent(ident);
}

status_t SensorDevice::activate(void* ident, int handle, int enabled)
{
    if (!mSensorDevice) return NO_INIT;
    status_t err(NO_ERROR);
    bool actuateHardware = false;

    Mutex::Autolock _l(mLock);
    Info& info( mActivationCount.editValueFor(handle) );

    ALOGD_IF(DEBUG_CONNECTIONS,
             "SensorDevice::activate: ident=%p, handle=0x%08x, enabled=%d, count=%zu",
             ident, handle, enabled, info.batchParams.size());

    if (enabled) {
        ALOGD_IF(DEBUG_CONNECTIONS, "enable index=%zd", info.batchParams.indexOfKey(ident));

        if (isClientDisabledLocked(ident)) {
            return INVALID_OPERATION;
        }

        if (info.batchParams.indexOfKey(ident) >= 0) {
          if (info.numActiveClients() == 1) {
              // This is the first connection, we need to activate the underlying h/w sensor.
              actuateHardware = true;
          }
        } else {
            // Log error. Every activate call should be preceded by a batch() call.
            ALOGE("\t >>>ERROR: activate called without batch");
        }
    } else {
        ALOGD_IF(DEBUG_CONNECTIONS, "disable index=%zd", info.batchParams.indexOfKey(ident));

        if (info.removeBatchParamsForIdent(ident) >= 0) {
            if (info.numActiveClients() == 0) {
                // This is the last connection, we need to de-activate the underlying h/w sensor.
                actuateHardware = true;
            } else {
                const int halVersion = getHalDeviceVersion();
                if (halVersion >= SENSORS_DEVICE_API_VERSION_1_1) {
                    // Call batch for this sensor with the previously calculated best effort
                    // batch_rate and timeout. One of the apps has unregistered for sensor
                    // events, and the best effort batch parameters might have changed.
                    ALOGD_IF(DEBUG_CONNECTIONS,
                             "\t>>> actuating h/w batch %d %d %" PRId64 " %" PRId64, handle,
                             info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                             info.bestBatchParams.batchTimeout);
                    mSensorDevice->batch(mSensorDevice, handle,info.bestBatchParams.flags,
                                         info.bestBatchParams.batchDelay,
                                         info.bestBatchParams.batchTimeout);
                }
            }
        } else {
            // sensor wasn't enabled for this ident
        }

        if (isClientDisabledLocked(ident)) {
            return NO_ERROR;
        }
    }

    if (actuateHardware) {
        ALOGD_IF(DEBUG_CONNECTIONS, "\t>>> actuating h/w activate handle=%d enabled=%d", handle,
                 enabled);
        err = mSensorDevice->activate(
                reinterpret_cast<struct sensors_poll_device_t *> (mSensorDevice), handle, enabled);
        ALOGE_IF(err, "Error %s sensor %d (%s)", enabled ? "activating" : "disabling", handle,
                 strerror(-err));

        if (err != NO_ERROR && enabled) {
            // Failure when enabling the sensor. Clean up on failure.
            info.removeBatchParamsForIdent(ident);
        }
    }

    // On older devices which do not support batch, call setDelay().
    if (getHalDeviceVersion() < SENSORS_DEVICE_API_VERSION_1_1 && info.numActiveClients() > 0) {
        ALOGD_IF(DEBUG_CONNECTIONS, "\t>>> actuating h/w setDelay %d %" PRId64, handle,
                 info.bestBatchParams.batchDelay);
        mSensorDevice->setDelay(
                reinterpret_cast<struct sensors_poll_device_t *>(mSensorDevice),
                handle, info.bestBatchParams.batchDelay);
    }
    return err;
}

status_t SensorDevice::batch(void* ident, int handle, int flags, int64_t samplingPeriodNs,
                             int64_t maxBatchReportLatencyNs) {
    if (!mSensorDevice) return NO_INIT;

    if (samplingPeriodNs < MINIMUM_EVENTS_PERIOD) {
        samplingPeriodNs = MINIMUM_EVENTS_PERIOD;
    }

    const int halVersion = getHalDeviceVersion();
    if (halVersion < SENSORS_DEVICE_API_VERSION_1_1 && maxBatchReportLatencyNs != 0) {
        // Batch is not supported on older devices return invalid operation.
        return INVALID_OPERATION;
    }

    ALOGD_IF(DEBUG_CONNECTIONS,
             "SensorDevice::batch: ident=%p, handle=0x%08x, flags=%d, period_ns=%" PRId64 " timeout=%" PRId64,
             ident, handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);

    Mutex::Autolock _l(mLock);
    Info& info(mActivationCount.editValueFor(handle));

    if (info.batchParams.indexOfKey(ident) < 0) {
        BatchParams params(flags, samplingPeriodNs, maxBatchReportLatencyNs);
        info.batchParams.add(ident, params);
    } else {
        // A batch has already been called with this ident. Update the batch parameters.
        info.setBatchParamsForIdent(ident, flags, samplingPeriodNs, maxBatchReportLatencyNs);
    }

    BatchParams prevBestBatchParams = info.bestBatchParams;
    // Find the minimum of all timeouts and batch_rates for this sensor.
    info.selectBatchParams();

    ALOGD_IF(DEBUG_CONNECTIONS,
             "\t>>> curr_period=%" PRId64 " min_period=%" PRId64
             " curr_timeout=%" PRId64 " min_timeout=%" PRId64,
             prevBestBatchParams.batchDelay, info.bestBatchParams.batchDelay,
             prevBestBatchParams.batchTimeout, info.bestBatchParams.batchTimeout);

    status_t err(NO_ERROR);
    // If the min period or min timeout has changed since the last batch call, call batch.
    if (prevBestBatchParams != info.bestBatchParams) {
        if (halVersion >= SENSORS_DEVICE_API_VERSION_1_1) {
            ALOGD_IF(DEBUG_CONNECTIONS, "\t>>> actuating h/w BATCH %d %d %" PRId64 " %" PRId64, handle,
                     info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                     info.bestBatchParams.batchTimeout);
            err = mSensorDevice->batch(mSensorDevice, handle, info.bestBatchParams.flags,
                                       info.bestBatchParams.batchDelay,
                                       info.bestBatchParams.batchTimeout);
        } else {
            // For older devices which do not support batch, call setDelay() after activate() is
            // called. Some older devices may not support calling setDelay before activate(), so
            // call setDelay in SensorDevice::activate() method.
        }
        if (err != NO_ERROR) {
            ALOGE("sensor batch failed %p %d %d %" PRId64 " %" PRId64 " err=%s",
                  mSensorDevice, handle,
                  info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                  info.bestBatchParams.batchTimeout, strerror(-err));
            info.removeBatchParamsForIdent(ident);
        }
    }
    return err;
}

status_t SensorDevice::setDelay(void* ident, int handle, int64_t samplingPeriodNs)
{
    if (!mSensorDevice) return NO_INIT;
    if (samplingPeriodNs < MINIMUM_EVENTS_PERIOD) {
        samplingPeriodNs = MINIMUM_EVENTS_PERIOD;
    }
    Mutex::Autolock _l(mLock);
    if (isClientDisabledLocked(ident)) return INVALID_OPERATION;
    Info& info( mActivationCount.editValueFor(handle) );
    // If the underlying sensor is NOT in continuous mode, setDelay() should return an error.
    // Calling setDelay() in batch mode is an invalid operation.
    if (info.bestBatchParams.batchTimeout != 0) {
      return INVALID_OPERATION;
    }
    ssize_t index = info.batchParams.indexOfKey(ident);
    if (index < 0) {
        return BAD_INDEX;
    }
    BatchParams& params = info.batchParams.editValueAt(index);
    params.batchDelay = samplingPeriodNs;
    info.selectBatchParams();
    return mSensorDevice->setDelay(reinterpret_cast<struct sensors_poll_device_t *>(mSensorDevice),
                                   handle, info.bestBatchParams.batchDelay);
}

int SensorDevice::getHalDeviceVersion() const {
    if (!mSensorDevice) return -1;
    return mSensorDevice->common.version;
}

status_t SensorDevice::flush(void* ident, int handle) {
    if (getHalDeviceVersion() < SENSORS_DEVICE_API_VERSION_1_1) {
        return INVALID_OPERATION;
    }
    if (isClientDisabled(ident)) return INVALID_OPERATION;
    ALOGD_IF(DEBUG_CONNECTIONS, "\t>>> actuating h/w flush %d", handle);
    return mSensorDevice->flush(mSensorDevice, handle);
}

bool SensorDevice::isClientDisabled(void* ident) {
    Mutex::Autolock _l(mLock);
    return isClientDisabledLocked(ident);
}

bool SensorDevice::isClientDisabledLocked(void* ident) {
    return mDisabledClients.indexOf(ident) >= 0;
}

void SensorDevice::enableAllSensors() {
    Mutex::Autolock _l(mLock);
    mDisabledClients.clear();
    const int halVersion = getHalDeviceVersion();
    for (size_t i = 0; i< mActivationCount.size(); ++i) {
        Info& info = mActivationCount.editValueAt(i);
        if (info.batchParams.isEmpty()) continue;
        info.selectBatchParams();
        const int sensor_handle = mActivationCount.keyAt(i);
        ALOGD_IF(DEBUG_CONNECTIONS, "\t>> reenable actuating h/w sensor enable handle=%d ",
                   sensor_handle);
        status_t err(NO_ERROR);
        if (halVersion > SENSORS_DEVICE_API_VERSION_1_0) {
            err = mSensorDevice->batch(mSensorDevice, sensor_handle,
                 info.bestBatchParams.flags, info.bestBatchParams.batchDelay,
                 info.bestBatchParams.batchTimeout);
            ALOGE_IF(err, "Error calling batch on sensor %d (%s)", sensor_handle, strerror(-err));
        }

        if (err == NO_ERROR) {
            err = mSensorDevice->activate(
                    reinterpret_cast<struct sensors_poll_device_t *>(mSensorDevice),
                    sensor_handle, 1);
            ALOGE_IF(err, "Error activating sensor %d (%s)", sensor_handle, strerror(-err));
        }

        if (halVersion <= SENSORS_DEVICE_API_VERSION_1_0) {
             err = mSensorDevice->setDelay(
                    reinterpret_cast<struct sensors_poll_device_t *>(mSensorDevice),
                    sensor_handle, info.bestBatchParams.batchDelay);
             ALOGE_IF(err, "Error calling setDelay sensor %d (%s)", sensor_handle, strerror(-err));
        }
    }
}

void SensorDevice::disableAllSensors() {
    Mutex::Autolock _l(mLock);
   for (size_t i = 0; i< mActivationCount.size(); ++i) {
        const Info& info = mActivationCount.valueAt(i);
        // Check if this sensor has been activated previously and disable it.
        if (info.batchParams.size() > 0) {
           const int sensor_handle = mActivationCount.keyAt(i);
           ALOGD_IF(DEBUG_CONNECTIONS, "\t>> actuating h/w sensor disable handle=%d ",
                   sensor_handle);
           mSensorDevice->activate(
                   reinterpret_cast<struct sensors_poll_device_t *> (mSensorDevice),
                   sensor_handle, 0);
           // Add all the connections that were registered for this sensor to the disabled
           // clients list.
           for (size_t j = 0; j < info.batchParams.size(); ++j) {
               mDisabledClients.add(info.batchParams.keyAt(j));
           }
        }
    }
}

status_t SensorDevice::injectSensorData(const sensors_event_t *injected_sensor_event) {
      ALOGD_IF(DEBUG_CONNECTIONS,
              "sensor_event handle=%d ts=%lld data=%.2f, %.2f, %.2f %.2f %.2f %.2f",
               injected_sensor_event->sensor,
               injected_sensor_event->timestamp, injected_sensor_event->data[0],
               injected_sensor_event->data[1], injected_sensor_event->data[2],
               injected_sensor_event->data[3], injected_sensor_event->data[4],
               injected_sensor_event->data[5]);
      if (getHalDeviceVersion() < SENSORS_DEVICE_API_VERSION_1_4) {
          return INVALID_OPERATION;
      }
      return mSensorDevice->inject_sensor_data(mSensorDevice, injected_sensor_event);
}

status_t SensorDevice::setMode(uint32_t mode) {
     if (getHalDeviceVersion() < SENSORS_DEVICE_API_VERSION_1_4) {
          return INVALID_OPERATION;
     }
     return mSensorModule->set_operation_mode(mode);
}

status_t SensorDevice::setSensorPhysicalData(const char* physicaldata)
{
    //ALOGD("SensorDevice::setSensorPhysicalData(%s)",physicaldata);
    Mutex::Autolock _l(mLock);
    if((mSensorModule->set_sensor_physical_data == NULL) || (getHalDeviceVersion() < SENSORS_DEVICE_API_VERSION_1_3_5))
    {
        return NO_INIT;
    }
    else
    {
        return mSensorModule->set_sensor_physical_data(physicaldata);
    }
}


// ---------------------------------------------------------------------------

int SensorDevice::Info::numActiveClients() {
    SensorDevice& device(SensorDevice::getInstance());
    int num = 0;
    for (size_t i = 0; i < batchParams.size(); ++i) {
        if (!device.isClientDisabledLocked(batchParams.keyAt(i))) {
            ++num;
        }
    }
    return num;
}

status_t SensorDevice::Info::setBatchParamsForIdent(void* ident, int flags,
                                                    int64_t samplingPeriodNs,
                                                    int64_t maxBatchReportLatencyNs) {
    ssize_t index = batchParams.indexOfKey(ident);
    if (index < 0) {
        ALOGE("Info::setBatchParamsForIdent(ident=%p, period_ns=%" PRId64 " timeout=%" PRId64 ") failed (%s)",
              ident, samplingPeriodNs, maxBatchReportLatencyNs, strerror(-index));
        return BAD_INDEX;
    }
    BatchParams& params = batchParams.editValueAt(index);
    params.flags = flags;
    params.batchDelay = samplingPeriodNs;
    params.batchTimeout = maxBatchReportLatencyNs;
    return NO_ERROR;
}

void SensorDevice::Info::selectBatchParams() {
    BatchParams bestParams(0, -1, -1);
    SensorDevice& device(SensorDevice::getInstance());

    for (size_t i = 0; i < batchParams.size(); ++i) {
        if (device.isClientDisabledLocked(batchParams.keyAt(i))) continue;
        BatchParams params = batchParams.valueAt(i);
        if (bestParams.batchDelay == -1 || params.batchDelay < bestParams.batchDelay) {
            bestParams.batchDelay = params.batchDelay;
        }
        if (bestParams.batchTimeout == -1 || params.batchTimeout < bestParams.batchTimeout) {
            bestParams.batchTimeout = params.batchTimeout;
        }
    }
    bestBatchParams = bestParams;
}

ssize_t SensorDevice::Info::removeBatchParamsForIdent(void* ident) {
    ssize_t idx = batchParams.removeItem(ident);
    if (idx >= 0) {
        selectBatchParams();
    }
    return idx;
}

// ---------------------------------------------------------------------------
}; // namespace android

