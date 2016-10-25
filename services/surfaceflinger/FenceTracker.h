/*
 * Copyright 2016 The Android Open Source Project
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

#ifndef ANDROID_FENCETRACKER_H
#define ANDROID_FENCETRACKER_H

#include <ui/Fence.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/Timers.h>
#include <utils/Vector.h>

#include <unordered_map>

namespace android {

class Layer;
struct FrameTimestamps;
/*
 * Keeps a circular buffer of fence/timestamp data for the last N frames in
 * SurfaceFlinger. Gets timestamps for fences after they have signaled.
 */
class FenceTracker {
public:
     FenceTracker();
     void dump(String8* outString);
     void addFrame(nsecs_t refreshStartTime, sp<Fence> retireFence,
             const Vector<sp<Layer>>& layers, sp<Fence> glDoneFence);
     bool getFrameTimestamps(const Layer& layer, uint64_t frameNumber,
             FrameTimestamps* outTimestamps);

protected:
     static constexpr size_t MAX_FRAME_HISTORY = 8;

     struct LayerRecord {
         String8 name; // layer name
         uint64_t frameNumber; // frame number for this layer
         bool isGlesComposition; // was GLES composition used for this layer?
         nsecs_t postedTime; // time when buffer was queued
         nsecs_t acquireTime; // timestamp from the acquire fence
         nsecs_t releaseTime; // timestamp from the release fence
         sp<Fence> acquireFence; // acquire fence
         sp<Fence> releaseFence; // release fence

         LayerRecord(const String8& name, uint64_t frameNumber,
                 bool isGlesComposition, nsecs_t postedTime,
                 nsecs_t acquireTime, nsecs_t releaseTime,
                 sp<Fence> acquireFence, sp<Fence> releaseFence) :
                 name(name), frameNumber(frameNumber),
                 isGlesComposition(isGlesComposition), postedTime(postedTime),
                 acquireTime(acquireTime), releaseTime(releaseTime),
                 acquireFence(acquireFence), releaseFence(releaseFence) {};
         LayerRecord() : name("uninitialized"), frameNumber(0),
                 isGlesComposition(false), postedTime(0), acquireTime(0),
                 releaseTime(0), acquireFence(Fence::NO_FENCE),
                 releaseFence(Fence::NO_FENCE) {};
     };

     struct FrameRecord {
         // global SurfaceFlinger frame counter
         uint64_t frameId;
         // layer data for this frame
         std::unordered_map<int32_t, LayerRecord> layers;
         // timestamp for when SurfaceFlinger::handleMessageRefresh() was called
         nsecs_t refreshStartTime;
         // timestamp from the retire fence
         nsecs_t retireTime;
         // timestamp from the GLES composition completion fence
         nsecs_t glesCompositionDoneTime;
         // primary display retire fence for this frame
         sp<Fence> retireFence;
         // if GLES composition was done, the fence for its completion
         sp<Fence> glesCompositionDoneFence;

         FrameRecord() : frameId(0), layers(), refreshStartTime(0),
                 retireTime(0), glesCompositionDoneTime(0),
                 retireFence(Fence::NO_FENCE),
                 glesCompositionDoneFence(Fence::NO_FENCE) {}
     };

     uint64_t mFrameCounter;
     uint32_t mOffset;
     FrameRecord mFrames[MAX_FRAME_HISTORY];
     Mutex mMutex;

     void checkFencesForCompletion();
};

}

#endif // ANDROID_FRAMETRACKER_H
