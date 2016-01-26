/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_HARDWAREPROPERTIESMANAGER_H
#define ANDROID_HARDWAREPROPERTIESMANAGER_H

namespace android {

// must be kept in sync with definitions in HardwarePropertiesManager.java
enum {
    DEVICE_TEMPERATURE_CPU = 0,
    DEVICE_TEMPERATURE_GPU = 1,
    DEVICE_TEMPERATURE_BATTERY = 2,
};

}; // namespace android

#endif // ANDROID_HARDWAREPROPERTIESMANAGER_H
