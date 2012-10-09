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

#define LOG_TAG "Fence"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <sync/sync.h>
#include <ui/Fence.h>
#include <unistd.h>
#include <utils/Log.h>
#include <utils/Trace.h>

namespace android {

const sp<Fence> Fence::NO_FENCE = sp<Fence>();

Fence::Fence() :
    mFenceFd(-1) {
}

Fence::Fence(int fenceFd) :
    mFenceFd(fenceFd) {
}

Fence::~Fence() {
    if (mFenceFd != -1) {
        close(mFenceFd);
    }
}

status_t Fence::wait(unsigned int timeout) {
    ATRACE_CALL();
    if (mFenceFd == -1) {
        return NO_ERROR;
    }
    int err = sync_wait(mFenceFd, timeout);
    return err < 0 ? -errno : status_t(NO_ERROR);
}

status_t Fence::waitForever(unsigned int warningTimeout, const char* logname) {
    ATRACE_CALL();
    if (mFenceFd == -1) {
        return NO_ERROR;
    }
    int err = sync_wait(mFenceFd, warningTimeout);
    if (err < 0 && errno == ETIME) {
        ALOGE("%s: fence %d didn't signal in %u ms", logname, mFenceFd,
                warningTimeout);
        err = sync_wait(mFenceFd, TIMEOUT_NEVER);
    }
    return err < 0 ? -errno : status_t(NO_ERROR);
}

sp<Fence> Fence::merge(const String8& name, const sp<Fence>& f1,
        const sp<Fence>& f2) {
    ATRACE_CALL();
    int result = sync_merge(name.string(), f1->mFenceFd, f2->mFenceFd);
    if (result == -1) {
        status_t err = -errno;
        ALOGE("merge: sync_merge(\"%s\", %d, %d) returned an error: %s (%d)",
                name.string(), f1->mFenceFd, f2->mFenceFd,
                strerror(-err), err);
        return NO_FENCE;
    }
    return sp<Fence>(new Fence(result));
}

int Fence::dup() const {
    if (mFenceFd == -1) {
        return -1;
    }
    return ::dup(mFenceFd);
}

size_t Fence::getFlattenedSize() const {
    return 0;
}

size_t Fence::getFdCount() const {
    return 1;
}

status_t Fence::flatten(void* buffer, size_t size, int fds[],
        size_t count) const {
    if (size != 0 || count != 1) {
        return BAD_VALUE;
    }

    fds[0] = mFenceFd;
    return NO_ERROR;
}

status_t Fence::unflatten(void const* buffer, size_t size, int fds[],
        size_t count) {
    if (size != 0 || count != 1) {
        return BAD_VALUE;
    }
    if (mFenceFd != -1) {
        // Don't unflatten if we already have a valid fd.
        return INVALID_OPERATION;
    }

    mFenceFd = fds[0];
    return NO_ERROR;
}

} // namespace android
