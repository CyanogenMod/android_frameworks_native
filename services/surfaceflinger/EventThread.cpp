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
#include <utils/Trace.h>

#include "DisplayHardware/DisplayHardware.h"
#include "EventThread.h"
#include "SurfaceFlinger.h"

// ---------------------------------------------------------------------------

namespace android {

// ---------------------------------------------------------------------------

EventThread::EventThread(const sp<SurfaceFlinger>& flinger)
    : mFlinger(flinger),
      mHw(flinger->graphicPlane(0).editDisplayHardware()),
      mLastVSyncTimestamp(0),
      mVSyncTimestamp(0),
      mDeliveredEvents(0)
{
}

void EventThread::onFirstRef() {
    mHw.setVSyncHandler(this);
    run("EventThread", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
}

sp<EventThread::Connection> EventThread::createEventConnection() const {
    return new Connection(const_cast<EventThread*>(this));
}

nsecs_t EventThread::getLastVSyncTimestamp() const {
    Mutex::Autolock _l(mLock);
    return mLastVSyncTimestamp;
}

nsecs_t EventThread::getVSyncPeriod() const {
    return mHw.getRefreshPeriod();

}

status_t EventThread::registerDisplayEventConnection(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.add(connection);
    mCondition.signal();
    return NO_ERROR;
}

status_t EventThread::unregisterDisplayEventConnection(
        const wp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    mDisplayEventConnections.remove(connection);
    mCondition.signal();
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
            mCondition.signal();
        }
    }
}

void EventThread::requestNextVsync(
        const sp<EventThread::Connection>& connection) {
    Mutex::Autolock _l(mLock);
    if (connection->count < 0) {
        connection->count = 0;
        mCondition.signal();
    }
}

void EventThread::onVSyncReceived(int, nsecs_t timestamp) {
    Mutex::Autolock _l(mLock);
    mVSyncTimestamp = timestamp;
    mCondition.signal();
}

bool EventThread::threadLoop() {

    nsecs_t timestamp;
    DisplayEventReceiver::Event vsync;
    Vector< wp<EventThread::Connection> > displayEventConnections;

    do {

        Mutex::Autolock _l(mLock);
        do {
            // check if we have received a VSYNC event
            if (mVSyncTimestamp) {
                // we have a VSYNC event pending
                timestamp = mVSyncTimestamp;
                mVSyncTimestamp = 0;
                break;
            }

            // check if we should be waiting for VSYNC events
            bool waitForNextVsync = false;
            size_t count = mDisplayEventConnections.size();
            for (size_t i=0 ; i<count ; i++) {
                sp<Connection> connection =
                        mDisplayEventConnections.itemAt(i).promote();
                if (connection!=0 && connection->count >= 0) {
                    // at least one continuous mode or active one-shot event
                    waitForNextVsync = true;
                    break;
                }
            }

            // enable or disable VSYNC events
            mHw.getHwComposer().eventControl(
                    HWComposer::EVENT_VSYNC, waitForNextVsync);

            // wait for something to happen
            mCondition.wait(mLock);
        } while(true);

        // process vsync event

        ATRACE_INT("VSYNC", mDeliveredEvents&1);
        mDeliveredEvents++;
        mLastVSyncTimestamp = timestamp;

        // now see if we still need to report this VSYNC event
        const size_t count = mDisplayEventConnections.size();
        for (size_t i=0 ; i<count ; i++) {
            bool reportVsync = false;
            sp<Connection> connection =
                    mDisplayEventConnections.itemAt(i).promote();
            if (connection == 0)
                continue;

            const int32_t count = connection->count;
            if (count >= 1) {
                if (count==1 || (mDeliveredEvents % count) == 0) {
                    // continuous event, and time to report it
                    reportVsync = true;
                }
            } else if (count >= -1) {
                if (count == 0) {
                    // fired this time around
                    reportVsync = true;
                }
                connection->count--;
            }
            if (reportVsync) {
                displayEventConnections.add(connection);
            }
        }
    } while (!displayEventConnections.size());

    // dispatch vsync events to listeners...
    vsync.header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
    vsync.header.timestamp = timestamp;
    vsync.vsync.count = mDeliveredEvents;

    const size_t count = displayEventConnections.size();
    for (size_t i=0 ; i<count ; i++) {
        sp<Connection> conn(displayEventConnections[i].promote());
        // make sure the connection didn't die
        if (conn != NULL) {
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
                removeDisplayEventConnection(displayEventConnections[i]);
            }
        } else {
            // somehow the connection is dead, but we still have it in our list
            // just clean the list.
            removeDisplayEventConnection(displayEventConnections[i]);
        }
    }

    // clear all our references without holding mLock
    displayEventConnections.clear();

    return true;
}

status_t EventThread::readyToRun() {
    ALOGI("EventThread ready to run.");
    return NO_ERROR;
}

void EventThread::dump(String8& result, char* buffer, size_t SIZE) const {
    Mutex::Autolock _l(mLock);
    result.append("VSYNC state:\n");
    snprintf(buffer, SIZE, "  numListeners=%u, events-delivered: %u\n",
            mDisplayEventConnections.size(), mDeliveredEvents);
    result.append(buffer);
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
