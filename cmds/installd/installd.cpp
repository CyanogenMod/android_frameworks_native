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

#include <fcntl.h>
#include <selinux/android.h>
#include <selinux/avc.h>
#include <sys/capability.h>
#include <sys/fsuid.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <android-base/logging.h>
#include <cutils/fs.h>
#include <cutils/log.h>               // TODO: Move everything to base::logging.
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <private/android_filesystem_config.h>

#include <commands.h>
#include <globals.h>
#include <installd_constants.h>
#include <installd_deps.h>  // Need to fill in requirements of commands.
#include <utils.h>

#ifndef LOG_TAG
#define LOG_TAG "installd"
#endif
#define SOCKET_PATH "installd"

#define BUFFER_MAX    1024  /* input buffer for commands */
#define TOKEN_MAX     16    /* max number of arguments in buffer */
#define REPLY_MAX     256   /* largest reply allowed */

namespace android {
namespace installd {

// Check that installd-deps sizes match cutils sizes.
static_assert(kPropertyKeyMax == PROPERTY_KEY_MAX, "Size mismatch.");
static_assert(kPropertyValueMax == PROPERTY_VALUE_MAX, "Size mismatch.");

////////////////////////
// Plug-in functions. //
////////////////////////

int get_property(const char *key, char *value, const char *default_value) {
    return property_get(key, value, default_value);
}

// Compute the output path of
bool calculate_oat_file_path(char path[PKG_PATH_MAX],
                             const char *oat_dir,
                             const char *apk_path,
                             const char *instruction_set) {
    char *file_name_start;
    char *file_name_end;

    file_name_start = strrchr(apk_path, '/');
    if (file_name_start == NULL) {
        ALOGE("apk_path '%s' has no '/'s in it\n", apk_path);
        return false;
    }
    file_name_end = strrchr(apk_path, '.');
    if (file_name_end < file_name_start) {
        ALOGE("apk_path '%s' has no extension\n", apk_path);
        return false;
    }

    // Calculate file_name
    int file_name_len = file_name_end - file_name_start - 1;
    char file_name[file_name_len + 1];
    memcpy(file_name, file_name_start + 1, file_name_len);
    file_name[file_name_len] = '\0';

    // <apk_parent_dir>/oat/<isa>/<file_name>.odex
    snprintf(path, PKG_PATH_MAX, "%s/%s/%s.odex", oat_dir, instruction_set, file_name);
    return true;
}

/*
 * Computes the odex file for the given apk_path and instruction_set.
 * /system/framework/whatever.jar -> /system/framework/oat/<isa>/whatever.odex
 *
 * Returns false if it failed to determine the odex file path.
 */
bool calculate_odex_file_path(char path[PKG_PATH_MAX],
                              const char *apk_path,
                              const char *instruction_set) {
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

bool create_cache_path(char path[PKG_PATH_MAX],
                       const char *src,
                       const char *instruction_set) {
    /* demand that we are an absolute path */
    if ((src == nullptr) || (src[0] != '/') || strstr(src,"..")) {
        return false;
    }

    size_t srclen = strlen(src);

    if (srclen > PKG_PATH_MAX) {        // XXX: PKG_NAME_MAX?
        return false;
    }

    size_t dstlen =
        android_data_dir.len +
        strlen(DALVIK_CACHE) +
        1 +
        strlen(instruction_set) +
        srclen +
        strlen(DALVIK_CACHE_POSTFIX) + 2;

    if (dstlen > PKG_PATH_MAX) {
        return false;
    }

    sprintf(path,"%s%s/%s/%s%s",
            android_data_dir.path,
            DALVIK_CACHE,
            instruction_set,
            src + 1, /* skip the leading / */
            DALVIK_CACHE_POSTFIX);

    char* tmp =
            path +
            android_data_dir.len +
            strlen(DALVIK_CACHE) +
            1 +
            strlen(instruction_set) + 1;

    for(; *tmp; tmp++) {
        if (*tmp == '/') {
            *tmp = '@';
        }
    }

    return true;
}


static char* parse_null(char* arg) {
    if (strcmp(arg, "!") == 0) {
        return nullptr;
    } else {
        return arg;
    }
}

static int do_ping(char **arg ATTRIBUTE_UNUSED, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    return 0;
}

static int do_create_app_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char *uuid, const char *pkgname, userid_t userid, int flags,
            appid_t appid, const char* seinfo, int target_sdk_version */
    return create_app_data(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]),
                           atoi(arg[4]), arg[5], atoi(arg[6]));
}

static int do_restorecon_app_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char* uuid, const char* pkgName, userid_t userid, int flags,
            appid_t appid, const char* seinfo */
    return restorecon_app_data(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]), atoi(arg[4]), arg[5]);
}

static int do_migrate_app_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char *uuid, const char *pkgname, userid_t userid, int flags */
    return migrate_app_data(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]));
}

static int do_clear_app_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char *uuid, const char *pkgname, userid_t userid, int flags, ino_t ce_data_inode */
    return clear_app_data(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]), atol(arg[4]));
}

static int do_destroy_app_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char *uuid, const char *pkgname, userid_t userid, int flags, ino_t ce_data_inode */
    return destroy_app_data(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]), atol(arg[4]));
}

// We use otapreopt_chroot to get into the chroot.
static constexpr const char* kOtaPreopt = "/system/bin/otapreopt_chroot";

static int do_ota_dexopt(const char* args[DEXOPT_PARAM_COUNT],
                         char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    // Time to fork and run otapreopt.

    // Check that the tool exists.
    struct stat s;
    if (stat(kOtaPreopt, &s) != 0) {
        LOG(ERROR) << "Otapreopt chroot tool not found.";
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        const char* argv[1 + DEXOPT_PARAM_COUNT + 1];
        argv[0] = kOtaPreopt;

        for (size_t i = 0; i < DEXOPT_PARAM_COUNT; ++i) {
            argv[i + 1] = args[i];
        }

        argv[DEXOPT_PARAM_COUNT + 1] = nullptr;

        execv(argv[0], (char * const *)argv);
        PLOG(ERROR) << "execv(OTAPREOPT_CHROOT) failed";
        exit(99);
    } else {
        int res = wait_child(pid);
        if (res == 0) {
            ALOGV("DexInv: --- END OTAPREOPT (success) ---\n");
        } else {
            ALOGE("DexInv: --- END OTAPREOPT --- status=0x%04x, process failed\n", res);
        }
        return res;
    }
}

static int do_regular_dexopt(const char* args[DEXOPT_PARAM_COUNT],
                             char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    return dexopt(args);
}

using DexoptFn = int (*)(const char* args[DEXOPT_PARAM_COUNT],
                         char reply[REPLY_MAX]);

static int do_dexopt(char **arg, char reply[REPLY_MAX])
{
    const char* args[DEXOPT_PARAM_COUNT];
    for (size_t i = 0; i < DEXOPT_PARAM_COUNT; ++i) {
        CHECK(arg[i] != nullptr);
        args[i] = arg[i];
    }

    int dexopt_flags = atoi(arg[6]);
    DexoptFn dexopt_fn;
    if ((dexopt_flags & DEXOPT_OTA) != 0) {
        dexopt_fn = do_ota_dexopt;
    } else {
        dexopt_fn = do_regular_dexopt;
    }
    return dexopt_fn(args, reply);
}

static int do_merge_profiles(char **arg, char reply[REPLY_MAX])
{
    uid_t uid = static_cast<uid_t>(atoi(arg[0]));
    const char* pkgname = arg[1];
    if (merge_profiles(uid, pkgname)) {
        strncpy(reply, "true", REPLY_MAX);
    } else {
        strncpy(reply, "false", REPLY_MAX);
    }
    return 0;
}

static int do_dump_profiles(char **arg, char reply[REPLY_MAX])
{
    uid_t uid = static_cast<uid_t>(atoi(arg[0]));
    const char* pkgname = arg[1];
    const char* dex_files = arg[2];
    if (dump_profile(uid, pkgname, dex_files)) {
        strncpy(reply, "true", REPLY_MAX);
    } else {
        strncpy(reply, "false", REPLY_MAX);
    }
    return 0;
}

static int do_mark_boot_complete(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    return mark_boot_complete(arg[0] /* instruction set */);
}

static int do_rm_dex(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    return rm_dex(arg[0], arg[1]); /* pkgname, instruction_set */
}

static int do_free_cache(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) /* TODO int:free_size */
{
    return free_cache(parse_null(arg[0]), (int64_t)atoll(arg[1])); /* uuid, free_size */
}

static int do_get_app_size(char **arg, char reply[REPLY_MAX]) {
    int64_t codesize = 0;
    int64_t datasize = 0;
    int64_t cachesize = 0;
    int64_t asecsize = 0;
    int res = 0;

    /* const char *uuid, const char *pkgname, int userid, int flags, ino_t ce_data_inode,
            const char* code_path */
    res = get_app_size(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]), atol(arg[4]),
            arg[5], &codesize, &datasize, &cachesize, &asecsize);

    /*
     * Each int64_t can take up 22 characters printed out. Make sure it
     * doesn't go over REPLY_MAX in the future.
     */
    snprintf(reply, REPLY_MAX, "%" PRId64 " %" PRId64 " %" PRId64 " %" PRId64,
            codesize, datasize, cachesize, asecsize);
    return res;
}

static int do_get_app_data_inode(char **arg, char reply[REPLY_MAX]) {
    ino_t inode = 0;
    int res = 0;

    /* const char *uuid, const char *pkgname, int userid, int flags */
    res = get_app_data_inode(parse_null(arg[0]), arg[1], atoi(arg[2]), atoi(arg[3]), &inode);

    snprintf(reply, REPLY_MAX, "%" PRId64, (int64_t) inode);
    return res;
}

static int do_move_complete_app(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    /* const char* from_uuid, const char *to_uuid, const char *package_name,
            const char *data_app_name, appid_t appid, const char* seinfo,
            int target_sdk_version */
    return move_complete_app(parse_null(arg[0]), parse_null(arg[1]), arg[2], arg[3],
                             atoi(arg[4]), arg[5], atoi(arg[6]));
}

static int do_create_user_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* const char *uuid, userid_t userid, int user_serial, int flags */
    return create_user_data(parse_null(arg[0]), atoi(arg[1]), atoi(arg[2]), atoi(arg[3]));
}

static int do_destroy_user_data(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* const char *uuid, userid_t userid, int flags */
    return destroy_user_data(parse_null(arg[0]), atoi(arg[1]), atoi(arg[2]));
}

static int do_linklib(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    return linklib(parse_null(arg[0]), arg[1], arg[2], atoi(arg[3]));
}

static int do_idmap(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    return idmap(arg[0], arg[1], atoi(arg[2]));
}

static int do_create_oat_dir(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* oat_dir, instruction_set */
    return create_oat_dir(arg[0], arg[1]);
}

static int do_rm_package_dir(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* oat_dir */
    return rm_package_dir(arg[0]);
}

static int do_clear_app_profiles(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* package_name */
    return clear_app_profiles(arg[0]);
}

static int do_destroy_app_profiles(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* package_name */
    return destroy_app_profiles(arg[0]);
}

static int do_link_file(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED)
{
    /* relative_path, from_base, to_base */
    return link_file(arg[0], arg[1], arg[2]);
}

static int do_move_ab(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    // apk_path, instruction_set, oat_dir
    return move_ab(arg[0], arg[1], arg[2]);
}

static int do_delete_odex(char **arg, char reply[REPLY_MAX] ATTRIBUTE_UNUSED) {
    // apk_path, instruction_set, oat_dir
    return delete_odex(arg[0], arg[1], arg[2]) ? 0 : -1;
}

struct cmdinfo {
    const char *name;
    unsigned numargs;
    int (*func)(char **arg, char reply[REPLY_MAX]);
};

struct cmdinfo cmds[] = {
    { "ping",                 0, do_ping },

    { "create_app_data",      7, do_create_app_data },
    { "restorecon_app_data",  6, do_restorecon_app_data },
    { "migrate_app_data",     4, do_migrate_app_data },
    { "clear_app_data",       5, do_clear_app_data },
    { "destroy_app_data",     5, do_destroy_app_data },
    { "move_complete_app",    7, do_move_complete_app },
    { "get_app_size",         6, do_get_app_size },
    { "get_app_data_inode",   4, do_get_app_data_inode },

    { "create_user_data",     4, do_create_user_data },
    { "destroy_user_data",    3, do_destroy_user_data },

    { "dexopt",              10, do_dexopt },
    { "markbootcomplete",     1, do_mark_boot_complete },
    { "rmdex",                2, do_rm_dex },
    { "freecache",            2, do_free_cache },
    { "linklib",              4, do_linklib },
    { "idmap",                3, do_idmap },
    { "createoatdir",         2, do_create_oat_dir },
    { "rmpackagedir",         1, do_rm_package_dir },
    { "clear_app_profiles",   1, do_clear_app_profiles },
    { "destroy_app_profiles", 1, do_destroy_app_profiles },
    { "linkfile",             3, do_link_file },
    { "move_ab",              3, do_move_ab },
    { "merge_profiles",       2, do_merge_profiles },
    { "dump_profiles",        3, do_dump_profiles },
    { "delete_odex",          3, do_delete_odex },
};

static int readx(int s, void *_buf, int count)
{
    char *buf = (char *) _buf;
    int n = 0, r;
    if (count < 0) return -1;
    while (n < count) {
        r = read(s, buf + n, count - n);
        if (r < 0) {
            if (errno == EINTR) continue;
            ALOGE("read error: %s\n", strerror(errno));
            return -1;
        }
        if (r == 0) {
            ALOGE("eof\n");
            return -1; /* EOF */
        }
        n += r;
    }
    return 0;
}

static int writex(int s, const void *_buf, int count)
{
    const char *buf = (const char *) _buf;
    int n = 0, r;
    if (count < 0) return -1;
    while (n < count) {
        r = write(s, buf + n, count - n);
        if (r < 0) {
            if (errno == EINTR) continue;
            ALOGE("write error: %s\n", strerror(errno));
            return -1;
        }
        n += r;
    }
    return 0;
}


/* Tokenize the command buffer, locate a matching command,
 * ensure that the required number of arguments are provided,
 * call the function(), return the result.
 */
static int execute(int s, char cmd[BUFFER_MAX])
{
    char reply[REPLY_MAX];
    char *arg[TOKEN_MAX+1];
    unsigned i;
    unsigned n = 0;
    unsigned short count;
    int ret = -1;

    // ALOGI("execute('%s')\n", cmd);

        /* default reply is "" */
    reply[0] = 0;

        /* n is number of args (not counting arg[0]) */
    arg[0] = cmd;
    while (*cmd) {
        if (isspace(*cmd)) {
            *cmd++ = 0;
            n++;
            arg[n] = cmd;
            if (n == TOKEN_MAX) {
                ALOGE("too many arguments\n");
                goto done;
            }
        }
        if (*cmd) {
          cmd++;
        }
    }

    for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if (!strcmp(cmds[i].name,arg[0])) {
            if (n != cmds[i].numargs) {
                ALOGE("%s requires %d arguments (%d given)\n",
                     cmds[i].name, cmds[i].numargs, n);
            } else {
                ret = cmds[i].func(arg + 1, reply);
            }
            goto done;
        }
    }
    ALOGE("unsupported command '%s'\n", arg[0]);

done:
    if (reply[0]) {
        n = snprintf(cmd, BUFFER_MAX, "%d %s", ret, reply);
    } else {
        n = snprintf(cmd, BUFFER_MAX, "%d", ret);
    }
    if (n > BUFFER_MAX) n = BUFFER_MAX;
    count = n;

    // ALOGI("reply: '%s'\n", cmd);
    if (writex(s, &count, sizeof(count))) return -1;
    if (writex(s, cmd, count)) return -1;
    return 0;
}

static bool initialize_globals() {
    const char* data_path = getenv("ANDROID_DATA");
    if (data_path == nullptr) {
        ALOGE("Could not find ANDROID_DATA");
        return false;
    }
    const char* root_path = getenv("ANDROID_ROOT");
    if (root_path == nullptr) {
        ALOGE("Could not find ANDROID_ROOT");
        return false;
    }

    return init_globals_from_data_and_root(data_path, root_path);
}

static int initialize_directories() {
    int res = -1;

    // Read current filesystem layout version to handle upgrade paths
    char version_path[PATH_MAX];
    snprintf(version_path, PATH_MAX, "%s.layout_version", android_data_dir.path);

    int oldVersion;
    if (fs_read_atomic_int(version_path, &oldVersion) == -1) {
        oldVersion = 0;
    }
    int version = oldVersion;

    if (version < 2) {
        SLOGD("Assuming that device has multi-user storage layout; upgrade no longer supported");
        version = 2;
    }

    if (ensure_config_user_dirs(0) == -1) {
        ALOGE("Failed to setup misc for user 0");
        goto fail;
    }

    if (version == 2) {
        ALOGD("Upgrading to /data/misc/user directories");

        char misc_dir[PATH_MAX];
        snprintf(misc_dir, PATH_MAX, "%smisc", android_data_dir.path);

        char keychain_added_dir[PATH_MAX];
        snprintf(keychain_added_dir, PATH_MAX, "%s/keychain/cacerts-added", misc_dir);

        char keychain_removed_dir[PATH_MAX];
        snprintf(keychain_removed_dir, PATH_MAX, "%s/keychain/cacerts-removed", misc_dir);

        DIR *dir;
        struct dirent *dirent;
        dir = opendir("/data/user");
        if (dir != NULL) {
            while ((dirent = readdir(dir))) {
                const char *name = dirent->d_name;

                // skip "." and ".."
                if (name[0] == '.') {
                    if (name[1] == 0) continue;
                    if ((name[1] == '.') && (name[2] == 0)) continue;
                }

                uint32_t user_id = atoi(name);

                // /data/misc/user/<user_id>
                if (ensure_config_user_dirs(user_id) == -1) {
                    goto fail;
                }

                char misc_added_dir[PATH_MAX];
                snprintf(misc_added_dir, PATH_MAX, "%s/user/%s/cacerts-added", misc_dir, name);

                char misc_removed_dir[PATH_MAX];
                snprintf(misc_removed_dir, PATH_MAX, "%s/user/%s/cacerts-removed", misc_dir, name);

                uid_t uid = multiuser_get_uid(user_id, AID_SYSTEM);
                gid_t gid = uid;
                if (access(keychain_added_dir, F_OK) == 0) {
                    if (copy_dir_files(keychain_added_dir, misc_added_dir, uid, gid) != 0) {
                        ALOGE("Some files failed to copy");
                    }
                }
                if (access(keychain_removed_dir, F_OK) == 0) {
                    if (copy_dir_files(keychain_removed_dir, misc_removed_dir, uid, gid) != 0) {
                        ALOGE("Some files failed to copy");
                    }
                }
            }
            closedir(dir);

            if (access(keychain_added_dir, F_OK) == 0) {
                delete_dir_contents(keychain_added_dir, 1, 0);
            }
            if (access(keychain_removed_dir, F_OK) == 0) {
                delete_dir_contents(keychain_removed_dir, 1, 0);
            }
        }

        version = 3;
    }

    // Persist layout version if changed
    if (version != oldVersion) {
        if (fs_write_atomic_int(version_path, version) == -1) {
            ALOGE("Failed to save version to %s: %s", version_path, strerror(errno));
            goto fail;
        }
    }

    // Success!
    res = 0;

fail:
    return res;
}

static int log_callback(int type, const char *fmt, ...) {
    va_list ap;
    int priority;

    switch (type) {
    case SELINUX_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
    case SELINUX_INFO:
        priority = ANDROID_LOG_INFO;
        break;
    default:
        priority = ANDROID_LOG_ERROR;
        break;
    }
    va_start(ap, fmt);
    LOG_PRI_VA(priority, "SELinux", fmt, ap);
    va_end(ap);
    return 0;
}

static int installd_main(const int argc ATTRIBUTE_UNUSED, char *argv[]) {
    char buf[BUFFER_MAX];
    struct sockaddr addr;
    socklen_t alen;
    int lsocket, s;
    int selinux_enabled = (is_selinux_enabled() > 0);

    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv);

    ALOGI("installd firing up\n");

    union selinux_callback cb;
    cb.func_log = log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);

    if (!initialize_globals()) {
        ALOGE("Could not initialize globals; exiting.\n");
        exit(1);
    }

    if (initialize_directories() < 0) {
        ALOGE("Could not create directories; exiting.\n");
        exit(1);
    }

    if (selinux_enabled && selinux_status_open(true) < 0) {
        ALOGE("Could not open selinux status; exiting.\n");
        exit(1);
    }

    lsocket = android_get_control_socket(SOCKET_PATH);
    if (lsocket < 0) {
        ALOGE("Failed to get socket from environment: %s\n", strerror(errno));
        exit(1);
    }
    if (listen(lsocket, 5)) {
        ALOGE("Listen on socket failed: %s\n", strerror(errno));
        exit(1);
    }
    fcntl(lsocket, F_SETFD, FD_CLOEXEC);

    for (;;) {
        alen = sizeof(addr);
        s = accept(lsocket, &addr, &alen);
        if (s < 0) {
            ALOGE("Accept failed: %s\n", strerror(errno));
            continue;
        }
        fcntl(s, F_SETFD, FD_CLOEXEC);

        ALOGI("new connection\n");
        for (;;) {
            unsigned short count;
            if (readx(s, &count, sizeof(count))) {
                ALOGE("failed to read size\n");
                break;
            }
            if ((count < 1) || (count >= BUFFER_MAX)) {
                ALOGE("invalid size %d\n", count);
                break;
            }
            if (readx(s, buf, count)) {
                ALOGE("failed to read command\n");
                break;
            }
            buf[count] = 0;
            if (selinux_enabled && selinux_status_updated() > 0) {
                selinux_android_seapp_context_reload();
            }
            if (execute(s, buf)) break;
        }
        ALOGI("closing connection\n");
        close(s);
    }

    return 0;
}

}  // namespace installd
}  // namespace android

int main(const int argc, char *argv[]) {
    return android::installd::installd_main(argc, argv);
}
