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
#include <utils/String8.h>
#include <utils/Flattenable.h>

#include <hardware/sensors.h>

#include <gui/Sensor.h>

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

Sensor::Sensor()
    : mHandle(0), mType(0),
      mMinValue(0), mMaxValue(0), mResolution(0),
      mPower(0), mMinDelay(0), mFifoReservedEventCount(0), mFifoMaxEventCount(0),
      mWakeUpSensor(false)
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
    mWakeUpSensor = false;

    // Set fifo event count zero for older devices which do not support batching. Fused
    // sensors also have their fifo counts set to zero.
    if (halVersion >= SENSORS_DEVICE_API_VERSION_1_1) {
        mFifoReservedEventCount = hwSensor->fifoReservedEventCount;
        mFifoMaxEventCount = hwSensor->fifoMaxEventCount;
    }

    // Ensure existing sensors have correct string type and required
    // permissions.
    switch (mType) {
    case SENSOR_TYPE_ACCELEROMETER:
        mStringType = SENSOR_STRING_TYPE_ACCELEROMETER;
        break;
    case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        mStringType = SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE;
        break;
    case SENSOR_TYPE_GAME_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR;
        break;
    case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
        break;
    case SENSOR_TYPE_GRAVITY:
        mStringType = SENSOR_STRING_TYPE_GRAVITY;
        break;
    case SENSOR_TYPE_GYROSCOPE:
        mStringType = SENSOR_STRING_TYPE_GYROSCOPE;
        break;
    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_GYROSCOPE_UNCALIBRATED;
        break;
    case SENSOR_TYPE_HEART_RATE:
        mStringType = SENSOR_STRING_TYPE_HEART_RATE;
        mRequiredPermission = SENSOR_PERMISSION_BODY_SENSORS;
        break;
    case SENSOR_TYPE_LIGHT:
        mStringType = SENSOR_STRING_TYPE_LIGHT;
        break;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
        mStringType = SENSOR_STRING_TYPE_LINEAR_ACCELERATION;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD:
        mStringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
        break;
    case SENSOR_TYPE_ORIENTATION:
        mStringType = SENSOR_STRING_TYPE_ORIENTATION;
        break;
    case SENSOR_TYPE_PRESSURE:
        mStringType = SENSOR_STRING_TYPE_PRESSURE;
        break;
    case SENSOR_TYPE_PROXIMITY:
        mStringType = SENSOR_STRING_TYPE_PROXIMITY;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_RELATIVE_HUMIDITY:
        mStringType = SENSOR_STRING_TYPE_RELATIVE_HUMIDITY;
        break;
    case SENSOR_TYPE_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_ROTATION_VECTOR;
        break;
    case SENSOR_TYPE_SIGNIFICANT_MOTION:
        mStringType = SENSOR_STRING_TYPE_SIGNIFICANT_MOTION;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_STEP_COUNTER:
        mStringType = SENSOR_STRING_TYPE_STEP_COUNTER;
        break;
    case SENSOR_TYPE_STEP_DETECTOR:
        mStringType = SENSOR_STRING_TYPE_STEP_DETECTOR;
        break;
    case SENSOR_TYPE_TEMPERATURE:
        mStringType = SENSOR_STRING_TYPE_TEMPERATURE;
        break;
    case SENSOR_TYPE_NON_WAKE_UP_PROXIMITY_SENSOR:
        mStringType = SENSOR_STRING_TYPE_NON_WAKE_UP_PROXIMITY_SENSOR;
        break;
    case SENSOR_TYPE_WAKE_UP_ACCELEROMETER:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_ACCELEROMETER;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_MAGNETIC_FIELD:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_MAGNETIC_FIELD;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_ORIENTATION:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_ORIENTATION;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_GYROSCOPE:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_GYROSCOPE;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_LIGHT:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_LIGHT;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_PRESSURE:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_PRESSURE;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_GRAVITY:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_GRAVITY;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_LINEAR_ACCELERATION:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_LINEAR_ACCELERATION;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_ROTATION_VECTOR;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_RELATIVE_HUMIDITY:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_RELATIVE_HUMIDITY;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_AMBIENT_TEMPERATURE:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_AMBIENT_TEMPERATURE;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_MAGNETIC_FIELD_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_MAGNETIC_FIELD_UNCALIBRATED;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_GAME_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_GAME_ROTATION_VECTOR;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_GYROSCOPE_UNCALIBRATED:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_GYROSCOPE_UNCALIBRATED;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_STEP_DETECTOR:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_STEP_DETECTOR;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_STEP_COUNTER:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_STEP_COUNTER;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_GEOMAGNETIC_ROTATION_VECTOR:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_GEOMAGNETIC_ROTATION_VECTOR;
        mWakeUpSensor = true;
        break;
    case SENSOR_TYPE_WAKE_UP_HEART_RATE:
        mStringType = SENSOR_STRING_TYPE_WAKE_UP_HEART_RATE;
        mRequiredPermission = SENSOR_PERMISSION_BODY_SENSORS;
        mWakeUpSensor = true;
        break;
    default:
        // Only pipe the stringType, requiredPermission and flags for custom sensors.
        if (halVersion >= SENSORS_DEVICE_API_VERSION_1_2 && hwSensor->stringType) {
            mStringType = hwSensor->stringType;
        }
        if (halVersion >= SENSORS_DEVICE_API_VERSION_1_2 && hwSensor->requiredPermission) {
            mRequiredPermission = hwSensor->requiredPermission;
        }
        if (halVersion >= SENSORS_DEVICE_API_VERSION_1_3) {
            mWakeUpSensor = hwSensor->flags & SENSOR_FLAG_WAKE_UP;
        }
        break;
    }
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

int32_t Sensor::getFifoReservedEventCount() const {
    return mFifoReservedEventCount;
}

int32_t Sensor::getFifoMaxEventCount() const {
    return mFifoMaxEventCount;
}

const String8& Sensor::getStringType() const {
    return mStringType;
}

const String8& Sensor::getRequiredPermission() const {
    return mRequiredPermission;
}

bool Sensor::isWakeUpSensor() const {
    return mWakeUpSensor;
}

size_t Sensor::getFlattenedSize() const
{
    size_t fixedSize =
            sizeof(int32_t) * 3 +
            sizeof(float) * 4 +
            sizeof(int32_t) * 3;

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
            sizeof(int32_t) * 3;
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
    return NO_ERROR;
}

void Sensor::flattenString8(void*& buffer, size_t& size,
        const String8& string8) {
    uint32_t len = string8.length();
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
