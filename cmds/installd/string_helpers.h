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

#ifndef ART_OTAPREOPT_STRING_HELPERS_H_
#define ART_OTAPREOPT_STRING_HELPERS_H_

#include <sstream>
#include <string>

#include <android-base/macros.h>

namespace android {
namespace installd {

static inline bool StringStartsWith(const std::string& target,
                                    const char* prefix) {
    return target.compare(0, strlen(prefix), prefix) == 0;
}

// Split the input according to the separator character. Doesn't honor quotation.
static inline std::vector<std::string> Split(const std::string& in, const char separator) {
    if (in.empty()) {
        return std::vector<std::string>();
    }

    std::vector<std::string> ret;
    std::stringstream strstr(in);
    std::string token;

    while (std::getline(strstr, token, separator)) {
        ret.push_back(token);
    }

    return ret;
}

template <typename StringT>
static inline std::string Join(const std::vector<StringT>& strings, char separator) {
    if (strings.empty()) {
        return "";
    }

    std::string result(strings[0]);
    for (size_t i = 1; i < strings.size(); ++i) {
        result += separator;
        result += strings[i];
    }
    return result;
}

}  // namespace installd
}  // namespace android

#endif  // ART_OTAPREOPT_STRING_HELPERS_H_
