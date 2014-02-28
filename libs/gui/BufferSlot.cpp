#include <gui/BufferSlot.h>

namespace android {

const char* BufferSlot::bufferStateName(BufferState state) {
    switch (state) {
        case BufferSlot::DEQUEUED: return "DEQUEUED";
        case BufferSlot::QUEUED: return "QUEUED";
        case BufferSlot::FREE: return "FREE";
        case BufferSlot::ACQUIRED: return "ACQUIRED";
        default: return "Unknown";
    }
}

} // namespace android
