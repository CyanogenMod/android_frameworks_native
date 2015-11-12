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

int install(const char *uuid, const char *pkgname, uid_t uid, gid_t gid, const char *seinfo);
int uninstall(const char *uuid, const char *pkgname, userid_t userid);
int renamepkg(const char *oldpkgname, const char *newpkgname);
int fix_uid(const char *uuid, const char *pkgname, uid_t uid, gid_t gid);
int delete_user_data(const char *uuid, const char *pkgname, userid_t userid);
int make_user_data(const char *uuid, const char *pkgname, uid_t uid,
        userid_t userid, const char* seinfo);
int copy_complete_app(const char* from_uuid, const char *to_uuid,
        const char *package_name, const char *data_app_name, appid_t appid,
        const char* seinfo);
int make_user_config(userid_t userid);
int delete_user(const char *uuid, userid_t userid);
int delete_cache(const char *uuid, const char *pkgname, userid_t userid);
int delete_code_cache(const char *uuid, const char *pkgname, userid_t userid);
int move_dex(const char *src, const char *dst, const char *instruction_set);
int rm_dex(const char *path, const char *instruction_set);
int protect(char *pkgname, gid_t gid);
int get_size(const char *uuid, const char *pkgname, int userid,
        const char *apkpath, const char *libdirpath,
        const char *fwdlock_apkpath, const char *asecpath,
        const char *instruction_set, int64_t *codesize, int64_t *datasize,
        int64_t *cachesize, int64_t *asecsize);
int free_cache(const char *uuid, int64_t free_size);
int dexopt(const char *apk_path, uid_t uid, const char *pkgName, const char *instruction_set,
           int dexopt_needed, const char* oat_dir, int dexopt_flags);
int mark_boot_complete(const char *instruction_set);
int movefiles();
int linklib(const char* uuid, const char* pkgname, const char* asecLibDir, int userId);
int idmap(const char *target_path, const char *overlay_path, uid_t uid);
int restorecon_data(const char *uuid, const char* pkgName, const char* seinfo, uid_t uid);
int create_oat_dir(const char* oat_dir, const char *instruction_set);
int rm_package_dir(const char* apk_path);
int move_package_dir(char path[PKG_PATH_MAX], const char *oat_dir, const char *apk_path,
                            const char *instruction_set);
int link_file(const char *relative_path, const char *from_base, const char *to_base);

}  // namespace installd
}  // namespace android

#endif  // COMMANDS_H_
