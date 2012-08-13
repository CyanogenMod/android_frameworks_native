/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <utils/Errors.h>
#include <binder/Parcel.h>
#include <gui/ISurfaceComposerClient.h>
#include <gui/ISurfaceTexture.h>
#include <private/gui/LayerState.h>

namespace android {

status_t layer_state_t::write(Parcel& output) const
{
    status_t err;

    err = output.write(transparentRegion);
    if (err < NO_ERROR) return err;

    // NOTE: regions are at the end of the structure
    size_t size = sizeof(layer_state_t);
    size -= sizeof(transparentRegion);
    err = output.write(this, size);
    return err;
}

status_t layer_state_t::read(const Parcel& input)
{
    status_t err;

    err = input.read(transparentRegion);
    if (err < NO_ERROR) return err;

    // NOTE: regions are at the end of the structure
    size_t size = sizeof(layer_state_t);
    size -= sizeof(transparentRegion);
    input.read(this, size);
    return NO_ERROR;
}

status_t ComposerState::write(Parcel& output) const {
    output.writeStrongBinder(client->asBinder());
    return state.write(output);
}

status_t ComposerState::read(const Parcel& input) {
    client = interface_cast<ISurfaceComposerClient>(input.readStrongBinder());
    return state.read(input);
}


status_t DisplayState::write(Parcel& output) const {
    output.writeStrongBinder(token);
    output.writeStrongBinder(surface->asBinder());
    output.writeInt32(what);
    output.writeInt32(layerStack);
    output.writeInt32(orientation);
    output.write(viewport);
    output.write(frame);
    return NO_ERROR;
}

status_t DisplayState::read(const Parcel& input) {
    token = input.readStrongBinder();
    surface = interface_cast<ISurfaceTexture>(input.readStrongBinder());
    what = input.readInt32();
    layerStack = input.readInt32();
    orientation = input.readInt32();
    input.read(viewport);
    input.read(frame);
    return NO_ERROR;
}


}; // namespace android
