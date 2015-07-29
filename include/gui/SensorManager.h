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

#ifndef ANDROID_GUI_SENSOR_MANAGER_H
#define ANDROID_GUI_SENSOR_MANAGER_H

#include <map>

#include <stdint.h>
#include <sys/types.h>

#include <binder/IBinder.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/Vector.h>
#include <utils/String8.h>

#include <gui/SensorEventQueue.h>

// ----------------------------------------------------------------------------
// Concrete types for the NDK
struct ASensorManager { };

// ----------------------------------------------------------------------------
namespace android {
// ----------------------------------------------------------------------------

class ISensorServer;
class Sensor;
class SensorEventQueue;
// ----------------------------------------------------------------------------

class SensorManager :
    public ASensorManager
{
public:
    static SensorManager& getInstanceForPackage(const String16& packageName) {
        Mutex::Autolock _l(sLock);

        SensorManager* sensorManager;
        std::map<String16, SensorManager*>::iterator iterator =
                sPackageInstances.find(packageName);

        if (iterator != sPackageInstances.end()) {
            sensorManager = iterator->second;
        } else {
            String16 opPackageName = packageName;

            // It is possible that the calling code has no access to the package name.
            // In this case we will get the packages for the calling UID and pick the
            // first one for attributing the app op. This will work correctly for
            // runtime permissions as for legacy apps we will toggle the app op for
            // all packages in the UID. The caveat is that the operation may be attributed
            // to the wrong package and stats based on app ops may be slightly off.
            if (opPackageName.size() <= 0) {
                sp<IBinder> binder = defaultServiceManager()->getService(String16("permission"));
                if (binder != 0) {
                    const uid_t uid = IPCThreadState::self()->getCallingUid();
                    Vector<String16> packages;
                    interface_cast<IPermissionController>(binder)->getPackagesForUid(uid, packages);
                    if (!packages.isEmpty()) {
                        opPackageName = packages[0];
                    } else {
                        ALOGE("No packages for calling UID");
                    }
                } else {
                    ALOGE("Cannot get permission service");
                }
            }

            sensorManager = new SensorManager(opPackageName);

            // If we had no package name, we looked it up from the UID and the sensor
            // manager instance we created should also be mapped to the empty package
            // name, to avoid looking up the packages for a UID and get the same result.
            if (packageName.size() <= 0) {
                sPackageInstances.insert(std::make_pair(String16(), sensorManager));
            }

            // Stash the per package sensor manager.
            sPackageInstances.insert(std::make_pair(opPackageName, sensorManager));
        }

        return *sensorManager;
    }

    SensorManager(const String16& opPackageName);
    ~SensorManager();

    ssize_t getSensorList(Sensor const* const** list) const;
    Sensor const* getDefaultSensor(int type);
    sp<SensorEventQueue> createEventQueue(String8 packageName = String8(""), int mode = 0);
    bool isDataInjectionEnabled();

private:
    // DeathRecipient interface
    void sensorManagerDied();

    status_t assertStateLocked() const;

private:
    static Mutex sLock;
    static std::map<String16, SensorManager*> sPackageInstances;

    mutable Mutex mLock;
    mutable sp<ISensorServer> mSensorServer;
    mutable Sensor const** mSensorList;
    mutable Vector<Sensor> mSensors;
    mutable sp<IBinder::DeathRecipient> mDeathObserver;
    const String16 mOpPackageName;
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_SENSOR_MANAGER_H
