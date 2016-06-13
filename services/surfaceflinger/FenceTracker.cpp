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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <inttypes.h>
#include "FenceTracker.h"
#include "Layer.h"
#include <utils/Trace.h>

namespace android {

FenceTracker::FenceTracker() :
        mFrameCounter(0),
        mOffset(0),
        mFrames(),
        mMutex() {
}

void FenceTracker::dump(String8* outString) {
    Mutex::Autolock lock(mMutex);
    checkFencesForCompletion();

    for (size_t i = 0; i < MAX_FRAME_HISTORY; i++) {
        int index = (mOffset + i) % MAX_FRAME_HISTORY;
        const FrameRecord& frame = mFrames[index];

        outString->appendFormat("Frame %" PRIu64 "\n", frame.frameId);
        outString->appendFormat("- Refresh start\t%" PRId64 "\n",
                frame.refreshStartTime);

        if (frame.glesCompositionDoneTime) {
            outString->appendFormat("- GLES done\t%" PRId64 "\n",
                    frame.glesCompositionDoneTime);
        } else if (frame.glesCompositionDoneFence != Fence::NO_FENCE) {
            outString->append("- GLES done\tNot signaled\n");
        }
        if (frame.retireTime) {
            outString->appendFormat("- Retire\t%" PRId64 "\n",
                    frame.retireTime);
        } else {
            outString->append("- Retire\tNot signaled\n");
        }
        for (const auto& kv : frame.layers) {
            const LayerRecord& layer = kv.second;
            outString->appendFormat("-- %s\n", layer.name.string());
            outString->appendFormat("---- Frame # %" PRIu64 " (%s)\n",
                    layer.frameNumber,
                    layer.isGlesComposition ? "GLES" : "HWC");
            outString->appendFormat("---- Posted\t%" PRId64 "\n",
                    layer.postedTime);
            if (layer.acquireTime) {
                outString->appendFormat("---- Acquire\t%" PRId64 "\n",
                        layer.acquireTime);
            } else {
                outString->append("---- Acquire\tNot signaled\n");
            }
            if (layer.releaseTime) {
                outString->appendFormat("---- Release\t%" PRId64 "\n",
                        layer.releaseTime);
            } else {
                outString->append("---- Release\tNot signaled\n");
            }
        }
    }
}

static inline bool isValidTimestamp(nsecs_t time) {
    return time > 0 && time < INT64_MAX;
}

void FenceTracker::checkFencesForCompletion() {
    ATRACE_CALL();
    for (auto& frame : mFrames) {
        if (frame.retireFence != Fence::NO_FENCE) {
            nsecs_t time = frame.retireFence->getSignalTime();
            if (isValidTimestamp(time)) {
                frame.retireTime = time;
                frame.retireFence = Fence::NO_FENCE;
            }
        }
        if (frame.glesCompositionDoneFence != Fence::NO_FENCE) {
            nsecs_t time = frame.glesCompositionDoneFence->getSignalTime();
            if (isValidTimestamp(time)) {
                frame.glesCompositionDoneTime = time;
                frame.glesCompositionDoneFence = Fence::NO_FENCE;
            }
        }
        for (auto& kv : frame.layers) {
            LayerRecord& layer = kv.second;
            if (layer.acquireFence != Fence::NO_FENCE) {
                nsecs_t time = layer.acquireFence->getSignalTime();
                if (isValidTimestamp(time)) {
                    layer.acquireTime = time;
                    layer.acquireFence = Fence::NO_FENCE;
                }
            }
            if (layer.releaseFence != Fence::NO_FENCE) {
                nsecs_t time = layer.releaseFence->getSignalTime();
                if (isValidTimestamp(time)) {
                    layer.releaseTime = time;
                    layer.releaseFence = Fence::NO_FENCE;
                }
            }
        }
    }
}

void FenceTracker::addFrame(nsecs_t refreshStartTime, sp<Fence> retireFence,
        const Vector<sp<Layer>>& layers, sp<Fence> glDoneFence) {
    ATRACE_CALL();
    Mutex::Autolock lock(mMutex);
    FrameRecord& frame = mFrames[mOffset];
    FrameRecord& prevFrame = mFrames[(mOffset + MAX_FRAME_HISTORY - 1) %
                                     MAX_FRAME_HISTORY];
    frame.layers.clear();

    bool wasGlesCompositionDone = false;
    const size_t count = layers.size();
    for (size_t i = 0; i < count; i++) {
        String8 name;
        uint64_t frameNumber;
        bool glesComposition;
        nsecs_t postedTime;
        sp<Fence> acquireFence;
        sp<Fence> prevReleaseFence;
        int32_t layerId = layers[i]->getSequence();

        layers[i]->getFenceData(&name, &frameNumber, &glesComposition,
                &postedTime, &acquireFence, &prevReleaseFence);
#ifdef USE_HWC2
        if (glesComposition) {
            frame.layers.emplace(std::piecewise_construct,
                    std::forward_as_tuple(layerId),
                    std::forward_as_tuple(name, frameNumber, glesComposition,
                    postedTime, 0, 0, acquireFence, prevReleaseFence));
            wasGlesCompositionDone = true;
        } else {
            frame.layers.emplace(std::piecewise_construct,
                    std::forward_as_tuple(layerId),
                    std::forward_as_tuple(name, frameNumber, glesComposition,
                    postedTime, 0, 0, acquireFence, Fence::NO_FENCE));
            auto prevLayer = prevFrame.layers.find(layerId);
            if (prevLayer != prevFrame.layers.end()) {
                prevLayer->second.releaseFence = prevReleaseFence;
            }
        }
#else
        frame.layers.emplace(std::piecewise_construct,
                std::forward_as_tuple(layerId),
                std::forward_as_tuple(name, frameNumber, glesComposition,
                postedTime, 0, 0, acquireFence,
                glesComposition ? Fence::NO_FENCE : prevReleaseFence));
        if (glesComposition) {
            wasGlesCompositionDone = true;
        }
#endif
        frame.layers.emplace(std::piecewise_construct,
                std::forward_as_tuple(layerId),
                std::forward_as_tuple(name, frameNumber, glesComposition,
                postedTime, 0, 0, acquireFence, prevReleaseFence));
    }

    frame.frameId = mFrameCounter;
    frame.refreshStartTime = refreshStartTime;
    frame.retireTime = 0;
    frame.glesCompositionDoneTime = 0;
    prevFrame.retireFence = retireFence;
    frame.retireFence = Fence::NO_FENCE;
    frame.glesCompositionDoneFence = wasGlesCompositionDone ? glDoneFence :
            Fence::NO_FENCE;

    mOffset = (mOffset + 1) % MAX_FRAME_HISTORY;
    mFrameCounter++;
}

bool FenceTracker::getFrameTimestamps(const Layer& layer,
        uint64_t frameNumber, FrameTimestamps* outTimestamps) {
    Mutex::Autolock lock(mMutex);
    checkFencesForCompletion();
    int32_t layerId = layer.getSequence();

    size_t i = 0;
    for (; i < MAX_FRAME_HISTORY; i++) {
       if (mFrames[i].layers.count(layerId) &&
               mFrames[i].layers[layerId].frameNumber == frameNumber) {
           break;
       }
    }
    if (i == MAX_FRAME_HISTORY) {
        return false;
    }

    const FrameRecord& frameRecord = mFrames[i];
    const LayerRecord& layerRecord = mFrames[i].layers[layerId];
    outTimestamps->frameNumber = frameNumber;
    outTimestamps->postedTime = layerRecord.postedTime;
    outTimestamps->acquireTime = layerRecord.acquireTime;
    outTimestamps->refreshStartTime = frameRecord.refreshStartTime;
    outTimestamps->glCompositionDoneTime = frameRecord.glesCompositionDoneTime;
    outTimestamps->displayRetireTime = frameRecord.retireTime;
    outTimestamps->releaseTime = layerRecord.releaseTime;
    return true;
}

} // namespace android
