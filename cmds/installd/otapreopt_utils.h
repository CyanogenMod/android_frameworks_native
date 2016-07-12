/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OTAPREOPT_UTILS_H_
#define OTAPREOPT_UTILS_H_

#include <regex>

namespace android {
namespace installd {

static inline bool ValidateTargetSlotSuffix(const std::string& input) {
    std::regex slot_suffix_regex("[a-zA-Z0-9_]+");
    std::smatch slot_suffix_match;
    return std::regex_match(input, slot_suffix_match, slot_suffix_regex);
}

}  // namespace installd
}  // namespace android

#endif  // OTAPREOPT_UTILS_H_
