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
#include <gui/IGraphicBufferProducer.h>
#include <private/gui/LayerState.h>

namespace android {

status_t layer_state_t::write(Parcel& output) const
{
    output.writeStrongBinder(surface);
    output.writeUint32(what);
    output.writeFloat(x);
    output.writeFloat(y);
    output.writeUint32(z);
    output.writeUint32(w);
    output.writeUint32(h);
    output.writeUint32(layerStack);
    output.writeFloat(blur);
    output.writeStrongBinder(blurMaskSurface);
    output.writeUint32(blurMaskSampling);
    output.writeFloat(blurMaskAlphaThreshold);
    output.writeFloat(alpha);
    output.writeUint32(flags);
    output.writeUint32(mask);
    *reinterpret_cast<layer_state_t::matrix22_t *>(
            output.writeInplace(sizeof(layer_state_t::matrix22_t))) = matrix;
    output.write(crop);
    output.write(finalCrop);
    output.writeStrongBinder(handle);
    output.writeUint64(frameNumber);
    output.writeInt32(overrideScalingMode);
    output.write(transparentRegion);
    return NO_ERROR;
}

status_t layer_state_t::read(const Parcel& input)
{
    surface = input.readStrongBinder();
    what = input.readUint32();
    x = input.readFloat();
    y = input.readFloat();
    z = input.readUint32();
    w = input.readUint32();
    h = input.readUint32();
    layerStack = input.readUint32();
    blur = input.readFloat();
    blurMaskSurface = input.readStrongBinder();
    blurMaskSampling = input.readUint32();
    blurMaskAlphaThreshold = input.readFloat();
    alpha = input.readFloat();
    flags = static_cast<uint8_t>(input.readUint32());
    mask = static_cast<uint8_t>(input.readUint32());
    const void* matrix_data = input.readInplace(sizeof(layer_state_t::matrix22_t));
    if (matrix_data) {
        matrix = *reinterpret_cast<layer_state_t::matrix22_t const *>(matrix_data);
    } else {
        return BAD_VALUE;
    }
    input.read(crop);
    input.read(finalCrop);
    handle = input.readStrongBinder();
    frameNumber = input.readUint64();
    overrideScalingMode = input.readInt32();
    input.read(transparentRegion);
    return NO_ERROR;
}

status_t ComposerState::write(Parcel& output) const {
    output.writeStrongBinder(IInterface::asBinder(client));
    return state.write(output);
}

status_t ComposerState::read(const Parcel& input) {
    client = interface_cast<ISurfaceComposerClient>(input.readStrongBinder());
    return state.read(input);
}


DisplayState::DisplayState() :
    what(0),
    layerStack(0),
    orientation(eOrientationDefault),
    viewport(Rect::EMPTY_RECT),
    frame(Rect::EMPTY_RECT),
    width(0),
    height(0) {
}

status_t DisplayState::write(Parcel& output) const {
    output.writeStrongBinder(token);
    output.writeStrongBinder(IInterface::asBinder(surface));
    output.writeUint32(what);
    output.writeUint32(layerStack);
    output.writeUint32(orientation);
    output.write(viewport);
    output.write(frame);
    output.writeUint32(width);
    output.writeUint32(height);
    return NO_ERROR;
}

status_t DisplayState::read(const Parcel& input) {
    token = input.readStrongBinder();
    surface = interface_cast<IGraphicBufferProducer>(input.readStrongBinder());
    what = input.readUint32();
    layerStack = input.readUint32();
    orientation = input.readUint32();
    input.read(viewport);
    input.read(frame);
    width = input.readUint32();
    height = input.readUint32();
    return NO_ERROR;
}


}; // namespace android
