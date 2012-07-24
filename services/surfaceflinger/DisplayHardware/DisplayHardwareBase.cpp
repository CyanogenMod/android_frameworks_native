/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "DisplayHardware/DisplayHardwareBase.h"

// ----------------------------------------------------------------------------
namespace android {

DisplayHardwareBase::DisplayHardwareBase(uint32_t displayIndex) {
    mScreenAcquired = true;
}

DisplayHardwareBase::~DisplayHardwareBase() {
}

bool DisplayHardwareBase::canDraw() const {
    return mScreenAcquired;
}

void DisplayHardwareBase::releaseScreen() const {
    mScreenAcquired = false;
}

void DisplayHardwareBase::acquireScreen() const {
    mScreenAcquired = true;
}

bool DisplayHardwareBase::isScreenAcquired() const {
    return mScreenAcquired;
}

}; // namespace android
