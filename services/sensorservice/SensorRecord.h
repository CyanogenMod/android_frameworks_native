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

#ifndef ANDROID_SENSOR_RECORD_H
#define ANDROID_SENSOR_RECORD_H

#include "SensorService.h"

namespace android {

class SensorService;

class SensorService::SensorRecord {
public:
    SensorRecord(const sp<SensorEventConnection>& connection);
    bool addConnection(const sp<SensorEventConnection>& connection);
    bool removeConnection(const wp<SensorEventConnection>& connection);
    size_t getNumConnections() const { return mConnections.size(); }

    void addPendingFlushConnection(const sp<SensorEventConnection>& connection);
    void removeFirstPendingFlushConnection();
    SensorEventConnection * getFirstPendingFlushConnection();
    void clearAllPendingFlushConnections();
private:
    SortedVector< wp<SensorEventConnection> > mConnections;
    // A queue of all flush() calls made on this sensor. Flush complete events
    // will be sent in this order.
    Vector< wp<SensorEventConnection> > mPendingFlushConnections;
};

}

#endif // ANDROID_SENSOR_RECORD_H
