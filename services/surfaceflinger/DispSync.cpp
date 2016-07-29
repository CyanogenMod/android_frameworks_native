/*
 * Copyright (C) 2013 The Android Open Source Project
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

// This is needed for stdint.h to define INT64_MAX in C++
#define __STDC_LIMIT_MACROS

#include <math.h>

#include <cutils/log.h>

#include <ui/Fence.h>

#include <utils/String8.h>
#include <utils/Thread.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#include "DispSync.h"
#include "EventLog/EventLog.h"

#include <algorithm>

using std::max;
using std::min;

namespace android {

// Setting this to true enables verbose tracing that can be used to debug
// vsync event model or phase issues.
static const bool kTraceDetailedInfo = false;

// Setting this to true adds a zero-phase tracer for correlating with hardware
// vsync events
static const bool kEnableZeroPhaseTracer = false;

// This is the threshold used to determine when hardware vsync events are
// needed to re-synchronize the software vsync model with the hardware.  The
// error metric used is the mean of the squared difference between each
// present time and the nearest software-predicted vsync.
static const nsecs_t kErrorThreshold = 160000000000;    // 400 usec squared

// This is the offset from the present fence timestamps to the corresponding
// vsync event.
static const int64_t kPresentTimeOffset = PRESENT_TIME_OFFSET_FROM_VSYNC_NS;

#undef LOG_TAG
#define LOG_TAG "DispSyncThread"
class DispSyncThread: public Thread {
public:

    DispSyncThread(const char* name):
            mName(name),
            mStop(false),
            mPeriod(0),
            mPhase(0),
            mReferenceTime(0),
            mWakeupLatency(0),
            mFrameNumber(0) {}

    virtual ~DispSyncThread() {}

    void updateModel(nsecs_t period, nsecs_t phase, nsecs_t referenceTime) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);
        mPeriod = period;
        mPhase = phase;
        mReferenceTime = referenceTime;
        ALOGV("[%s] updateModel: mPeriod = %" PRId64 ", mPhase = %" PRId64
                " mReferenceTime = %" PRId64, mName, ns2us(mPeriod),
                ns2us(mPhase), ns2us(mReferenceTime));
        mCond.signal();
    }

    void stop() {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);
        mStop = true;
        mCond.signal();
    }

    virtual bool threadLoop() {
        status_t err;
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);

        while (true) {
            Vector<CallbackInvocation> callbackInvocations;

            nsecs_t targetTime = 0;

            { // Scope for lock
                Mutex::Autolock lock(mMutex);

                if (kTraceDetailedInfo) {
                    ATRACE_INT64("DispSync:Frame", mFrameNumber);
                }
                ALOGV("[%s] Frame %" PRId64, mName, mFrameNumber);
                ++mFrameNumber;

                if (mStop) {
                    return false;
                }

                if (mPeriod == 0) {
                    err = mCond.wait(mMutex);
                    if (err != NO_ERROR) {
                        ALOGE("error waiting for new events: %s (%d)",
                                strerror(-err), err);
                        return false;
                    }
                    continue;
                }

                targetTime = computeNextEventTimeLocked(now);

                bool isWakeup = false;

                if (now < targetTime) {
                    if (kTraceDetailedInfo) ATRACE_NAME("DispSync waiting");

                    if (targetTime == INT64_MAX) {
                        ALOGV("[%s] Waiting forever", mName);
                        err = mCond.wait(mMutex);
                    } else {
                        ALOGV("[%s] Waiting until %" PRId64, mName,
                                ns2us(targetTime));
                        err = mCond.waitRelative(mMutex, targetTime - now);
                    }

                    if (err == TIMED_OUT) {
                        isWakeup = true;
                    } else if (err != NO_ERROR) {
                        ALOGE("error waiting for next event: %s (%d)",
                                strerror(-err), err);
                        return false;
                    }
                }

                now = systemTime(SYSTEM_TIME_MONOTONIC);

                // Don't correct by more than 1.5 ms
                static const nsecs_t kMaxWakeupLatency = us2ns(1500);

                if (isWakeup) {
                    mWakeupLatency = ((mWakeupLatency * 63) +
                            (now - targetTime)) / 64;
                    mWakeupLatency = min(mWakeupLatency, kMaxWakeupLatency);
                    if (kTraceDetailedInfo) {
                        ATRACE_INT64("DispSync:WakeupLat", now - targetTime);
                        ATRACE_INT64("DispSync:AvgWakeupLat", mWakeupLatency);
                    }
                }

                callbackInvocations = gatherCallbackInvocationsLocked(now);
            }

            if (callbackInvocations.size() > 0) {
                fireCallbackInvocations(callbackInvocations);
            }
        }

        return false;
    }

    status_t addEventListener(const char* name, nsecs_t phase,
            const sp<DispSync::Callback>& callback) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);

        for (size_t i = 0; i < mEventListeners.size(); i++) {
            if (mEventListeners[i].mCallback == callback) {
                return BAD_VALUE;
            }
        }

        EventListener listener;
        listener.mName = name;
        listener.mPhase = phase;
        listener.mCallback = callback;

        // We want to allow the firstmost future event to fire without
        // allowing any past events to fire
        listener.mLastEventTime = systemTime() - mPeriod / 2 + mPhase -
                mWakeupLatency;

        mEventListeners.push(listener);

        mCond.signal();

        return NO_ERROR;
    }

    status_t removeEventListener(const sp<DispSync::Callback>& callback) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);

        for (size_t i = 0; i < mEventListeners.size(); i++) {
            if (mEventListeners[i].mCallback == callback) {
                mEventListeners.removeAt(i);
                mCond.signal();
                return NO_ERROR;
            }
        }

        return BAD_VALUE;
    }

    // This method is only here to handle the kIgnorePresentFences case.
    bool hasAnyEventListeners() {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);
        return !mEventListeners.empty();
    }

private:

    struct EventListener {
        const char* mName;
        nsecs_t mPhase;
        nsecs_t mLastEventTime;
        sp<DispSync::Callback> mCallback;
    };

    struct CallbackInvocation {
        sp<DispSync::Callback> mCallback;
        nsecs_t mEventTime;
    };

    nsecs_t computeNextEventTimeLocked(nsecs_t now) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        ALOGV("[%s] computeNextEventTimeLocked", mName);
        nsecs_t nextEventTime = INT64_MAX;
        for (size_t i = 0; i < mEventListeners.size(); i++) {
            nsecs_t t = computeListenerNextEventTimeLocked(mEventListeners[i],
                    now);

            if (t < nextEventTime) {
                nextEventTime = t;
            }
        }

        ALOGV("[%s] nextEventTime = %" PRId64, mName, ns2us(nextEventTime));
        return nextEventTime;
    }

    Vector<CallbackInvocation> gatherCallbackInvocationsLocked(nsecs_t now) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        ALOGV("[%s] gatherCallbackInvocationsLocked @ %" PRId64, mName,
                ns2us(now));

        Vector<CallbackInvocation> callbackInvocations;
        nsecs_t onePeriodAgo = now - mPeriod;

        for (size_t i = 0; i < mEventListeners.size(); i++) {
            nsecs_t t = computeListenerNextEventTimeLocked(mEventListeners[i],
                    onePeriodAgo);

            if (t < now) {
                CallbackInvocation ci;
                ci.mCallback = mEventListeners[i].mCallback;
                ci.mEventTime = t;
                ALOGV("[%s] [%s] Preparing to fire", mName,
                        mEventListeners[i].mName);
                callbackInvocations.push(ci);
                mEventListeners.editItemAt(i).mLastEventTime = t;
            }
        }

        return callbackInvocations;
    }

    nsecs_t computeListenerNextEventTimeLocked(const EventListener& listener,
            nsecs_t baseTime) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        ALOGV("[%s] [%s] computeListenerNextEventTimeLocked(%" PRId64 ")",
                mName, listener.mName, ns2us(baseTime));

        nsecs_t lastEventTime = listener.mLastEventTime + mWakeupLatency;
        ALOGV("[%s] lastEventTime: %" PRId64, mName, ns2us(lastEventTime));
        if (baseTime < lastEventTime) {
            baseTime = lastEventTime;
            ALOGV("[%s] Clamping baseTime to lastEventTime -> %" PRId64, mName,
                    ns2us(baseTime));
        }

        baseTime -= mReferenceTime;
        ALOGV("[%s] Relative baseTime = %" PRId64, mName, ns2us(baseTime));
        nsecs_t phase = mPhase + listener.mPhase;
        ALOGV("[%s] Phase = %" PRId64, mName, ns2us(phase));
        baseTime -= phase;
        ALOGV("[%s] baseTime - phase = %" PRId64, mName, ns2us(baseTime));

        // If our previous time is before the reference (because the reference
        // has since been updated), the division by mPeriod will truncate
        // towards zero instead of computing the floor. Since in all cases
        // before the reference we want the next time to be effectively now, we
        // set baseTime to -mPeriod so that numPeriods will be -1.
        // When we add 1 and the phase, we will be at the correct event time for
        // this period.
        if (baseTime < 0) {
            ALOGV("[%s] Correcting negative baseTime", mName);
            baseTime = -mPeriod;
        }

        nsecs_t numPeriods = baseTime / mPeriod;
        ALOGV("[%s] numPeriods = %" PRId64, mName, numPeriods);
        nsecs_t t = (numPeriods + 1) * mPeriod + phase;
        ALOGV("[%s] t = %" PRId64, mName, ns2us(t));
        t += mReferenceTime;
        ALOGV("[%s] Absolute t = %" PRId64, mName, ns2us(t));

        // Check that it's been slightly more than half a period since the last
        // event so that we don't accidentally fall into double-rate vsyncs
        if (t - listener.mLastEventTime < (3 * mPeriod / 5)) {
            t += mPeriod;
            ALOGV("[%s] Modifying t -> %" PRId64, mName, ns2us(t));
        }

        t -= mWakeupLatency;
        ALOGV("[%s] Corrected for wakeup latency -> %" PRId64, mName, ns2us(t));

        return t;
    }

    void fireCallbackInvocations(const Vector<CallbackInvocation>& callbacks) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        for (size_t i = 0; i < callbacks.size(); i++) {
            callbacks[i].mCallback->onDispSyncEvent(callbacks[i].mEventTime);
        }
    }

    const char* const mName;

    bool mStop;

    nsecs_t mPeriod;
    nsecs_t mPhase;
    nsecs_t mReferenceTime;
    nsecs_t mWakeupLatency;

    int64_t mFrameNumber;

    Vector<EventListener> mEventListeners;

    Mutex mMutex;
    Condition mCond;
};

#undef LOG_TAG
#define LOG_TAG "DispSync"

class ZeroPhaseTracer : public DispSync::Callback {
public:
    ZeroPhaseTracer() : mParity(false) {}

    virtual void onDispSyncEvent(nsecs_t /*when*/) {
        mParity = !mParity;
        ATRACE_INT("ZERO_PHASE_VSYNC", mParity ? 1 : 0);
    }

private:
    bool mParity;
};

DispSync::DispSync(const char* name) :
        mName(name),
        mRefreshSkipCount(0),
        mThread(new DispSyncThread(name)) {

    mThread->run("DispSync", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
    // set DispSync to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 1;
    if (sched_setscheduler(mThread->getTid(), SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO for DispSyncThread");
    }


    reset();
    beginResync();

    if (kTraceDetailedInfo) {
        // If we're not getting present fences then the ZeroPhaseTracer
        // would prevent HW vsync event from ever being turned off.
        // Even if we're just ignoring the fences, the zero-phase tracing is
        // not needed because any time there is an event registered we will
        // turn on the HW vsync events.
        if (!kIgnorePresentFences && kEnableZeroPhaseTracer) {
            addEventListener("ZeroPhaseTracer", 0, new ZeroPhaseTracer());
        }
    }
}

DispSync::~DispSync() {}

void DispSync::reset() {
    Mutex::Autolock lock(mMutex);

    mPhase = 0;
    mReferenceTime = 0;
    mModelUpdated = false;
    mNumResyncSamples = 0;
    mFirstResyncSample = 0;
    mNumResyncSamplesSincePresent = 0;
    resetErrorLocked();
}

bool DispSync::addPresentFence(const sp<Fence>& fence) {
    Mutex::Autolock lock(mMutex);

    mPresentFences[mPresentSampleOffset] = fence;
    mPresentTimes[mPresentSampleOffset] = 0;
    mPresentSampleOffset = (mPresentSampleOffset + 1) % NUM_PRESENT_SAMPLES;
    mNumResyncSamplesSincePresent = 0;

    for (size_t i = 0; i < NUM_PRESENT_SAMPLES; i++) {
        const sp<Fence>& f(mPresentFences[i]);
        if (f != NULL) {
            nsecs_t t = f->getSignalTime();
            if (t < INT64_MAX) {
                mPresentFences[i].clear();
                mPresentTimes[i] = t + kPresentTimeOffset;
            }
        }
    }

    updateErrorLocked();

    return !mModelUpdated || mError > kErrorThreshold;
}

void DispSync::beginResync() {
    Mutex::Autolock lock(mMutex);
    ALOGV("[%s] beginResync", mName);
    mModelUpdated = false;
    mNumResyncSamples = 0;
}

bool DispSync::addResyncSample(nsecs_t timestamp) {
    Mutex::Autolock lock(mMutex);

    ALOGV("[%s] addResyncSample(%" PRId64 ")", mName, ns2us(timestamp));

    size_t idx = (mFirstResyncSample + mNumResyncSamples) % MAX_RESYNC_SAMPLES;
    mResyncSamples[idx] = timestamp;
    if (mNumResyncSamples == 0) {
        mPhase = 0;
        mReferenceTime = timestamp;
        ALOGV("[%s] First resync sample: mPeriod = %" PRId64 ", mPhase = 0, "
                "mReferenceTime = %" PRId64, mName, ns2us(mPeriod),
                ns2us(mReferenceTime));
        mThread->updateModel(mPeriod, mPhase, mReferenceTime);
    }

    if (mNumResyncSamples < MAX_RESYNC_SAMPLES) {
        mNumResyncSamples++;
    } else {
        mFirstResyncSample = (mFirstResyncSample + 1) % MAX_RESYNC_SAMPLES;
    }

    updateModelLocked();

    if (mNumResyncSamplesSincePresent++ > MAX_RESYNC_SAMPLES_WITHOUT_PRESENT) {
        resetErrorLocked();
    }

    if (kIgnorePresentFences) {
        // If we don't have the sync framework we will never have
        // addPresentFence called.  This means we have no way to know whether
        // or not we're synchronized with the HW vsyncs, so we just request
        // that the HW vsync events be turned on whenever we need to generate
        // SW vsync events.
        return mThread->hasAnyEventListeners();
    }

    // Check against kErrorThreshold / 2 to add some hysteresis before having to
    // resync again
    bool modelLocked = mModelUpdated && mError < (kErrorThreshold / 2);
    ALOGV("[%s] addResyncSample returning %s", mName,
            modelLocked ? "locked" : "unlocked");
    return !modelLocked;
}

void DispSync::endResync() {
}

status_t DispSync::addEventListener(const char* name, nsecs_t phase,
        const sp<Callback>& callback) {
    Mutex::Autolock lock(mMutex);
    return mThread->addEventListener(name, phase, callback);
}

void DispSync::setRefreshSkipCount(int count) {
    Mutex::Autolock lock(mMutex);
    ALOGD("setRefreshSkipCount(%d)", count);
    mRefreshSkipCount = count;
    updateModelLocked();
}

status_t DispSync::removeEventListener(const sp<Callback>& callback) {
    Mutex::Autolock lock(mMutex);
    return mThread->removeEventListener(callback);
}

void DispSync::setPeriod(nsecs_t period) {
    Mutex::Autolock lock(mMutex);
    mPeriod = period;
    mPhase = 0;
    mReferenceTime = 0;
    mThread->updateModel(mPeriod, mPhase, mReferenceTime);
}

nsecs_t DispSync::getPeriod() {
    // lock mutex as mPeriod changes multiple times in updateModelLocked
    Mutex::Autolock lock(mMutex);
    return mPeriod;
}

void DispSync::updateModelLocked() {
    ALOGV("[%s] updateModelLocked %zu", mName, mNumResyncSamples);
    if (mNumResyncSamples >= MIN_RESYNC_SAMPLES_FOR_UPDATE) {
        ALOGV("[%s] Computing...", mName);
        nsecs_t durationSum = 0;
        nsecs_t minDuration = INT64_MAX;
        nsecs_t maxDuration = 0;
        for (size_t i = 1; i < mNumResyncSamples; i++) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            size_t prev = (idx + MAX_RESYNC_SAMPLES - 1) % MAX_RESYNC_SAMPLES;
            nsecs_t duration = mResyncSamples[idx] - mResyncSamples[prev];
            durationSum += duration;
            minDuration = min(minDuration, duration);
            maxDuration = max(maxDuration, duration);
        }

        // Exclude the min and max from the average
        durationSum -= minDuration + maxDuration;
        mPeriod = durationSum / (mNumResyncSamples - 3);

        ALOGV("[%s] mPeriod = %" PRId64, mName, ns2us(mPeriod));

        double sampleAvgX = 0;
        double sampleAvgY = 0;
        double scale = 2.0 * M_PI / double(mPeriod);
        // Intentionally skip the first sample
        for (size_t i = 1; i < mNumResyncSamples; i++) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            nsecs_t sample = mResyncSamples[idx] - mReferenceTime;
            double samplePhase = double(sample % mPeriod) * scale;
            sampleAvgX += cos(samplePhase);
            sampleAvgY += sin(samplePhase);
        }

        sampleAvgX /= double(mNumResyncSamples - 1);
        sampleAvgY /= double(mNumResyncSamples - 1);

        mPhase = nsecs_t(atan2(sampleAvgY, sampleAvgX) / scale);

        ALOGV("[%s] mPhase = %" PRId64, mName, ns2us(mPhase));

        if (mPhase < -(mPeriod / 2)) {
            mPhase += mPeriod;
            ALOGV("[%s] Adjusting mPhase -> %" PRId64, mName, ns2us(mPhase));
        }

        if (kTraceDetailedInfo) {
            ATRACE_INT64("DispSync:Period", mPeriod);
            ATRACE_INT64("DispSync:Phase", mPhase + mPeriod / 2);
        }

        // Artificially inflate the period if requested.
        mPeriod += mPeriod * mRefreshSkipCount;

        mThread->updateModel(mPeriod, mPhase, mReferenceTime);
        mModelUpdated = true;
    }
}

void DispSync::updateErrorLocked() {
    if (!mModelUpdated) {
        return;
    }

    // Need to compare present fences against the un-adjusted refresh period,
    // since they might arrive between two events.
    nsecs_t period = mPeriod / (1 + mRefreshSkipCount);

    int numErrSamples = 0;
    nsecs_t sqErrSum = 0;

    for (size_t i = 0; i < NUM_PRESENT_SAMPLES; i++) {
        nsecs_t sample = mPresentTimes[i] - mReferenceTime;
        if (sample > mPhase) {
            nsecs_t sampleErr = (sample - mPhase) % period;
            if (sampleErr > period / 2) {
                sampleErr -= period;
            }
            sqErrSum += sampleErr * sampleErr;
            numErrSamples++;
        }
    }

    if (numErrSamples > 0) {
        mError = sqErrSum / numErrSamples;
    } else {
        mError = 0;
    }

    if (kTraceDetailedInfo) {
        ATRACE_INT64("DispSync:Error", mError);
    }
}

void DispSync::resetErrorLocked() {
    mPresentSampleOffset = 0;
    mError = 0;
    for (size_t i = 0; i < NUM_PRESENT_SAMPLES; i++) {
        mPresentFences[i].clear();
        mPresentTimes[i] = 0;
    }
}

nsecs_t DispSync::computeNextRefresh(int periodOffset) const {
    Mutex::Autolock lock(mMutex);
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    nsecs_t phase = mReferenceTime + mPhase;
    return (((now - phase) / mPeriod) + periodOffset + 1) * mPeriod + phase;
}

void DispSync::dump(String8& result) const {
    Mutex::Autolock lock(mMutex);
    result.appendFormat("present fences are %s\n",
            kIgnorePresentFences ? "ignored" : "used");
    result.appendFormat("mPeriod: %" PRId64 " ns (%.3f fps; skipCount=%d)\n",
            mPeriod, 1000000000.0 / mPeriod, mRefreshSkipCount);
    result.appendFormat("mPhase: %" PRId64 " ns\n", mPhase);
    result.appendFormat("mError: %" PRId64 " ns (sqrt=%.1f)\n",
            mError, sqrt(mError));
    result.appendFormat("mNumResyncSamplesSincePresent: %d (limit %d)\n",
            mNumResyncSamplesSincePresent, MAX_RESYNC_SAMPLES_WITHOUT_PRESENT);
    result.appendFormat("mNumResyncSamples: %zd (max %d)\n",
            mNumResyncSamples, MAX_RESYNC_SAMPLES);

    result.appendFormat("mResyncSamples:\n");
    nsecs_t previous = -1;
    for (size_t i = 0; i < mNumResyncSamples; i++) {
        size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
        nsecs_t sampleTime = mResyncSamples[idx];
        if (i == 0) {
            result.appendFormat("  %" PRId64 "\n", sampleTime);
        } else {
            result.appendFormat("  %" PRId64 " (+%" PRId64 ")\n",
                    sampleTime, sampleTime - previous);
        }
        previous = sampleTime;
    }

    result.appendFormat("mPresentFences / mPresentTimes [%d]:\n",
            NUM_PRESENT_SAMPLES);
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    previous = 0;
    for (size_t i = 0; i < NUM_PRESENT_SAMPLES; i++) {
        size_t idx = (i + mPresentSampleOffset) % NUM_PRESENT_SAMPLES;
        bool signaled = mPresentFences[idx] == NULL;
        nsecs_t presentTime = mPresentTimes[idx];
        if (!signaled) {
            result.appendFormat("  [unsignaled fence]\n");
        } else if (presentTime == 0) {
            result.appendFormat("  0\n");
        } else if (previous == 0) {
            result.appendFormat("  %" PRId64 "  (%.3f ms ago)\n", presentTime,
                    (now - presentTime) / 1000000.0);
        } else {
            result.appendFormat("  %" PRId64 " (+%" PRId64 " / %.3f)  (%.3f ms ago)\n",
                    presentTime, presentTime - previous,
                    (presentTime - previous) / (double) mPeriod,
                    (now - presentTime) / 1000000.0);
        }
        previous = presentTime;
    }

    result.appendFormat("current monotonic time: %" PRId64 "\n", now);
}

} // namespace android
