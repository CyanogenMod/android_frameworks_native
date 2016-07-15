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

#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>

#if defined(__APPLE__)
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/fs.h>
#include <cutils/log.h>
#include <private/android_filesystem_config.h>

#include "globals.h"  // extern variables.

#ifndef LOG_TAG
#define LOG_TAG "installd"
#endif

#define CACHE_NOISY(x) //x
#define DEBUG_XATTRS 0

using android::base::StringPrintf;

namespace android {
namespace installd {

/**
 * Check that given string is valid filename, and that it attempts no
 * parent or child directory traversal.
 */
static bool is_valid_filename(const std::string& name) {
    if (name.empty() || (name == ".") || (name == "..")
            || (name.find('/') != std::string::npos)) {
        return false;
    } else {
        return true;
    }
}

static void check_package_name(const char* package_name) {
    CHECK(is_valid_filename(package_name));
    CHECK(is_valid_package_name(package_name) == 0);
}

/**
 * Create the path name where package app contents should be stored for
 * the given volume UUID and package name.  An empty UUID is assumed to
 * be internal storage.
 */
std::string create_data_app_package_path(const char* volume_uuid,
        const char* package_name) {
    check_package_name(package_name);
    return StringPrintf("%s/%s",
            create_data_app_path(volume_uuid).c_str(), package_name);
}

/**
 * Create the path name where package data should be stored for the given
 * volume UUID, package name, and user ID. An empty UUID is assumed to be
 * internal storage.
 */
std::string create_data_user_ce_package_path(const char* volume_uuid,
        userid_t user, const char* package_name) {
    check_package_name(package_name);
    return StringPrintf("%s/%s",
            create_data_user_ce_path(volume_uuid, user).c_str(), package_name);
}

std::string create_data_user_ce_package_path(const char* volume_uuid, userid_t user,
        const char* package_name, ino_t ce_data_inode) {
    // For testing purposes, rely on the inode when defined; this could be
    // optimized to use access() in the future.
    auto fallback = create_data_user_ce_package_path(volume_uuid, user, package_name);
    if (ce_data_inode != 0) {
        auto user_path = create_data_user_ce_path(volume_uuid, user);
        DIR* dir = opendir(user_path.c_str());
        if (dir == nullptr) {
            PLOG(ERROR) << "Failed to opendir " << user_path;
            return fallback;
        }

        struct dirent* ent;
        while ((ent = readdir(dir))) {
            if (ent->d_ino == ce_data_inode) {
                auto resolved = StringPrintf("%s/%s", user_path.c_str(), ent->d_name);
#if DEBUG_XATTRS
                if (resolved != fallback) {
                    LOG(DEBUG) << "Resolved path " << resolved << " for inode " << ce_data_inode
                            << " instead of " << fallback;
                }
#endif
                closedir(dir);
                return resolved;
            }
        }
        LOG(WARNING) << "Failed to resolve inode " << ce_data_inode << "; using " << fallback;
        closedir(dir);
        return fallback;
    } else {
        return fallback;
    }
}

std::string create_data_user_de_package_path(const char* volume_uuid,
        userid_t user, const char* package_name) {
    check_package_name(package_name);
    return StringPrintf("%s/%s",
            create_data_user_de_path(volume_uuid, user).c_str(), package_name);
}

int create_pkg_path(char path[PKG_PATH_MAX], const char *pkgname,
        const char *postfix, userid_t userid) {
    if (is_valid_package_name(pkgname) != 0) {
        path[0] = '\0';
        return -1;
    }

    std::string _tmp(create_data_user_ce_package_path(nullptr, userid, pkgname) + postfix);
    const char* tmp = _tmp.c_str();
    if (strlen(tmp) >= PKG_PATH_MAX) {
        path[0] = '\0';
        return -1;
    } else {
        strcpy(path, tmp);
        return 0;
    }
}

std::string create_data_path(const char* volume_uuid) {
    if (volume_uuid == nullptr) {
        return "/data";
    } else {
        CHECK(is_valid_filename(volume_uuid));
        return StringPrintf("/mnt/expand/%s", volume_uuid);
    }
}

/**
 * Create the path name for app data.
 */
std::string create_data_app_path(const char* volume_uuid) {
    return StringPrintf("%s/app", create_data_path(volume_uuid).c_str());
}

/**
 * Create the path name for user data for a certain userid.
 */
std::string create_data_user_ce_path(const char* volume_uuid, userid_t userid) {
    std::string data(create_data_path(volume_uuid));
    if (volume_uuid == nullptr) {
        if (userid == 0) {
            return StringPrintf("%s/data", data.c_str());
        } else {
            return StringPrintf("%s/user/%u", data.c_str(), userid);
        }
    } else {
        return StringPrintf("%s/user/%u", data.c_str(), userid);
    }
}

/**
 * Create the path name for device encrypted user data for a certain userid.
 */
std::string create_data_user_de_path(const char* volume_uuid, userid_t userid) {
    std::string data(create_data_path(volume_uuid));
    return StringPrintf("%s/user_de/%u", data.c_str(), userid);
}

/**
 * Create the path name for media for a certain userid.
 */
std::string create_data_media_path(const char* volume_uuid, userid_t userid) {
    return StringPrintf("%s/media/%u", create_data_path(volume_uuid).c_str(), userid);
}

std::string create_data_misc_legacy_path(userid_t userid) {
    return StringPrintf("%s/misc/user/%u", create_data_path(nullptr).c_str(), userid);
}

std::string create_data_user_profiles_path(userid_t userid) {
    return StringPrintf("%s/cur/%u", android_profiles_dir.path, userid);
}

std::string create_data_user_profile_package_path(userid_t user, const char* package_name) {
    check_package_name(package_name);
    return StringPrintf("%s/%s",create_data_user_profiles_path(user).c_str(), package_name);
}

std::string create_data_ref_profile_package_path(const char* package_name) {
    check_package_name(package_name);
    return StringPrintf("%s/ref/%s", android_profiles_dir.path, package_name);
}

std::vector<userid_t> get_known_users(const char* volume_uuid) {
    std::vector<userid_t> users;

    // We always have an owner
    users.push_back(0);

    std::string path(create_data_path(volume_uuid) + "/" + SECONDARY_USER_PREFIX);
    DIR* dir = opendir(path.c_str());
    if (dir == NULL) {
        // Unable to discover other users, but at least return owner
        PLOG(ERROR) << "Failed to opendir " << path;
        return users;
    }

    struct dirent* ent;
    while ((ent = readdir(dir))) {
        if (ent->d_type != DT_DIR) {
            continue;
        }

        char* end;
        userid_t user = strtol(ent->d_name, &end, 10);
        if (*end == '\0' && user != 0) {
            LOG(DEBUG) << "Found valid user " << user;
            users.push_back(user);
        }
    }
    closedir(dir);

    return users;
}

int create_move_path(char path[PKG_PATH_MAX],
    const char* pkgname,
    const char* leaf,
    userid_t userid ATTRIBUTE_UNUSED)
{
    if ((android_data_dir.len + strlen(PRIMARY_USER_PREFIX) + strlen(pkgname) + strlen(leaf) + 1)
            >= PKG_PATH_MAX) {
        return -1;
    }

    sprintf(path, "%s%s%s/%s", android_data_dir.path, PRIMARY_USER_PREFIX, pkgname, leaf);
    return 0;
}

/**
 * Checks whether the package name is valid. Returns -1 on error and
 * 0 on success.
 */
int is_valid_package_name(const char* pkgname) {
    const char *x = pkgname;
    int alpha = -1;

    if (strlen(pkgname) > PKG_NAME_MAX) {
        return -1;
    }

    while (*x) {
        if (isalnum(*x) || (*x == '_')) {
                /* alphanumeric or underscore are fine */
        } else if (*x == '.') {
            if ((x == pkgname) || (x[1] == '.') || (x[1] == 0)) {
                    /* periods must not be first, last, or doubled */
                ALOGE("invalid package name '%s'\n", pkgname);
                return -1;
            }
        } else if (*x == '-') {
            /* Suffix -X is fine to let versioning of packages.
               But whatever follows should be alphanumeric.*/
            alpha = 1;
        } else {
                /* anything not A-Z, a-z, 0-9, _, or . is invalid */
            ALOGE("invalid package name '%s'\n", pkgname);
            return -1;
        }

        x++;
    }

    if (alpha == 1) {
        // Skip current character
        x++;
        while (*x) {
            if (!isalnum(*x)) {
                ALOGE("invalid package name '%s' should include only numbers after -\n", pkgname);
                return -1;
            }
            x++;
        }
    }

    return 0;
}

static int _delete_dir_contents(DIR *d,
                                int (*exclusion_predicate)(const char *name, const int is_dir))
{
    int result = 0;
    struct dirent *de;
    int dfd;

    dfd = dirfd(d);

    if (dfd < 0) return -1;

    while ((de = readdir(d))) {
        const char *name = de->d_name;

            /* check using the exclusion predicate, if provided */
        if (exclusion_predicate && exclusion_predicate(name, (de->d_type == DT_DIR))) {
            continue;
        }

        if (de->d_type == DT_DIR) {
            int subfd;
            DIR *subdir;

                /* always skip "." and ".." */
            if (name[0] == '.') {
                if (name[1] == 0) continue;
                if ((name[1] == '.') && (name[2] == 0)) continue;
            }

            subfd = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (subfd < 0) {
                ALOGE("Couldn't openat %s: %s\n", name, strerror(errno));
                result = -1;
                continue;
            }
            subdir = fdopendir(subfd);
            if (subdir == NULL) {
                ALOGE("Couldn't fdopendir %s: %s\n", name, strerror(errno));
                close(subfd);
                result = -1;
                continue;
            }
            if (_delete_dir_contents(subdir, exclusion_predicate)) {
                result = -1;
            }
            closedir(subdir);
            if (unlinkat(dfd, name, AT_REMOVEDIR) < 0) {
                ALOGE("Couldn't unlinkat %s: %s\n", name, strerror(errno));
                result = -1;
            }
        } else {
            if (unlinkat(dfd, name, 0) < 0) {
                ALOGE("Couldn't unlinkat %s: %s\n", name, strerror(errno));
                result = -1;
            }
        }
    }

    return result;
}

int delete_dir_contents(const std::string& pathname, bool ignore_if_missing) {
    return delete_dir_contents(pathname.c_str(), 0, NULL, ignore_if_missing);
}

int delete_dir_contents_and_dir(const std::string& pathname, bool ignore_if_missing) {
    return delete_dir_contents(pathname.c_str(), 1, NULL, ignore_if_missing);
}

int delete_dir_contents(const char *pathname,
                        int also_delete_dir,
                        int (*exclusion_predicate)(const char*, const int),
                        bool ignore_if_missing)
{
    int res = 0;
    DIR *d;

    d = opendir(pathname);
    if (d == NULL) {
        if (ignore_if_missing && (errno == ENOENT)) {
            return 0;
        }
        ALOGE("Couldn't opendir %s: %s\n", pathname, strerror(errno));
        return -errno;
    }
    res = _delete_dir_contents(d, exclusion_predicate);
    closedir(d);
    if (also_delete_dir) {
        if (rmdir(pathname)) {
            ALOGE("Couldn't rmdir %s: %s\n", pathname, strerror(errno));
            res = -1;
        }
    }
    return res;
}

int delete_dir_contents_fd(int dfd, const char *name)
{
    int fd, res;
    DIR *d;

    fd = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("Couldn't openat %s: %s\n", name, strerror(errno));
        return -1;
    }
    d = fdopendir(fd);
    if (d == NULL) {
        ALOGE("Couldn't fdopendir %s: %s\n", name, strerror(errno));
        close(fd);
        return -1;
    }
    res = _delete_dir_contents(d, 0);
    closedir(d);
    return res;
}

static int _copy_owner_permissions(int srcfd, int dstfd)
{
    struct stat st;
    if (fstat(srcfd, &st) != 0) {
        return -1;
    }
    if (fchmod(dstfd, st.st_mode) != 0) {
        return -1;
    }
    return 0;
}

static int _copy_dir_files(int sdfd, int ddfd, uid_t owner, gid_t group)
{
    int result = 0;
    if (_copy_owner_permissions(sdfd, ddfd) != 0) {
        ALOGE("_copy_dir_files failed to copy dir permissions\n");
    }
    if (fchown(ddfd, owner, group) != 0) {
        ALOGE("_copy_dir_files failed to change dir owner\n");
    }

    DIR *ds = fdopendir(sdfd);
    if (ds == NULL) {
        ALOGE("Couldn't fdopendir: %s\n", strerror(errno));
        return -1;
    }
    struct dirent *de;
    while ((de = readdir(ds))) {
        if (de->d_type != DT_REG) {
            continue;
        }

        const char *name = de->d_name;
        int fsfd = openat(sdfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        int fdfd = openat(ddfd, name, O_WRONLY | O_NOFOLLOW | O_CLOEXEC | O_CREAT, 0600);
        if (fsfd == -1 || fdfd == -1) {
            ALOGW("Couldn't copy %s: %s\n", name, strerror(errno));
        } else {
            if (_copy_owner_permissions(fsfd, fdfd) != 0) {
                ALOGE("Failed to change file permissions\n");
            }
            if (fchown(fdfd, owner, group) != 0) {
                ALOGE("Failed to change file owner\n");
            }

            char buf[8192];
            ssize_t size;
            while ((size = read(fsfd, buf, sizeof(buf))) > 0) {
                write(fdfd, buf, size);
            }
            if (size < 0) {
                ALOGW("Couldn't copy %s: %s\n", name, strerror(errno));
                result = -1;
            }
        }
        close(fdfd);
        close(fsfd);
    }

    return result;
}

int copy_dir_files(const char *srcname,
                   const char *dstname,
                   uid_t owner,
                   uid_t group)
{
    int res = 0;
    DIR *ds = NULL;
    DIR *dd = NULL;

    ds = opendir(srcname);
    if (ds == NULL) {
        ALOGE("Couldn't opendir %s: %s\n", srcname, strerror(errno));
        return -errno;
    }

    mkdir(dstname, 0600);
    dd = opendir(dstname);
    if (dd == NULL) {
        ALOGE("Couldn't opendir %s: %s\n", dstname, strerror(errno));
        closedir(ds);
        return -errno;
    }

    int sdfd = dirfd(ds);
    int ddfd = dirfd(dd);
    if (sdfd != -1 && ddfd != -1) {
        res = _copy_dir_files(sdfd, ddfd, owner, group);
    } else {
        res = -errno;
    }
    closedir(dd);
    closedir(ds);
    return res;
}

int64_t data_disk_free(const std::string& data_path)
{
    struct statfs sfs;
    if (statfs(data_path.c_str(), &sfs) == 0) {
        return sfs.f_bavail * sfs.f_bsize;
    } else {
        PLOG(ERROR) << "Couldn't statfs " << data_path;
        return -1;
    }
}

cache_t* start_cache_collection()
{
    cache_t* cache = (cache_t*)calloc(1, sizeof(cache_t));
    return cache;
}

#define CACHE_BLOCK_SIZE (512*1024)

static void* _cache_malloc(cache_t* cache, size_t len)
{
    len = (len+3)&~3;
    if (len > (CACHE_BLOCK_SIZE/2)) {
        // It doesn't make sense to try to put this allocation into one
        // of our blocks, because it is so big.  Instead, make a new dedicated
        // block for it.
        int8_t* res = (int8_t*)malloc(len+sizeof(void*));
        if (res == NULL) {
            return NULL;
        }
        CACHE_NOISY(ALOGI("Allocated large cache mem block: %p size %zu", res, len));
        // Link it into our list of blocks, not disrupting the current one.
        if (cache->memBlocks == NULL) {
            *(void**)res = NULL;
            cache->memBlocks = res;
        } else {
            *(void**)res = *(void**)cache->memBlocks;
            *(void**)cache->memBlocks = res;
        }
        return res + sizeof(void*);
    }
    int8_t* res = cache->curMemBlockAvail;
    int8_t* nextPos = res + len;
    if (cache->memBlocks == NULL || nextPos > cache->curMemBlockEnd) {
        int8_t* newBlock = (int8_t*) malloc(CACHE_BLOCK_SIZE);
        if (newBlock == NULL) {
            return NULL;
        }
        CACHE_NOISY(ALOGI("Allocated new cache mem block: %p", newBlock));
        *(void**)newBlock = cache->memBlocks;
        cache->memBlocks = newBlock;
        res = cache->curMemBlockAvail = newBlock + sizeof(void*);
        cache->curMemBlockEnd = newBlock + CACHE_BLOCK_SIZE;
        nextPos = res + len;
    }
    CACHE_NOISY(ALOGI("cache_malloc: ret %p size %zu, block=%p, nextPos=%p",
            res, len, cache->memBlocks, nextPos));
    cache->curMemBlockAvail = nextPos;
    return res;
}

static void* _cache_realloc(cache_t* cache, void* cur, size_t origLen, size_t len)
{
    // This isn't really a realloc, but it is good enough for our purposes here.
    void* alloc = _cache_malloc(cache, len);
    if (alloc != NULL && cur != NULL) {
        memcpy(alloc, cur, origLen < len ? origLen : len);
    }
    return alloc;
}

static void _inc_num_cache_collected(cache_t* cache)
{
    cache->numCollected++;
    if ((cache->numCollected%20000) == 0) {
        ALOGI("Collected cache so far: %zd directories, %zd files",
            cache->numDirs, cache->numFiles);
    }
}

static cache_dir_t* _add_cache_dir_t(cache_t* cache, cache_dir_t* parent, const char *name)
{
    size_t nameLen = strlen(name);
    cache_dir_t* dir = (cache_dir_t*)_cache_malloc(cache, sizeof(cache_dir_t)+nameLen+1);
    if (dir != NULL) {
        dir->parent = parent;
        dir->childCount = 0;
        dir->hiddenCount = 0;
        dir->deleted = 0;
        strcpy(dir->name, name);
        if (cache->numDirs >= cache->availDirs) {
            size_t newAvail = cache->availDirs < 1000 ? 1000 : cache->availDirs*2;
            cache_dir_t** newDirs = (cache_dir_t**)_cache_realloc(cache, cache->dirs,
                    cache->availDirs*sizeof(cache_dir_t*), newAvail*sizeof(cache_dir_t*));
            if (newDirs == NULL) {
                ALOGE("Failure growing cache dirs array for %s\n", name);
                return NULL;
            }
            cache->availDirs = newAvail;
            cache->dirs = newDirs;
        }
        cache->dirs[cache->numDirs] = dir;
        cache->numDirs++;
        if (parent != NULL) {
            parent->childCount++;
        }
        _inc_num_cache_collected(cache);
    } else {
        ALOGE("Failure allocating cache_dir_t for %s\n", name);
    }
    return dir;
}

static cache_file_t* _add_cache_file_t(cache_t* cache, cache_dir_t* dir, time_t modTime,
        const char *name)
{
    size_t nameLen = strlen(name);
    cache_file_t* file = (cache_file_t*)_cache_malloc(cache, sizeof(cache_file_t)+nameLen+1);
    if (file != NULL) {
        file->dir = dir;
        file->modTime = modTime;
        strcpy(file->name, name);
        if (cache->numFiles >= cache->availFiles) {
            size_t newAvail = cache->availFiles < 1000 ? 1000 : cache->availFiles*2;
            cache_file_t** newFiles = (cache_file_t**)_cache_realloc(cache, cache->files,
                    cache->availFiles*sizeof(cache_file_t*), newAvail*sizeof(cache_file_t*));
            if (newFiles == NULL) {
                ALOGE("Failure growing cache file array for %s\n", name);
                return NULL;
            }
            cache->availFiles = newAvail;
            cache->files = newFiles;
        }
        CACHE_NOISY(ALOGI("Setting file %p at position %zd in array %p", file,
                cache->numFiles, cache->files));
        cache->files[cache->numFiles] = file;
        cache->numFiles++;
        dir->childCount++;
        _inc_num_cache_collected(cache);
    } else {
        ALOGE("Failure allocating cache_file_t for %s\n", name);
    }
    return file;
}

static int _add_cache_files(cache_t *cache, cache_dir_t *parentDir, const char *dirName,
        DIR* dir, char *pathBase, char *pathPos, size_t pathAvailLen)
{
    struct dirent *de;
    cache_dir_t* cacheDir = NULL;
    int dfd;

    CACHE_NOISY(ALOGI("_add_cache_files: parent=%p dirName=%s dir=%p pathBase=%s",
            parentDir, dirName, dir, pathBase));

    dfd = dirfd(dir);

    if (dfd < 0) return 0;

    // Sub-directories always get added to the data structure, so if they
    // are empty we will know about them to delete them later.
    cacheDir = _add_cache_dir_t(cache, parentDir, dirName);

    while ((de = readdir(dir))) {
        const char *name = de->d_name;

        if (de->d_type == DT_DIR) {
            int subfd;
            DIR *subdir;

                /* always skip "." and ".." */
            if (name[0] == '.') {
                if (name[1] == 0) continue;
                if ((name[1] == '.') && (name[2] == 0)) continue;
            }

            subfd = openat(dfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (subfd < 0) {
                ALOGE("Couldn't openat %s: %s\n", name, strerror(errno));
                continue;
            }
            subdir = fdopendir(subfd);
            if (subdir == NULL) {
                ALOGE("Couldn't fdopendir %s: %s\n", name, strerror(errno));
                close(subfd);
                continue;
            }
            if (cacheDir == NULL) {
                cacheDir = _add_cache_dir_t(cache, parentDir, dirName);
            }
            if (cacheDir != NULL) {
                // Update pathBase for the new path...  this may change dirName
                // if that is also pointing to the path, but we are done with it
                // now.
                size_t finallen = snprintf(pathPos, pathAvailLen, "/%s", name);
                CACHE_NOISY(ALOGI("Collecting dir %s\n", pathBase));
                if (finallen < pathAvailLen) {
                    _add_cache_files(cache, cacheDir, name, subdir, pathBase,
                            pathPos+finallen, pathAvailLen-finallen);
                } else {
                    // Whoops, the final path is too long!  We'll just delete
                    // this directory.
                    ALOGW("Cache dir %s truncated in path %s; deleting dir\n",
                            name, pathBase);
                    _delete_dir_contents(subdir, NULL);
                    if (unlinkat(dfd, name, AT_REMOVEDIR) < 0) {
                        ALOGE("Couldn't unlinkat %s: %s\n", name, strerror(errno));
                    }
                }
            }
            closedir(subdir);
        } else if (de->d_type == DT_REG) {
            // Skip files that start with '.'; they will be deleted if
            // their entire directory is deleted.  This allows for metadata
            // like ".nomedia" to remain in the directory until the entire
            // directory is deleted.
            if (cacheDir == NULL) {
                cacheDir = _add_cache_dir_t(cache, parentDir, dirName);
            }
            if (name[0] == '.') {
                cacheDir->hiddenCount++;
                continue;
            }
            if (cacheDir != NULL) {
                // Build final full path for file...  this may change dirName
                // if that is also pointing to the path, but we are done with it
                // now.
                size_t finallen = snprintf(pathPos, pathAvailLen, "/%s", name);
                CACHE_NOISY(ALOGI("Collecting file %s\n", pathBase));
                if (finallen < pathAvailLen) {
                    struct stat s;
                    if (stat(pathBase, &s) >= 0) {
                        _add_cache_file_t(cache, cacheDir, s.st_mtime, name);
                    } else {
                        ALOGW("Unable to stat cache file %s; deleting\n", pathBase);
                        if (unlink(pathBase) < 0) {
                            ALOGE("Couldn't unlink %s: %s\n", pathBase, strerror(errno));
                        }
                    }
                } else {
                    // Whoops, the final path is too long!  We'll just delete
                    // this file.
                    ALOGW("Cache file %s truncated in path %s; deleting\n",
                            name, pathBase);
                    if (unlinkat(dfd, name, 0) < 0) {
                        *pathPos = 0;
                        ALOGE("Couldn't unlinkat %s in %s: %s\n", name, pathBase,
                                strerror(errno));
                    }
                }
            }
        } else {
            cacheDir->hiddenCount++;
        }
    }
    return 0;
}

int get_path_inode(const std::string& path, ino_t *inode) {
    struct stat buf;
    memset(&buf, 0, sizeof(buf));
    if (stat(path.c_str(), &buf) != 0) {
        PLOG(WARNING) << "Failed to stat " << path;
        return -1;
    } else {
        *inode = buf.st_ino;
        return 0;
    }
}

/**
 * Write the inode of a specific child file into the given xattr on the
 * parent directory. This allows you to find the child later, even if its
 * name is encrypted.
 */
int write_path_inode(const std::string& parent, const char* name, const char* inode_xattr) {
    ino_t inode = 0;
    uint64_t inode_raw = 0;
    auto path = StringPrintf("%s/%s", parent.c_str(), name);

    if (get_path_inode(path, &inode) != 0) {
        // Path probably doesn't exist yet; ignore
        return 0;
    }

    // Check to see if already set correctly
    if (getxattr(parent.c_str(), inode_xattr, &inode_raw, sizeof(inode_raw)) == sizeof(inode_raw)) {
        if (inode_raw == inode) {
            // Already set correctly; skip writing
            return 0;
        } else {
            PLOG(WARNING) << "Mismatched inode value; found " << inode
                    << " on disk but marked value was " << inode_raw << "; overwriting";
        }
    }

    inode_raw = inode;
    if (setxattr(parent.c_str(), inode_xattr, &inode_raw, sizeof(inode_raw), 0) != 0) {
        PLOG(ERROR) << "Failed to write xattr " << inode_xattr << " at " << parent;
        return -1;
    } else {
        return 0;
    }
}

/**
 * Read the inode of a specific child file from the given xattr on the
 * parent directory. Returns a currently valid path for that child, which
 * might have an encrypted name.
 */
std::string read_path_inode(const std::string& parent, const char* name, const char* inode_xattr) {
    ino_t inode = 0;
    uint64_t inode_raw = 0;
    auto fallback = StringPrintf("%s/%s", parent.c_str(), name);

    // Lookup the inode value written earlier
    if (getxattr(parent.c_str(), inode_xattr, &inode_raw, sizeof(inode_raw)) == sizeof(inode_raw)) {
        inode = inode_raw;
    }

    // For testing purposes, rely on the inode when defined; this could be
    // optimized to use access() in the future.
    if (inode != 0) {
        DIR* dir = opendir(parent.c_str());
        if (dir == nullptr) {
            PLOG(ERROR) << "Failed to opendir " << parent;
            return fallback;
        }

        struct dirent* ent;
        while ((ent = readdir(dir))) {
            if (ent->d_ino == inode) {
                auto resolved = StringPrintf("%s/%s", parent.c_str(), ent->d_name);
#if DEBUG_XATTRS
                if (resolved != fallback) {
                    LOG(DEBUG) << "Resolved path " << resolved << " for inode " << inode
                            << " instead of " << fallback;
                }
#endif
                closedir(dir);
                return resolved;
            }
        }
        LOG(WARNING) << "Failed to resolve inode " << inode << "; using " << fallback;
        closedir(dir);
        return fallback;
    } else {
        return fallback;
    }
}

void add_cache_files(cache_t* cache, const std::string& data_path) {
    DIR *d;
    struct dirent *de;
    char dirname[PATH_MAX];

    const char* basepath = data_path.c_str();
    CACHE_NOISY(ALOGI("add_cache_files: basepath=%s\n", basepath));

    d = opendir(basepath);
    if (d == NULL) {
        return;
    }

    while ((de = readdir(d))) {
        if (de->d_type == DT_DIR) {
            DIR* subdir;
            const char *name = de->d_name;

                /* always skip "." and ".." */
            if (name[0] == '.') {
                if (name[1] == 0) continue;
                if ((name[1] == '.') && (name[2] == 0)) continue;
            }

            auto parent = StringPrintf("%s/%s", basepath, name);
            auto resolved = read_path_inode(parent, "cache", kXattrInodeCache);
            strcpy(dirname, resolved.c_str());
            CACHE_NOISY(ALOGI("Adding cache files from dir: %s\n", dirname));

            subdir = opendir(dirname);
            if (subdir != NULL) {
                size_t dirnameLen = strlen(dirname);
                _add_cache_files(cache, NULL, dirname, subdir, dirname, dirname+dirnameLen,
                        PATH_MAX - dirnameLen);
                closedir(subdir);
            }
        }
    }

    closedir(d);
}

static char *create_dir_path(char path[PATH_MAX], cache_dir_t* dir)
{
    char *pos = path;
    if (dir->parent != NULL) {
        pos = create_dir_path(path, dir->parent);
    }
    // Note that we don't need to worry about going beyond the buffer,
    // since when we were constructing the cache entries our maximum
    // buffer size for full paths was PATH_MAX.
    strcpy(pos, dir->name);
    pos += strlen(pos);
    *pos = '/';
    pos++;
    *pos = 0;
    return pos;
}

static void delete_cache_dir(char path[PATH_MAX], cache_dir_t* dir)
{
    if (dir->parent != NULL) {
        create_dir_path(path, dir);
        ALOGI("DEL DIR %s\n", path);
        if (dir->hiddenCount <= 0) {
            if (rmdir(path)) {
                ALOGE("Couldn't rmdir %s: %s\n", path, strerror(errno));
                return;
            }
        } else {
            // The directory contains hidden files so we need to delete
            // them along with the directory itself.
            if (delete_dir_contents(path, 1, NULL)) {
                return;
            }
        }
        dir->parent->childCount--;
        dir->deleted = 1;
        if (dir->parent->childCount <= 0) {
            delete_cache_dir(path, dir->parent);
        }
    } else if (dir->hiddenCount > 0) {
        // This is a root directory, but it has hidden files.  Get rid of
        // all of those files, but not the directory itself.
        create_dir_path(path, dir);
        ALOGI("DEL CONTENTS %s\n", path);
        delete_dir_contents(path, 0, NULL);
    }
}

static int cache_modtime_sort(const void *lhsP, const void *rhsP)
{
    const cache_file_t *lhs = *(const cache_file_t**)lhsP;
    const cache_file_t *rhs = *(const cache_file_t**)rhsP;
    return lhs->modTime < rhs->modTime ? -1 : (lhs->modTime > rhs->modTime ? 1 : 0);
}

void clear_cache_files(const std::string& data_path, cache_t* cache, int64_t free_size)
{
    size_t i;
    int skip = 0;
    char path[PATH_MAX];

    ALOGI("Collected cache files: %zd directories, %zd files",
        cache->numDirs, cache->numFiles);

    CACHE_NOISY(ALOGI("Sorting files..."));
    qsort(cache->files, cache->numFiles, sizeof(cache_file_t*),
            cache_modtime_sort);

    CACHE_NOISY(ALOGI("Cleaning empty directories..."));
    for (i=cache->numDirs; i>0; i--) {
        cache_dir_t* dir = cache->dirs[i-1];
        if (dir->childCount <= 0 && !dir->deleted) {
            delete_cache_dir(path, dir);
        }
    }

    CACHE_NOISY(ALOGI("Trimming files..."));
    for (i=0; i<cache->numFiles; i++) {
        skip++;
        if (skip > 10) {
            if (data_disk_free(data_path) > free_size) {
                return;
            }
            skip = 0;
        }
        cache_file_t* file = cache->files[i];
        strcpy(create_dir_path(path, file->dir), file->name);
        ALOGI("DEL (mod %d) %s\n", (int)file->modTime, path);
        if (unlink(path) < 0) {
            ALOGE("Couldn't unlink %s: %s\n", path, strerror(errno));
        }
        file->dir->childCount--;
        if (file->dir->childCount <= 0) {
            delete_cache_dir(path, file->dir);
        }
    }
}

void finish_cache_collection(cache_t* cache)
{
    CACHE_NOISY(size_t i;)

    CACHE_NOISY(ALOGI("clear_cache_files: %zu dirs, %zu files\n", cache->numDirs, cache->numFiles));
    CACHE_NOISY(
        for (i=0; i<cache->numDirs; i++) {
            cache_dir_t* dir = cache->dirs[i];
            ALOGI("dir #%zu: %p %s parent=%p\n", i, dir, dir->name, dir->parent);
        })
    CACHE_NOISY(
        for (i=0; i<cache->numFiles; i++) {
            cache_file_t* file = cache->files[i];
            ALOGI("file #%zu: %p %s time=%d dir=%p\n", i, file, file->name,
                    (int)file->modTime, file->dir);
        })
    void* block = cache->memBlocks;
    while (block != NULL) {
        void* nextBlock = *(void**)block;
        CACHE_NOISY(ALOGI("Freeing cache mem block: %p", block));
        free(block);
        block = nextBlock;
    }
    free(cache);
}

/**
 * Validate that the path is valid in the context of the provided directory.
 * The path is allowed to have at most one subdirectory and no indirections
 * to top level directories (i.e. have "..").
 */
static int validate_path(const dir_rec_t* dir, const char* path, int maxSubdirs) {
    size_t dir_len = dir->len;
    const char* subdir = strchr(path + dir_len, '/');

    // Only allow the path to have at most one subdirectory.
    if (subdir != NULL) {
        ++subdir;
        if ((--maxSubdirs == 0) && strchr(subdir, '/') != NULL) {
            ALOGE("invalid apk path '%s' (subdir?)\n", path);
            return -1;
        }
    }

    // Directories can't have a period directly after the directory markers to prevent "..".
    if ((path[dir_len] == '.') || ((subdir != NULL) && (*subdir == '.'))) {
        ALOGE("invalid apk path '%s' (trickery)\n", path);
        return -1;
    }

    return 0;
}

/**
 * Checks whether a path points to a system app (.apk file). Returns 0
 * if it is a system app or -1 if it is not.
 */
int validate_system_app_path(const char* path) {
    size_t i;

    for (i = 0; i < android_system_dirs.count; i++) {
        const size_t dir_len = android_system_dirs.dirs[i].len;
        if (!strncmp(path, android_system_dirs.dirs[i].path, dir_len)) {
            return validate_path(android_system_dirs.dirs + i, path, 1);
        }
    }

    return -1;
}

/**
 * Get the contents of a environment variable that contains a path. Caller
 * owns the string that is inserted into the directory record. Returns
 * 0 on success and -1 on error.
 */
int get_path_from_env(dir_rec_t* rec, const char* var) {
    const char* path = getenv(var);
    int ret = get_path_from_string(rec, path);
    if (ret < 0) {
        ALOGW("Problem finding value for environment variable %s\n", var);
    }
    return ret;
}

/**
 * Puts the string into the record as a directory. Appends '/' to the end
 * of all paths. Caller owns the string that is inserted into the directory
 * record. A null value will result in an error.
 *
 * Returns 0 on success and -1 on error.
 */
int get_path_from_string(dir_rec_t* rec, const char* path) {
    if (path == NULL) {
        return -1;
    } else {
        const size_t path_len = strlen(path);
        if (path_len <= 0) {
            return -1;
        }

        // Make sure path is absolute.
        if (path[0] != '/') {
            return -1;
        }

        if (path[path_len - 1] == '/') {
            // Path ends with a forward slash. Make our own copy.

            rec->path = strdup(path);
            if (rec->path == NULL) {
                return -1;
            }

            rec->len = path_len;
        } else {
            // Path does not end with a slash. Generate a new string.
            char *dst;

            // Add space for slash and terminating null.
            size_t dst_size = path_len + 2;

            rec->path = (char*) malloc(dst_size);
            if (rec->path == NULL) {
                return -1;
            }

            dst = rec->path;

            if (append_and_increment(&dst, path, &dst_size) < 0
                    || append_and_increment(&dst, "/", &dst_size)) {
                ALOGE("Error canonicalizing path");
                return -1;
            }

            rec->len = dst - rec->path;
        }
    }
    return 0;
}

int copy_and_append(dir_rec_t* dst, const dir_rec_t* src, const char* suffix) {
    dst->len = src->len + strlen(suffix);
    const size_t dstSize = dst->len + 1;
    dst->path = (char*) malloc(dstSize);

    if (dst->path == NULL
            || snprintf(dst->path, dstSize, "%s%s", src->path, suffix)
                    != (ssize_t) dst->len) {
        ALOGE("Could not allocate memory to hold appended path; aborting\n");
        return -1;
    }

    return 0;
}

/**
 * Check whether path points to a valid path for an APK file. The path must
 * begin with a whitelisted prefix path and must be no deeper than |maxSubdirs| within
 * that path. Returns -1 when an invalid path is encountered and 0 when a valid path
 * is encountered.
 */
static int validate_apk_path_internal(const char *path, int maxSubdirs) {
    const dir_rec_t* dir = NULL;
    if (!strncmp(path, android_app_dir.path, android_app_dir.len)) {
        dir = &android_app_dir;
    } else if (!strncmp(path, android_app_private_dir.path, android_app_private_dir.len)) {
        dir = &android_app_private_dir;
    } else if (!strncmp(path, android_app_ephemeral_dir.path, android_app_ephemeral_dir.len)) {
        dir = &android_app_ephemeral_dir;
    } else if (!strncmp(path, android_asec_dir.path, android_asec_dir.len)) {
        dir = &android_asec_dir;
    } else if (!strncmp(path, android_mnt_expand_dir.path, android_mnt_expand_dir.len)) {
        dir = &android_mnt_expand_dir;
        if (maxSubdirs < 2) {
            maxSubdirs = 2;
        }
    } else {
        return -1;
    }

    return validate_path(dir, path, maxSubdirs);
}

int validate_apk_path(const char* path) {
    return validate_apk_path_internal(path, 1 /* maxSubdirs */);
}

int validate_apk_path_subdirs(const char* path) {
    return validate_apk_path_internal(path, 3 /* maxSubdirs */);
}

int append_and_increment(char** dst, const char* src, size_t* dst_size) {
    ssize_t ret = strlcpy(*dst, src, *dst_size);
    if (ret < 0 || (size_t) ret >= *dst_size) {
        return -1;
    }
    *dst += ret;
    *dst_size -= ret;
    return 0;
}

char *build_string2(const char *s1, const char *s2) {
    if (s1 == NULL || s2 == NULL) return NULL;

    int len_s1 = strlen(s1);
    int len_s2 = strlen(s2);
    int len = len_s1 + len_s2 + 1;
    char *result = (char *) malloc(len);
    if (result == NULL) return NULL;

    strcpy(result, s1);
    strcpy(result + len_s1, s2);

    return result;
}

char *build_string3(const char *s1, const char *s2, const char *s3) {
    if (s1 == NULL || s2 == NULL || s3 == NULL) return NULL;

    int len_s1 = strlen(s1);
    int len_s2 = strlen(s2);
    int len_s3 = strlen(s3);
    int len = len_s1 + len_s2 + len_s3 + 1;
    char *result = (char *) malloc(len);
    if (result == NULL) return NULL;

    strcpy(result, s1);
    strcpy(result + len_s1, s2);
    strcpy(result + len_s1 + len_s2, s3);

    return result;
}

int ensure_config_user_dirs(userid_t userid) {
    // writable by system, readable by any app within the same user
    const int uid = multiuser_get_uid(userid, AID_SYSTEM);
    const int gid = multiuser_get_uid(userid, AID_EVERYBODY);

    // Ensure /data/misc/user/<userid> exists
    auto path = create_data_misc_legacy_path(userid);
    return fs_prepare_dir(path.c_str(), 0750, uid, gid);
}

int wait_child(pid_t pid)
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

}  // namespace installd
}  // namespace android
