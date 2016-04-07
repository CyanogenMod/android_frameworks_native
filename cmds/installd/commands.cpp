/*
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

#include "commands.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <android-base/stringprintf.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <cutils/fs.h>
#include <cutils/log.h>               // TODO: Move everything to base/logging.
#include <cutils/sched_policy.h>
#include <diskusage/dirsize.h>
#include <logwrap/logwrap.h>
#include <private/android_filesystem_config.h>
#include <selinux/android.h>
#include <system/thread_defs.h>

#include <globals.h>
#include <installd_deps.h>
#include <utils.h>

#ifndef LOG_TAG
#define LOG_TAG "installd"
#endif

using android::base::StringPrintf;

namespace android {
namespace installd {

static constexpr const char* kCpPath = "/system/bin/cp";
static constexpr const char* kXattrDefault = "user.default";

#define MIN_RESTRICTED_HOME_SDK_VERSION 24 // > M

typedef int fd_t;

static bool property_get_bool(const char* property_name, bool default_value = false) {
    char tmp_property_value[kPropertyValueMax];
    bool have_property = get_property(property_name, tmp_property_value, nullptr) > 0;
    if (!have_property) {
        return default_value;
    }
    return strcmp(tmp_property_value, "true") == 0;
}

int create_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags,
        appid_t appid, const char* seinfo, int target_sdk_version) {
    uid_t uid = multiuser_get_uid(userid, appid);
    int target_mode = target_sdk_version >= MIN_RESTRICTED_HOME_SDK_VERSION ? 0700 : 0751;
    if (flags & FLAG_STORAGE_CE) {
        auto path = create_data_user_package_path(uuid, userid, pkgname);
        if (fs_prepare_dir_strict(path.c_str(), target_mode, uid, uid) != 0) {
            PLOG(ERROR) << "Failed to prepare " << path;
            return -1;
        }
        if (selinux_android_setfilecon(path.c_str(), pkgname, seinfo, uid) < 0) {
            PLOG(ERROR) << "Failed to setfilecon " << path;
            return -1;
        }
    }
    if (flags & FLAG_STORAGE_DE) {
        auto path = create_data_user_de_package_path(uuid, userid, pkgname);
        if (fs_prepare_dir_strict(path.c_str(), target_mode, uid, uid) == -1) {
            PLOG(ERROR) << "Failed to prepare " << path;
            // TODO: include result once 25796509 is fixed
            return 0;
        }
        if (selinux_android_setfilecon(path.c_str(), pkgname, seinfo, uid) < 0) {
            PLOG(ERROR) << "Failed to setfilecon " << path;
            // TODO: include result once 25796509 is fixed
            return 0;
        }

        if (property_get_bool("dalvik.vm.usejitprofiles")) {
            const std::string profile_path = create_data_user_profile_package_path(userid, pkgname);
            // read-write-execute only for the app user.
            if (fs_prepare_dir_strict(profile_path.c_str(), 0700, uid, uid) != 0) {
                PLOG(ERROR) << "Failed to prepare " << profile_path;
                return -1;
            }
            const std::string ref_profile_path = create_data_ref_profile_package_path(pkgname);
            // dex2oat/profman runs under the shared app gid and it needs to read/write reference
            // profiles.
            appid_t shared_app_gid = multiuser_get_shared_app_gid(uid);
            if (fs_prepare_dir_strict(
                    ref_profile_path.c_str(), 0700, shared_app_gid, shared_app_gid) != 0) {
                PLOG(ERROR) << "Failed to prepare " << ref_profile_path;
                return -1;
            }
        }
    }
    return 0;
}

int migrate_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags) {
    // This method only exists to upgrade system apps that have requested
    // forceDeviceEncrypted, so their default storage always lives in a
    // consistent location.  This only works on non-FBE devices, since we
    // never want to risk exposing data on a device with real CE/DE storage.

    auto ce_path = create_data_user_package_path(uuid, userid, pkgname);
    auto de_path = create_data_user_de_package_path(uuid, userid, pkgname);

    // If neither directory is marked as default, assume CE is default
    if (getxattr(ce_path.c_str(), kXattrDefault, nullptr, 0) == -1
            && getxattr(de_path.c_str(), kXattrDefault, nullptr, 0) == -1) {
        if (setxattr(ce_path.c_str(), kXattrDefault, nullptr, 0, 0) != 0) {
            PLOG(ERROR) << "Failed to mark default storage " << ce_path;
            return -1;
        }
    }

    // Migrate default data location if needed
    auto target = (flags & FLAG_STORAGE_DE) ? de_path : ce_path;
    auto source = (flags & FLAG_STORAGE_DE) ? ce_path : de_path;

    if (getxattr(target.c_str(), kXattrDefault, nullptr, 0) == -1) {
        LOG(WARNING) << "Requested default storage " << target
                << " is not active; migrating from " << source;
        if (delete_dir_contents_and_dir(target) != 0) {
            PLOG(ERROR) << "Failed to delete";
            return -1;
        }
        if (rename(source.c_str(), target.c_str()) != 0) {
            PLOG(ERROR) << "Failed to rename";
            return -1;
        }
    }

    return 0;
}

// Keep profile paths in sync with ActivityThread.
constexpr const char* PRIMARY_PROFILE_NAME = "primary.prof";
static std::string create_primary_profile(const std::string& profile_dir) {
    return StringPrintf("%s/%s", profile_dir.c_str(), PRIMARY_PROFILE_NAME);
}

static bool clear_profile(const std::string& profile) {
    base::unique_fd ufd(open(profile.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC));
    if (ufd.get() < 0) {
        if (errno != ENOENT) {
            PLOG(WARNING) << "Could not open profile " << profile;
            return false;
        } else {
            // Nothing to clear. That's ok.
            return true;
        }
    }

    if (flock(ufd.get(), LOCK_EX | LOCK_NB) != 0) {
        if (errno != EWOULDBLOCK) {
            PLOG(WARNING) << "Error locking profile " << profile;
        }
        // This implies that the app owning this profile is running
        // (and has acquired the lock).
        //
        // If we can't acquire the lock bail out since clearing is useless anyway
        // (the app will write again to the profile).
        //
        // Note:
        // This does not impact the this is not an issue for the profiling correctness.
        // In case this is needed because of an app upgrade, profiles will still be
        // eventually cleared by the app itself due to checksum mismatch.
        // If this is needed because profman advised, then keeping the data around
        // until the next run is again not an issue.
        //
        // If the app attempts to acquire a lock while we've held one here,
        // it will simply skip the current write cycle.
        return false;
    }

    bool truncated = ftruncate(ufd.get(), 0) == 0;
    if (!truncated) {
        PLOG(WARNING) << "Could not truncate " << profile;
    }
    if (flock(ufd.get(), LOCK_UN) != 0) {
        PLOG(WARNING) << "Error unlocking profile " << profile;
    }
    return truncated;
}

static bool clear_reference_profile(const char* pkgname) {
    std::string reference_profile_dir = create_data_ref_profile_package_path(pkgname);
    std::string reference_profile = create_primary_profile(reference_profile_dir);
    return clear_profile(reference_profile);
}

static bool clear_current_profile(const char* pkgname, userid_t user) {
    std::string profile_dir = create_data_user_profile_package_path(user, pkgname);
    std::string profile = create_primary_profile(profile_dir);
    return clear_profile(profile);
}

static bool clear_current_profiles(const char* pkgname) {
    bool success = true;
    std::vector<userid_t> users = get_known_users(/*volume_uuid*/ nullptr);
    for (auto user : users) {
        success &= clear_current_profile(pkgname, user);
    }
    return success;
}

int clear_app_profiles(const char* pkgname) {
    bool success = true;
    success &= clear_reference_profile(pkgname);
    success &= clear_current_profiles(pkgname);
    return success ? 0 : -1;
}

int clear_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags) {
    std::string suffix = "";
    bool only_cache = false;
    if (flags & FLAG_CLEAR_CACHE_ONLY) {
        suffix = CACHE_DIR_POSTFIX;
        only_cache = true;
    } else if (flags & FLAG_CLEAR_CODE_CACHE_ONLY) {
        suffix = CODE_CACHE_DIR_POSTFIX;
        only_cache = true;
    }

    int res = 0;
    if (flags & FLAG_STORAGE_CE) {
        auto path = create_data_user_package_path(uuid, userid, pkgname) + suffix;
        if (access(path.c_str(), F_OK) == 0) {
            res |= delete_dir_contents(path);
        }
    }
    if (flags & FLAG_STORAGE_DE) {
        auto path = create_data_user_de_package_path(uuid, userid, pkgname) + suffix;
        if (access(path.c_str(), F_OK) == 0) {
            // TODO: include result once 25796509 is fixed
            delete_dir_contents(path);
        }
        if (!only_cache) {
            if (!clear_current_profile(pkgname, userid)) {
                res |= -1;
            }
        }
    }
    return res;
}

static int destroy_app_reference_profile(const char *pkgname) {
    return delete_dir_contents_and_dir(
        create_data_ref_profile_package_path(pkgname),
        /*ignore_if_missing*/ true);
}

static int destroy_app_current_profiles(const char *pkgname, userid_t userid) {
    return delete_dir_contents_and_dir(
        create_data_user_profile_package_path(userid, pkgname),
        /*ignore_if_missing*/ true);
}

int destroy_app_profiles(const char *pkgname) {
    int result = 0;
    std::vector<userid_t> users = get_known_users(/*volume_uuid*/ nullptr);
    for (auto user : users) {
        result |= destroy_app_current_profiles(pkgname, user);
    }
    result |= destroy_app_reference_profile(pkgname);
    return result;
}

int destroy_app_data(const char *uuid, const char *pkgname, userid_t userid, int flags) {
    int res = 0;
    if (flags & FLAG_STORAGE_CE) {
        res |= delete_dir_contents_and_dir(
                create_data_user_package_path(uuid, userid, pkgname));
    }
    if (flags & FLAG_STORAGE_DE) {
        res |= delete_dir_contents_and_dir(
                create_data_user_de_package_path(uuid, userid, pkgname));
        destroy_app_current_profiles(pkgname, userid);
        // TODO(calin): If the package is still installed by other users it's probably
        // beneficial to keep the reference profile around.
        // Verify if it's ok to do that.
        destroy_app_reference_profile(pkgname);
    }
    return res;
}

int move_complete_app(const char *from_uuid, const char *to_uuid, const char *package_name,
        const char *data_app_name, appid_t appid, const char* seinfo, int target_sdk_version) {
    std::vector<userid_t> users = get_known_users(from_uuid);

    // Copy app
    {
        std::string from(create_data_app_package_path(from_uuid, data_app_name));
        std::string to(create_data_app_package_path(to_uuid, data_app_name));
        std::string to_parent(create_data_app_path(to_uuid));

        char *argv[] = {
            (char*) kCpPath,
            (char*) "-F", /* delete any existing destination file first (--remove-destination) */
            (char*) "-p", /* preserve timestamps, ownership, and permissions */
            (char*) "-R", /* recurse into subdirectories (DEST must be a directory) */
            (char*) "-P", /* Do not follow symlinks [default] */
            (char*) "-d", /* don't dereference symlinks */
            (char*) from.c_str(),
            (char*) to_parent.c_str()
        };

        LOG(DEBUG) << "Copying " << from << " to " << to;
        int rc = android_fork_execvp(ARRAY_SIZE(argv), argv, NULL, false, true);

        if (rc != 0) {
            LOG(ERROR) << "Failed copying " << from << " to " << to
                    << ": status " << rc;
            goto fail;
        }

        if (selinux_android_restorecon(to.c_str(), SELINUX_ANDROID_RESTORECON_RECURSE) != 0) {
            LOG(ERROR) << "Failed to restorecon " << to;
            goto fail;
        }
    }

    // Copy private data for all known users
    // TODO: handle user_de paths
    for (auto user : users) {
        std::string from(create_data_user_package_path(from_uuid, user, package_name));
        std::string to(create_data_user_package_path(to_uuid, user, package_name));
        std::string to_parent(create_data_user_path(to_uuid, user));

        // Data source may not exist for all users; that's okay
        if (access(from.c_str(), F_OK) != 0) {
            LOG(INFO) << "Missing source " << from;
            continue;
        }

        std::string user_path(create_data_user_path(to_uuid, user));
        if (fs_prepare_dir(user_path.c_str(), 0771, AID_SYSTEM, AID_SYSTEM) != 0) {
            LOG(ERROR) << "Failed to prepare user target " << user_path;
            goto fail;
        }

        if (create_app_data(to_uuid, package_name, user, FLAG_STORAGE_CE | FLAG_STORAGE_DE,
                appid, seinfo, target_sdk_version) != 0) {
            LOG(ERROR) << "Failed to create package target " << to;
            goto fail;
        }

        char *argv[] = {
            (char*) kCpPath,
            (char*) "-F", /* delete any existing destination file first (--remove-destination) */
            (char*) "-p", /* preserve timestamps, ownership, and permissions */
            (char*) "-R", /* recurse into subdirectories (DEST must be a directory) */
            (char*) "-P", /* Do not follow symlinks [default] */
            (char*) "-d", /* don't dereference symlinks */
            (char*) from.c_str(),
            (char*) to_parent.c_str()
        };

        LOG(DEBUG) << "Copying " << from << " to " << to;
        int rc = android_fork_execvp(ARRAY_SIZE(argv), argv, NULL, false, true);

        if (rc != 0) {
            LOG(ERROR) << "Failed copying " << from << " to " << to
                    << ": status " << rc;
            goto fail;
        }

        if (restorecon_app_data(to_uuid, package_name, user, FLAG_STORAGE_CE | FLAG_STORAGE_DE,
                appid, seinfo) != 0) {
            LOG(ERROR) << "Failed to restorecon";
            goto fail;
        }
    }

    // We let the framework scan the new location and persist that before
    // deleting the data in the old location; this ordering ensures that
    // we can recover from things like battery pulls.
    return 0;

fail:
    // Nuke everything we might have already copied
    {
        std::string to(create_data_app_package_path(to_uuid, data_app_name));
        if (delete_dir_contents(to.c_str(), 1, NULL) != 0) {
            LOG(WARNING) << "Failed to rollback " << to;
        }
    }
    for (auto user : users) {
        std::string to(create_data_user_package_path(to_uuid, user, package_name));
        if (delete_dir_contents(to.c_str(), 1, NULL) != 0) {
            LOG(WARNING) << "Failed to rollback " << to;
        }
    }
    return -1;
}

int make_user_config(userid_t userid)
{
    if (ensure_config_user_dirs(userid) == -1) {
        return -1;
    }

    return 0;
}

int delete_user(const char *uuid, userid_t userid) {
    int res = 0;

    std::string data_path(create_data_user_path(uuid, userid));
    std::string data_de_path(create_data_user_de_path(uuid, userid));
    std::string media_path(create_data_media_path(uuid, userid));
    std::string profiles_path(create_data_user_profiles_path(userid));

    res |= delete_dir_contents_and_dir(data_path);
    // TODO: include result once 25796509 is fixed
    delete_dir_contents_and_dir(data_de_path);
    res |= delete_dir_contents_and_dir(media_path);
    res |= delete_dir_contents_and_dir(profiles_path);

    // Config paths only exist on internal storage
    if (uuid == nullptr) {
        char config_path[PATH_MAX];
        if ((create_user_config_path(config_path, userid) != 0)
                || (delete_dir_contents(config_path, 1, NULL) != 0)) {
            res = -1;
        }
    }

    return res;
}

/* Try to ensure free_size bytes of storage are available.
 * Returns 0 on success.
 * This is rather simple-minded because doing a full LRU would
 * be potentially memory-intensive, and without atime it would
 * also require that apps constantly modify file metadata even
 * when just reading from the cache, which is pretty awful.
 */
int free_cache(const char *uuid, int64_t free_size)
{
    cache_t* cache;
    int64_t avail;
    DIR *d;
    struct dirent *de;
    char tmpdir[PATH_MAX];
    char *dirpos;

    std::string data_path(create_data_path(uuid));

    avail = data_disk_free(data_path);
    if (avail < 0) return -1;

    ALOGI("free_cache(%" PRId64 ") avail %" PRId64 "\n", free_size, avail);
    if (avail >= free_size) return 0;

    cache = start_cache_collection();

    // Special case for owner on internal storage
    if (uuid == nullptr) {
        std::string _tmpdir(create_data_user_path(nullptr, 0));
        add_cache_files(cache, _tmpdir.c_str(), "cache");
    }

    // Search for other users and add any cache files from them.
    std::string _tmpdir(create_data_path(uuid) + "/" + SECONDARY_USER_PREFIX);
    strcpy(tmpdir, _tmpdir.c_str());

    dirpos = tmpdir + strlen(tmpdir);
    d = opendir(tmpdir);
    if (d != NULL) {
        while ((de = readdir(d))) {
            if (de->d_type == DT_DIR) {
                const char *name = de->d_name;
                    /* always skip "." and ".." */
                if (name[0] == '.') {
                    if (name[1] == 0) continue;
                    if ((name[1] == '.') && (name[2] == 0)) continue;
                }
                if ((strlen(name)+(dirpos-tmpdir)) < (sizeof(tmpdir)-1)) {
                    strcpy(dirpos, name);
                    //ALOGI("adding cache files from %s\n", tmpdir);
                    add_cache_files(cache, tmpdir, "cache");
                } else {
                    ALOGW("Path exceeds limit: %s%s", tmpdir, name);
                }
            }
        }
        closedir(d);
    }

    // Collect cache files on external storage for all users (if it is mounted as part
    // of the internal storage).
    strcpy(tmpdir, android_media_dir.path);
    dirpos = tmpdir + strlen(tmpdir);
    d = opendir(tmpdir);
    if (d != NULL) {
        while ((de = readdir(d))) {
            if (de->d_type == DT_DIR) {
                const char *name = de->d_name;
                    /* skip any dir that doesn't start with a number, so not a user */
                if (name[0] < '0' || name[0] > '9') {
                    continue;
                }
                if ((strlen(name)+(dirpos-tmpdir)) < (sizeof(tmpdir)-1)) {
                    strcpy(dirpos, name);
                    if (lookup_media_dir(tmpdir, "Android") == 0
                            && lookup_media_dir(tmpdir, "data") == 0) {
                        //ALOGI("adding cache files from %s\n", tmpdir);
                        add_cache_files(cache, tmpdir, "cache");
                    }
                } else {
                    ALOGW("Path exceeds limit: %s%s", tmpdir, name);
                }
            }
        }
        closedir(d);
    }

    clear_cache_files(data_path, cache, free_size);
    finish_cache_collection(cache);

    return data_disk_free(data_path) >= free_size ? 0 : -1;
}

int rm_dex(const char *path, const char *instruction_set)
{
    char dex_path[PKG_PATH_MAX];

    if (validate_apk_path(path) && validate_system_app_path(path)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", path);
        return -1;
    }

    if (!create_cache_path(dex_path, path, instruction_set)) return -1;

    ALOGV("unlink %s\n", dex_path);
    if (unlink(dex_path) < 0) {
        if (errno != ENOENT) {
            ALOGE("Couldn't unlink %s: %s\n", dex_path, strerror(errno));
        }
        return -1;
    } else {
        return 0;
    }
}

int get_app_size(const char *uuid, const char *pkgname, int userid, int flags,
        const char *apkpath, const char *libdirpath, const char *fwdlock_apkpath,
        const char *asecpath, const char *instruction_set, int64_t *_codesize, int64_t *_datasize,
        int64_t *_cachesize, int64_t* _asecsize) {
    DIR *d;
    int dfd;
    struct dirent *de;
    struct stat s;
    char path[PKG_PATH_MAX];

    int64_t codesize = 0;
    int64_t datasize = 0;
    int64_t cachesize = 0;
    int64_t asecsize = 0;

    /* count the source apk as code -- but only if it's not
     * on the /system partition and its not on the sdcard. */
    if (validate_system_app_path(apkpath) &&
            strncmp(apkpath, android_asec_dir.path, android_asec_dir.len) != 0) {
        if (stat(apkpath, &s) == 0) {
            codesize += stat_size(&s);
            if (S_ISDIR(s.st_mode)) {
                d = opendir(apkpath);
                if (d != NULL) {
                    dfd = dirfd(d);
                    codesize += calculate_dir_size(dfd);
                    closedir(d);
                }
            }
        }
    }

    /* count the forward locked apk as code if it is given */
    if (fwdlock_apkpath != NULL && fwdlock_apkpath[0] != '!') {
        if (stat(fwdlock_apkpath, &s) == 0) {
            codesize += stat_size(&s);
        }
    }

    /* count the cached dexfile as code */
    if (create_cache_path(path, apkpath, instruction_set)) {
        if (stat(path, &s) == 0) {
            codesize += stat_size(&s);
        }
    }

    /* add in size of any libraries */
    if (libdirpath != NULL && libdirpath[0] != '!') {
        d = opendir(libdirpath);
        if (d != NULL) {
            dfd = dirfd(d);
            codesize += calculate_dir_size(dfd);
            closedir(d);
        }
    }

    /* compute asec size if it is given */
    if (asecpath != NULL && asecpath[0] != '!') {
        if (stat(asecpath, &s) == 0) {
            asecsize += stat_size(&s);
        }
    }

    std::vector<userid_t> users;
    if (userid == -1) {
        users = get_known_users(uuid);
    } else {
        users.push_back(userid);
    }

    for (auto user : users) {
        // TODO: handle user_de directories
        if (!(flags & FLAG_STORAGE_CE)) continue;

        std::string _pkgdir(create_data_user_package_path(uuid, user, pkgname));
        const char* pkgdir = _pkgdir.c_str();

        d = opendir(pkgdir);
        if (d == NULL) {
            PLOG(WARNING) << "Failed to open " << pkgdir;
            continue;
        }
        dfd = dirfd(d);

        /* most stuff in the pkgdir is data, except for the "cache"
         * directory and below, which is cache, and the "lib" directory
         * and below, which is code...
         */
        while ((de = readdir(d))) {
            const char *name = de->d_name;

            if (de->d_type == DT_DIR) {
                int subfd;
                int64_t statsize = 0;
                int64_t dirsize = 0;
                    /* always skip "." and ".." */
                if (name[0] == '.') {
                    if (name[1] == 0) continue;
                    if ((name[1] == '.') && (name[2] == 0)) continue;
                }
                if (fstatat(dfd, name, &s, AT_SYMLINK_NOFOLLOW) == 0) {
                    statsize = stat_size(&s);
                }
                subfd = openat(dfd, name, O_RDONLY | O_DIRECTORY);
                if (subfd >= 0) {
                    dirsize = calculate_dir_size(subfd);
                }
                if(!strcmp(name,"lib")) {
                    codesize += dirsize + statsize;
                } else if(!strcmp(name,"cache")) {
                    cachesize += dirsize + statsize;
                } else {
                    datasize += dirsize + statsize;
                }
            } else if (de->d_type == DT_LNK && !strcmp(name,"lib")) {
                // This is the symbolic link to the application's library
                // code.  We'll count this as code instead of data, since
                // it is not something that the app creates.
                if (fstatat(dfd, name, &s, AT_SYMLINK_NOFOLLOW) == 0) {
                    codesize += stat_size(&s);
                }
            } else {
                if (fstatat(dfd, name, &s, AT_SYMLINK_NOFOLLOW) == 0) {
                    datasize += stat_size(&s);
                }
            }
        }
        closedir(d);
    }
    *_codesize = codesize;
    *_datasize = datasize;
    *_cachesize = cachesize;
    *_asecsize = asecsize;
    return 0;
}

static int split_count(const char *str)
{
  char *ctx;
  int count = 0;
  char buf[kPropertyValueMax];

  strncpy(buf, str, sizeof(buf));
  char *pBuf = buf;

  while(strtok_r(pBuf, " ", &ctx) != NULL) {
    count++;
    pBuf = NULL;
  }

  return count;
}

static int split(char *buf, const char **argv)
{
  char *ctx;
  int count = 0;
  char *tok;
  char *pBuf = buf;

  while((tok = strtok_r(pBuf, " ", &ctx)) != NULL) {
    argv[count++] = tok;
    pBuf = NULL;
  }

  return count;
}

static void run_patchoat(int input_fd, int oat_fd, const char* input_file_name,
    const char* output_file_name, const char *pkgname ATTRIBUTE_UNUSED, const char *instruction_set)
{
    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig
    static const unsigned int MAX_INSTRUCTION_SET_LEN = 7;

    static const char* PATCHOAT_BIN = "/system/bin/patchoat";
    if (strlen(instruction_set) >= MAX_INSTRUCTION_SET_LEN) {
        ALOGE("Instruction set %s longer than max length of %d",
              instruction_set, MAX_INSTRUCTION_SET_LEN);
        return;
    }

    /* input_file_name/input_fd should be the .odex/.oat file that is precompiled. I think*/
    char instruction_set_arg[strlen("--instruction-set=") + MAX_INSTRUCTION_SET_LEN];
    char output_oat_fd_arg[strlen("--output-oat-fd=") + MAX_INT_LEN];
    char input_oat_fd_arg[strlen("--input-oat-fd=") + MAX_INT_LEN];
    const char* patched_image_location_arg = "--patched-image-location=/system/framework/boot.art";
    // The caller has already gotten all the locks we need.
    const char* no_lock_arg = "--no-lock-output";
    sprintf(instruction_set_arg, "--instruction-set=%s", instruction_set);
    sprintf(output_oat_fd_arg, "--output-oat-fd=%d", oat_fd);
    sprintf(input_oat_fd_arg, "--input-oat-fd=%d", input_fd);
    ALOGV("Running %s isa=%s in-fd=%d (%s) out-fd=%d (%s)\n",
          PATCHOAT_BIN, instruction_set, input_fd, input_file_name, oat_fd, output_file_name);

    /* patchoat, patched-image-location, no-lock, isa, input-fd, output-fd */
    char* argv[7];
    argv[0] = (char*) PATCHOAT_BIN;
    argv[1] = (char*) patched_image_location_arg;
    argv[2] = (char*) no_lock_arg;
    argv[3] = instruction_set_arg;
    argv[4] = output_oat_fd_arg;
    argv[5] = input_oat_fd_arg;
    argv[6] = NULL;

    execv(PATCHOAT_BIN, (char* const *)argv);
    ALOGE("execv(%s) failed: %s\n", PATCHOAT_BIN, strerror(errno));
}

static void run_dex2oat(int zip_fd, int oat_fd, int image_fd, const char* input_file_name,
        const char* output_file_name, int swap_fd, const char *instruction_set,
        const char* compiler_filter, bool vm_safe_mode, bool debuggable, bool post_bootcomplete,
        int profile_fd) {
    static const unsigned int MAX_INSTRUCTION_SET_LEN = 7;

    if (strlen(instruction_set) >= MAX_INSTRUCTION_SET_LEN) {
        ALOGE("Instruction set %s longer than max length of %d",
              instruction_set, MAX_INSTRUCTION_SET_LEN);
        return;
    }

    char dex2oat_Xms_flag[kPropertyValueMax];
    bool have_dex2oat_Xms_flag = get_property("dalvik.vm.dex2oat-Xms", dex2oat_Xms_flag, NULL) > 0;

    char dex2oat_Xmx_flag[kPropertyValueMax];
    bool have_dex2oat_Xmx_flag = get_property("dalvik.vm.dex2oat-Xmx", dex2oat_Xmx_flag, NULL) > 0;

    char dex2oat_threads_buf[kPropertyValueMax];
    bool have_dex2oat_threads_flag = get_property(post_bootcomplete
                                                      ? "dalvik.vm.dex2oat-threads"
                                                      : "dalvik.vm.boot-dex2oat-threads",
                                                  dex2oat_threads_buf,
                                                  NULL) > 0;
    char dex2oat_threads_arg[kPropertyValueMax + 2];
    if (have_dex2oat_threads_flag) {
        sprintf(dex2oat_threads_arg, "-j%s", dex2oat_threads_buf);
    }

    char dex2oat_isa_features_key[kPropertyKeyMax];
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    char dex2oat_isa_features[kPropertyValueMax];
    bool have_dex2oat_isa_features = get_property(dex2oat_isa_features_key,
                                                  dex2oat_isa_features, NULL) > 0;

    char dex2oat_isa_variant_key[kPropertyKeyMax];
    sprintf(dex2oat_isa_variant_key, "dalvik.vm.isa.%s.variant", instruction_set);
    char dex2oat_isa_variant[kPropertyValueMax];
    bool have_dex2oat_isa_variant = get_property(dex2oat_isa_variant_key,
                                                 dex2oat_isa_variant, NULL) > 0;

    const char *dex2oat_norelocation = "-Xnorelocate";
    bool have_dex2oat_relocation_skip_flag = false;

    char dex2oat_flags[kPropertyValueMax];
    int dex2oat_flags_count = get_property("dalvik.vm.dex2oat-flags",
                                 dex2oat_flags, NULL) <= 0 ? 0 : split_count(dex2oat_flags);
    ALOGV("dalvik.vm.dex2oat-flags=%s\n", dex2oat_flags);

    // If we booting without the real /data, don't spend time compiling.
    char vold_decrypt[kPropertyValueMax];
    bool have_vold_decrypt = get_property("vold.decrypt", vold_decrypt, "") > 0;
    bool skip_compilation = (have_vold_decrypt &&
                             (strcmp(vold_decrypt, "trigger_restart_min_framework") == 0 ||
                             (strcmp(vold_decrypt, "1") == 0)));

    bool generate_debug_info = property_get_bool("debug.generate-debug-info");

    char app_image_format[kPropertyValueMax];
    char image_format_arg[strlen("--image-format=") + kPropertyValueMax];
    bool have_app_image_format =
            image_fd >= 0 && get_property("dalvik.vm.appimageformat", app_image_format, NULL) > 0;
    if (have_app_image_format) {
        sprintf(image_format_arg, "--image-format=%s", app_image_format);
    }

    static const char* DEX2OAT_BIN = "/system/bin/dex2oat";

    static const char* RUNTIME_ARG = "--runtime-arg";

    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig

    char zip_fd_arg[strlen("--zip-fd=") + MAX_INT_LEN];
    char zip_location_arg[strlen("--zip-location=") + PKG_PATH_MAX];
    char oat_fd_arg[strlen("--oat-fd=") + MAX_INT_LEN];
    char oat_location_arg[strlen("--oat-location=") + PKG_PATH_MAX];
    char instruction_set_arg[strlen("--instruction-set=") + MAX_INSTRUCTION_SET_LEN];
    char instruction_set_variant_arg[strlen("--instruction-set-variant=") + kPropertyValueMax];
    char instruction_set_features_arg[strlen("--instruction-set-features=") + kPropertyValueMax];
    char dex2oat_Xms_arg[strlen("-Xms") + kPropertyValueMax];
    char dex2oat_Xmx_arg[strlen("-Xmx") + kPropertyValueMax];
    char dex2oat_compiler_filter_arg[strlen("--compiler-filter=") + kPropertyValueMax];
    bool have_dex2oat_swap_fd = false;
    char dex2oat_swap_fd[strlen("--swap-fd=") + MAX_INT_LEN];
    bool have_dex2oat_image_fd = false;
    char dex2oat_image_fd[strlen("--app-image-fd=") + MAX_INT_LEN];

    sprintf(zip_fd_arg, "--zip-fd=%d", zip_fd);
    sprintf(zip_location_arg, "--zip-location=%s", input_file_name);
    sprintf(oat_fd_arg, "--oat-fd=%d", oat_fd);
    sprintf(oat_location_arg, "--oat-location=%s", output_file_name);
    sprintf(instruction_set_arg, "--instruction-set=%s", instruction_set);
    sprintf(instruction_set_variant_arg, "--instruction-set-variant=%s", dex2oat_isa_variant);
    sprintf(instruction_set_features_arg, "--instruction-set-features=%s", dex2oat_isa_features);
    if (swap_fd >= 0) {
        have_dex2oat_swap_fd = true;
        sprintf(dex2oat_swap_fd, "--swap-fd=%d", swap_fd);
    }
    if (image_fd >= 0) {
        have_dex2oat_image_fd = true;
        sprintf(dex2oat_image_fd, "--app-image-fd=%d", image_fd);
    }

    if (have_dex2oat_Xms_flag) {
        sprintf(dex2oat_Xms_arg, "-Xms%s", dex2oat_Xms_flag);
    }
    if (have_dex2oat_Xmx_flag) {
        sprintf(dex2oat_Xmx_arg, "-Xmx%s", dex2oat_Xmx_flag);
    }

    // Compute compiler filter.

    bool have_dex2oat_compiler_filter_flag;
    if (skip_compilation) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=verify-none");
        have_dex2oat_compiler_filter_flag = true;
        have_dex2oat_relocation_skip_flag = true;
    } else if (vm_safe_mode) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=interpret-only");
        have_dex2oat_compiler_filter_flag = true;
    } else if (compiler_filter != nullptr &&
            strlen(compiler_filter) + strlen("--compiler-filter=") <
                    arraysize(dex2oat_compiler_filter_arg)) {
        sprintf(dex2oat_compiler_filter_arg, "--compiler-filter=%s", compiler_filter);
        have_dex2oat_compiler_filter_flag = true;
    } else {
        char dex2oat_compiler_filter_flag[kPropertyValueMax];
        have_dex2oat_compiler_filter_flag = get_property("dalvik.vm.dex2oat-filter",
                                                         dex2oat_compiler_filter_flag, NULL) > 0;
        if (have_dex2oat_compiler_filter_flag) {
            sprintf(dex2oat_compiler_filter_arg,
                    "--compiler-filter=%s",
                    dex2oat_compiler_filter_flag);
        }
    }

    // Check whether all apps should be compiled debuggable.
    if (!debuggable) {
        char prop_buf[kPropertyValueMax];
        debuggable =
                (get_property("dalvik.vm.always_debuggable", prop_buf, "0") > 0) &&
                (prop_buf[0] == '1');
    }
    char profile_arg[strlen("--profile-file-fd=") + MAX_INT_LEN];
    if (profile_fd != -1) {
        sprintf(profile_arg, "--profile-file-fd=%d", profile_fd);
    }


    ALOGV("Running %s in=%s out=%s\n", DEX2OAT_BIN, input_file_name, output_file_name);

    const char* argv[7  // program name, mandatory arguments and the final NULL
                     + (have_dex2oat_isa_variant ? 1 : 0)
                     + (have_dex2oat_isa_features ? 1 : 0)
                     + (have_dex2oat_Xms_flag ? 2 : 0)
                     + (have_dex2oat_Xmx_flag ? 2 : 0)
                     + (have_dex2oat_compiler_filter_flag ? 1 : 0)
                     + (have_dex2oat_threads_flag ? 1 : 0)
                     + (have_dex2oat_swap_fd ? 1 : 0)
                     + (have_dex2oat_image_fd ? 1 : 0)
                     + (have_dex2oat_relocation_skip_flag ? 2 : 0)
                     + (generate_debug_info ? 1 : 0)
                     + (debuggable ? 1 : 0)
                     + (have_app_image_format ? 1 : 0)
                     + dex2oat_flags_count
                     + (profile_fd == -1 ? 0 : 1)];
    int i = 0;
    argv[i++] = DEX2OAT_BIN;
    argv[i++] = zip_fd_arg;
    argv[i++] = zip_location_arg;
    argv[i++] = oat_fd_arg;
    argv[i++] = oat_location_arg;
    argv[i++] = instruction_set_arg;
    if (have_dex2oat_isa_variant) {
        argv[i++] = instruction_set_variant_arg;
    }
    if (have_dex2oat_isa_features) {
        argv[i++] = instruction_set_features_arg;
    }
    if (have_dex2oat_Xms_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_Xms_arg;
    }
    if (have_dex2oat_Xmx_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_Xmx_arg;
    }
    if (have_dex2oat_compiler_filter_flag) {
        argv[i++] = dex2oat_compiler_filter_arg;
    }
    if (have_dex2oat_threads_flag) {
        argv[i++] = dex2oat_threads_arg;
    }
    if (have_dex2oat_swap_fd) {
        argv[i++] = dex2oat_swap_fd;
    }
    if (have_dex2oat_image_fd) {
        argv[i++] = dex2oat_image_fd;
    }
    if (generate_debug_info) {
        argv[i++] = "--generate-debug-info";
    }
    if (debuggable) {
        argv[i++] = "--debuggable";
    }
    if (have_app_image_format) {
        argv[i++] = image_format_arg;
    }
    if (dex2oat_flags_count) {
        i += split(dex2oat_flags, argv + i);
    }
    if (have_dex2oat_relocation_skip_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_norelocation;
    }
    if (profile_fd != -1) {
        argv[i++] = profile_arg;
    }
    // Do not add after dex2oat_flags, they should override others for debugging.
    argv[i] = NULL;

    execv(DEX2OAT_BIN, (char * const *)argv);
    ALOGE("execv(%s) failed: %s\n", DEX2OAT_BIN, strerror(errno));
}

/*
 * Whether dexopt should use a swap file when compiling an APK.
 *
 * If kAlwaysProvideSwapFile, do this on all devices (dex2oat will make a more informed decision
 * itself, anyways).
 *
 * Otherwise, read "dalvik.vm.dex2oat-swap". If the property exists, return whether it is "true".
 *
 * Otherwise, return true if this is a low-mem device.
 *
 * Otherwise, return default value.
 */
static bool kAlwaysProvideSwapFile = false;
static bool kDefaultProvideSwapFile = true;

static bool ShouldUseSwapFileForDexopt() {
    if (kAlwaysProvideSwapFile) {
        return true;
    }

    // Check the "override" property. If it exists, return value == "true".
    char dex2oat_prop_buf[kPropertyValueMax];
    if (get_property("dalvik.vm.dex2oat-swap", dex2oat_prop_buf, "") > 0) {
        if (strcmp(dex2oat_prop_buf, "true") == 0) {
            return true;
        } else {
            return false;
        }
    }

    // Shortcut for default value. This is an implementation optimization for the process sketched
    // above. If the default value is true, we can avoid to check whether this is a low-mem device,
    // as low-mem is never returning false. The compiler will optimize this away if it can.
    if (kDefaultProvideSwapFile) {
        return true;
    }

    bool is_low_mem = property_get_bool("ro.config.low_ram");
    if (is_low_mem) {
        return true;
    }

    // Default value must be false here.
    return kDefaultProvideSwapFile;
}

static void SetDex2OatAndPatchOatScheduling(bool set_to_bg) {
    if (set_to_bg) {
        if (set_sched_policy(0, SP_BACKGROUND) < 0) {
            ALOGE("set_sched_policy failed: %s\n", strerror(errno));
            exit(70);
        }
        if (setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_BACKGROUND) < 0) {
            ALOGE("setpriority failed: %s\n", strerror(errno));
            exit(71);
        }
    }
}

static void close_all_fds(const std::vector<fd_t>& fds, const char* description) {
    for (size_t i = 0; i < fds.size(); i++) {
        if (close(fds[i]) != 0) {
            PLOG(WARNING) << "Failed to close fd for " << description << " at index " << i;
        }
    }
}

static fd_t open_profile_dir(const std::string& profile_dir) {
    struct stat buffer;
    if (TEMP_FAILURE_RETRY(lstat(profile_dir.c_str(), &buffer)) == -1) {
        PLOG(ERROR) << "Failed to lstat profile_dir: " << profile_dir;
        return -1;
    }

    fd_t profile_dir_fd = TEMP_FAILURE_RETRY(open(profile_dir.c_str(),
            O_PATH | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    if (profile_dir_fd < 0) {
        PLOG(ERROR) << "Failed to open profile_dir: " << profile_dir;
    }
    return profile_dir_fd;
}

static fd_t open_primary_profile_file_from_dir(const std::string& profile_dir, mode_t open_mode) {
    fd_t profile_dir_fd  = open_profile_dir(profile_dir);
    if (profile_dir_fd < 0) {
        return -1;
    }

    fd_t profile_fd = -1;
    std::string profile_file = create_primary_profile(profile_dir);

    profile_fd = TEMP_FAILURE_RETRY(open(profile_file.c_str(), open_mode | O_NOFOLLOW));
    if (profile_fd == -1) {
        // It's not an error if the profile file does not exist.
        if (errno != ENOENT) {
            PLOG(ERROR) << "Failed to lstat profile_dir: " << profile_dir;
        }
    }
    // TODO(calin): use AutoCloseFD instead of closing the fd manually.
    if (close(profile_dir_fd) != 0) {
        PLOG(WARNING) << "Could not close profile dir " << profile_dir;
    }
    return profile_fd;
}

static fd_t open_primary_profile_file(userid_t user, const char* pkgname) {
    std::string profile_dir = create_data_user_profile_package_path(user, pkgname);
    return open_primary_profile_file_from_dir(profile_dir, O_RDONLY);
}

static fd_t open_reference_profile(uid_t uid, const char* pkgname, bool read_write) {
    std::string reference_profile_dir = create_data_ref_profile_package_path(pkgname);
    int flags = read_write ? O_RDWR | O_CREAT : O_RDONLY;
    fd_t fd = open_primary_profile_file_from_dir(reference_profile_dir, flags);
    if (fd < 0) {
        return -1;
    }
    if (read_write) {
        // Fix the owner.
        if (fchown(fd, uid, uid) < 0) {
            close(fd);
            return -1;
        }
    }
    return fd;
}

static void open_profile_files(uid_t uid, const char* pkgname,
            /*out*/ std::vector<fd_t>* profiles_fd, /*out*/ fd_t* reference_profile_fd) {
    // Open the reference profile in read-write mode as profman might need to save the merge.
    *reference_profile_fd = open_reference_profile(uid, pkgname, /*read_write*/ true);
    if (*reference_profile_fd < 0) {
        // We can't access the reference profile file.
        return;
    }

    std::vector<userid_t> users = get_known_users(/*volume_uuid*/ nullptr);
    for (auto user : users) {
        fd_t profile_fd = open_primary_profile_file(user, pkgname);
        // Add to the lists only if both fds are valid.
        if (profile_fd >= 0) {
            profiles_fd->push_back(profile_fd);
        }
    }
}

static void drop_capabilities(uid_t uid) {
    if (setgid(uid) != 0) {
        ALOGE("setgid(%d) failed in installd during dexopt\n", uid);
        exit(64);
    }
    if (setuid(uid) != 0) {
        ALOGE("setuid(%d) failed in installd during dexopt\n", uid);
        exit(65);
    }
    // drop capabilities
    struct __user_cap_header_struct capheader;
    struct __user_cap_data_struct capdata[2];
    memset(&capheader, 0, sizeof(capheader));
    memset(&capdata, 0, sizeof(capdata));
    capheader.version = _LINUX_CAPABILITY_VERSION_3;
    if (capset(&capheader, &capdata[0]) < 0) {
        ALOGE("capset failed: %s\n", strerror(errno));
        exit(66);
    }
}

static constexpr int PROFMAN_BIN_RETURN_CODE_COMPILE = 0;
static constexpr int PROFMAN_BIN_RETURN_CODE_SKIP_COMPILATION = 1;
static constexpr int PROFMAN_BIN_RETURN_CODE_BAD_PROFILES = 2;
static constexpr int PROFMAN_BIN_RETURN_CODE_ERROR_IO = 3;
static constexpr int PROFMAN_BIN_RETURN_CODE_ERROR_LOCKING = 4;

static void run_profman(const std::vector<fd_t>& profiles_fd, fd_t reference_profile_fd) {
    static const size_t MAX_INT_LEN = 32;
    static const char* PROFMAN_BIN = "/system/bin/profman";

    std::vector<std::string> profile_args(profiles_fd.size());
    char profile_buf[strlen("--profile-file-fd=") + MAX_INT_LEN];
    for (size_t k = 0; k < profiles_fd.size(); k++) {
        sprintf(profile_buf, "--profile-file-fd=%d", profiles_fd[k]);
        profile_args[k].assign(profile_buf);
    }
    char reference_profile_arg[strlen("--reference-profile-file-fd=") + MAX_INT_LEN];
    sprintf(reference_profile_arg, "--reference-profile-file-fd=%d", reference_profile_fd);

    // program name, reference profile fd, the final NULL and the profile fds
    const char* argv[3 + profiles_fd.size()];
    int i = 0;
    argv[i++] = PROFMAN_BIN;
    argv[i++] = reference_profile_arg;
    for (size_t k = 0; k < profile_args.size(); k++) {
        argv[i++] = profile_args[k].c_str();
    }
    // Do not add after dex2oat_flags, they should override others for debugging.
    argv[i] = NULL;

    execv(PROFMAN_BIN, (char * const *)argv);
    ALOGE("execv(%s) failed: %s\n", PROFMAN_BIN, strerror(errno));
    exit(68);   /* only get here on exec failure */
}

// Decides if profile guided compilation is needed or not based on existing profiles.
// Returns true if there is enough information in the current profiles that worth
// a re-compilation of the package.
// If the return value is true all the current profiles would have been merged into
// the reference profiles accessible with open_reference_profile().
static bool analyse_profiles(uid_t uid, const char* pkgname) {
    std::vector<fd_t> profiles_fd;
    fd_t reference_profile_fd = -1;
    open_profile_files(uid, pkgname, &profiles_fd, &reference_profile_fd);
    if (profiles_fd.empty() || (reference_profile_fd == -1)) {
        // Skip profile guided compilation because no profiles were found.
        // Or if the reference profile info couldn't be opened.
        close_all_fds(profiles_fd, "profiles_fd");
        if ((reference_profile_fd != - 1) && (close(reference_profile_fd) != 0)) {
            PLOG(WARNING) << "Failed to close fd for reference profile";
        }
        return false;
    }

    ALOGV("PROFMAN: --- BEGIN '%s' ---\n", pkgname);

    pid_t pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
        drop_capabilities(uid);
        run_profman(profiles_fd, reference_profile_fd);
        exit(68);   /* only get here on exec failure */
    }
    /* parent */
    int return_code = wait_child(pid);
    bool need_to_compile = false;
    bool should_clear_current_profiles = false;
    bool should_clear_reference_profile = false;
    if (!WIFEXITED(return_code)) {
        LOG(WARNING) << "profman failed for package " << pkgname << ": " << return_code;
    } else {
        return_code = WEXITSTATUS(return_code);
        switch (return_code) {
            case PROFMAN_BIN_RETURN_CODE_COMPILE:
                need_to_compile = true;
                should_clear_current_profiles = true;
                should_clear_reference_profile = false;
                break;
            case PROFMAN_BIN_RETURN_CODE_SKIP_COMPILATION:
                need_to_compile = false;
                should_clear_current_profiles = false;
                should_clear_reference_profile = false;
                break;
            case PROFMAN_BIN_RETURN_CODE_BAD_PROFILES:
                LOG(WARNING) << "Bad profiles for package " << pkgname;
                need_to_compile = false;
                should_clear_current_profiles = true;
                should_clear_reference_profile = true;
                break;
            case PROFMAN_BIN_RETURN_CODE_ERROR_IO:  // fall-through
            case PROFMAN_BIN_RETURN_CODE_ERROR_LOCKING:
                // Temporary IO problem (e.g. locking). Ignore but log a warning.
                LOG(WARNING) << "IO error while reading profiles for package " << pkgname;
                need_to_compile = false;
                should_clear_current_profiles = false;
                should_clear_reference_profile = false;
                break;
           default:
                // Unknown return code or error. Unlink profiles.
                LOG(WARNING) << "Unknown error code while processing profiles for package " << pkgname
                        << ": " << return_code;
                need_to_compile = false;
                should_clear_current_profiles = true;
                should_clear_reference_profile = true;
                break;
        }
    }
    close_all_fds(profiles_fd, "profiles_fd");
    if (close(reference_profile_fd) != 0) {
        PLOG(WARNING) << "Failed to close fd for reference profile";
    }
    if (should_clear_current_profiles) {
        clear_current_profiles(pkgname);
    }
    if (should_clear_reference_profile) {
        clear_reference_profile(pkgname);
    }
    return need_to_compile;
}

static void trim_extension(char* path) {
  // Trim the extension.
  int pos = strlen(path);
  for (; pos >= 0 && path[pos] != '.'; --pos) {}
  if (pos >= 0) {
      path[pos] = '\0';  // Trim extension
  }
}

static bool add_extension_to_file_name(char* file_name, const char* extension) {
    if (strlen(file_name) + strlen(extension) + 1 > PKG_PATH_MAX) {
        return false;
    }
    strcat(file_name, extension);
    return true;
}

static int open_output_file(char* file_name, bool recreate, int permissions) {
    int flags = O_RDWR | O_CREAT;
    if (recreate) {
        if (unlink(file_name) < 0) {
            if (errno != ENOENT) {
                PLOG(ERROR) << "open_output_file: Couldn't unlink " << file_name;
            }
        }
        flags |= O_EXCL;
    }
    return open(file_name, flags, permissions);
}

static bool set_permissions_and_ownership(int fd, bool is_public, int uid, const char* path) {
    if (fchmod(fd,
               S_IRUSR|S_IWUSR|S_IRGRP |
               (is_public ? S_IROTH : 0)) < 0) {
        ALOGE("installd cannot chmod '%s' during dexopt\n", path);
        return false;
    } else if (fchown(fd, AID_SYSTEM, uid) < 0) {
        ALOGE("installd cannot chown '%s' during dexopt\n", path);
        return false;
    }
    return true;
}

static bool create_oat_out_path(const char* apk_path, const char* instruction_set,
            const char* oat_dir, /*out*/ char* out_path) {
    // Early best-effort check whether we can fit the the path into our buffers.
    // Note: the cache path will require an additional 5 bytes for ".swap", but we'll try to run
    // without a swap file, if necessary. Reference profiles file also add an extra ".prof"
    // extension to the cache path (5 bytes).
    if (strlen(apk_path) >= (PKG_PATH_MAX - 8)) {
        ALOGE("apk_path too long '%s'\n", apk_path);
        return false;
    }

    if (oat_dir != NULL && oat_dir[0] != '!') {
        if (validate_apk_path(oat_dir)) {
            ALOGE("invalid oat_dir '%s'\n", oat_dir);
            return false;
        }
        if (!calculate_oat_file_path(out_path, oat_dir, apk_path, instruction_set)) {
            return false;
        }
    } else {
        if (!create_cache_path(out_path, apk_path, instruction_set)) {
            return false;
        }
    }
    return true;
}

// TODO: Consider returning error codes.
bool merge_profiles(uid_t uid, const char *pkgname) {
    return analyse_profiles(uid, pkgname);
}

int dexopt(const char* apk_path, uid_t uid, const char* pkgname, const char* instruction_set,
           int dexopt_needed, const char* oat_dir, int dexopt_flags, const char* compiler_filter,
           const char* volume_uuid ATTRIBUTE_UNUSED)
{
    struct utimbuf ut;
    struct stat input_stat;
    char out_path[PKG_PATH_MAX];
    char swap_file_name[PKG_PATH_MAX];
    char image_path[PKG_PATH_MAX];
    const char *input_file;
    char in_odex_path[PKG_PATH_MAX];
    int res;
    fd_t input_fd=-1, out_fd=-1, image_fd=-1, swap_fd=-1;
    bool is_public = ((dexopt_flags & DEXOPT_PUBLIC) != 0);
    bool vm_safe_mode = (dexopt_flags & DEXOPT_SAFEMODE) != 0;
    bool debuggable = (dexopt_flags & DEXOPT_DEBUGGABLE) != 0;
    bool boot_complete = (dexopt_flags & DEXOPT_BOOTCOMPLETE) != 0;
    bool profile_guided = (dexopt_flags & DEXOPT_PROFILE_GUIDED) != 0;

    CHECK(pkgname != nullptr);
    CHECK(pkgname[0] != 0);

    fd_t reference_profile_fd = -1;
    // Public apps should not be compiled with profile information ever. Same goes for the special
    // package '*' used for the system server.
    if (!is_public && pkgname[0] != '*') {
        // Open reference profile in read only mode as dex2oat does not get write permissions.
        reference_profile_fd = open_reference_profile(uid, pkgname, /*read_write*/ false);
        // Note: it's OK to not find a profile here.
    }

    if ((dexopt_flags & ~DEXOPT_MASK) != 0) {
        LOG_FATAL("dexopt flags contains unknown fields\n");
    }

    if (!create_oat_out_path(apk_path, instruction_set, oat_dir, out_path)) {
        return false;
    }

    switch (dexopt_needed) {
        case DEXOPT_DEX2OAT_NEEDED:
            input_file = apk_path;
            break;

        case DEXOPT_PATCHOAT_NEEDED:
            if (!calculate_odex_file_path(in_odex_path, apk_path, instruction_set)) {
                return -1;
            }
            input_file = in_odex_path;
            break;

        case DEXOPT_SELF_PATCHOAT_NEEDED:
            input_file = out_path;
            break;

        default:
            ALOGE("Invalid dexopt needed: %d\n", dexopt_needed);
            exit(72);
    }

    memset(&input_stat, 0, sizeof(input_stat));
    stat(input_file, &input_stat);

    input_fd = open(input_file, O_RDONLY, 0);
    if (input_fd < 0) {
        ALOGE("installd cannot open '%s' for input during dexopt\n", input_file);
        return -1;
    }

    out_fd = open_output_file(out_path, /*recreate*/true, /*permissions*/0644);
    if (out_fd < 0) {
        ALOGE("installd cannot open '%s' for output during dexopt\n", out_path);
        goto fail;
    }
    if (!set_permissions_and_ownership(out_fd, is_public, uid, out_path)) {
        goto fail;
    }

    // Create a swap file if necessary.
    if (ShouldUseSwapFileForDexopt()) {
        // Make sure there really is enough space.
        strcpy(swap_file_name, out_path);
        if (add_extension_to_file_name(swap_file_name, ".swap")) {
            swap_fd = open_output_file(swap_file_name, /*recreate*/true, /*permissions*/0600);
        }
        if (swap_fd < 0) {
            // Could not create swap file. Optimistically go on and hope that we can compile
            // without it.
            ALOGE("installd could not create '%s' for swap during dexopt\n", swap_file_name);
        } else {
            // Immediately unlink. We don't really want to hit flash.
            if (unlink(swap_file_name) < 0) {
                PLOG(ERROR) << "Couldn't unlink swap file " << swap_file_name;
            }
        }
    }

    // Avoid generating an app image for extract only since it will not contain any classes.
    strcpy(image_path, out_path);
    trim_extension(image_path);
    if (add_extension_to_file_name(image_path, ".art")) {
      char app_image_format[kPropertyValueMax];
      bool have_app_image_format =
              get_property("dalvik.vm.appimageformat", app_image_format, NULL) > 0;
      // Use app images only if it is enabled (by a set image format) and we are compiling
      // profile-guided (so the app image doesn't conservatively contain all classes).
      if (profile_guided && have_app_image_format) {
          // Recreate is true since we do not want to modify a mapped image. If the app is already
          // running and we modify the image file, it can cause crashes (b/27493510).
          image_fd = open_output_file(image_path, /*recreate*/true, /*permissions*/0600);
          if (image_fd < 0) {
              // Could not create application image file. Go on since we can compile without it.
              ALOGE("installd could not create '%s' for image file during dexopt\n", image_path);
          } else if (!set_permissions_and_ownership(image_fd, is_public, uid, image_path)) {
              image_fd = -1;
          }
      }
      // If we have a valid image file path but no image fd, erase the image file.
      if (image_fd < 0) {
          if (unlink(image_path) < 0) {
              if (errno != ENOENT) {
                  PLOG(ERROR) << "Couldn't unlink image file " << image_path;
              }
          }
      }
    }

    ALOGV("DexInv: --- BEGIN '%s' ---\n", input_file);

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
        drop_capabilities(uid);

        SetDex2OatAndPatchOatScheduling(boot_complete);
        if (flock(out_fd, LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed: %s\n", out_path, strerror(errno));
            exit(67);
        }

        if (dexopt_needed == DEXOPT_PATCHOAT_NEEDED
            || dexopt_needed == DEXOPT_SELF_PATCHOAT_NEEDED) {
            run_patchoat(input_fd, out_fd, input_file, out_path, pkgname, instruction_set);
        } else if (dexopt_needed == DEXOPT_DEX2OAT_NEEDED) {
            // Pass dex2oat the relative path to the input file.
            const char *input_file_name = strrchr(input_file, '/');
            if (input_file_name == NULL) {
                input_file_name = input_file;
            } else {
                input_file_name++;
            }
            run_dex2oat(input_fd, out_fd, image_fd, input_file_name, out_path, swap_fd,
                        instruction_set, compiler_filter, vm_safe_mode, debuggable, boot_complete,
                        reference_profile_fd);
        } else {
            ALOGE("Invalid dexopt needed: %d\n", dexopt_needed);
            exit(73);
        }
        exit(68);   /* only get here on exec failure */
    } else {
        res = wait_child(pid);
        if (res == 0) {
            ALOGV("DexInv: --- END '%s' (success) ---\n", input_file);
        } else {
            ALOGE("DexInv: --- END '%s' --- status=0x%04x, process failed\n", input_file, res);
            goto fail;
        }
    }

    ut.actime = input_stat.st_atime;
    ut.modtime = input_stat.st_mtime;
    utime(out_path, &ut);

    close(out_fd);
    close(input_fd);
    if (swap_fd >= 0) {
        close(swap_fd);
    }
    if (reference_profile_fd >= 0) {
        close(reference_profile_fd);
    }
    if (image_fd >= 0) {
        close(image_fd);
    }
    return 0;

fail:
    if (out_fd >= 0) {
        close(out_fd);
        unlink(out_path);
    }
    if (input_fd >= 0) {
        close(input_fd);
    }
    if (reference_profile_fd >= 0) {
        close(reference_profile_fd);
        // We failed to compile. Unlink the reference profile. Current profiles are already unlinked
        // when profmoan advises compilation.
        clear_reference_profile(pkgname);
    }
    if (swap_fd >= 0) {
        close(swap_fd);
    }
    if (image_fd >= 0) {
        close(image_fd);
    }
    return -1;
}

int mark_boot_complete(const char* instruction_set)
{
  char boot_marker_path[PKG_PATH_MAX];
  sprintf(boot_marker_path,
          "%s/%s/%s/.booting",
          android_data_dir.path,
          DALVIK_CACHE,
          instruction_set);

  ALOGV("mark_boot_complete : %s", boot_marker_path);
  if (unlink(boot_marker_path) != 0) {
      ALOGE("Unable to unlink boot marker at %s, error=%s", boot_marker_path,
            strerror(errno));
      return -1;
  }

  return 0;
}

void mkinnerdirs(char* path, int basepos, mode_t mode, int uid, int gid,
        struct stat* statbuf)
{
    while (path[basepos] != 0) {
        if (path[basepos] == '/') {
            path[basepos] = 0;
            if (lstat(path, statbuf) < 0) {
                ALOGV("Making directory: %s\n", path);
                if (mkdir(path, mode) == 0) {
                    chown(path, uid, gid);
                } else {
                    ALOGW("Unable to make directory %s: %s\n", path, strerror(errno));
                }
            }
            path[basepos] = '/';
            basepos++;
        }
        basepos++;
    }
}

int linklib(const char* uuid, const char* pkgname, const char* asecLibDir, int userId)
{
    struct stat s, libStat;
    int rc = 0;

    std::string _pkgdir(create_data_user_package_path(uuid, userId, pkgname));
    std::string _libsymlink(_pkgdir + PKG_LIB_POSTFIX);

    const char* pkgdir = _pkgdir.c_str();
    const char* libsymlink = _libsymlink.c_str();

    if (stat(pkgdir, &s) < 0) return -1;

    if (chown(pkgdir, AID_INSTALL, AID_INSTALL) < 0) {
        ALOGE("failed to chown '%s': %s\n", pkgdir, strerror(errno));
        return -1;
    }

    if (chmod(pkgdir, 0700) < 0) {
        ALOGE("linklib() 1: failed to chmod '%s': %s\n", pkgdir, strerror(errno));
        rc = -1;
        goto out;
    }

    if (lstat(libsymlink, &libStat) < 0) {
        if (errno != ENOENT) {
            ALOGE("couldn't stat lib dir: %s\n", strerror(errno));
            rc = -1;
            goto out;
        }
    } else {
        if (S_ISDIR(libStat.st_mode)) {
            if (delete_dir_contents(libsymlink, 1, NULL) < 0) {
                rc = -1;
                goto out;
            }
        } else if (S_ISLNK(libStat.st_mode)) {
            if (unlink(libsymlink) < 0) {
                ALOGE("couldn't unlink lib dir: %s\n", strerror(errno));
                rc = -1;
                goto out;
            }
        }
    }

    if (symlink(asecLibDir, libsymlink) < 0) {
        ALOGE("couldn't symlink directory '%s' -> '%s': %s\n", libsymlink, asecLibDir,
                strerror(errno));
        rc = -errno;
        goto out;
    }

out:
    if (chmod(pkgdir, s.st_mode) < 0) {
        ALOGE("linklib() 2: failed to chmod '%s': %s\n", pkgdir, strerror(errno));
        rc = -errno;
    }

    if (chown(pkgdir, s.st_uid, s.st_gid) < 0) {
        ALOGE("failed to chown '%s' : %s\n", pkgdir, strerror(errno));
        return -errno;
    }

    return rc;
}

static void run_idmap(const char *target_apk, const char *overlay_apk, int idmap_fd)
{
    static const char *IDMAP_BIN = "/system/bin/idmap";
    static const size_t MAX_INT_LEN = 32;
    char idmap_str[MAX_INT_LEN];

    snprintf(idmap_str, sizeof(idmap_str), "%d", idmap_fd);

    execl(IDMAP_BIN, IDMAP_BIN, "--fd", target_apk, overlay_apk, idmap_str, (char*)NULL);
    ALOGE("execl(%s) failed: %s\n", IDMAP_BIN, strerror(errno));
}

// Transform string /a/b/c.apk to (prefix)/a@b@c.apk@(suffix)
// eg /a/b/c.apk to /data/resource-cache/a@b@c.apk@idmap
static int flatten_path(const char *prefix, const char *suffix,
        const char *overlay_path, char *idmap_path, size_t N)
{
    if (overlay_path == NULL || idmap_path == NULL) {
        return -1;
    }
    const size_t len_overlay_path = strlen(overlay_path);
    // will access overlay_path + 1 further below; requires absolute path
    if (len_overlay_path < 2 || *overlay_path != '/') {
        return -1;
    }
    const size_t len_idmap_root = strlen(prefix);
    const size_t len_suffix = strlen(suffix);
    if (SIZE_MAX - len_idmap_root < len_overlay_path ||
            SIZE_MAX - (len_idmap_root + len_overlay_path) < len_suffix) {
        // additions below would cause overflow
        return -1;
    }
    if (N < len_idmap_root + len_overlay_path + len_suffix) {
        return -1;
    }
    memset(idmap_path, 0, N);
    snprintf(idmap_path, N, "%s%s%s", prefix, overlay_path + 1, suffix);
    char *ch = idmap_path + len_idmap_root;
    while (*ch != '\0') {
        if (*ch == '/') {
            *ch = '@';
        }
        ++ch;
    }
    return 0;
}

int idmap(const char *target_apk, const char *overlay_apk, uid_t uid)
{
    ALOGV("idmap target_apk=%s overlay_apk=%s uid=%d\n", target_apk, overlay_apk, uid);

    int idmap_fd = -1;
    char idmap_path[PATH_MAX];

    if (flatten_path(IDMAP_PREFIX, IDMAP_SUFFIX, overlay_apk,
                idmap_path, sizeof(idmap_path)) == -1) {
        ALOGE("idmap cannot generate idmap path for overlay %s\n", overlay_apk);
        goto fail;
    }

    unlink(idmap_path);
    idmap_fd = open(idmap_path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (idmap_fd < 0) {
        ALOGE("idmap cannot open '%s' for output: %s\n", idmap_path, strerror(errno));
        goto fail;
    }
    if (fchown(idmap_fd, AID_SYSTEM, uid) < 0) {
        ALOGE("idmap cannot chown '%s'\n", idmap_path);
        goto fail;
    }
    if (fchmod(idmap_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
        ALOGE("idmap cannot chmod '%s'\n", idmap_path);
        goto fail;
    }

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
        if (setgid(uid) != 0) {
            ALOGE("setgid(%d) failed during idmap\n", uid);
            exit(1);
        }
        if (setuid(uid) != 0) {
            ALOGE("setuid(%d) failed during idmap\n", uid);
            exit(1);
        }
        if (flock(idmap_fd, LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed during idmap: %s\n", idmap_path, strerror(errno));
            exit(1);
        }

        run_idmap(target_apk, overlay_apk, idmap_fd);
        exit(1); /* only if exec call to idmap failed */
    } else {
        int status = wait_child(pid);
        if (status != 0) {
            ALOGE("idmap failed, status=0x%04x\n", status);
            goto fail;
        }
    }

    close(idmap_fd);
    return 0;
fail:
    if (idmap_fd >= 0) {
        close(idmap_fd);
        unlink(idmap_path);
    }
    return -1;
}

int restorecon_app_data(const char* uuid, const char* pkgName, userid_t userid, int flags,
        appid_t appid, const char* seinfo) {
    int res = 0;

    // SELINUX_ANDROID_RESTORECON_DATADATA flag is set by libselinux. Not needed here.
    unsigned int seflags = SELINUX_ANDROID_RESTORECON_RECURSE;

    if (!pkgName || !seinfo) {
        ALOGE("Package name or seinfo tag is null when trying to restorecon.");
        return -1;
    }

    uid_t uid = multiuser_get_uid(userid, appid);
    if (flags & FLAG_STORAGE_CE) {
        auto path = create_data_user_package_path(uuid, userid, pkgName);
        if (selinux_android_restorecon_pkgdir(path.c_str(), seinfo, uid, seflags) < 0) {
            PLOG(ERROR) << "restorecon failed for " << path;
            res = -1;
        }
    }
    if (flags & FLAG_STORAGE_DE) {
        auto path = create_data_user_de_package_path(uuid, userid, pkgName);
        if (selinux_android_restorecon_pkgdir(path.c_str(), seinfo, uid, seflags) < 0) {
            PLOG(ERROR) << "restorecon failed for " << path;
            // TODO: include result once 25796509 is fixed
        }
    }

    return res;
}

int create_oat_dir(const char* oat_dir, const char* instruction_set)
{
    char oat_instr_dir[PKG_PATH_MAX];

    if (validate_apk_path(oat_dir)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", oat_dir);
        return -1;
    }
    if (fs_prepare_dir(oat_dir, S_IRWXU | S_IRWXG | S_IXOTH, AID_SYSTEM, AID_INSTALL)) {
        return -1;
    }
    if (selinux_android_restorecon(oat_dir, 0)) {
        ALOGE("cannot restorecon dir '%s': %s\n", oat_dir, strerror(errno));
        return -1;
    }
    snprintf(oat_instr_dir, PKG_PATH_MAX, "%s/%s", oat_dir, instruction_set);
    if (fs_prepare_dir(oat_instr_dir, S_IRWXU | S_IRWXG | S_IXOTH, AID_SYSTEM, AID_INSTALL)) {
        return -1;
    }
    return 0;
}

int rm_package_dir(const char* apk_path)
{
    if (validate_apk_path(apk_path)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", apk_path);
        return -1;
    }
    return delete_dir_contents(apk_path, 1 /* also_delete_dir */ , NULL /* exclusion_predicate */);
}

int link_file(const char* relative_path, const char* from_base, const char* to_base) {
    char from_path[PKG_PATH_MAX];
    char to_path[PKG_PATH_MAX];
    snprintf(from_path, PKG_PATH_MAX, "%s/%s", from_base, relative_path);
    snprintf(to_path, PKG_PATH_MAX, "%s/%s", to_base, relative_path);

    if (validate_apk_path_subdirs(from_path)) {
        ALOGE("invalid app data sub-path '%s' (bad prefix)\n", from_path);
        return -1;
    }

    if (validate_apk_path_subdirs(to_path)) {
        ALOGE("invalid app data sub-path '%s' (bad prefix)\n", to_path);
        return -1;
    }

    const int ret = link(from_path, to_path);
    if (ret < 0) {
        ALOGE("link(%s, %s) failed : %s", from_path, to_path, strerror(errno));
        return -1;
    }

    return 0;
}

// Helper for move_ab, so that we can have common failure-case cleanup.
static bool unlink_and_rename(const char* from, const char* to) {
    // Check whether "from" exists, and if so whether it's regular. If it is, unlink. Otherwise,
    // return a failure.
    struct stat s;
    if (stat(to, &s) == 0) {
        if (!S_ISREG(s.st_mode)) {
            LOG(ERROR) << from << " is not a regular file to replace for A/B.";
            return false;
        }
        if (unlink(to) != 0) {
            LOG(ERROR) << "Could not unlink " << to << " to move A/B.";
            return false;
        }
    } else {
        // This may be a permission problem. We could investigate the error code, but we'll just
        // let the rename failure do the work for us.
    }

    // Try to rename "to" to "from."
    if (rename(from, to) != 0) {
        PLOG(ERROR) << "Could not rename " << from << " to " << to;
        return false;
    }

    return true;
}

int move_ab(const char* apk_path, const char* instruction_set, const char* oat_dir) {
    if (apk_path == nullptr || instruction_set == nullptr || oat_dir == nullptr) {
        LOG(ERROR) << "Cannot move_ab with null input";
        return -1;
    }
    if (validate_apk_path(apk_path) != 0) {
        LOG(ERROR) << "invalid apk_path " << apk_path;
        return -1;
    }
    if (validate_apk_path(oat_dir) != 0) {
        LOG(ERROR) << "invalid oat_dir " << oat_dir;
        return -1;
    }

    char a_path[PKG_PATH_MAX];
    if (!calculate_oat_file_path(a_path, oat_dir, apk_path, instruction_set)) {
        return -1;
    }

    // B path = A path + ".b"
    std::string b_path = StringPrintf("%s.b", a_path);

    // Check whether B exists.
    {
        struct stat s;
        if (stat(b_path.c_str(), &s) != 0) {
            // Silently ignore for now. The service calling this isn't smart enough to understand
            // lack of artifacts at the moment.
            return -1;
        }
        if (!S_ISREG(s.st_mode)) {
            LOG(ERROR) << "A/B artifact " << b_path << " is not a regular file.";
            // Try to unlink, but swallow errors.
            unlink(b_path.c_str());
            return -1;
        }
    }

    // Rename B to A.
    if (!unlink_and_rename(b_path.c_str(), a_path)) {
        // Delete the b_path so we don't try again (or fail earlier).
        if (unlink(b_path.c_str()) != 0) {
            PLOG(ERROR) << "Could not unlink " << b_path;
        }

        return -1;
    }

    return 0;
}

}  // namespace installd
}  // namespace android
