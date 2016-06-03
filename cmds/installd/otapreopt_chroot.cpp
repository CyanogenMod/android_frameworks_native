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

#include <linux/unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>

#include <installd_constants.h>

#ifndef LOG_TAG
#define LOG_TAG "otapreopt"
#endif

using android::base::StringPrintf;

namespace android {
namespace installd {

static int otapreopt_chroot(const int argc, char **arg) {
    // We need to run the otapreopt tool from the postinstall partition. As such, set up a
    // mount namespace and change root.

    // Create our own mount namespace.
    if (unshare(CLONE_NEWNS) != 0) {
        PLOG(ERROR) << "Failed to unshare() for otapreopt.";
        exit(200);
    }

    // Make postinstall private, so that our changes don't propagate.
    if (mount("", "/postinstall", nullptr, MS_PRIVATE, nullptr) != 0) {
        PLOG(ERROR) << "Failed to mount private.";
        exit(201);
    }

    // Bind mount necessary directories.
    constexpr const char* kBindMounts[] = {
            "/data", "/dev", "/proc", "/sys"
    };
    for (size_t i = 0; i < arraysize(kBindMounts); ++i) {
        std::string trg = StringPrintf("/postinstall%s", kBindMounts[i]);
        if (mount(kBindMounts[i], trg.c_str(), nullptr, MS_BIND, nullptr) != 0) {
            PLOG(ERROR) << "Failed to bind-mount " << kBindMounts[i];
            exit(202);
        }
    }

    // Chdir into /postinstall.
    if (chdir("/postinstall") != 0) {
        PLOG(ERROR) << "Unable to chdir into /postinstall.";
        exit(203);
    }

    // Make /postinstall the root in our mount namespace.
    if (chroot(".")  != 0) {
        PLOG(ERROR) << "Failed to chroot";
        exit(204);
    }

    if (chdir("/") != 0) {
        PLOG(ERROR) << "Unable to chdir into /.";
        exit(205);
    }

    // Now go on and run otapreopt.

    const char* argv[1 + DEXOPT_PARAM_COUNT + 1];
    CHECK_EQ(static_cast<size_t>(argc), DEXOPT_PARAM_COUNT + 1);
    argv[0] = "/system/bin/otapreopt";
    for (size_t i = 1; i <= DEXOPT_PARAM_COUNT; ++i) {
        argv[i] = arg[i];
    }
    argv[DEXOPT_PARAM_COUNT + 1] = nullptr;

    execv(argv[0], (char * const *)argv);
    PLOG(ERROR) << "execv(OTAPREOPT) failed.";
    exit(99);
}

}  // namespace installd
}  // namespace android

int main(const int argc, char *argv[]) {
    return android::installd::otapreopt_chroot(argc, argv);
}
