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

#ifndef ANDROID_SENSOR_REGISTRATION_INFO_H
#define ANDROID_SENSOR_REGISTRATION_INFO_H

namespace android {

class SensorService;

struct SensorService::SensorRegistrationInfo {
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
       return (info.mHour == INT32_MIN &&
               info.mMin == INT32_MIN &&
               info.mSec == INT32_MIN);
    }
};

} // namespace android;

#endif // ANDROID_SENSOR_REGISTRATION_INFO_H


