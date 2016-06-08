/*
 ** Copyright 2016, The Android Open Source Project
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

#include <algorithm>
#include <inttypes.h>
#include <random>
#include <regex>
#include <selinux/android.h>
#include <selinux/avc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cutils/fs.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include <commands.h>
#include <file_parsing.h>
#include <globals.h>
#include <installd_deps.h>  // Need to fill in requirements of commands.
#include <system_properties.h>
#include <utils.h>

#ifndef LOG_TAG
#define LOG_TAG "otapreopt"
#endif

#define BUFFER_MAX    1024  /* input buffer for commands */
#define TOKEN_MAX     16    /* max number of arguments in buffer */
#define REPLY_MAX     256   /* largest reply allowed */

using android::base::EndsWith;
using android::base::Join;
using android::base::Split;
using android::base::StartsWith;
using android::base::StringPrintf;

namespace android {
namespace installd {

static constexpr const char* kBootClassPathPropertyName = "BOOTCLASSPATH";
static constexpr const char* kAndroidRootPathPropertyName = "ANDROID_ROOT";
static constexpr const char* kOTARootDirectory = "/system-b";
static constexpr size_t kISAIndex = 3;

template<typename T>
static constexpr T RoundDown(T x, typename std::decay<T>::type n) {
    return DCHECK_CONSTEXPR(IsPowerOfTwo(n), , T(0))(x & -n);
}

template<typename T>
static constexpr T RoundUp(T x, typename std::remove_reference<T>::type n) {
    return RoundDown(x + n - 1, n);
}

class OTAPreoptService {
 public:
    static constexpr const char* kOTADataDirectory = "/data/ota";

    // Main driver. Performs the following steps.
    //
    // 1) Parse options (read system properties etc from B partition).
    //
    // 2) Read in package data.
    //
    // 3) Prepare environment variables.
    //
    // 4) Prepare(compile) boot image, if necessary.
    //
    // 5) Run update.
    int Main(int argc, char** argv) {
        if (!ReadSystemProperties()) {
            LOG(ERROR)<< "Failed reading system properties.";
            return 1;
        }

        if (!ReadEnvironment()) {
            LOG(ERROR) << "Failed reading environment properties.";
            return 2;
        }

        if (!ReadPackage(argc, argv)) {
            LOG(ERROR) << "Failed reading command line file.";
            return 3;
        }

        PrepareEnvironment();

        if (!PrepareBootImage()) {
            LOG(ERROR) << "Failed preparing boot image.";
            return 4;
        }

        int dexopt_retcode = RunPreopt();

        return dexopt_retcode;
    }

    int GetProperty(const char* key, char* value, const char* default_value) {
        const std::string* prop_value = system_properties_.GetProperty(key);
        if (prop_value == nullptr) {
            if (default_value == nullptr) {
                return 0;
            }
            // Copy in the default value.
            strncpy(value, default_value, kPropertyValueMax - 1);
            value[kPropertyValueMax - 1] = 0;
            return strlen(default_value);// TODO: Need to truncate?
        }
        size_t size = std::min(kPropertyValueMax - 1, prop_value->length());
        strncpy(value, prop_value->data(), size);
        value[size] = 0;
        return static_cast<int>(size);
    }

private:
    bool ReadSystemProperties() {
        static constexpr const char* kPropertyFiles[] = {
                "/default.prop", "/system/build.prop"
        };

        for (size_t i = 0; i < arraysize(kPropertyFiles); ++i) {
            if (!system_properties_.Load(kPropertyFiles[i])) {
                return false;
            }
        }

        return true;
    }

    bool ReadEnvironment() {
        // Parse the environment variables from init.environ.rc, which have the form
        //   export NAME VALUE
        // For simplicity, don't respect string quotation. The values we are interested in can be
        // encoded without them.
        std::regex export_regex("\\s*export\\s+(\\S+)\\s+(\\S+)");
        bool parse_result = ParseFile("/init.environ.rc", [&](const std::string& line) {
            std::smatch export_match;
            if (!std::regex_match(line, export_match, export_regex)) {
                return true;
            }

            if (export_match.size() != 3) {
                return true;
            }

            std::string name = export_match[1].str();
            std::string value = export_match[2].str();

            system_properties_.SetProperty(name, value);

            return true;
        });
        if (!parse_result) {
            return false;
        }

        // Check that we found important properties.
        constexpr const char* kRequiredProperties[] = {
                kBootClassPathPropertyName, kAndroidRootPathPropertyName
        };
        for (size_t i = 0; i < arraysize(kRequiredProperties); ++i) {
            if (system_properties_.GetProperty(kRequiredProperties[i]) == nullptr) {
                return false;
            }
        }

        return true;
    }

    bool ReadPackage(int argc ATTRIBUTE_UNUSED, char** argv) {
        size_t index = 0;
        static_assert(DEXOPT_PARAM_COUNT == ARRAY_SIZE(package_parameters_),
                      "Unexpected dexopt param count");
        while (index < DEXOPT_PARAM_COUNT &&
                argv[index + 1] != nullptr) {
            package_parameters_[index] = argv[index + 1];
            index++;
        }
        if (index != ARRAY_SIZE(package_parameters_) || argv[index + 1] != nullptr) {
            LOG(ERROR) << "Wrong number of parameters";
            return false;
        }

        return true;
    }

    void PrepareEnvironment() {
        CHECK(system_properties_.GetProperty(kBootClassPathPropertyName) != nullptr);
        const std::string& boot_cp =
                *system_properties_.GetProperty(kBootClassPathPropertyName);
        environ_.push_back(StringPrintf("BOOTCLASSPATH=%s", boot_cp.c_str()));
        environ_.push_back(StringPrintf("ANDROID_DATA=%s", kOTADataDirectory));
        CHECK(system_properties_.GetProperty(kAndroidRootPathPropertyName) != nullptr);
        const std::string& android_root =
                *system_properties_.GetProperty(kAndroidRootPathPropertyName);
        environ_.push_back(StringPrintf("ANDROID_ROOT=%s", android_root.c_str()));

        for (const std::string& e : environ_) {
            putenv(const_cast<char*>(e.c_str()));
        }
    }

    // Ensure that we have the right boot image. The first time any app is
    // compiled, we'll try to generate it.
    bool PrepareBootImage() {
        if (package_parameters_[kISAIndex] == nullptr) {
            LOG(ERROR) << "Instruction set missing.";
            return false;
        }
        const char* isa = package_parameters_[kISAIndex];

        // Check whether the file exists where expected.
        std::string dalvik_cache = std::string(kOTADataDirectory) + "/" + DALVIK_CACHE;
        std::string isa_path = dalvik_cache + "/" + isa;
        std::string art_path = isa_path + "/system@framework@boot.art";
        std::string oat_path = isa_path + "/system@framework@boot.oat";
        if (access(art_path.c_str(), F_OK) == 0 &&
                access(oat_path.c_str(), F_OK) == 0) {
            // Files exist, assume everything is alright.
            return true;
        }

        // Create the directories, if necessary.
        if (access(dalvik_cache.c_str(), F_OK) != 0) {
            if (mkdir(dalvik_cache.c_str(), 0711) != 0) {
                PLOG(ERROR) << "Could not create dalvik-cache dir";
                return false;
            }
        }
        if (access(isa_path.c_str(), F_OK) != 0) {
            if (mkdir(isa_path.c_str(), 0711) != 0) {
                PLOG(ERROR) << "Could not create dalvik-cache isa dir";
                return false;
            }
        }

        // Prepare to create.
        // TODO: Delete files, just for a blank slate.
        const std::string& boot_cp = *system_properties_.GetProperty(kBootClassPathPropertyName);

        std::string preopted_boot_art_path = StringPrintf("/system/framework/%s/boot.art", isa);
        if (access(preopted_boot_art_path.c_str(), F_OK) == 0) {
          return PatchoatBootImage(art_path, isa);
        } else {
          // No preopted boot image. Try to compile.
          return Dex2oatBootImage(boot_cp, art_path, oat_path, isa);
        }
    }

    bool PatchoatBootImage(const std::string& art_path, const char* isa) {
        // This needs to be kept in sync with ART, see art/runtime/gc/space/image_space.cc.

        std::vector<std::string> cmd;
        cmd.push_back("/system/bin/patchoat");

        cmd.push_back("--input-image-location=/system/framework/boot.art");
        cmd.push_back(StringPrintf("--output-image-file=%s", art_path.c_str()));

        cmd.push_back(StringPrintf("--instruction-set=%s", isa));

        int32_t base_offset = ChooseRelocationOffsetDelta(ART_BASE_ADDRESS_MIN_DELTA,
                                                          ART_BASE_ADDRESS_MAX_DELTA);
        cmd.push_back(StringPrintf("--base-offset-delta=%d", base_offset));

        std::string error_msg;
        bool result = Exec(cmd, &error_msg);
        if (!result) {
            LOG(ERROR) << "Could not generate boot image: " << error_msg;
        }
        return result;
    }

    bool Dex2oatBootImage(const std::string& boot_cp,
                          const std::string& art_path,
                          const std::string& oat_path,
                          const char* isa) {
        // This needs to be kept in sync with ART, see art/runtime/gc/space/image_space.cc.
        std::vector<std::string> cmd;
        cmd.push_back("/system/bin/dex2oat");
        cmd.push_back(StringPrintf("--image=%s", art_path.c_str()));
        for (const std::string& boot_part : Split(boot_cp, ":")) {
            cmd.push_back(StringPrintf("--dex-file=%s", boot_part.c_str()));
        }
        cmd.push_back(StringPrintf("--oat-file=%s", oat_path.c_str()));

        int32_t base_offset = ChooseRelocationOffsetDelta(ART_BASE_ADDRESS_MIN_DELTA,
                ART_BASE_ADDRESS_MAX_DELTA);
        cmd.push_back(StringPrintf("--base=0x%x", ART_BASE_ADDRESS + base_offset));

        cmd.push_back(StringPrintf("--instruction-set=%s", isa));

        // These things are pushed by AndroidRuntime, see frameworks/base/core/jni/AndroidRuntime.cpp.
        AddCompilerOptionFromSystemProperty("dalvik.vm.image-dex2oat-Xms",
                "-Xms",
                true,
                cmd);
        AddCompilerOptionFromSystemProperty("dalvik.vm.image-dex2oat-Xmx",
                "-Xmx",
                true,
                cmd);
        AddCompilerOptionFromSystemProperty("dalvik.vm.image-dex2oat-filter",
                "--compiler-filter=",
                false,
                cmd);
        cmd.push_back("--image-classes=/system/etc/preloaded-classes");
        // TODO: Compiled-classes.
        const std::string* extra_opts =
                system_properties_.GetProperty("dalvik.vm.image-dex2oat-flags");
        if (extra_opts != nullptr) {
            std::vector<std::string> extra_vals = Split(*extra_opts, " ");
            cmd.insert(cmd.end(), extra_vals.begin(), extra_vals.end());
        }
        // TODO: Should we lower this? It's usually set close to max, because
        //       normally there's not much else going on at boot.
        AddCompilerOptionFromSystemProperty("dalvik.vm.image-dex2oat-threads",
                "-j",
                false,
                cmd);
        AddCompilerOptionFromSystemProperty(
                StringPrintf("dalvik.vm.isa.%s.variant", isa).c_str(),
                "--instruction-set-variant=",
                false,
                cmd);
        AddCompilerOptionFromSystemProperty(
                StringPrintf("dalvik.vm.isa.%s.features", isa).c_str(),
                "--instruction-set-features=",
                false,
                cmd);

        std::string error_msg;
        bool result = Exec(cmd, &error_msg);
        if (!result) {
            LOG(ERROR) << "Could not generate boot image: " << error_msg;
        }
        return result;
    }

    static const char* ParseNull(const char* arg) {
        return (strcmp(arg, "!") == 0) ? nullptr : arg;
    }

    int RunPreopt() {
        // Run the preopt.
        //
        // There's one thing we have to be careful about: we may/will be asked to compile an app
        // living in the system image. This may be a valid request - if the app wasn't compiled,
        // e.g., if the system image wasn't large enough to include preopted files. However, the
        // data we have is from the old system, so the driver (the OTA service) can't actually
        // know. Thus, we will get requests for apps that have preopted components. To avoid
        // duplication (we'd generate files that are not used and are *not* cleaned up), do two
        // simple checks:
        //
        // 1) Does the apk_path start with the value of ANDROID_ROOT? (~in the system image)
        //    (For simplicity, assume the value of ANDROID_ROOT does not contain a symlink.)
        //
        // 2) If you replace the name in the apk_path with "oat," does the path exist?
        //    (=have a subdirectory for preopted files)
        //
        // If the answer to both is yes, skip the dexopt.
        //
        // Note: while one may think it's OK to call dexopt and it will fail (because APKs should
        //       be stripped), that's not true for APKs signed outside the build system (so the
        //       jar content must be exactly the same).

        //       (This is ugly as it's the only thing where we need to understand the contents
        //        of package_parameters_, but it beats postponing the decision or using the call-
        //        backs to do weird things.)
        constexpr size_t kApkPathIndex = 0;
        CHECK_GT(DEXOPT_PARAM_COUNT, kApkPathIndex);
        CHECK(package_parameters_[kApkPathIndex] != nullptr);
        CHECK(system_properties_.GetProperty(kAndroidRootPathPropertyName) != nullptr);
        if (StartsWith(package_parameters_[kApkPathIndex],
                       system_properties_.GetProperty(kAndroidRootPathPropertyName)->c_str())) {
            const char* last_slash = strrchr(package_parameters_[kApkPathIndex], '/');
            if (last_slash != nullptr) {
                std::string path(package_parameters_[kApkPathIndex],
                                 last_slash - package_parameters_[kApkPathIndex] + 1);
                CHECK(EndsWith(path, "/"));
                path = path + "oat";
                if (access(path.c_str(), F_OK) == 0) {
                    return 0;
                }
            }
        }

        return dexopt(package_parameters_);
    }

    ////////////////////////////////////
    // Helpers, mostly taken from ART //
    ////////////////////////////////////

    // Wrapper on fork/execv to run a command in a subprocess.
    bool Exec(const std::vector<std::string>& arg_vector, std::string* error_msg) {
        const std::string command_line = Join(arg_vector, ' ');

        CHECK_GE(arg_vector.size(), 1U) << command_line;

        // Convert the args to char pointers.
        const char* program = arg_vector[0].c_str();
        std::vector<char*> args;
        for (size_t i = 0; i < arg_vector.size(); ++i) {
            const std::string& arg = arg_vector[i];
            char* arg_str = const_cast<char*>(arg.c_str());
            CHECK(arg_str != nullptr) << i;
            args.push_back(arg_str);
        }
        args.push_back(nullptr);

        // Fork and exec.
        pid_t pid = fork();
        if (pid == 0) {
            // No allocation allowed between fork and exec.

            // Change process groups, so we don't get reaped by ProcessManager.
            setpgid(0, 0);

            execv(program, &args[0]);

            PLOG(ERROR) << "Failed to execv(" << command_line << ")";
            // _exit to avoid atexit handlers in child.
            _exit(1);
        } else {
            if (pid == -1) {
                *error_msg = StringPrintf("Failed to execv(%s) because fork failed: %s",
                        command_line.c_str(), strerror(errno));
                return false;
            }

            // wait for subprocess to finish
            int status;
            pid_t got_pid = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
            if (got_pid != pid) {
                *error_msg = StringPrintf("Failed after fork for execv(%s) because waitpid failed: "
                        "wanted %d, got %d: %s",
                        command_line.c_str(), pid, got_pid, strerror(errno));
                return false;
            }
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                *error_msg = StringPrintf("Failed execv(%s) because non-0 exit status",
                        command_line.c_str());
                return false;
            }
        }
        return true;
    }

    // Choose a random relocation offset. Taken from art/runtime/gc/image_space.cc.
    static int32_t ChooseRelocationOffsetDelta(int32_t min_delta, int32_t max_delta) {
        constexpr size_t kPageSize = PAGE_SIZE;
        CHECK_EQ(min_delta % kPageSize, 0u);
        CHECK_EQ(max_delta % kPageSize, 0u);
        CHECK_LT(min_delta, max_delta);

        std::default_random_engine generator;
        generator.seed(GetSeed());
        std::uniform_int_distribution<int32_t> distribution(min_delta, max_delta);
        int32_t r = distribution(generator);
        if (r % 2 == 0) {
            r = RoundUp(r, kPageSize);
        } else {
            r = RoundDown(r, kPageSize);
        }
        CHECK_LE(min_delta, r);
        CHECK_GE(max_delta, r);
        CHECK_EQ(r % kPageSize, 0u);
        return r;
    }

    static uint64_t GetSeed() {
#ifdef __BIONIC__
        // Bionic exposes arc4random, use it.
        uint64_t random_data;
        arc4random_buf(&random_data, sizeof(random_data));
        return random_data;
#else
#error "This is only supposed to run with bionic. Otherwise, implement..."
#endif
    }

    void AddCompilerOptionFromSystemProperty(const char* system_property,
            const char* prefix,
            bool runtime,
            std::vector<std::string>& out) {
        const std::string* value =
        system_properties_.GetProperty(system_property);
        if (value != nullptr) {
            if (runtime) {
                out.push_back("--runtime-arg");
            }
            if (prefix != nullptr) {
                out.push_back(StringPrintf("%s%s", prefix, value->c_str()));
            } else {
                out.push_back(*value);
            }
        }
    }

    // Stores the system properties read out of the B partition. We need to use these properties
    // to compile, instead of the A properties we could get from init/get_property.
    SystemProperties system_properties_;

    const char* package_parameters_[DEXOPT_PARAM_COUNT];

    // Store environment values we need to set.
    std::vector<std::string> environ_;
};

OTAPreoptService gOps;

////////////////////////
// Plug-in functions. //
////////////////////////

int get_property(const char *key, char *value, const char *default_value) {
    return gOps.GetProperty(key, value, default_value);
}

// Compute the output path of
bool calculate_oat_file_path(char path[PKG_PATH_MAX], const char *oat_dir,
                             const char *apk_path,
                             const char *instruction_set) {
    const char *file_name_start;
    const char *file_name_end;

    file_name_start = strrchr(apk_path, '/');
    if (file_name_start == nullptr) {
        ALOGE("apk_path '%s' has no '/'s in it\n", apk_path);
        return false;
    }
    file_name_end = strrchr(file_name_start, '.');
    if (file_name_end == nullptr) {
        ALOGE("apk_path '%s' has no extension\n", apk_path);
        return false;
    }

    // Calculate file_name
    file_name_start++;  // Move past '/', is valid as file_name_end is valid.
    size_t file_name_len = file_name_end - file_name_start;
    std::string file_name(file_name_start, file_name_len);

    // <apk_parent_dir>/oat/<isa>/<file_name>.odex.b
    snprintf(path, PKG_PATH_MAX, "%s/%s/%s.odex.b", oat_dir, instruction_set,
             file_name.c_str());
    return true;
}

/*
 * Computes the odex file for the given apk_path and instruction_set.
 * /system/framework/whatever.jar -> /system/framework/oat/<isa>/whatever.odex
 *
 * Returns false if it failed to determine the odex file path.
 */
bool calculate_odex_file_path(char path[PKG_PATH_MAX], const char *apk_path,
                              const char *instruction_set) {
    if (StringPrintf("%soat/%s/odex.b", apk_path, instruction_set).length() + 1 > PKG_PATH_MAX) {
        ALOGE("apk_path '%s' may be too long to form odex file path.\n", apk_path);
        return false;
    }

    const char *path_end = strrchr(apk_path, '/');
    if (path_end == nullptr) {
        ALOGE("apk_path '%s' has no '/'s in it?!\n", apk_path);
        return false;
    }
    std::string path_component(apk_path, path_end - apk_path);

    const char *name_begin = path_end + 1;
    const char *extension_start = strrchr(name_begin, '.');
    if (extension_start == nullptr) {
        ALOGE("apk_path '%s' has no extension.\n", apk_path);
        return false;
    }
    std::string name_component(name_begin, extension_start - name_begin);

    std::string new_path = StringPrintf("%s/oat/%s/%s.odex.b",
                                        path_component.c_str(),
                                        instruction_set,
                                        name_component.c_str());
    CHECK_LT(new_path.length(), PKG_PATH_MAX);
    strcpy(path, new_path.c_str());
    return true;
}

bool create_cache_path(char path[PKG_PATH_MAX],
                       const char *src,
                       const char *instruction_set) {
    size_t srclen = strlen(src);

        /* demand that we are an absolute path */
    if ((src == 0) || (src[0] != '/') || strstr(src,"..")) {
        return false;
    }

    if (srclen > PKG_PATH_MAX) {        // XXX: PKG_NAME_MAX?
        return false;
    }

    std::string from_src = std::string(src + 1);
    std::replace(from_src.begin(), from_src.end(), '/', '@');

    std::string assembled_path = StringPrintf("%s/%s/%s/%s%s",
                                              OTAPreoptService::kOTADataDirectory,
                                              DALVIK_CACHE,
                                              instruction_set,
                                              from_src.c_str(),
                                              DALVIK_CACHE_POSTFIX2);

    if (assembled_path.length() + 1 > PKG_PATH_MAX) {
        return false;
    }
    strcpy(path, assembled_path.c_str());

    return true;
}

bool initialize_globals() {
    const char* data_path = getenv("ANDROID_DATA");
    if (data_path == nullptr) {
        ALOGE("Could not find ANDROID_DATA");
        return false;
    }
    return init_globals_from_data_and_root(data_path, kOTARootDirectory);
}

static bool initialize_directories() {
    // This is different from the normal installd. We only do the base
    // directory, the rest will be created on demand when each app is compiled.
    mode_t old_umask = umask(0);
    LOG(INFO) << "Old umask: " << old_umask;
    if (access(OTAPreoptService::kOTADataDirectory, R_OK) < 0) {
        ALOGE("Could not access %s\n", OTAPreoptService::kOTADataDirectory);
        return false;
    }
    return true;
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

static int otapreopt_main(const int argc, char *argv[]) {
    int selinux_enabled = (is_selinux_enabled() > 0);

    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv);

    ALOGI("otapreopt firing up\n");

    if (argc < 2) {
        ALOGE("Expecting parameters");
        exit(1);
    }

    union selinux_callback cb;
    cb.func_log = log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);

    if (!initialize_globals()) {
        ALOGE("Could not initialize globals; exiting.\n");
        exit(1);
    }

    if (!initialize_directories()) {
        ALOGE("Could not create directories; exiting.\n");
        exit(1);
    }

    if (selinux_enabled && selinux_status_open(true) < 0) {
        ALOGE("Could not open selinux status; exiting.\n");
        exit(1);
    }

    int ret = android::installd::gOps.Main(argc, argv);

    return ret;
}

}  // namespace installd
}  // namespace android

int main(const int argc, char *argv[]) {
    return android::installd::otapreopt_main(argc, argv);
}
