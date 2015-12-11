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

#include "MostRecentEventLogger.h"

namespace android {

SensorService::MostRecentEventLogger::MostRecentEventLogger(int sensorType) :
        mSensorType(sensorType), mNextInd(0) {

    mBufSize = (sensorType == SENSOR_TYPE_STEP_COUNTER ||
                sensorType == SENSOR_TYPE_SIGNIFICANT_MOTION ||
                sensorType == SENSOR_TYPE_ACCELEROMETER) ? LOG_SIZE : LOG_SIZE_LARGE;

    mTrimmedSensorEventArr = new TrimmedSensorEvent *[mBufSize];
    mSensorType = sensorType;
    for (int i = 0; i < mBufSize; ++i) {
        mTrimmedSensorEventArr[i] = new TrimmedSensorEvent(mSensorType);
    }
}

void SensorService::MostRecentEventLogger::addEvent(const sensors_event_t& event) {
    TrimmedSensorEvent *curr_event = mTrimmedSensorEventArr[mNextInd];
    curr_event->mTimestamp = event.timestamp;
    if (mSensorType == SENSOR_TYPE_STEP_COUNTER) {
        curr_event->mStepCounter = event.u64.step_counter;
    } else {
        memcpy(curr_event->mData, event.data,
                 sizeof(float) * SensorService::getNumEventsForSensorType(mSensorType));
    }
    time_t rawtime = time(NULL);
    struct tm * timeinfo = localtime(&rawtime);
    curr_event->mHour = timeinfo->tm_hour;
    curr_event->mMin = timeinfo->tm_min;
    curr_event->mSec = timeinfo->tm_sec;
    mNextInd = (mNextInd + 1) % mBufSize;
}

void SensorService::MostRecentEventLogger::printBuffer(String8& result) const {
    const int numData = SensorService::getNumEventsForSensorType(mSensorType);
    int i = mNextInd, eventNum = 1;
    result.appendFormat("last %d events = < ", mBufSize);
    do {
        if (TrimmedSensorEvent::isSentinel(*mTrimmedSensorEventArr[i])) {
            // Sentinel, ignore.
            i = (i + 1) % mBufSize;
            continue;
        }
        result.appendFormat("%d) ", eventNum++);
        if (mSensorType == SENSOR_TYPE_STEP_COUNTER) {
            result.appendFormat("%llu,", mTrimmedSensorEventArr[i]->mStepCounter);
        } else {
            for (int j = 0; j < numData; ++j) {
                result.appendFormat("%5.1f,", mTrimmedSensorEventArr[i]->mData[j]);
            }
        }
        result.appendFormat("%lld %02d:%02d:%02d ", mTrimmedSensorEventArr[i]->mTimestamp,
                mTrimmedSensorEventArr[i]->mHour, mTrimmedSensorEventArr[i]->mMin,
                mTrimmedSensorEventArr[i]->mSec);
        i = (i + 1) % mBufSize;
    } while (i != mNextInd);
    result.appendFormat(">\n");
}

bool SensorService::MostRecentEventLogger::populateLastEvent(sensors_event_t *event) {
    int lastEventInd = (mNextInd - 1 + mBufSize) % mBufSize;
    // Check if the buffer is empty.
    if (TrimmedSensorEvent::isSentinel(*mTrimmedSensorEventArr[lastEventInd])) {
        return false;
    }
    event->version = sizeof(sensors_event_t);
    event->type = mSensorType;
    event->timestamp = mTrimmedSensorEventArr[lastEventInd]->mTimestamp;
    if (mSensorType == SENSOR_TYPE_STEP_COUNTER) {
          event->u64.step_counter = mTrimmedSensorEventArr[lastEventInd]->mStepCounter;
    } else {
        memcpy(event->data, mTrimmedSensorEventArr[lastEventInd]->mData,
                 sizeof(float) * SensorService::getNumEventsForSensorType(mSensorType));
    }
    return true;
}

SensorService::MostRecentEventLogger::~MostRecentEventLogger() {
    for (int i = 0; i < mBufSize; ++i) {
        delete mTrimmedSensorEventArr[i];
    }
    delete [] mTrimmedSensorEventArr;
}

// -----------------------------------------------------------------------------
SensorService::MostRecentEventLogger::TrimmedSensorEvent::TrimmedSensorEvent(int sensorType) {
    mTimestamp = -1;
    const int numData = SensorService::getNumEventsForSensorType(sensorType);
    if (sensorType == SENSOR_TYPE_STEP_COUNTER) {
        mStepCounter = 0;
    } else {
        mData = new float[numData];
        for (int i = 0; i < numData; ++i) {
            mData[i] = -1.0;
        }
    }
    mHour = mMin = mSec = INT32_MIN;
}

bool SensorService::MostRecentEventLogger::TrimmedSensorEvent::
    isSentinel(const TrimmedSensorEvent& event) {
    return (event.mHour == INT32_MIN && event.mMin == INT32_MIN && event.mSec == INT32_MIN);
}

} // namespace android
