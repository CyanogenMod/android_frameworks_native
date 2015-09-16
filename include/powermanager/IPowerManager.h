/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ANDROID_IPOWERMANAGER_H
#define ANDROID_IPOWERMANAGER_H

#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <hardware/power.h>

namespace android {

// ----------------------------------------------------------------------------

class IPowerManager : public IInterface
{
public:
    // These transaction IDs must be kept in sync with the method order from
    // IPowerManager.aidl.
    enum {
        ACQUIRE_WAKE_LOCK = IBinder::FIRST_CALL_TRANSACTION,
        ACQUIRE_WAKE_LOCK_UID = IBinder::FIRST_CALL_TRANSACTION + 1,
        RELEASE_WAKE_LOCK = IBinder::FIRST_CALL_TRANSACTION + 2,
        UPDATE_WAKE_LOCK_UIDS = IBinder::FIRST_CALL_TRANSACTION + 3,
        POWER_HINT = IBinder::FIRST_CALL_TRANSACTION + 4,
    };

    DECLARE_META_INTERFACE(PowerManager);

    // The parcels created by these methods must be kept in sync with the
    // corresponding methods from IPowerManager.aidl.
    // FIXME remove the bool isOneWay parameters as they are not oneway in the .aidl
    virtual status_t acquireWakeLock(int flags, const sp<IBinder>& lock, const String16& tag,
            const String16& packageName, bool isOneWay = false) = 0;
    virtual status_t acquireWakeLockWithUid(int flags, const sp<IBinder>& lock, const String16& tag,
            const String16& packageName, int uid, bool isOneWay = false) = 0;
    virtual status_t releaseWakeLock(const sp<IBinder>& lock, int flags, bool isOneWay = false) = 0;
    virtual status_t updateWakeLockUids(const sp<IBinder>& lock, int len, const int *uids,
            bool isOneWay = false) = 0;
    // oneway in the .aidl
    virtual status_t powerHint(int hintId, int data) = 0;
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_IPOWERMANAGER_H
