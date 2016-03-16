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

#include <ui/HdrCapabilities.h>

#include <binder/Parcel.h>

namespace android {

status_t HdrCapabilities::writeToParcel(Parcel* parcel) const
{
    status_t result = parcel->writeInt32Vector(mSupportedHdrTypes);
    if (result != OK) {
        return result;
    }
    result = parcel->writeFloat(mMaxLuminance);
    if (result != OK) {
        return result;
    }
    result = parcel->writeFloat(mMaxAverageLuminance);
    if (result != OK) {
        return result;
    }
    result = parcel->writeFloat(mMinLuminance);
    return result;
}

status_t HdrCapabilities::readFromParcel(const Parcel* parcel)
{
    status_t result = parcel->readInt32Vector(&mSupportedHdrTypes);
    if (result != OK) {
        return result;
    }
    result = parcel->readFloat(&mMaxLuminance);
    if (result != OK) {
        return result;
    }
    result = parcel->readFloat(&mMaxAverageLuminance);
    if (result != OK) {
        return result;
    }
    result = parcel->readFloat(&mMinLuminance);
    return result;
}

} // namespace android
