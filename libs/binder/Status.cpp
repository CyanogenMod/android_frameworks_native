/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <binder/Status.h>

namespace android {
namespace binder {

Status Status::fromExceptionCode(int32_t exception_code) {
    return Status(exception_code, "");
}

Status Status::fromStatusT(status_t status) {
    Status ret;
    ret.setFromStatusT(status);
    return ret;
}

Status Status::ok() {
    return Status();
}

Status::Status(int32_t exception_code, const String8& message)
    : mException(exception_code),
      mMessage(message) {}

Status::Status(int32_t exception_code, const char* message)
    : mException(exception_code),
      mMessage(message) {}

status_t Status::readFromParcel(const Parcel& parcel) {
    status_t status = parcel.readInt32(&mException);
    if (status != OK) {
        setFromStatusT(status);
        return status;
    }

    // Skip over fat response headers.  Not used (or propagated) in native code.
    if (mException == EX_HAS_REPLY_HEADER) {
        // Note that the header size includes the 4 byte size field.
        const int32_t header_start = parcel.dataPosition();
        int32_t header_size;
        status = parcel.readInt32(&header_size);
        if (status != OK) {
            setFromStatusT(status);
            return status;
        }
        parcel.setDataPosition(header_start + header_size);
        // And fat response headers are currently only used when there are no
        // exceptions, so act like there was no error.
        mException = EX_NONE;
    }

    if (mException == EX_NONE) {
        return status;
    }

    // The remote threw an exception.  Get the message back.
    mMessage = String8(parcel.readString16());

    return status;
}

status_t Status::writeToParcel(Parcel* parcel) const {
    status_t status = parcel->writeInt32(mException);
    if (status != OK) { return status; }
    if (mException == EX_NONE) {
        // We have no more information to write.
        return status;
    }
    status = parcel->writeString16(String16(mMessage));
    return status;
}

void Status::setFromStatusT(status_t status) {
    switch (status) {
        case NO_ERROR:
            mException = EX_NONE;
            mMessage.clear();
            break;
        case UNEXPECTED_NULL:
            mException = EX_NULL_POINTER;
            mMessage.setTo("Unexpected null reference in Parcel");
            break;
        default:
            mException = EX_TRANSACTION_FAILED;
            mMessage.setTo("Transaction failed");
            break;
    }
}

void Status::setException(int32_t ex, const String8& message) {
    mException = ex;
    mMessage.setTo(message);
}

void Status::getException(int32_t* returned_exception,
                          String8* returned_message) const {
    if (returned_exception) {
        *returned_exception = mException;
    }
    if (returned_message) {
        returned_message->setTo(mMessage);
    }
}

String8 Status::toString8() const {
    String8 ret;
    if (mException == EX_NONE) {
        ret.append("No error");
    } else {
        ret.appendFormat("Status(%d): '", mException);
        ret.append(String8(mMessage));
        ret.append("'");
    }
    return ret;
}

}  // namespace binder
}  // namespace android
