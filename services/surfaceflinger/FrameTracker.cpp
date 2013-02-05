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

// This is needed for stdint.h to define INT64_MAX in C++
#define __STDC_LIMIT_MACROS

#include <ui/Fence.h>

#include <utils/String8.h>

#include "FrameTracker.h"

namespace android {

FrameTracker::FrameTracker() :
        mOffset(0),
        mNumFences(0) {
}

void FrameTracker::setDesiredPresentTime(nsecs_t presentTime) {
    Mutex::Autolock lock(mMutex);
    mFrameRecords[mOffset].desiredPresentTime = presentTime;
}

void FrameTracker::setFrameReadyTime(nsecs_t readyTime) {
    Mutex::Autolock lock(mMutex);
    mFrameRecords[mOffset].frameReadyTime = readyTime;
}

void FrameTracker::setFrameReadyFence(const sp<Fence>& readyFence) {
    Mutex::Autolock lock(mMutex);
    mFrameRecords[mOffset].frameReadyFence = readyFence;
    mNumFences++;
}

void FrameTracker::setActualPresentTime(nsecs_t presentTime) {
    Mutex::Autolock lock(mMutex);
    mFrameRecords[mOffset].actualPresentTime = presentTime;
}

void FrameTracker::setActualPresentFence(const sp<Fence>& readyFence) {
    Mutex::Autolock lock(mMutex);
    mFrameRecords[mOffset].actualPresentFence = readyFence;
    mNumFences++;
}

void FrameTracker::advanceFrame() {
    Mutex::Autolock lock(mMutex);
    mOffset = (mOffset+1) % NUM_FRAME_RECORDS;
    mFrameRecords[mOffset].desiredPresentTime = INT64_MAX;
    mFrameRecords[mOffset].frameReadyTime = INT64_MAX;
    mFrameRecords[mOffset].actualPresentTime = INT64_MAX;

    if (mFrameRecords[mOffset].frameReadyFence != NULL) {
        // We're clobbering an unsignaled fence, so we need to decrement the
        // fence count.
        mFrameRecords[mOffset].frameReadyFence = NULL;
        mNumFences--;
    }

    if (mFrameRecords[mOffset].actualPresentFence != NULL) {
        // We're clobbering an unsignaled fence, so we need to decrement the
        // fence count.
        mFrameRecords[mOffset].actualPresentFence = NULL;
        mNumFences--;
    }

    // Clean up the signaled fences to keep the number of open fence FDs in
    // this process reasonable.
    processFencesLocked();
}

void FrameTracker::clear() {
    Mutex::Autolock lock(mMutex);
    for (size_t i = 0; i < NUM_FRAME_RECORDS; i++) {
        mFrameRecords[i].desiredPresentTime = 0;
        mFrameRecords[i].frameReadyTime = 0;
        mFrameRecords[i].actualPresentTime = 0;
        mFrameRecords[i].frameReadyFence.clear();
        mFrameRecords[i].actualPresentFence.clear();
    }
    mNumFences = 0;
    mFrameRecords[mOffset].desiredPresentTime = INT64_MAX;
    mFrameRecords[mOffset].frameReadyTime = INT64_MAX;
    mFrameRecords[mOffset].actualPresentTime = INT64_MAX;
}

void FrameTracker::processFencesLocked() const {
    FrameRecord* records = const_cast<FrameRecord*>(mFrameRecords);
    int& numFences = const_cast<int&>(mNumFences);

    for (int i = 1; i < NUM_FRAME_RECORDS && numFences > 0; i++) {
        size_t idx = (mOffset+NUM_FRAME_RECORDS-i) % NUM_FRAME_RECORDS;

        const sp<Fence>& rfence = records[idx].frameReadyFence;
        if (rfence != NULL) {
            records[idx].frameReadyTime = rfence->getSignalTime();
            if (records[idx].frameReadyTime < INT64_MAX) {
                records[idx].frameReadyFence = NULL;
                numFences--;
            }
        }

        const sp<Fence>& pfence = records[idx].actualPresentFence;
        if (pfence != NULL) {
            records[idx].actualPresentTime = pfence->getSignalTime();
            if (records[idx].actualPresentTime < INT64_MAX) {
                records[idx].actualPresentFence = NULL;
                numFences--;
            }
        }
    }
}

void FrameTracker::dump(String8& result) const {
    Mutex::Autolock lock(mMutex);
    processFencesLocked();

    const size_t o = mOffset;
    for (size_t i = 1; i < NUM_FRAME_RECORDS; i++) {
        const size_t index = (o+i) % NUM_FRAME_RECORDS;
        result.appendFormat("%lld\t%lld\t%lld\n",
            mFrameRecords[index].desiredPresentTime,
            mFrameRecords[index].actualPresentTime,
            mFrameRecords[index].frameReadyTime);
    }
    result.append("\n");
}

} // namespace android
