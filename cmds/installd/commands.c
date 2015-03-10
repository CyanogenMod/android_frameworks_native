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

#include <inttypes.h>
#include <sys/capability.h>
#include "installd.h"
#include <cutils/sched_policy.h>
#include <diskusage/dirsize.h>
#include <selinux/android.h>
#include <system/thread_defs.h>

/* Directory records that are used in execution of commands. */
dir_rec_t android_data_dir;
dir_rec_t android_asec_dir;
dir_rec_t android_app_dir;
dir_rec_t android_app_private_dir;
dir_rec_t android_app_lib_dir;
dir_rec_t android_media_dir;
dir_rec_array_t android_system_dirs;

int install(const char *pkgname, uid_t uid, gid_t gid, const char *seinfo)
{
    char pkgdir[PKG_PATH_MAX];
    char libsymlink[PKG_PATH_MAX];
    char applibdir[PKG_PATH_MAX];
    struct stat libStat;

    if ((uid < AID_SYSTEM) || (gid < AID_SYSTEM)) {
        ALOGE("invalid uid/gid: %d %d\n", uid, gid);
        return -1;
    }

    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, 0)) {
        ALOGE("cannot create package path\n");
        return -1;
    }

    if (create_pkg_path(libsymlink, pkgname, PKG_LIB_POSTFIX, 0)) {
        ALOGE("cannot create package lib symlink origin path\n");
        return -1;
    }

    if (create_pkg_path_in_dir(applibdir, &android_app_lib_dir, pkgname, PKG_DIR_POSTFIX)) {
        ALOGE("cannot create package lib symlink dest path\n");
        return -1;
    }

    if (mkdir(pkgdir, 0751) < 0) {
        ALOGE("cannot create dir '%s': %s\n", pkgdir, strerror(errno));
        return -1;
    }
    if (chmod(pkgdir, 0751) < 0) {
        ALOGE("cannot chmod dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -1;
    }

    if (lstat(libsymlink, &libStat) < 0) {
        if (errno != ENOENT) {
            ALOGE("couldn't stat lib dir: %s\n", strerror(errno));
            return -1;
        }
    } else {
        if (S_ISDIR(libStat.st_mode)) {
            if (delete_dir_contents(libsymlink, 1, NULL) < 0) {
                ALOGE("couldn't delete lib directory during install for: %s", libsymlink);
                return -1;
            }
        } else if (S_ISLNK(libStat.st_mode)) {
            if (unlink(libsymlink) < 0) {
                ALOGE("couldn't unlink lib directory during install for: %s", libsymlink);
                return -1;
            }
        }
    }

    if (selinux_android_setfilecon(pkgdir, pkgname, seinfo, uid) < 0) {
        ALOGE("cannot setfilecon dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(libsymlink);
        unlink(pkgdir);
        return -errno;
    }

    if (symlink(applibdir, libsymlink) < 0) {
        ALOGE("couldn't symlink directory '%s' -> '%s': %s\n", libsymlink, applibdir,
                strerror(errno));
        unlink(pkgdir);
        return -1;
    }

    if (chown(pkgdir, uid, gid) < 0) {
        ALOGE("cannot chown dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(libsymlink);
        unlink(pkgdir);
        return -1;
    }

    return 0;
}

int uninstall(const char *pkgname, userid_t userid)
{
    char pkgdir[PKG_PATH_MAX];

    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, userid))
        return -1;

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

int fix_uid(const char *pkgname, uid_t uid, gid_t gid)
{
    char pkgdir[PKG_PATH_MAX];
    struct stat s;
    int rc = 0;

    if ((uid < AID_SYSTEM) || (gid < AID_SYSTEM)) {
        ALOGE("invalid uid/gid: %d %d\n", uid, gid);
        return -1;
    }

    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, 0)) {
        ALOGE("cannot create package path\n");
        return -1;
    }

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

int delete_user_data(const char *pkgname, userid_t userid)
{
    char pkgdir[PKG_PATH_MAX];

    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, userid))
        return -1;

    return delete_dir_contents(pkgdir, 0, NULL);
}

int make_user_data(const char *pkgname, uid_t uid, userid_t userid, const char* seinfo)
{
    char pkgdir[PKG_PATH_MAX];
    char applibdir[PKG_PATH_MAX];
    char libsymlink[PKG_PATH_MAX];
    struct stat libStat;

    // Create the data dir for the package
    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, userid)) {
        return -1;
    }
    if (create_pkg_path(libsymlink, pkgname, PKG_LIB_POSTFIX, userid)) {
        ALOGE("cannot create package lib symlink origin path\n");
        return -1;
    }
    if (create_pkg_path_in_dir(applibdir, &android_app_lib_dir, pkgname, PKG_DIR_POSTFIX)) {
        ALOGE("cannot create package lib symlink dest path\n");
        return -1;
    }

    if (mkdir(pkgdir, 0751) < 0) {
        ALOGE("cannot create dir '%s': %s\n", pkgdir, strerror(errno));
        return -errno;
    }
    if (chmod(pkgdir, 0751) < 0) {
        ALOGE("cannot chmod dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(pkgdir);
        return -errno;
    }

    if (lstat(libsymlink, &libStat) < 0) {
        if (errno != ENOENT) {
            ALOGE("couldn't stat lib dir for non-primary: %s\n", strerror(errno));
            unlink(pkgdir);
            return -1;
        }
    } else {
        if (S_ISDIR(libStat.st_mode)) {
            if (delete_dir_contents(libsymlink, 1, NULL) < 0) {
                ALOGE("couldn't delete lib directory during install for non-primary: %s",
                        libsymlink);
                unlink(pkgdir);
                return -1;
            }
        } else if (S_ISLNK(libStat.st_mode)) {
            if (unlink(libsymlink) < 0) {
                ALOGE("couldn't unlink lib directory during install for non-primary: %s",
                        libsymlink);
                unlink(pkgdir);
                return -1;
            }
        }
    }

    if (selinux_android_setfilecon(pkgdir, pkgname, seinfo, uid) < 0) {
        ALOGE("cannot setfilecon dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(libsymlink);
        unlink(pkgdir);
        return -errno;
    }

    if (symlink(applibdir, libsymlink) < 0) {
        ALOGE("couldn't symlink directory for non-primary '%s' -> '%s': %s\n", libsymlink,
                applibdir, strerror(errno));
        unlink(pkgdir);
        return -1;
    }

    if (chown(pkgdir, uid, uid) < 0) {
        ALOGE("cannot chown dir '%s': %s\n", pkgdir, strerror(errno));
        unlink(libsymlink);
        unlink(pkgdir);
        return -errno;
    }

    return 0;
}

int make_user_config(userid_t userid)
{
    if (ensure_config_user_dirs(userid) == -1) {
        return -1;
    }

    return 0;
}

int delete_user(userid_t userid)
{
    int status = 0;

    char data_path[PKG_PATH_MAX];
    if ((create_user_path(data_path, userid) != 0)
            || (delete_dir_contents(data_path, 1, NULL) != 0)) {
        status = -1;
    }

    char media_path[PATH_MAX];
    if ((create_user_media_path(media_path, userid) != 0)
            || (delete_dir_contents(media_path, 1, NULL) != 0)) {
        status = -1;
    }

    char config_path[PATH_MAX];
    if ((create_user_config_path(config_path, userid) != 0)
            || (delete_dir_contents(config_path, 1, NULL) != 0)) {
        status = -1;
    }

    return status;
}

int delete_cache(const char *pkgname, userid_t userid)
{
    char cachedir[PKG_PATH_MAX];

    if (create_pkg_path(cachedir, pkgname, CACHE_DIR_POSTFIX, userid))
        return -1;

    /* delete contents, not the directory, no exceptions */
    return delete_dir_contents(cachedir, 0, NULL);
}

int delete_code_cache(const char *pkgname, userid_t userid)
{
    char codecachedir[PKG_PATH_MAX];
    struct stat s;

    if (create_pkg_path(codecachedir, pkgname, CODE_CACHE_DIR_POSTFIX, userid))
        return -1;

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
int free_cache(int64_t free_size)
{
    cache_t* cache;
    int64_t avail;
    DIR *d;
    struct dirent *de;
    char tmpdir[PATH_MAX];
    char *dirpos;

    avail = data_disk_free();
    if (avail < 0) return -1;

    ALOGI("free_cache(%" PRId64 ") avail %" PRId64 "\n", free_size, avail);
    if (avail >= free_size) return 0;

    cache = start_cache_collection();

    // Collect cache files for primary user.
    if (create_user_path(tmpdir, 0) == 0) {
        //ALOGI("adding cache files from %s\n", tmpdir);
        add_cache_files(cache, tmpdir, "cache");
    }

    // Search for other users and add any cache files from them.
    snprintf(tmpdir, sizeof(tmpdir), "%s%s", android_data_dir.path,
            SECONDARY_USER_PREFIX);
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

    clear_cache_files(cache, free_size);
    finish_cache_collection(cache);

    return data_disk_free() >= free_size ? 0 : -1;
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

int get_size(const char *pkgname, userid_t userid, const char *apkpath,
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
         * on the /system partition and its not on the sdcard.
         */
    if (validate_system_app_path(apkpath) &&
            strncmp(apkpath, android_asec_dir.path, android_asec_dir.len) != 0) {
        if (stat(apkpath, &s) == 0) {
            codesize += stat_size(&s);
        }
    }
        /* count the forward locked apk as code if it is given
         */
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

        /* compute asec size if it is given
         */
    if (asecpath != NULL && asecpath[0] != '!') {
        if (stat(asecpath, &s) == 0) {
            asecsize += stat_size(&s);
        }
    }

    if (create_pkg_path(path, pkgname, PKG_DIR_POSTFIX, userid)) {
        goto done;
    }

    d = opendir(path);
    if (d == NULL) {
        goto done;
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
done:
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

static void run_dexopt(int zip_fd, int odex_fd, const char* input_file_name,
    const char* output_file_name)
{
    /* platform-specific flags affecting optimization and verification */
    char dexopt_flags[PROPERTY_VALUE_MAX];
    property_get("dalvik.vm.dexopt-flags", dexopt_flags, "");
    ALOGV("dalvik.vm.dexopt-flags=%s\n", dexopt_flags);

    static const char* DEX_OPT_BIN = "/system/bin/dexopt";
    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig
    char zip_num[MAX_INT_LEN];
    char odex_num[MAX_INT_LEN];

    sprintf(zip_num, "%d", zip_fd);
    sprintf(odex_num, "%d", odex_fd);

    ALOGV("Running %s in=%s out=%s\n", DEX_OPT_BIN, input_file_name, output_file_name);
    execl(DEX_OPT_BIN, DEX_OPT_BIN, "--zip", zip_num, odex_num, input_file_name,
        dexopt_flags, (char*) NULL);
    ALOGE("execl(%s) failed: %s\n", DEX_OPT_BIN, strerror(errno));
}

static void run_patchoat(int input_fd, int oat_fd, const char* input_file_name,
    const char* output_file_name, const char *pkgname, const char *instruction_set)
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
    ALOGE("Running %s isa=%s in-fd=%d (%s) out-fd=%d (%s)\n",
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

static void run_dex2oat(int zip_fd, int oat_fd, const char* input_file_name,
    const char* output_file_name, int swap_fd, const char *pkgname, const char *instruction_set,
    bool vm_safe_mode)
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

    char dex2oat_isa_features_key[PROPERTY_KEY_MAX];
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    char dex2oat_isa_features[PROPERTY_VALUE_MAX];
    bool have_dex2oat_isa_features = property_get(dex2oat_isa_features_key,
                                                  dex2oat_isa_features, NULL) > 0;

    char dex2oat_flags[PROPERTY_VALUE_MAX];
    bool have_dex2oat_flags = property_get("dalvik.vm.dex2oat-flags", dex2oat_flags, NULL) > 0;
    ALOGV("dalvik.vm.dex2oat-flags=%s\n", dex2oat_flags);

    // If we booting without the real /data, don't spend time compiling.
    char vold_decrypt[PROPERTY_VALUE_MAX];
    bool have_vold_decrypt = property_get("vold.decrypt", vold_decrypt, "") > 0;
    bool skip_compilation = (have_vold_decrypt &&
                             (strcmp(vold_decrypt, "trigger_restart_min_framework") == 0 ||
                             (strcmp(vold_decrypt, "1") == 0)));

    static const char* DEX2OAT_BIN = "/system/bin/dex2oat";

    static const char* RUNTIME_ARG = "--runtime-arg";

    static const int MAX_INT_LEN = 12;      // '-'+10dig+'\0' -OR- 0x+8dig

    char zip_fd_arg[strlen("--zip-fd=") + MAX_INT_LEN];
    char zip_location_arg[strlen("--zip-location=") + PKG_PATH_MAX];
    char oat_fd_arg[strlen("--oat-fd=") + MAX_INT_LEN];
    char oat_location_arg[strlen("--oat-location=") + PKG_PATH_MAX];
    char instruction_set_arg[strlen("--instruction-set=") + MAX_INSTRUCTION_SET_LEN];
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
    } else if (vm_safe_mode) {
        strcpy(dex2oat_compiler_filter_arg, "--compiler-filter=interpret-only");
        have_dex2oat_compiler_filter_flag = true;
    } else if (have_dex2oat_compiler_filter_flag) {
        sprintf(dex2oat_compiler_filter_arg, "--compiler-filter=%s", dex2oat_compiler_filter_flag);
    }

    ALOGV("Running %s in=%s out=%s\n", DEX2OAT_BIN, input_file_name, output_file_name);

    char* argv[7  // program name, mandatory arguments and the final NULL
               + (have_dex2oat_isa_features ? 1 : 0)
               + (have_profile_file ? 1 : 0)
               + (have_top_k_profile_threshold ? 1 : 0)
               + (have_dex2oat_Xms_flag ? 2 : 0)
               + (have_dex2oat_Xmx_flag ? 2 : 0)
               + (have_dex2oat_compiler_filter_flag ? 1 : 0)
               + (have_dex2oat_flags ? 1 : 0)
               + (have_dex2oat_swap_fd ? 1 : 0)];
    int i = 0;
    argv[i++] = (char*)DEX2OAT_BIN;
    argv[i++] = zip_fd_arg;
    argv[i++] = zip_location_arg;
    argv[i++] = oat_fd_arg;
    argv[i++] = oat_location_arg;
    argv[i++] = instruction_set_arg;
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
        argv[i++] = (char*)RUNTIME_ARG;
        argv[i++] = dex2oat_Xms_arg;
    }
    if (have_dex2oat_Xmx_flag) {
        argv[i++] = (char*)RUNTIME_ARG;
        argv[i++] = dex2oat_Xmx_arg;
    }
    if (have_dex2oat_compiler_filter_flag) {
        argv[i++] = dex2oat_compiler_filter_arg;
    }
    if (have_dex2oat_flags) {
        argv[i++] = dex2oat_flags;
    }
    if (have_dex2oat_swap_fd) {
        argv[i++] = dex2oat_swap_fd;
    }
    // Do not add after dex2oat_flags, they should override others for debugging.
    argv[i] = NULL;

    execv(DEX2OAT_BIN, (char* const *)argv);
    ALOGE("execl(%s) failed: %s\n", DEX2OAT_BIN, strerror(errno));
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
 * Whether dexopt should use a swap file when compiling an APK. If kAlwaysProvideSwapFile, do this
 * on all devices (dex2oat will make a more informed decision itself, anyways). Otherwise, only do
 * this on a low-mem device.
 */
static bool kAlwaysProvideSwapFile = true;

static bool ShouldUseSwapFileForDexopt() {
    if (kAlwaysProvideSwapFile) {
        return true;
    }

    char low_mem_buf[PROPERTY_VALUE_MAX];
    property_get("ro.config.low_ram", low_mem_buf, "");
    return (strcmp(low_mem_buf, "true") == 0);
}

int dexopt(const char *apk_path, uid_t uid, bool is_public,
           const char *pkgname, const char *instruction_set,
           bool vm_safe_mode, bool is_patchoat)
{
    struct utimbuf ut;
    struct stat input_stat, dex_stat;
    char out_path[PKG_PATH_MAX];
    char persist_sys_dalvik_vm_lib[PROPERTY_VALUE_MAX];
    char swap_file_name[PKG_PATH_MAX];
    char *end;
    const char *input_file;
    char in_odex_path[PKG_PATH_MAX];
    int res, input_fd=-1, out_fd=-1, swap_fd=-1;

    // Early best-effort check whether we can fit the the path into our buffers.
    // Note: the cache path will require an additional 5 bytes for ".swap", but we'll try to run
    // without a swap file, if necessary.
    if (strlen(apk_path) >= (PKG_PATH_MAX - 8)) {
        return -1;
    }

    /* The command to run depend on the value of persist.sys.dalvik.vm.lib */
    property_get("persist.sys.dalvik.vm.lib.2", persist_sys_dalvik_vm_lib, "libart.so");

    if (is_patchoat && strncmp(persist_sys_dalvik_vm_lib, "libart", 6) != 0) {
        /* We may only patch if we are libart */
        ALOGE("Patching is only supported in libart\n");
        return -1;
    }

    /* Before anything else: is there a .odex file?  If so, we have
     * precompiled the apk and there is nothing to do here.
     *
     * We skip this if we are doing a patchoat.
     */
    strcpy(out_path, apk_path);
    end = strrchr(out_path, '.');
    if (end != NULL && !is_patchoat) {
        strcpy(end, ".odex");
        if (stat(out_path, &dex_stat) == 0) {
            return 0;
        }
    }

    if (create_cache_path(out_path, apk_path, instruction_set)) {
        return -1;
    }

    if (is_patchoat) {
        /* /system/framework/whatever.jar -> /system/framework/<isa>/whatever.odex */
        strcpy(in_odex_path, apk_path);
        end = strrchr(in_odex_path, '/');
        if (end == NULL) {
            ALOGE("apk_path '%s' has no '/'s in it?!\n", apk_path);
            return -1;
        }
        const char *apk_end = apk_path + (end - in_odex_path); // strrchr(apk_path, '/');
        strcpy(end + 1, instruction_set); // in_odex_path now is /system/framework/<isa>\0
        strcat(in_odex_path, apk_end);
        end = strrchr(in_odex_path, '.');
        if (end == NULL) {
            return -1;
        }
        strcpy(end + 1, "odex");
        input_file = in_odex_path;
    } else {
        input_file = apk_path;
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
    if (!is_patchoat && ShouldUseSwapFileForDexopt()) {
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
        if (set_sched_policy(0, SP_BACKGROUND) < 0) {
            ALOGE("set_sched_policy failed: %s\n", strerror(errno));
            exit(70);
        }
        if (setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_BACKGROUND) < 0) {
            ALOGE("setpriority failed: %s\n", strerror(errno));
            exit(71);
        }
        if (flock(out_fd, LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed: %s\n", out_path, strerror(errno));
            exit(67);
        }

        if (strncmp(persist_sys_dalvik_vm_lib, "libdvm", 6) == 0) {
            run_dexopt(input_fd, out_fd, input_file, out_path);
        } else if (strncmp(persist_sys_dalvik_vm_lib, "libart", 6) == 0) {
            if (is_patchoat) {
                run_patchoat(input_fd, out_fd, input_file, out_path, pkgname, instruction_set);
            } else {
                run_dex2oat(input_fd, out_fd, input_file, out_path, swap_fd, pkgname,
                            instruction_set, vm_safe_mode);
            }
        } else {
            exit(69);   /* Unexpected persist.sys.dalvik.vm.lib value */
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

int linklib(const char* pkgname, const char* asecLibDir, int userId)
{
    char pkgdir[PKG_PATH_MAX];
    char libsymlink[PKG_PATH_MAX];
    struct stat s, libStat;
    int rc = 0;

    if (create_pkg_path(pkgdir, pkgname, PKG_DIR_POSTFIX, userId)) {
        ALOGE("cannot create package path\n");
        return -1;
    }
    if (create_pkg_path(libsymlink, pkgname, PKG_LIB_POSTFIX, userId)) {
        ALOGE("cannot create package lib symlink origin path\n");
        return -1;
    }

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

int restorecon_data(const char* pkgName, const char* seinfo, uid_t uid)
{
    struct dirent *entry;
    DIR *d;
    struct stat s;
    char *userdir;
    char *primarydir;
    char *pkgdir;
    int ret = 0;

    // SELINUX_ANDROID_RESTORECON_DATADATA flag is set by libselinux. Not needed here.
    unsigned int flags = SELINUX_ANDROID_RESTORECON_RECURSE;

    if (!pkgName || !seinfo) {
        ALOGE("Package name or seinfo tag is null when trying to restorecon.");
        return -1;
    }

    if (asprintf(&primarydir, "%s%s%s", android_data_dir.path, PRIMARY_USER_PREFIX, pkgName) < 0) {
        return -1;
    }

    // Relabel for primary user.
    if (selinux_android_restorecon_pkgdir(primarydir, seinfo, uid, flags) < 0) {
        ALOGE("restorecon failed for %s: %s\n", primarydir, strerror(errno));
        ret |= -1;
    }

    if (asprintf(&userdir, "%s%s", android_data_dir.path, SECONDARY_USER_PREFIX) < 0) {
        free(primarydir);
        return -1;
    }

    // Relabel package directory for all secondary users.
    d = opendir(userdir);
    if (d == NULL) {
        free(primarydir);
        free(userdir);
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

        if (asprintf(&pkgdir, "%s%s/%s", userdir, user, pkgName) < 0) {
            continue;
        }

        if (stat(pkgdir, &s) < 0) {
            free(pkgdir);
            continue;
        }

        if (selinux_android_restorecon_pkgdir(pkgdir, seinfo, uid, flags) < 0) {
            ALOGE("restorecon failed for %s: %s\n", pkgdir, strerror(errno));
            ret |= -1;
        }
        free(pkgdir);
    }

    closedir(d);
    free(primarydir);
    free(userdir);
    return ret;
}

