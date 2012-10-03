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

#include <gui/GuiConfig.h>

namespace android {

void appendGuiConfigString(String8& configStr)
{
    static const char* config =
            " [libgui"
#ifdef USE_FENCE_SYNC
            " USE_FENCE_SYNC"
#endif
#ifdef USE_NATIVE_FENCE_SYNC
            " USE_NATIVE_FENCE_SYNC"
#endif
#ifdef USE_WAIT_SYNC
            " USE_WAIT_SYNC"
#endif
            "]";
    configStr.append(config);
}

}; // namespace android
