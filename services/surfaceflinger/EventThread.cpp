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

#include <cutils/compiler.h>

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


void EventThread::onVSyncReceived(const wp<IBinder>&, nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);
    mVSyncTimestamp = timestamp;
    mVSyncCount++;
    mCondition.broadcast();
}

bool EventThread::threadLoop() {
    DisplayEventReceiver::Event vsync;
    Vector< sp<EventThread::Connection> > signalConnections;
    signalConnections = waitForEvent(&vsync);

    // dispatch vsync events to listeners...
    const size_t count = signalConnections.size();
    for (size_t i=0 ; i<count ; i++) {
        const sp<Connection>& conn(signalConnections[i]);
        // now see if we still need to report this VSYNC event
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
            removeDisplayEventConnection(signalConnections[i]);
        }
    }
    return true;
}

Vector< sp<EventThread::Connection> > EventThread::waitForEvent(
        DisplayEventReceiver::Event* event)
{
    Mutex::Autolock _l(mLock);

    size_t vsyncCount;
    nsecs_t timestamp;
    Vector< sp<EventThread::Connection> > signalConnections;

    do {
        // latch VSYNC event if any
        bool waitForVSync = false;
        vsyncCount = mVSyncCount;
        timestamp = mVSyncTimestamp;
        mVSyncTimestamp = 0;

        // find out connections waiting for events
        size_t count = mDisplayEventConnections.size();
        for (size_t i=0 ; i<count ; i++) {
            sp<Connection> connection(mDisplayEventConnections[i].promote());
            if (connection != NULL) {
                if (connection->count >= 0) {
                    // we need vsync events because at least
                    // one connection is waiting for it
                    waitForVSync = true;
                    if (timestamp) {
                        // we consume the event only if it's time
                        // (ie: we received a vsync event)
                        if (connection->count == 0) {
                            // fired this time around
                            connection->count = -1;
                            signalConnections.add(connection);
                        } else if (connection->count == 1 ||
                                (vsyncCount % connection->count) == 0) {
                            // continuous event, and time to report it
                            signalConnections.add(connection);
                        }
                    }
                }
            } else {
                // we couldn't promote this reference, the connection has
                // died, so clean-up!
                mDisplayEventConnections.removeAt(i);
                --i; --count;
            }
        }

        // Here we figure out if we need to enable or disable vsyncs
        if (timestamp && !waitForVSync) {
            // we received a VSYNC but we have no clients
            // don't report it, and disable VSYNC events
            disableVSyncLocked();
        } else if (!timestamp && waitForVSync) {
            enableVSyncLocked();
        }

        // note: !timestamp implies signalConnections.isEmpty()
        if (!timestamp) {
            // wait for something to happen
            if (CC_UNLIKELY(mUseSoftwareVSync && waitForVSync)) {
                // h/w vsync cannot be used (screen is off), so we use
                // a  timeout instead. it doesn't matter how imprecise this
                // is, we just need to make sure to serve the clients
                if (mCondition.waitRelative(mLock, ms2ns(16)) == TIMED_OUT) {
                    mVSyncTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                    mVSyncCount++;
                }
            } else {
                // This is where we spend most of our time, waiting
                // for a vsync events and registered clients
                mCondition.wait(mLock);
            }
        }
    } while (signalConnections.isEmpty());

    // here we're guaranteed to have a timestamp and some connections to signal

    // dispatch vsync events to listeners...
    event->header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
    event->header.timestamp = timestamp;
    event->vsync.count = vsyncCount;

    return signalConnections;
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
    // do nothing here -- clean-up will happen automatically
    // when the main thread wakes up
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
