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
#include <stdint.h>
#include <sys/types.h>
#include <sys/limits.h>

#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/Flattenable.h>

#include <hardware/sensors.h>

#include <binder/AppOpsManager.h>
#include <binder/IServiceManager.h>

#include <gui/Sensor.h>
#include <log/log.h>

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

Sensor::Sensor()
    : mHandle(0), mType(0),
      mMinValue(0), mMaxValue(0), mResolution(0),
      mPower(0), mMinDelay(0), mFifoReservedEventCount(0), mFifoMaxEventCount(0),
      mMaxDelay(0), mFlags(0)
{
}

Sensor::Sensor(struct sensor_t const* hwSensor, int halVersion)
{
    mName = hwSensor->name;
    mVendor = hwSensor->vendor;
    mVersion = hwSensor->version;
    mHandle = hwSensor->handle;
    mType = hwSensor->type;
    mMinValue = 0;                      // FIXME: minValue
    mMaxValue = hwSensor->maxRange;     // FIXME: maxValue
    mResolution = hwSensor->resolution;
    mPower = hwSensor->power;
    mMinDelay = hwSensor->minDelay;
    mFlags = 0;

    // Set fifo event count zero for older devices which do not support batching. Fused
    // sensors also have their fifo counts set to zero.
    if (halVersion > SENSORS_DEVICE_API_VERSION_1_0) {
        mFifoReservedEventCount = hwSensor->fifoReservedEventCount;
        mFifoMaxEventCount = hwSensor->fifoMaxEventCount;
    } else {
        mFifoReservedEventCount = 0;
        mFifoMaxEventCount = 0;
    }

    if (halVersion >= SENSORS_DEVICE_API_VERSION_1_3) {
        if (hwSensor->maxDelay > INT_MAX) {
            // Max delay is declared as a 64 bit integer for 64 bit architectures. But it should
            // always fit in a 32 bit integer, log error and cap it to INT_MAX.
            ALOGE("Sensor maxDelay overflow error %s %" PRId64, mName.string(),
                  static_cast<int64_t>(hwSensor->maxDelay));
            mMaxDelay = INT_MAX;
        } else {
            mMaxDelay = static_cast<int32_t>(hwSensor->maxDelay);
        }
    } else {
        // For older hals set maxDelay to 0.
        mMaxDelay = 0;
    }

    // Ensure existing sensors have correct string type, required permissions and reporting mode.
    // Set reportingMode for all android defined sensor types, set wake-up flag only for proximity
    // sensor, significant motion, tilt, pick_up gesture, wake gesture and glance gesture on older
    // HALs. Newer HALs can define both wake-up and non wake-up proximity sensors.
    // All the OEM defined defined sensors have flags set to whatever is provided by the HAL.
    switch (mType) {
    case SENSOR_TYPE_ACCELEROMETER:
        mStringType = SENSOR_STRING_TYPE_ACCELEROMETER;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        mStringType = SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        break;
    case SENSOR_TYPE_GAME_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_GRAVITY:
        mStringType = SENSOR_STRING_TYPE_GRAVITY;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_GYROSCOPE:
        mStringType = SENSOR_STRING_TYPE_GYROSCOPE;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_HEART_RATE: {
        mStringType = SENSOR_STRING_TYPE_HEART_RATE;
#ifndef NO_SENSOR_PERMISSION_CHECK
        mRequiredPermission = SENSOR_PERMISSION_BODY_SENSORS;
        AppOpsManager appOps;
        mRequiredAppOp = appOps.permissionToOpCode(String16(SENSOR_PERMISSION_BODY_SENSORS));
#endif
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        } break;
    case SENSOR_TYPE_LIGHT:
        mStringType = SENSOR_STRING_TYPE_LIGHT;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        break;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
        mStringType = SENSOR_STRING_TYPE_LINEAR_ACCELERATION;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD:
        mStringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_ORIENTATION:
        mStringType = SENSOR_STRING_TYPE_ORIENTATION;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_PRESSURE:
        mStringType = SENSOR_STRING_TYPE_PRESSURE;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_PROXIMITY:
        mStringType = SENSOR_STRING_TYPE_PROXIMITY;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    case SENSOR_TYPE_RELATIVE_HUMIDITY:
        mStringType = SENSOR_STRING_TYPE_RELATIVE_HUMIDITY;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        break;
    case SENSOR_TYPE_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_ROTATION_VECTOR;
        mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
        break;
    case SENSOR_TYPE_SIGNIFICANT_MOTION:
        mStringType = SENSOR_STRING_TYPE_SIGNIFICANT_MOTION;
        mFlags |= SENSOR_FLAG_ONE_SHOT_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    case SENSOR_TYPE_STEP_COUNTER:
        mStringType = SENSOR_STRING_TYPE_STEP_COUNTER;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        break;
    case SENSOR_TYPE_STEP_DETECTOR:
        mStringType = SENSOR_STRING_TYPE_STEP_DETECTOR;
        mFlags |= SENSOR_FLAG_SPECIAL_REPORTING_MODE;
        break;
    case SENSOR_TYPE_TEMPERATURE:
        mStringType = SENSOR_STRING_TYPE_TEMPERATURE;
        mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
        break;
    case SENSOR_TYPE_TILT_DETECTOR:
        mStringType = SENSOR_STRING_TYPE_TILT_DETECTOR;
        mFlags |= SENSOR_FLAG_SPECIAL_REPORTING_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
         break;
    case SENSOR_TYPE_WAKE_GESTURE:
        mStringType = SENSOR_STRING_TYPE_WAKE_GESTURE;
        mFlags |= SENSOR_FLAG_ONE_SHOT_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    case SENSOR_TYPE_GLANCE_GESTURE:
        mStringType = SENSOR_STRING_TYPE_GLANCE_GESTURE;
        mFlags |= SENSOR_FLAG_ONE_SHOT_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    case SENSOR_TYPE_PICK_UP_GESTURE:
        mStringType = SENSOR_STRING_TYPE_PICK_UP_GESTURE;
        mFlags |= SENSOR_FLAG_ONE_SHOT_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    case SENSOR_TYPE_WRIST_TILT_GESTURE:
        mStringType = SENSOR_STRING_TYPE_WRIST_TILT_GESTURE;
        mFlags |= SENSOR_FLAG_SPECIAL_REPORTING_MODE;
        if (halVersion < SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags |= SENSOR_FLAG_WAKE_UP;
        }
        break;
    default:
        // Only pipe the stringType, requiredPermission and flags for custom sensors.
        if (halVersion > SENSORS_DEVICE_API_VERSION_1_0 && hwSensor->stringType) {
            mStringType = hwSensor->stringType;
        }
#ifndef NO_SENSOR_PERMISSION_CHECK
        if (halVersion > SENSORS_DEVICE_API_VERSION_1_0 && hwSensor->requiredPermission) {
            mRequiredPermission = hwSensor->requiredPermission;
            if (!strcmp(mRequiredPermission, SENSOR_PERMISSION_BODY_SENSORS)) {
                AppOpsManager appOps;
                mRequiredAppOp = appOps.permissionToOpCode(String16(SENSOR_PERMISSION_BODY_SENSORS));
            }
        }
#endif

        if (halVersion >= SENSORS_DEVICE_API_VERSION_1_3) {
            mFlags = static_cast<uint32_t>(hwSensor->flags);
        } else {
            // This is an OEM defined sensor on an older HAL. Use minDelay to determine the
            // reporting mode of the sensor.
            if (mMinDelay > 0) {
                mFlags |= SENSOR_FLAG_CONTINUOUS_MODE;
            } else if (mMinDelay == 0) {
                mFlags |= SENSOR_FLAG_ON_CHANGE_MODE;
            } else if (mMinDelay < 0) {
                mFlags |= SENSOR_FLAG_ONE_SHOT_MODE;
            }
        }
        break;
    }

    // Set DATA_INJECTION flag here. Defined in HAL 1_4.
    if (halVersion >= SENSORS_DEVICE_API_VERSION_1_4) {
        mFlags |= (hwSensor->flags & DATA_INJECTION_MASK);
    }

    // For the newer HALs log errors if reporting mask flags are set incorrectly.
    if (halVersion >= SENSORS_DEVICE_API_VERSION_1_3) {
        // Wake-up flag is set here.
        mFlags |= (hwSensor->flags & SENSOR_FLAG_WAKE_UP);
        if (mFlags != hwSensor->flags) {
            int actualReportingMode =
                 (hwSensor->flags & REPORTING_MODE_MASK) >> REPORTING_MODE_SHIFT;
            int expectedReportingMode = (mFlags & REPORTING_MODE_MASK) >> REPORTING_MODE_SHIFT;
            if (actualReportingMode != expectedReportingMode) {
                ALOGE("Reporting Mode incorrect: sensor %s handle=%d type=%d "
                       "actual=%d expected=%d",
                       mName.string(), mHandle, mType, actualReportingMode, expectedReportingMode);
            }

        }
    }

#ifndef NO_SENSOR_PERMISSION_CHECK
    if (mRequiredPermission.length() > 0) {
        // If the sensor is protected by a permission we need to know if it is
        // a runtime one to determine whether we can use the permission cache.
        sp<IBinder> binder = defaultServiceManager()->getService(String16("permission"));
        if (binder != 0) {
            sp<IPermissionController> permCtrl = interface_cast<IPermissionController>(binder);
            mRequiredPermissionRuntime = permCtrl->isRuntimePermission(
                    String16(mRequiredPermission));
        }
    }
#endif
}

Sensor::~Sensor()
{
}

const String8& Sensor::getName() const {
    return mName;
}

const String8& Sensor::getVendor() const {
    return mVendor;
}

int32_t Sensor::getHandle() const {
    return mHandle;
}

int32_t Sensor::getType() const {
    return mType;
}

float Sensor::getMinValue() const {
    return mMinValue;
}

float Sensor::getMaxValue() const {
    return mMaxValue;
}

float Sensor::getResolution() const {
    return mResolution;
}

float Sensor::getPowerUsage() const {
    return mPower;
}

int32_t Sensor::getMinDelay() const {
    return mMinDelay;
}

nsecs_t Sensor::getMinDelayNs() const {
    return getMinDelay() * 1000;
}

int32_t Sensor::getVersion() const {
    return mVersion;
}

uint32_t Sensor::getFifoReservedEventCount() const {
    return mFifoReservedEventCount;
}

uint32_t Sensor::getFifoMaxEventCount() const {
    return mFifoMaxEventCount;
}

const String8& Sensor::getStringType() const {
    return mStringType;
}

const String8& Sensor::getRequiredPermission() const {
    return mRequiredPermission;
}

bool Sensor::isRequiredPermissionRuntime() const {
    return mRequiredPermissionRuntime;
}

int32_t Sensor::getRequiredAppOp() const {
    return mRequiredAppOp;
}

int32_t Sensor::getMaxDelay() const {
    return mMaxDelay;
}

uint32_t Sensor::getFlags() const {
    return mFlags;
}

bool Sensor::isWakeUpSensor() const {
    return mFlags & SENSOR_FLAG_WAKE_UP;
}

int32_t Sensor::getReportingMode() const {
    return ((mFlags & REPORTING_MODE_MASK) >> REPORTING_MODE_SHIFT);
}

size_t Sensor::getFlattenedSize() const
{
    size_t fixedSize =
            sizeof(int32_t) * 3 +
            sizeof(float) * 4 +
            sizeof(int32_t) * 6 +
            sizeof(bool);

    size_t variableSize =
            sizeof(uint32_t) + FlattenableUtils::align<4>(mName.length()) +
            sizeof(uint32_t) + FlattenableUtils::align<4>(mVendor.length()) +
            sizeof(uint32_t) + FlattenableUtils::align<4>(mStringType.length()) +
            sizeof(uint32_t) + FlattenableUtils::align<4>(mRequiredPermission.length());

    return fixedSize + variableSize;
}

status_t Sensor::flatten(void* buffer, size_t size) const {
    if (size < getFlattenedSize()) {
        return NO_MEMORY;
    }

    flattenString8(buffer, size, mName);
    flattenString8(buffer, size, mVendor);
    FlattenableUtils::write(buffer, size, mVersion);
    FlattenableUtils::write(buffer, size, mHandle);
    FlattenableUtils::write(buffer, size, mType);
    FlattenableUtils::write(buffer, size, mMinValue);
    FlattenableUtils::write(buffer, size, mMaxValue);
    FlattenableUtils::write(buffer, size, mResolution);
    FlattenableUtils::write(buffer, size, mPower);
    FlattenableUtils::write(buffer, size, mMinDelay);
    FlattenableUtils::write(buffer, size, mFifoReservedEventCount);
    FlattenableUtils::write(buffer, size, mFifoMaxEventCount);
    flattenString8(buffer, size, mStringType);
    flattenString8(buffer, size, mRequiredPermission);
    FlattenableUtils::write(buffer, size, mRequiredPermissionRuntime);
    FlattenableUtils::write(buffer, size, mRequiredAppOp);
    FlattenableUtils::write(buffer, size, mMaxDelay);
    FlattenableUtils::write(buffer, size, mFlags);
    return NO_ERROR;
}

status_t Sensor::unflatten(void const* buffer, size_t size) {
    if (!unflattenString8(buffer, size, mName)) {
        return NO_MEMORY;
    }
    if (!unflattenString8(buffer, size, mVendor)) {
        return NO_MEMORY;
    }

    size_t fixedSize =
            sizeof(int32_t) * 3 +
            sizeof(float) * 4 +
            sizeof(int32_t) * 5;
    if (size < fixedSize) {
        return NO_MEMORY;
    }

    FlattenableUtils::read(buffer, size, mVersion);
    FlattenableUtils::read(buffer, size, mHandle);
    FlattenableUtils::read(buffer, size, mType);
    FlattenableUtils::read(buffer, size, mMinValue);
    FlattenableUtils::read(buffer, size, mMaxValue);
    FlattenableUtils::read(buffer, size, mResolution);
    FlattenableUtils::read(buffer, size, mPower);
    FlattenableUtils::read(buffer, size, mMinDelay);
    FlattenableUtils::read(buffer, size, mFifoReservedEventCount);
    FlattenableUtils::read(buffer, size, mFifoMaxEventCount);

    if (!unflattenString8(buffer, size, mStringType)) {
        return NO_MEMORY;
    }
    if (!unflattenString8(buffer, size, mRequiredPermission)) {
        return NO_MEMORY;
    }
    FlattenableUtils::read(buffer, size, mRequiredPermissionRuntime);
    FlattenableUtils::read(buffer, size, mRequiredAppOp);
    FlattenableUtils::read(buffer, size, mMaxDelay);
    FlattenableUtils::read(buffer, size, mFlags);
    return NO_ERROR;
}

void Sensor::flattenString8(void*& buffer, size_t& size,
        const String8& string8) {
    uint32_t len = static_cast<uint32_t>(string8.length());
    FlattenableUtils::write(buffer, size, len);
    memcpy(static_cast<char*>(buffer), string8.string(), len);
    FlattenableUtils::advance(buffer, size, FlattenableUtils::align<4>(len));
}

bool Sensor::unflattenString8(void const*& buffer, size_t& size, String8& outputString8) {
    uint32_t len;
    if (size < sizeof(len)) {
        return false;
    }
    FlattenableUtils::read(buffer, size, len);
    if (size < len) {
        return false;
    }
    outputString8.setTo(static_cast<char const*>(buffer), len);
    FlattenableUtils::advance(buffer, size, FlattenableUtils::align<4>(len));
    return true;
}

// ----------------------------------------------------------------------------
}; // namespace android
