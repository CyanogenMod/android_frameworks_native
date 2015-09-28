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

#include "installd.h"

#include <base/stringprintf.h>
#include <base/logging.h>
#include <cutils/sched_policy.h>
#include <diskusage/dirsize.h>
#include <logwrap/logwrap.h>
#include <system/thread_defs.h>
#include <selinux/android.h>

#include <inttypes.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <unistd.h>

using android::base::StringPrintf;

/* Directory records that are used in execution of commands. */
dir_rec_t android_data_dir;
dir_rec_t android_asec_dir;
dir_rec_t android_app_dir;
dir_rec_t android_app_private_dir;
dir_rec_t android_app_lib_dir;
dir_rec_t android_media_dir;
dir_rec_t android_mnt_expand_dir;
dir_rec_array_t android_system_dirs;

static const char* kCpPath = "/system/bin/cp";

int install(const char *uuid, const char *pkgname, uid_t uid, gid_t gid, const char *seinfo)
{
    if ((uid < AID_SYSTEM) || (gid < AID_SYSTEM)) {
        ALOGE("invalid uid/gid: %d %d\n", uid, gid);
        return -1;
    }

    std::string _pkgdir(create_data_user_package_path(uuid, 0, pkgname));
    const char* pkgdir = _pkgdir.c_str();

    if (mkdir(pkgdir, 0751) < 0) {
        ALOGE("cannot create dir '%s': %s\n", pkgdir, strerror(errno));
        return -1;
    }
    if (chmod(pkgdir, 0751) < 0) {
        ALOGE("cannot chmod dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -1;
    }

    if (selinux_android_setfilecon(pkgdir, pkgname, seinfo, uid) < 0) {
        ALOGE("cannot setfilecon dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    if (chown(pkgdir, uid, gid) < 0) {
        ALOGE("cannot chown dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -1;
    }

    return 0;
}

int uninstall(const char *uuid, const char *pkgname, userid_t userid)
{
    std::string _pkgdir(create_data_user_package_path(uuid, userid, pkgname));
    const char* pkgdir = _pkgdir.c_str();

    remove_profile_file(pkgname);

    /* delete contents AND directory, no exceptions */
    return delete_dir_contents(pkgdir, 1, NULL);
}

int renamepkg(const char *oldpkgname, const char *newpkgname)
{
    char oldpkgdir[PKG_PATH_MAX];
    char newpkgdir[PKG_PATH_MAX];

    if (create_pkg_path(oldpkgdir, oldpkgname, PKG_DIR_POSTFIX, 0))
        return -1;
    if (create_pkg_path(newpkgdir, newpkgname, PKG_DIR_POSTFIX, 0))
        return -1;

    if (rename(oldpkgdir, newpkgdir) < 0) {
        ALOGE("cannot rename dir '%s' to '%s': %s\n", oldpkgdir, newpkgdir, strerror(errno));
        return -errno;
    }
    return 0;
}

int fix_uid(const char *uuid, const char *pkgname, uid_t uid, gid_t gid)
{
    struct stat s;

    if ((uid < AID_SYSTEM) || (gid < AID_SYSTEM)) {
        ALOGE("invalid uid/gid: %d %d\n", uid, gid);
        return -1;
    }

    std::string _pkgdir(create_data_user_package_path(uuid, 0, pkgname));
    const char* pkgdir = _pkgdir.c_str();

    if (stat(pkgdir, &s) < 0) return -1;

    if (s.st_uid != 0 || s.st_gid != 0) {
        ALOGE("fixing uid of non-root pkg: %s %" PRIu32 " %" PRIu32 "\n", pkgdir, s.st_uid, s.st_gid);
        return -1;
    }

    if (chmod(pkgdir, 0751) < 0) {
        ALOGE("cannot chmod dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }
    if (chown(pkgdir, uid, gid) < 0) {
        ALOGE("cannot chown dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    return 0;
}

int delete_user_data(const char *uuid, const char *pkgname, userid_t userid)
{
    std::string _pkgdir(create_data_user_package_path(uuid, userid, pkgname));
    const char* pkgdir = _pkgdir.c_str();

    return delete_dir_contents(pkgdir, 0, NULL);
}

int make_user_data(const char *uuid, const char *pkgname, uid_t uid, userid_t userid, const char* seinfo)
{
    std::string _pkgdir(create_data_user_package_path(uuid, userid, pkgname));
    const char* pkgdir = _pkgdir.c_str();

    if (mkdir(pkgdir, 0751) < 0) {
        ALOGE("cannot create dir '%s': %s\n", pkgdir, strerror(errno));
        return -errno;
    }
    if (chmod(pkgdir, 0751) < 0) {
        ALOGE("cannot chmod dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    if (selinux_android_setfilecon(pkgdir, pkgname, seinfo, uid) < 0) {
        ALOGE("cannot setfilecon dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    if (chown(pkgdir, uid, uid) < 0) {
        ALOGE("cannot chown dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    return 0;
}

int copy_complete_app(const char *from_uuid, const char *to_uuid,
        const char *package_name, const char *data_app_name, appid_t appid,
        const char* seinfo) {
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

        uid_t uid = multiuser_get_uid(user, appid);
        if (make_user_data(to_uuid, package_name, uid, user, seinfo) != 0) {
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
    }

    if (restorecon_data(to_uuid, package_name, seinfo, multiuser_get_uid(0, appid)) != 0) {
        LOG(ERROR) << "Failed to restorecon";
        goto fail;
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

int delete_user(const char *uuid, userid_t userid)
{
    int status = 0;

    std::string data_path(create_data_user_path(uuid, userid));
    if (delete_dir_contents(data_path.c_str(), 1, NULL) != 0) {
        status = -1;
    }

    std::string media_path(create_data_media_path(uuid, userid));
    if (delete_dir_contents(media_path.c_str(), 1, NULL) != 0) {
        status = -1;
    }

    // Config paths only exist on internal storage
    if (uuid == nullptr) {
        char config_path[PATH_MAX];
        if ((create_user_config_path(config_path, userid) != 0)
                || (delete_dir_contents(config_path, 1, NULL) != 0)) {
            status = -1;
        }
    }

    return status;
}

int delete_cache(const char *uuid, const char *pkgname, userid_t userid)
{
    std::string _cachedir(
            create_data_user_package_path(uuid, userid, pkgname) + CACHE_DIR_POSTFIX);
    const char* cachedir = _cachedir.c_str();

    /* delete contents, not the directory, no exceptions */
    return delete_dir_contents(cachedir, 0, NULL);
}

int delete_code_cache(const char *uuid, const char *pkgname, userid_t userid)
{
    std::string _codecachedir(
            create_data_user_package_path(uuid, userid, pkgname) + CODE_CACHE_DIR_POSTFIX);
    const char* codecachedir = _codecachedir.c_str();

    struct stat s;

    /* it's okay if code cache is missing */
    if (lstat(codecachedir, &s) == -1 && errno == ENOENT) {
        return 0;
    }

    /* delete contents, not the directory, no exceptions */
    return delete_dir_contents(codecachedir, 0, NULL);
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

int move_dex(const char *src, const char *dst, const char *instruction_set)
{
    char src_dex[PKG_PATH_MAX];
    char dst_dex[PKG_PATH_MAX];

    if (validate_apk_path(src)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", src);
        return -1;
    }
    if (validate_apk_path(dst)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", dst);
        return -1;
    }

    if (create_cache_path(src_dex, src, instruction_set)) return -1;
    if (create_cache_path(dst_dex, dst, instruction_set)) return -1;

    ALOGV("move %s -> %s\n", src_dex, dst_dex);
    if (rename(src_dex, dst_dex) < 0) {
        ALOGE("Couldn't move %s: %s\n", src_dex, strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

int rm_dex(const char *path, const char *instruction_set)
{
    char dex_path[PKG_PATH_MAX];

    if (validate_apk_path(path) && validate_system_app_path(path)) {
        ALOGE("invalid apk path '%s' (bad prefix)\n", path);
        return -1;
    }

    if (create_cache_path(dex_path, path, instruction_set)) return -1;

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

int get_size(const char *uuid, const char *pkgname, int userid, const char *apkpath,
             const char *libdirpath, const char *fwdlock_apkpath, const char *asecpath,
             const char *instruction_set, int64_t *_codesize, int64_t *_datasize,
             int64_t *_cachesize, int64_t* _asecsize)
{
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
    if (!create_cache_path(path, apkpath, instruction_set)) {
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

int create_cache_path(char path[PKG_PATH_MAX], const char *src, const char *instruction_set)
{
    char *tmp;
    int srclen;
    int dstlen;

    srclen = strlen(src);

        /* demand that we are an absolute path */
    if ((src == 0) || (src[0] != '/') || strstr(src,"..")) {
        return -1;
    }

    if (srclen > PKG_PATH_MAX) {        // XXX: PKG_NAME_MAX?
        return -1;
    }

    dstlen = srclen + strlen(DALVIK_CACHE_PREFIX) +
        strlen(instruction_set) +
        strlen(DALVIK_CACHE_POSTFIX) + 2;

    if (dstlen > PKG_PATH_MAX) {
        return -1;
    }

    sprintf(path,"%s%s/%s%s",
            DALVIK_CACHE_PREFIX,
            instruction_set,
            src + 1, /* skip the leading / */
            DALVIK_CACHE_POSTFIX);

    for(tmp = path + strlen(DALVIK_CACHE_PREFIX) + strlen(instruction_set) + 1; *tmp; tmp++) {
        if (*tmp == '/') {
            *tmp = '@';
        }
    }

    return 0;
}

static int split_count(const char *str)
{
  char *ctx;
  int count = 0;
  char buf[PROPERTY_VALUE_MAX];

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
    const char* output_file_name, const char *pkgname __unused, const char *instruction_set)
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

static bool check_boolean_property(const char* property_name, bool default_value = false) {
    char tmp_property_value[PROPERTY_VALUE_MAX];
    bool have_property = property_get(property_name, tmp_property_value, nullptr) > 0;
    if (!have_property) {
        return default_value;
    }
    return strcmp(tmp_property_value, "true") == 0;
}

static void run_dex2oat(int zip_fd, int oat_fd, const char* input_file_name,
    const char* output_file_name, int swap_fd, const char *pkgname, const char *instruction_set,
    bool vm_safe_mode, bool debuggable, bool post_bootcomplete)
{
    static const unsigned int MAX_INSTRUCTION_SET_LEN = 7;

    if (strlen(instruction_set) >= MAX_INSTRUCTION_SET_LEN) {
        ALOGE("Instruction set %s longer than max length of %d",
              instruction_set, MAX_INSTRUCTION_SET_LEN);
        return;
    }

    char prop_buf[PROPERTY_VALUE_MAX];
    bool profiler = (property_get("dalvik.vm.profiler", prop_buf, "0") > 0) && (prop_buf[0] == '1');

    char dex2oat_Xms_flag[PROPERTY_VALUE_MAX];
    bool have_dex2oat_Xms_flag = property_get("dalvik.vm.dex2oat-Xms", dex2oat_Xms_flag, NULL) > 0;

    char dex2oat_Xmx_flag[PROPERTY_VALUE_MAX];
    bool have_dex2oat_Xmx_flag = property_get("dalvik.vm.dex2oat-Xmx", dex2oat_Xmx_flag, NULL) > 0;

    char dex2oat_compiler_filter_flag[PROPERTY_VALUE_MAX];
    bool have_dex2oat_compiler_filter_flag = property_get("dalvik.vm.dex2oat-filter",
                                                          dex2oat_compiler_filter_flag, NULL) > 0;

    char dex2oat_threads_buf[PROPERTY_VALUE_MAX];
    bool have_dex2oat_threads_flag = property_get(post_bootcomplete
                                                      ? "dalvik.vm.dex2oat-threads"
                                                      : "dalvik.vm.boot-dex2oat-threads",
                                                  dex2oat_threads_buf,
                                                  NULL) > 0;
    char dex2oat_threads_arg[PROPERTY_VALUE_MAX + 2];
    if (have_dex2oat_threads_flag) {
        sprintf(dex2oat_threads_arg, "-j%s", dex2oat_threads_buf);
    }

    char dex2oat_isa_features_key[PROPERTY_KEY_MAX];
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    char dex2oat_isa_features[PROPERTY_VALUE_MAX];
    bool have_dex2oat_isa_features = property_get(dex2oat_isa_features_key,
                                                  dex2oat_isa_features, NULL) > 0;

    char dex2oat_isa_variant_key[PROPERTY_KEY_MAX];
    sprintf(dex2oat_isa_variant_key, "dalvik.vm.isa.%s.variant", instruction_set);
    char dex2oat_isa_variant[PROPERTY_VALUE_MAX];
    bool have_dex2oat_isa_variant = property_get(dex2oat_isa_variant_key,
                                                 dex2oat_isa_variant, NULL) > 0;

    const char *dex2oat_norelocation = "-Xnorelocate";
    bool have_dex2oat_relocation_skip_flag = false;

    char dex2oat_flags[PROPERTY_VALUE_MAX];
    int dex2oat_flags_count = property_get("dalvik.vm.dex2oat-flags",
                                 dex2oat_flags, NULL) <= 0 ? 0 : split_count(dex2oat_flags);
    ALOGV("dalvik.vm.dex2oat-flags=%s\n", dex2oat_flags);

    // If we booting without the real /data, don't spend time compiling.
    char vold_decrypt[PROPERTY_VALUE_MAX];
    bool have_vold_decrypt = property_get("vold.decrypt", vold_decrypt, "") > 0;
    bool skip_compilation = (have_vold_decrypt &&
                             (strcmp(vold_decrypt, "trigger_restart_min_framework") == 0 ||
                             (strcmp(vold_decrypt, "1") == 0)));

    bool use_jit = check_boolean_property("debug.usejit");
    bool generate_debug_info = check_boolean_property("debug.generate-debug-info");

    static const char* DEX2OAT_BIN = "/system/bin/dex2oat";

    static const char* RUNTIME_ARG = "--runtime-arg";

    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig

    char zip_fd_arg[strlen("--zip-fd=") + MAX_INT_LEN];
    char zip_location_arg[strlen("--zip-location=") + PKG_PATH_MAX];
    char oat_fd_arg[strlen("--oat-fd=") + MAX_INT_LEN];
    char oat_location_arg[strlen("--oat-location=") + PKG_PATH_MAX];
    char instruction_set_arg[strlen("--instruction-set=") + MAX_INSTRUCTION_SET_LEN];
    char instruction_set_variant_arg[strlen("--instruction-set-variant=") + PROPERTY_VALUE_MAX];
    char instruction_set_features_arg[strlen("--instruction-set-features=") + PROPERTY_VALUE_MAX];
    char profile_file_arg[strlen("--profile-file=") + PKG_PATH_MAX];
    char top_k_profile_threshold_arg[strlen("--top-k-profile-threshold=") + PROPERTY_VALUE_MAX];
    char dex2oat_Xms_arg[strlen("-Xms") + PROPERTY_VALUE_MAX];
    char dex2oat_Xmx_arg[strlen("-Xmx") + PROPERTY_VALUE_MAX];
    char dex2oat_compiler_filter_arg[strlen("--compiler-filter=") + PROPERTY_VALUE_MAX];
    bool have_dex2oat_swap_fd = false;
    char dex2oat_swap_fd[strlen("--swap-fd=") + MAX_INT_LEN];

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

    bool have_profile_file = false;
    bool have_top_k_profile_threshold = false;
    if (profiler && (strcmp(pkgname, "*") != 0)) {
        char profile_file[PKG_PATH_MAX];
        snprintf(profile_file, sizeof(profile_file), "%s/%s",
                 DALVIK_CACHE_PREFIX "profiles", pkgname);
        struct stat st;
        if ((stat(profile_file, &st) == 0) && (st.st_size > 0)) {
            sprintf(profile_file_arg, "--profile-file=%s", profile_file);
            have_profile_file = true;
            if (property_get("dalvik.vm.profile.top-k-thr", prop_buf, NULL) > 0) {
                snprintf(top_k_profile_threshold_arg, sizeof(top_k_profile_threshold_arg),
                         "--top-k-profile-threshold=%s", prop_buf);
                have_top_k_profile_threshold = true;
            }
        }
    }

    if (have_dex2oat_Xms_flag) {
        sprintf(dex2oat_Xms_arg, "-Xms%s", dex2oat_Xms_flag);
    }
    if (have_dex2oat_Xmx_flag) {
        sprintf(dex2oat_Xmx_arg, "-Xmx%s", dex2oat_Xmx_flag);
    }
    if (skip_compilation) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=verify-none");
        have_dex2oat_compiler_filter_flag = true;
        have_dex2oat_relocation_skip_flag = true;
    } else if (vm_safe_mode) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=interpret-only");
        have_dex2oat_compiler_filter_flag = true;
    } else if (use_jit) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=verify-at-runtime");
        have_dex2oat_compiler_filter_flag = true;
    } else if (have_dex2oat_compiler_filter_flag) {
        sprintf(dex2oat_compiler_filter_arg, "--compiler-filter=%s", dex2oat_compiler_filter_flag);
    }

    // Check whether all apps should be compiled debuggable.
    if (!debuggable) {
        debuggable =
                (property_get("dalvik.vm.always_debuggable", prop_buf, "0") > 0) &&
                (prop_buf[0] == '1');
    }

    ALOGV("Running %s in=%s out=%s\n", DEX2OAT_BIN, input_file_name, output_file_name);

    const char* argv[7  // program name, mandatory arguments and the final NULL
                     + (have_dex2oat_isa_variant ? 1 : 0)
                     + (have_dex2oat_isa_features ? 1 : 0)
                     + (have_profile_file ? 1 : 0)
                     + (have_top_k_profile_threshold ? 1 : 0)
                     + (have_dex2oat_Xms_flag ? 2 : 0)
                     + (have_dex2oat_Xmx_flag ? 2 : 0)
                     + (have_dex2oat_compiler_filter_flag ? 1 : 0)
                     + (have_dex2oat_threads_flag ? 1 : 0)
                     + (have_dex2oat_swap_fd ? 1 : 0)
                     + (have_dex2oat_relocation_skip_flag ? 2 : 0)
                     + (generate_debug_info ? 1 : 0)
                     + (debuggable ? 1 : 0)
                     + dex2oat_flags_count];
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
    if (have_profile_file) {
        argv[i++] = profile_file_arg;
    }
    if (have_top_k_profile_threshold) {
        argv[i++] = top_k_profile_threshold_arg;
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
    if (generate_debug_info) {
        argv[i++] = "--generate-debug-info";
    }
    if (debuggable) {
        argv[i++] = "--debuggable";
    }
    if (dex2oat_flags_count) {
        i += split(dex2oat_flags, argv + i);
    }
    if (have_dex2oat_relocation_skip_flag) {
        argv[i++] = RUNTIME_ARG;
        argv[i++] = dex2oat_norelocation;
    }
    // Do not add after dex2oat_flags, they should override others for debugging.
    argv[i] = NULL;

    execv(DEX2OAT_BIN, (char * const *)argv);
    ALOGE("execv(%s) failed: %s\n", DEX2OAT_BIN, strerror(errno));
}

static int wait_child(pid_t pid)
{
    int status;
    pid_t got_pid;

    while (1) {
        got_pid = waitpid(pid, &status, 0);
        if (got_pid == -1 && errno == EINTR) {
            printf("waitpid interrupted, retrying\n");
        } else {
            break;
        }
    }
    if (got_pid != pid) {
        ALOGW("waitpid failed: wanted %d, got %d: %s\n",
            (int) pid, (int) got_pid, strerror(errno));
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    } else {
        return status;      /* always nonzero */
    }
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
    char dex2oat_prop_buf[PROPERTY_VALUE_MAX];
    if (property_get("dalvik.vm.dex2oat-swap", dex2oat_prop_buf, "") > 0) {
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

    bool is_low_mem = check_boolean_property("ro.config.low_ram");
    if (is_low_mem) {
        return true;
    }

    // Default value must be false here.
    return kDefaultProvideSwapFile;
}

/*
 * Computes the odex file for the given apk_path and instruction_set.
 * /system/framework/whatever.jar -> /system/framework/oat/<isa>/whatever.odex
 *
 * Returns false if it failed to determine the odex file path.
 */
static bool calculate_odex_file_path(char path[PKG_PATH_MAX],
                                     const char *apk_path,
                                     const char *instruction_set)
{
    if (strlen(apk_path) + strlen("oat/") + strlen(instruction_set)
        + strlen("/") + strlen("odex") + 1 > PKG_PATH_MAX) {
      ALOGE("apk_path '%s' may be too long to form odex file path.\n", apk_path);
      return false;
    }

    strcpy(path, apk_path);
    char *end = strrchr(path, '/');
    if (end == NULL) {
      ALOGE("apk_path '%s' has no '/'s in it?!\n", apk_path);
      return false;
    }
    const char *apk_end = apk_path + (end - path); // strrchr(apk_path, '/');

    strcpy(end + 1, "oat/");       // path = /system/framework/oat/\0
    strcat(path, instruction_set); // path = /system/framework/oat/<isa>\0
    strcat(path, apk_end);         // path = /system/framework/oat/<isa>/whatever.jar\0
    end = strrchr(path, '.');
    if (end == NULL) {
      ALOGE("apk_path '%s' has no extension.\n", apk_path);
      return false;
    }
    strcpy(end + 1, "odex");
    return true;
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

int dexopt(const char *apk_path, uid_t uid, bool is_public,
           const char *pkgname, const char *instruction_set, int dexopt_needed,
           bool vm_safe_mode, bool debuggable, const char* oat_dir, bool boot_complete)
{
    struct utimbuf ut;
    struct stat input_stat;
    char out_path[PKG_PATH_MAX];
    char swap_file_name[PKG_PATH_MAX];
    const char *input_file;
    char in_odex_path[PKG_PATH_MAX];
    int res, input_fd=-1, out_fd=-1, swap_fd=-1;

    // Early best-effort check whether we can fit the the path into our buffers.
    // Note: the cache path will require an additional 5 bytes for ".swap", but we'll try to run
    // without a swap file, if necessary.
    if (strlen(apk_path) >= (PKG_PATH_MAX - 8)) {
        ALOGE("apk_path too long '%s'\n", apk_path);
        return -1;
    }

    if (oat_dir != NULL && oat_dir[0] != '!') {
        if (validate_apk_path(oat_dir)) {
            ALOGE("invalid oat_dir '%s'\n", oat_dir);
            return -1;
        }
        if (calculate_oat_file_path(out_path, oat_dir, apk_path, instruction_set)) {
            return -1;
        }
    } else {
        if (create_cache_path(out_path, apk_path, instruction_set)) {
            return -1;
        }
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

    unlink(out_path);
    out_fd = open(out_path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (out_fd < 0) {
        ALOGE("installd cannot open '%s' for output during dexopt\n", out_path);
        goto fail;
    }
    if (fchmod(out_fd,
               S_IRUSR|S_IWUSR|S_IRGRP |
               (is_public ? S_IROTH : 0)) < 0) {
        ALOGE("installd cannot chmod '%s' during dexopt\n", out_path);
        goto fail;
    }
    if (fchown(out_fd, AID_SYSTEM, uid) < 0) {
        ALOGE("installd cannot chown '%s' during dexopt\n", out_path);
        goto fail;
    }

    // Create profile file if there is a package name present.
    if (strcmp(pkgname, "*") != 0) {
        create_profile_file(pkgname, uid);
    }

    // Create a swap file if necessary.
    if (ShouldUseSwapFileForDexopt()) {
        // Make sure there really is enough space.
        size_t out_len = strlen(out_path);
        if (out_len + strlen(".swap") + 1 <= PKG_PATH_MAX) {
            strcpy(swap_file_name, out_path);
            strcpy(swap_file_name + strlen(out_path), ".swap");
            unlink(swap_file_name);
            swap_fd = open(swap_file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (swap_fd < 0) {
                // Could not create swap file. Optimistically go on and hope that we can compile
                // without it.
                ALOGE("installd could not create '%s' for swap during dexopt\n", swap_file_name);
            } else {
                // Immediately unlink. We don't really want to hit flash.
                unlink(swap_file_name);
            }
        } else {
            // Swap file path is too long. Try to run without.
            ALOGE("installd could not create swap file for path %s during dexopt\n", out_path);
        }
    }

    ALOGV("DexInv: --- BEGIN '%s' ---\n", input_file);

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
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
        SetDex2OatAndPatchOatScheduling(boot_complete);
        if (flock(out_fd, LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed: %s\n", out_path, strerror(errno));
            exit(67);
        }

        if (dexopt_needed == DEXOPT_PATCHOAT_NEEDED
            || dexopt_needed == DEXOPT_SELF_PATCHOAT_NEEDED) {
            run_patchoat(input_fd, out_fd, input_file, out_path, pkgname, instruction_set);
        } else if (dexopt_needed == DEXOPT_DEX2OAT_NEEDED) {
            const char *input_file_name = strrchr(input_file, '/');
            if (input_file_name == NULL) {
                input_file_name = input_file;
            } else {
                input_file_name++;
            }
            run_dex2oat(input_fd, out_fd, input_file_name, out_path, swap_fd, pkgname,
                        instruction_set, vm_safe_mode, debuggable, boot_complete);
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
    if (swap_fd != -1) {
        close(swap_fd);
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
    return -1;
}

int mark_boot_complete(const char* instruction_set)
{
  char boot_marker_path[PKG_PATH_MAX];
  sprintf(boot_marker_path,"%s%s/.booting", DALVIK_CACHE_PREFIX, instruction_set);

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

int movefileordir(char* srcpath, char* dstpath, int dstbasepos,
        int dstuid, int dstgid, struct stat* statbuf)
{
    DIR *d;
    struct dirent *de;
    int res;

    int srcend = strlen(srcpath);
    int dstend = strlen(dstpath);

    if (lstat(srcpath, statbuf) < 0) {
        ALOGW("Unable to stat %s: %s\n", srcpath, strerror(errno));
        return 1;
    }

    if ((statbuf->st_mode&S_IFDIR) == 0) {
        mkinnerdirs(dstpath, dstbasepos, S_IRWXU|S_IRWXG|S_IXOTH,
                dstuid, dstgid, statbuf);
        ALOGV("Renaming %s to %s (uid %d)\n", srcpath, dstpath, dstuid);
        if (rename(srcpath, dstpath) >= 0) {
            if (chown(dstpath, dstuid, dstgid) < 0) {
                ALOGE("cannot chown %s: %s\n", dstpath, strerror(errno));
                unlink(dstpath);
                return 1;
            }
        } else {
            ALOGW("Unable to rename %s to %s: %s\n",
                srcpath, dstpath, strerror(errno));
            return 1;
        }
        return 0;
    }

    d = opendir(srcpath);
    if (d == NULL) {
        ALOGW("Unable to opendir %s: %s\n", srcpath, strerror(errno));
        return 1;
    }

    res = 0;

    while ((de = readdir(d))) {
        const char *name = de->d_name;
            /* always skip "." and ".." */
        if (name[0] == '.') {
            if (name[1] == 0) continue;
            if ((name[1] == '.') && (name[2] == 0)) continue;
        }

        if ((srcend+strlen(name)) >= (PKG_PATH_MAX-2)) {
            ALOGW("Source path too long; skipping: %s/%s\n", srcpath, name);
            continue;
        }

        if ((dstend+strlen(name)) >= (PKG_PATH_MAX-2)) {
            ALOGW("Destination path too long; skipping: %s/%s\n", dstpath, name);
            continue;
        }

        srcpath[srcend] = dstpath[dstend] = '/';
        strcpy(srcpath+srcend+1, name);
        strcpy(dstpath+dstend+1, name);

        if (movefileordir(srcpath, dstpath, dstbasepos, dstuid, dstgid, statbuf) != 0) {
            res = 1;
        }

        // Note: we will be leaving empty directories behind in srcpath,
        // but that is okay, the package manager will be erasing all of the
        // data associated with .apks that disappear.

        srcpath[srcend] = dstpath[dstend] = 0;
    }

    closedir(d);
    return res;
}

int movefiles()
{
    DIR *d;
    int dfd, subfd;
    struct dirent *de;
    struct stat s;
    char buf[PKG_PATH_MAX+1];
    int bufp, bufe, bufi, readlen;

    char srcpkg[PKG_NAME_MAX];
    char dstpkg[PKG_NAME_MAX];
    char srcpath[PKG_PATH_MAX];
    char dstpath[PKG_PATH_MAX];
    int dstuid=-1, dstgid=-1;
    int hasspace;

    d = opendir(UPDATE_COMMANDS_DIR_PREFIX);
    if (d == NULL) {
        goto done;
    }
    dfd = dirfd(d);

        /* Iterate through all files in the directory, executing the
         * file movements requested there-in.
         */
    while ((de = readdir(d))) {
        const char *name = de->d_name;

        if (de->d_type == DT_DIR) {
            continue;
        } else {
            subfd = openat(dfd, name, O_RDONLY);
            if (subfd < 0) {
                ALOGW("Unable to open update commands at %s%s\n",
                        UPDATE_COMMANDS_DIR_PREFIX, name);
                continue;
            }

            bufp = 0;
            bufe = 0;
            buf[PKG_PATH_MAX] = 0;
            srcpkg[0] = dstpkg[0] = 0;
            while (1) {
                bufi = bufp;
                while (bufi < bufe && buf[bufi] != '\n') {
                    bufi++;
                }
                if (bufi < bufe) {
                    buf[bufi] = 0;
                    ALOGV("Processing line: %s\n", buf+bufp);
                    hasspace = 0;
                    while (bufp < bufi && isspace(buf[bufp])) {
                        hasspace = 1;
                        bufp++;
                    }
                    if (buf[bufp] == '#' || bufp == bufi) {
                        // skip comments and empty lines.
                    } else if (hasspace) {
                        if (dstpkg[0] == 0) {
                            ALOGW("Path before package line in %s%s: %s\n",
                                    UPDATE_COMMANDS_DIR_PREFIX, name, buf+bufp);
                        } else if (srcpkg[0] == 0) {
                            // Skip -- source package no longer exists.
                        } else {
                            ALOGV("Move file: %s (from %s to %s)\n", buf+bufp, srcpkg, dstpkg);
                            if (!create_move_path(srcpath, srcpkg, buf+bufp, 0) &&
                                    !create_move_path(dstpath, dstpkg, buf+bufp, 0)) {
                                movefileordir(srcpath, dstpath,
                                        strlen(dstpath)-strlen(buf+bufp),
                                        dstuid, dstgid, &s);
                            }
                        }
                    } else {
                        char* div = strchr(buf+bufp, ':');
                        if (div == NULL) {
                            ALOGW("Bad package spec in %s%s; no ':' sep: %s\n",
                                    UPDATE_COMMANDS_DIR_PREFIX, name, buf+bufp);
                        } else {
                            *div = 0;
                            div++;
                            if (strlen(buf+bufp) < PKG_NAME_MAX) {
                                strcpy(dstpkg, buf+bufp);
                            } else {
                                srcpkg[0] = dstpkg[0] = 0;
                                ALOGW("Package name too long in %s%s: %s\n",
                                        UPDATE_COMMANDS_DIR_PREFIX, name, buf+bufp);
                            }
                            if (strlen(div) < PKG_NAME_MAX) {
                                strcpy(srcpkg, div);
                            } else {
                                srcpkg[0] = dstpkg[0] = 0;
                                ALOGW("Package name too long in %s%s: %s\n",
                                        UPDATE_COMMANDS_DIR_PREFIX, name, div);
                            }
                            if (srcpkg[0] != 0) {
                                if (!create_pkg_path(srcpath, srcpkg, PKG_DIR_POSTFIX, 0)) {
                                    if (lstat(srcpath, &s) < 0) {
                                        // Package no longer exists -- skip.
                                        srcpkg[0] = 0;
                                    }
                                } else {
                                    srcpkg[0] = 0;
                                    ALOGW("Can't create path %s in %s%s\n",
                                            div, UPDATE_COMMANDS_DIR_PREFIX, name);
                                }
                                if (srcpkg[0] != 0) {
                                    if (!create_pkg_path(dstpath, dstpkg, PKG_DIR_POSTFIX, 0)) {
                                        if (lstat(dstpath, &s) == 0) {
                                            dstuid = s.st_uid;
                                            dstgid = s.st_gid;
                                        } else {
                                            // Destination package doesn't
                                            // exist...  due to original-package,
                                            // this is normal, so don't be
                                            // noisy about it.
                                            srcpkg[0] = 0;
                                        }
                                    } else {
                                        srcpkg[0] = 0;
                                        ALOGW("Can't create path %s in %s%s\n",
                                                div, UPDATE_COMMANDS_DIR_PREFIX, name);
                                    }
                                }
                                ALOGV("Transfering from %s to %s: uid=%d\n",
                                    srcpkg, dstpkg, dstuid);
                            }
                        }
                    }
                    bufp = bufi+1;
                } else {
                    if (bufp == 0) {
                        if (bufp < bufe) {
                            ALOGW("Line too long in %s%s, skipping: %s\n",
                                    UPDATE_COMMANDS_DIR_PREFIX, name, buf);
                        }
                    } else if (bufp < bufe) {
                        memcpy(buf, buf+bufp, bufe-bufp);
                        bufe -= bufp;
                        bufp = 0;
                    }
                    readlen = read(subfd, buf+bufe, PKG_PATH_MAX-bufe);
                    if (readlen < 0) {
                        ALOGW("Failure reading update commands in %s%s: %s\n",
                                UPDATE_COMMANDS_DIR_PREFIX, name, strerror(errno));
                        break;
                    } else if (readlen == 0) {
                        break;
                    }
                    bufe += readlen;
                    buf[bufe] = 0;
                    ALOGV("Read buf: %s\n", buf);
                }
            }
            close(subfd);
        }
    }
    closedir(d);
done:
    return 0;
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

int restorecon_data(const char* uuid, const char* pkgName,
                    const char* seinfo, uid_t uid)
{
    struct dirent *entry;
    DIR *d;
    struct stat s;
    int ret = 0;

    // SELINUX_ANDROID_RESTORECON_DATADATA flag is set by libselinux. Not needed here.
    unsigned int flags = SELINUX_ANDROID_RESTORECON_RECURSE;

    if (!pkgName || !seinfo) {
        ALOGE("Package name or seinfo tag is null when trying to restorecon.");
        return -1;
    }

    // Special case for owner on internal storage
    if (uuid == nullptr) {
        std::string path(create_data_user_package_path(nullptr, 0, pkgName));

        if (selinux_android_restorecon_pkgdir(path.c_str(), seinfo, uid, flags) < 0) {
            PLOG(ERROR) << "restorecon failed for " << path;
            ret |= -1;
        }
    }

    // Relabel package directory for all secondary users.
    std::string userdir(create_data_path(uuid) + "/" + SECONDARY_USER_PREFIX);
    d = opendir(userdir.c_str());
    if (d == NULL) {
        return -1;
    }

    while ((entry = readdir(d))) {
        if (entry->d_type != DT_DIR) {
            continue;
        }

        const char *user = entry->d_name;
        // Ignore "." and ".."
        if (!strcmp(user, ".") || !strcmp(user, "..")) {
            continue;
        }

        // user directories start with a number
        if (user[0] < '0' || user[0] > '9') {
            ALOGE("Expecting numbered directory during restorecon. Instead got '%s'.", user);
            continue;
        }

        std::string pkgdir(StringPrintf("%s%s/%s", userdir.c_str(), user, pkgName));
        if (stat(pkgdir.c_str(), &s) < 0) {
            continue;
        }

        if (selinux_android_restorecon_pkgdir(pkgdir.c_str(), seinfo, s.st_uid, flags) < 0) {
            PLOG(ERROR) << "restorecon failed for " << pkgdir;
            ret |= -1;
        }
    }

    closedir(d);
    return ret;
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

int calculate_oat_file_path(char path[PKG_PATH_MAX], const char *oat_dir, const char *apk_path,
        const char *instruction_set) {
    char *file_name_start;
    char *file_name_end;

    file_name_start = strrchr(apk_path, '/');
    if (file_name_start == NULL) {
         ALOGE("apk_path '%s' has no '/'s in it\n", apk_path);
        return -1;
    }
    file_name_end = strrchr(apk_path, '.');
    if (file_name_end < file_name_start) {
        ALOGE("apk_path '%s' has no extension\n", apk_path);
        return -1;
    }

    // Calculate file_name
    int file_name_len = file_name_end - file_name_start - 1;
    char file_name[file_name_len + 1];
    memcpy(file_name, file_name_start + 1, file_name_len);
    file_name[file_name_len] = '\0';

    // <apk_parent_dir>/oat/<isa>/<file_name>.odex
    snprintf(path, PKG_PATH_MAX, "%s/%s/%s.odex", oat_dir, instruction_set, file_name);
    return 0;
}
