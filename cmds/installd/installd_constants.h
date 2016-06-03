/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef INSTALLD_CONSTANTS_H_
#define INSTALLD_CONSTANTS_H_

namespace android {
namespace installd {

constexpr size_t DEXOPT_PARAM_COUNT = 10U;

/* elements combined with a valid package name to form paths */

constexpr const char* PRIMARY_USER_PREFIX = "data/";
constexpr const char* SECONDARY_USER_PREFIX = "user/";

constexpr const char* PKG_DIR_POSTFIX = "";

constexpr const char* PKG_LIB_POSTFIX = "/lib";

constexpr const char* CACHE_DIR_POSTFIX = "/cache";
constexpr const char* CODE_CACHE_DIR_POSTFIX = "/code_cache";

constexpr const char* APP_SUBDIR = "app/"; // sub-directory under ANDROID_DATA
constexpr const char* PRIV_APP_SUBDIR = "priv-app/"; // sub-directory under ANDROID_DATA
constexpr const char* EPHEMERAL_APP_SUBDIR = "app-ephemeral/"; // sub-directory under ANDROID_DATA

constexpr const char* APP_LIB_SUBDIR = "app-lib/"; // sub-directory under ANDROID_DATA

constexpr const char* MEDIA_SUBDIR = "media/"; // sub-directory under ANDROID_DATA

constexpr const char* PROFILES_SUBDIR = "misc/profiles"; // sub-directory under ANDROID_DATA

/* other handy constants */

constexpr const char* PRIVATE_APP_SUBDIR = "app-private/"; // sub-directory under ANDROID_DATA

// This is used as a string literal, can't be constants. TODO: std::string...
#define DALVIK_CACHE "dalvik-cache"
constexpr const char* DALVIK_CACHE_POSTFIX = "/classes.dex";
constexpr const char* DALVIK_CACHE_POSTFIX2 = "@classes.dex";

constexpr const char* IDMAP_PREFIX = "/data/resource-cache/";
constexpr const char* IDMAP_SUFFIX = "@idmap";

constexpr size_t PKG_NAME_MAX = 128u;   /* largest allowed package name */
constexpr size_t PKG_PATH_MAX = 256u;   /* max size of any path we use */

// NOTE: keep in sync with StorageManager
constexpr int FLAG_STORAGE_DE = 1 << 0;
constexpr int FLAG_STORAGE_CE = 1 << 1;

// NOTE: keep in sync with Installer
constexpr int FLAG_CLEAR_CACHE_ONLY = 1 << 8;
constexpr int FLAG_CLEAR_CODE_CACHE_ONLY = 1 << 9;

/* dexopt needed flags matching those in dalvik.system.DexFile */
constexpr int DEXOPT_DEX2OAT_NEEDED       = 1;
constexpr int DEXOPT_PATCHOAT_NEEDED      = 2;
constexpr int DEXOPT_SELF_PATCHOAT_NEEDED = 3;

/****************************************************************************
 * IMPORTANT: These values are passed from Java code. Keep them in sync with
 * frameworks/base/services/core/java/com/android/server/pm/Installer.java
 ***************************************************************************/
constexpr int DEXOPT_PUBLIC         = 1 << 1;
constexpr int DEXOPT_SAFEMODE       = 1 << 2;
constexpr int DEXOPT_DEBUGGABLE     = 1 << 3;
constexpr int DEXOPT_BOOTCOMPLETE   = 1 << 4;
constexpr int DEXOPT_PROFILE_GUIDED = 1 << 5;
constexpr int DEXOPT_OTA            = 1 << 6;

/* all known values for dexopt flags */
constexpr int DEXOPT_MASK =
    DEXOPT_PUBLIC
    | DEXOPT_SAFEMODE
    | DEXOPT_DEBUGGABLE
    | DEXOPT_BOOTCOMPLETE
    | DEXOPT_PROFILE_GUIDED
    | DEXOPT_OTA;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

}  // namespace installd
}  // namespace android

#endif  // INSTALLD_CONSTANTS_H_
