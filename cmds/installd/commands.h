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

#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <inttypes.h>
#include <unistd.h>

#include <cutils/multiuser.h>

#include <installd_constants.h>

namespace android {
namespace installd {

int create_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags,
        appid_t appid, const char* seinfo, int target_sdk_version);
int restorecon_app_data(const char* uuid, const char* pkgName, userid_t userid, int flags,
        appid_t appid, const char* seinfo);
int migrate_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags);
int clear_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags,
        ino_t ce_data_inode);
int destroy_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags,
        ino_t ce_data_inode);

int move_complete_app(const char* from_uuid, const char *to_uuid, const char *package_name,
        const char *data_app_name, appid_t appid, const char* seinfo, int target_sdk_version);

int get_app_size(const char *uuid, const char *pkgname, int userid, int flags, ino_t ce_data_inode,
        const char* code_path, int64_t *codesize, int64_t *datasize, int64_t *cachesize,
        int64_t *asecsize);
int get_app_data_inode(const char *uuid, const char *pkgname, int userid, int flags, ino_t *inode);

int create_user_data(const char *uuid, userid_t userid, int user_serial, int flags);
int destroy_user_data(const char *uuid, userid_t userid, int flags);

int rm_dex(const char *path, const char *instruction_set);
int free_cache(const char *uuid, int64_t free_size);

bool merge_profiles(uid_t uid, const char *pkgname);

bool dump_profile(uid_t uid, const char *pkgname, const char *dex_files);

int dexopt(const char *apk_path,
           uid_t uid,
           const char *pkgName,
           const char *instruction_set,
           int dexopt_needed,
           const char* oat_dir,
           int dexopt_flags,
           const char* compiler_filter,
           const char* volume_uuid,
           const char* shared_libraries);
static_assert(DEXOPT_PARAM_COUNT == 10U, "Unexpected dexopt param size");

// Helper for the above, converting arguments.
int dexopt(const char* params[DEXOPT_PARAM_COUNT]);

int mark_boot_complete(const char *instruction_set);
int linklib(const char* uuid, const char* pkgname, const char* asecLibDir, int userId);
int idmap(const char *target_path, const char *overlay_path, uid_t uid);
int create_oat_dir(const char* oat_dir, const char *instruction_set);
int rm_package_dir(const char* apk_path);
int clear_app_profiles(const char* pkgname);
int destroy_app_profiles(const char* pkgname);
int link_file(const char *relative_path, const char *from_base, const char *to_base);

// Move a B version over to the A location. Only works for oat_dir != nullptr.
int move_ab(const char *apk_path, const char *instruction_set, const char* oat_dir);

}  // namespace installd
}  // namespace android

#endif  // COMMANDS_H_
