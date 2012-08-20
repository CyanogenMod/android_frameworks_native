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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <stdint.h>
#include <sys/types.h>

#include <gui/BitTube.h>
#include <gui/IDisplayEventConnection.h>
#include <gui/DisplayEventReceiver.h>

#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/Trace.h>

#include "EventThread.h"
#include "SurfaceFlinger.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

EventThread::EventThread(const sp<SurfaceFlinger>& flinger)
    : mFlinger(flinger),
      mVSyncTimestamp(0),
      mUseSoftwareVSync(false),
      mVSyncCount(0),
      mDebugVsyncEnabled(false) {
}

void EventThread::onFirstRef() {
    run("EventThread", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
}

sp<EventThread::Connection> EventThread::createEventConnection() const {
    return new Connection(const_cast<EventThread*>(this));
}

status_t EventThread::registerDisplayEventConnection(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.add(connection);
    mCondition.broadcast();
    return NO_ERROR;
}

status_t EventThread::unregisterDisplayEventConnection(
        const wp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.remove(connection);
    mCondition.broadcast();
    return NO_ERROR;
}

void EventThread::removeDisplayEventConnection(
        const wp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.remove(connection);
}

void EventThread::setVsyncRate(uint32_t count,
        const sp<EventThread::Connection>& connection) {
    if (int32_t(count) >= 0) { // server must protect against bad params
        Mutex::Autolock _l(mLock);
        const int32_t new_count = (count == 0) ? -1 : count;
        if (connection->count != new_count) {
            connection->count = new_count;
            mCondition.broadcast();
        }
    }
}

void EventThread::requestNextVsync(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    if (connection->count < 0) {
        connection->count = 0;
        mCondition.broadcast();
    }
}

void EventThread::onScreenReleased() {
    Mutex::Autolock _l(mLock);
    if (!mUseSoftwareVSync) {
        // disable reliance on h/w vsync
        mUseSoftwareVSync = true;
        mCondition.broadcast();
    }
}

void EventThread::onScreenAcquired() {
    Mutex::Autolock _l(mLock);
    if (mUseSoftwareVSync) {
        // resume use of h/w vsync
        mUseSoftwareVSync = false;
        mCondition.broadcast();
    }
}


void EventThread::onVSyncReceived(int, nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);
    mVSyncTimestamp = timestamp;
    mVSyncCount++;
    mCondition.broadcast();
}

bool EventThread::threadLoop() {

    nsecs_t timestamp;
    size_t vsyncCount;
    size_t activeEvents;
    DisplayEventReceiver::Event vsync;
    Vector< sp<EventThread::Connection> > activeConnections;

    do {
        Mutex::Autolock _l(mLock);
        // latch VSYNC event if any
        timestamp = mVSyncTimestamp;
        mVSyncTimestamp = 0;

        // check if we should be waiting for VSYNC events
        activeEvents = 0;
        bool waitForNextVsync = false;
        size_t count = mDisplayEventConnections.size();
        for (size_t i=0 ; i<count ; i++) {
            sp<Connection> connection(mDisplayEventConnections[i].promote());
            if (connection != NULL) {
                activeConnections.add(connection);
                if (connection->count >= 0) {
                    // at least one continuous mode or active one-shot event
                    waitForNextVsync = true;
                    activeEvents++;
                    break;
                }
            }
        }

        if (timestamp) {
            if (!waitForNextVsync) {
                // we received a VSYNC but we have no clients
                // don't report it, and disable VSYNC events
                disableVSyncLocked();
            } else {
                // report VSYNC event
                break;
            }
        } else {
            // never disable VSYNC events immediately, instead
            // we'll wait to receive the event and we'll
            // reevaluate whether we need to dispatch it and/or
            // disable VSYNC events then.
            if (waitForNextVsync) {
                // enable
                enableVSyncLocked();
            }
        }

        // wait for something to happen
        if (mUseSoftwareVSync && waitForNextVsync) {
            // h/w vsync cannot be used (screen is off), so we use
            // a  timeout instead. it doesn't matter how imprecise this
            // is, we just need to make sure to serve the clients
            if (mCondition.waitRelative(mLock, ms2ns(16)) == TIMED_OUT) {
                mVSyncTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                mVSyncCount++;
            }
        } else {
            mCondition.wait(mLock);
        }
        vsyncCount = mVSyncCount;
    } while (!activeConnections.size());

    if (!activeEvents) {
        // no events to return. start over.
        // (here we make sure to exit the scope of this function
        //  so that we release our Connection references)
        return true;
    }

    // dispatch vsync events to listeners...
    vsync.header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
    vsync.header.timestamp = timestamp;
    vsync.vsync.count = vsyncCount;

    const size_t count = activeConnections.size();
    for (size_t i=0 ; i<count ; i++) {
        const sp<Connection>& conn(activeConnections[i]);
        // now see if we still need to report this VSYNC event
        const int32_t vcount = conn->count;
        if (vcount >= 0) {
            bool reportVsync = false;
            if (vcount == 0) {
                // fired this time around
                conn->count = -1;
                reportVsync = true;
            } else if (vcount == 1 || (vsyncCount % vcount) == 0) {
                // continuous event, and time to report it
                reportVsync = true;
            }

            if (reportVsync) {
                status_t err = conn->postEvent(vsync);
                if (err == -EAGAIN || err == -EWOULDBLOCK) {
                    // The destination doesn't accept events anymore, it's probably
                    // full. For now, we just drop the events on the floor.
                    // Note that some events cannot be dropped and would have to be
                    // re-sent later. Right-now we don't have the ability to do
                    // this, but it doesn't matter for VSYNC.
                } else if (err < 0) {
                    // handle any other error on the pipe as fatal. the only
                    // reasonable thing to do is to clean-up this connection.
                    // The most common error we'll get here is -EPIPE.
                    removeDisplayEventConnection(activeConnections[i]);
                }
            }
        }
    }

    return true;
}

void EventThread::enableVSyncLocked() {
    if (!mUseSoftwareVSync) {
        // never enable h/w VSYNC when screen is off
        mFlinger->eventControl(SurfaceFlinger::EVENT_VSYNC, true);
        mPowerHAL.vsyncHint(true);
    }
    mDebugVsyncEnabled = true;
}

void EventThread::disableVSyncLocked() {
    mFlinger->eventControl(SurfaceFlinger::EVENT_VSYNC, false);
    mPowerHAL.vsyncHint(false);
    mDebugVsyncEnabled = false;
}

status_t EventThread::readyToRun() {
    ALOGI("EventThread ready to run.");
    return NO_ERROR;
}

void EventThread::dump(String8& result, char* buffer, size_t SIZE) const {
    Mutex::Autolock _l(mLock);
    result.appendFormat("VSYNC state: %s\n",
            mDebugVsyncEnabled?"enabled":"disabled");
    result.appendFormat("  soft-vsync: %s\n",
            mUseSoftwareVSync?"enabled":"disabled");
    result.appendFormat("  numListeners=%u,\n  events-delivered: %u\n",
            mDisplayEventConnections.size(), mVSyncCount);
    for (size_t i=0 ; i<mDisplayEventConnections.size() ; i++) {
        sp<Connection> connection =
                mDisplayEventConnections.itemAt(i).promote();
        result.appendFormat("    %p: count=%d\n",
                connection.get(), connection!=NULL ? connection->count : 0);
    }
}

// ---------------------------------------------------------------------------

EventThread::Connection::Connection(
        const sp<EventThread>& eventThread)
    : count(-1), mEventThread(eventThread), mChannel(new BitTube())
{
}

EventThread::Connection::~Connection() {
    mEventThread->unregisterDisplayEventConnection(this);
}

void EventThread::Connection::onFirstRef() {
    // NOTE: mEventThread doesn't hold a strong reference on us
    mEventThread->registerDisplayEventConnection(this);
}

sp<BitTube> EventThread::Connection::getDataChannel() const {
    return mChannel;
}

void EventThread::Connection::setVsyncRate(uint32_t count) {
    mEventThread->setVsyncRate(count, this);
}

void EventThread::Connection::requestNextVsync() {
    mEventThread->requestNextVsync(this);
}

status_t EventThread::Connection::postEvent(
        const DisplayEventReceiver::Event& event) {
    ssize_t size = DisplayEventReceiver::sendEvents(mChannel, &event, 1);
    return size < 0 ? status_t(size) : status_t(NO_ERROR);
}

// ---------------------------------------------------------------------------

}; // namespace android
