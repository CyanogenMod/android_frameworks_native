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

#include <cutils/log.h>
#include <utils/Errors.h>

#include <binder/IServiceManager.h>
#include <powermanager/IPowerManager.h>
#include <powermanager/PowerManager.h>

#include "PowerHAL.h"

namespace android {
// ---------------------------------------------------------------------------

status_t PowerHAL::vsyncHint(bool enabled) {
    Mutex::Autolock _l(mlock);
    if (mPowerManager == NULL) {
        const String16 serviceName("power");
        sp<IBinder> bs = defaultServiceManager()->checkService(serviceName);
        if (bs == NULL) {
            return NAME_NOT_FOUND;
        }
        mPowerManager = interface_cast<IPowerManager>(bs);
    }
    status_t status = mPowerManager->powerHint(POWER_HINT_VSYNC, enabled ? 1 : 0);
    if(status == DEAD_OBJECT) {
        mPowerManager = NULL;
    }
    return status;
}

// ---------------------------------------------------------------------------
}; // namespace android

