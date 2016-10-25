/*
** Copyright 2015, The Android Open Source Project
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

#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>               // TODO: Move everything to base::logging.

#include <globals.h>
#include <installd_constants.h>
#include <utils.h>

#ifndef LOG_TAG
#define LOG_TAG "installd"
#endif

namespace android {
namespace installd {

static constexpr const char* APP_SUBDIR = "app/"; // sub-directory under ANDROID_DATA

static constexpr const char* PRIV_APP_SUBDIR = "priv-app/"; // sub-directory under ANDROID_DATA

static constexpr const char* EPHEMERAL_APP_SUBDIR = "app-ephemeral/"; // sub-directory under
                                                                      // ANDROID_DATA

static constexpr const char* APP_LIB_SUBDIR = "app-lib/"; // sub-directory under ANDROID_DATA

static constexpr const char* MEDIA_SUBDIR = "media/"; // sub-directory under ANDROID_DATA

static constexpr const char* PROFILES_SUBDIR = "misc/profiles"; // sub-directory under ANDROID_DATA

static constexpr const char* PRIVATE_APP_SUBDIR = "app-private/"; // sub-directory under
                                                                  // ANDROID_DATA

/* Directory records that are used in execution of commands. */
dir_rec_t android_app_dir;
dir_rec_t android_app_ephemeral_dir;
dir_rec_t android_app_lib_dir;
dir_rec_t android_app_private_dir;
dir_rec_t android_asec_dir;
dir_rec_t android_data_dir;
dir_rec_t android_media_dir;
dir_rec_t android_mnt_expand_dir;
dir_rec_t android_profiles_dir;

dir_rec_array_t android_system_dirs;

/**
 * Initialize all the global variables that are used elsewhere. Returns 0 upon
 * success and -1 on error.
 */
void free_globals() {
    size_t i;

    for (i = 0; i < android_system_dirs.count; i++) {
        if (android_system_dirs.dirs[i].path != NULL) {
            free(android_system_dirs.dirs[i].path);
        }
    }

    free(android_system_dirs.dirs);
}

bool init_globals_from_data_and_root(const char* data, const char* root) {
    // Get the android data directory.
    if (get_path_from_string(&android_data_dir, data) < 0) {
        return false;
    }

    // Get the android app directory.
    if (copy_and_append(&android_app_dir, &android_data_dir, APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android protected app directory.
    if (copy_and_append(&android_app_private_dir, &android_data_dir, PRIVATE_APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android ephemeral app directory.
    if (copy_and_append(&android_app_ephemeral_dir, &android_data_dir, EPHEMERAL_APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android app native library directory.
    if (copy_and_append(&android_app_lib_dir, &android_data_dir, APP_LIB_SUBDIR) < 0) {
        return false;
    }

    // Get the sd-card ASEC mount point.
    if (get_path_from_env(&android_asec_dir, ASEC_MOUNTPOINT_ENV_NAME) < 0) {
        return false;
    }

    // Get the android media directory.
    if (copy_and_append(&android_media_dir, &android_data_dir, MEDIA_SUBDIR) < 0) {
        return false;
    }

    // Get the android external app directory.
    if (get_path_from_string(&android_mnt_expand_dir, "/mnt/expand/") < 0) {
        return false;
    }

    // Get the android profiles directory.
    if (copy_and_append(&android_profiles_dir, &android_data_dir, PROFILES_SUBDIR) < 0) {
        return false;
    }

    // Take note of the system and vendor directories.
    android_system_dirs.count = 4;

    android_system_dirs.dirs = (dir_rec_t*) calloc(android_system_dirs.count, sizeof(dir_rec_t));
    if (android_system_dirs.dirs == NULL) {
        ALOGE("Couldn't allocate array for dirs; aborting\n");
        return false;
    }

    dir_rec_t android_root_dir;
    if (get_path_from_string(&android_root_dir, root) < 0) {
        return false;
    }

    android_system_dirs.dirs[0].path = build_string2(android_root_dir.path, APP_SUBDIR);
    android_system_dirs.dirs[0].len = strlen(android_system_dirs.dirs[0].path);

    android_system_dirs.dirs[1].path = build_string2(android_root_dir.path, PRIV_APP_SUBDIR);
    android_system_dirs.dirs[1].len = strlen(android_system_dirs.dirs[1].path);

    android_system_dirs.dirs[2].path = strdup("/vendor/app/");
    android_system_dirs.dirs[2].len = strlen(android_system_dirs.dirs[2].path);

    android_system_dirs.dirs[3].path = strdup("/oem/app/");
    android_system_dirs.dirs[3].len = strlen(android_system_dirs.dirs[3].path);

    return true;
}

}  // namespace installd
}  // namespace android
