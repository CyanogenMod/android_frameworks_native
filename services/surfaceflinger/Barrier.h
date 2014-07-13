/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef ANDROID_BARRIER_H
#define ANDROID_BARRIER_H

#include <stdint.h>
#include <sys/types.h>
#include <utils/threads.h>

namespace android {

class Barrier
{
public:
    inline Barrier() : state(CLOSED) { }
    inline ~Barrier() { }

    // Release any threads waiting at the Barrier.
    // Provides release semantics: preceding loads and stores will be visible
    // to other threads before they wake up.
    void open() {
        Mutex::Autolock _l(lock);
        state = OPENED;
        cv.broadcast();
    }

    // Reset the Barrier, so wait() will block until open() has been called.
    void close() {
        Mutex::Autolock _l(lock);
        state = CLOSED;
    }

    // Wait until the Barrier is OPEN.
    // Provides acquire semantics: no subsequent loads or stores will occur
    // until wait() returns.
    void wait() const {
        Mutex::Autolock _l(lock);
        while (state == CLOSED) {
            cv.wait(lock);
        }
    }
private:
    enum { OPENED, CLOSED };
    mutable     Mutex       lock;
    mutable     Condition   cv;
    volatile    int         state;
};

}; // namespace android

#endif // ANDROID_BARRIER_H
