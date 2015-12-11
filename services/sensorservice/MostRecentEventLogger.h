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

#ifndef ANDROID_MOST_RECENT_EVENT_LOGGER_H
#define ANDROID_MOST_RECENT_EVENT_LOGGER_H

#include "SensorService.h"

namespace android {

class SensorService;

// A circular buffer of TrimmedSensorEvents. The size of this buffer is typically 10. The last N
// events generated from the sensor are stored in this buffer. The buffer is NOT cleared when the
// sensor unregisters and as a result very old data in the dumpsys output can be seen, which is an
// intended behavior.
class SensorService::MostRecentEventLogger {
public:
    MostRecentEventLogger(int sensorType);
    void addEvent(const sensors_event_t& event);
    void printBuffer(String8& buffer) const;
    bool populateLastEvent(sensors_event_t *event);
    ~MostRecentEventLogger();

private:
    // sensor_event_t with only the data and the timestamp.
    static const size_t LOG_SIZE = 10;
    static const size_t LOG_SIZE_LARGE = 50;

    struct TrimmedSensorEvent {
        union {
            float *mData;
            uint64_t mStepCounter;
        };
        // Timestamp from the sensors_event_t.
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

    int mNextInd;
    int mSensorType;
    int mBufSize;
    TrimmedSensorEvent ** mTrimmedSensorEventArr;
};

} // namespace android;

#endif // ANDROID_MOST_RECENT_EVENT_LOGGER_H

