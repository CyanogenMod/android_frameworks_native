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

#include "SensorRecord.h"

#include "SensorEventConnection.h"

namespace android {

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
    // Remove this connections from the queue of flush() calls made on this sensor.
    for (Vector< wp<SensorEventConnection> >::iterator it = mPendingFlushConnections.begin();
            it != mPendingFlushConnections.end(); ) {
        if (it->unsafe_get() == connection.unsafe_get()) {
            it = mPendingFlushConnections.erase(it);
        } else {
            ++it;
        }
    }
    return mConnections.size() ? false : true;
}

void SensorService::SensorRecord::addPendingFlushConnection(
        const sp<SensorEventConnection>& connection) {
    mPendingFlushConnections.add(connection);
}

void SensorService::SensorRecord::removeFirstPendingFlushConnection() {
    if (mPendingFlushConnections.size() > 0) {
        mPendingFlushConnections.removeAt(0);
    }
}

SensorService::SensorEventConnection *
        SensorService::SensorRecord::getFirstPendingFlushConnection() {
    if (mPendingFlushConnections.size() > 0) {
        return mPendingFlushConnections[0].unsafe_get();
    }
    return NULL;
}

void SensorService::SensorRecord::clearAllPendingFlushConnections() {
    mPendingFlushConnections.clear();
}

} // namespace android
