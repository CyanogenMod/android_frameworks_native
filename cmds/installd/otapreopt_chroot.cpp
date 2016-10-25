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

#include <fcntl.h>
#include <linux/unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <sstream>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>

#include <commands.h>
#include <otapreopt_utils.h>

#ifndef LOG_TAG
#define LOG_TAG "otapreopt"
#endif

using android::base::StringPrintf;

namespace android {
namespace installd {

static void CloseDescriptor(int fd) {
    if (fd >= 0) {
        int result = close(fd);
        UNUSED(result);  // Ignore result. Printing to logcat will open a new descriptor
                         // that we do *not* want.
    }
}

static void CloseDescriptor(const char* descriptor_string) {
    int fd = -1;
    std::istringstream stream(descriptor_string);
    stream >> fd;
    if (!stream.fail()) {
        CloseDescriptor(fd);
    }
}

// Entry for otapreopt_chroot. Expected parameters are:
//   [cmd] [status-fd] [target-slot] "dexopt" [dexopt-params]
// The file descriptor denoted by status-fd will be closed. The rest of the parameters will
// be passed on to otapreopt in the chroot.
static int otapreopt_chroot(const int argc, char **arg) {
    // Close all file descriptors. They are coming from the caller, we do not want to pass them
    // on across our fork/exec into a different domain.
    // 1) Default descriptors.
    CloseDescriptor(STDIN_FILENO);
    CloseDescriptor(STDOUT_FILENO);
    CloseDescriptor(STDERR_FILENO);
    // 2) The status channel.
    CloseDescriptor(arg[1]);

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

    // Try to mount the vendor partition. update_engine doesn't do this for us, but we
    // want it for vendor APKs.
    // Notes:
    //  1) We pretty much guess a name here and hope to find the partition by name.
    //     It is just as complicated and brittle to scan /proc/mounts. But this requires
    //     validating the target-slot so as not to try to mount some totally random path.
    //  2) We're in a mount namespace here, so when we die, this will be cleaned up.
    //  3) Ignore errors. Printing anything at this stage will open a file descriptor
    //     for logging.
    if (!ValidateTargetSlotSuffix(arg[2])) {
        LOG(ERROR) << "Target slot suffix not legal: " << arg[2];
        exit(207);
    }
    std::string vendor_partition = StringPrintf("/dev/block/bootdevice/by-name/vendor%s",
                                                arg[2]);
    int vendor_result = mount(vendor_partition.c_str(),
                              "/postinstall/vendor",
                              "ext4",
                              MS_RDONLY,
                              /* data */ nullptr);
    UNUSED(vendor_result);

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

    // Incoming:  cmd + status-fd + target-slot + "dexopt" + dexopt-params + null
    // Outgoing:  cmd             + target-slot + "dexopt" + dexopt-params + null
    constexpr size_t kInArguments =   1                       // Binary name.
                                    + 1                       // status file descriptor.
                                    + 1                       // target-slot.
                                    + 1                       // "dexopt."
                                    + DEXOPT_PARAM_COUNT      // dexopt parameters.
                                    + 1;                      // null termination.
    constexpr size_t kOutArguments =   1                       // Binary name.
                                     + 1                       // target-slot.
                                     + 1                       // "dexopt."
                                     + DEXOPT_PARAM_COUNT      // dexopt parameters.
                                     + 1;                      // null termination.
    const char* argv[kOutArguments];
    if (static_cast<size_t>(argc) !=  kInArguments - 1 /* null termination */) {
        LOG(ERROR) << "Unexpected argument size "
                   << argc
                   << " vs "
                   << (kInArguments - 1);
        for (size_t i = 0; i < static_cast<size_t>(argc); ++i) {
            if (arg[i] == nullptr) {
                LOG(ERROR) << "(null)";
            } else {
                LOG(ERROR) << "\"" << arg[i] << "\"";
            }
        }
        exit(206);
    }
    argv[0] = "/system/bin/otapreopt";

    // The first parameter is the status file descriptor, skip.

    for (size_t i = 1; i <= kOutArguments - 2 /* cmd + null */; ++i) {
        argv[i] = arg[i + 1];
    }
    argv[kOutArguments - 1] = nullptr;

    execv(argv[0], (char * const *)argv);
    PLOG(ERROR) << "execv(OTAPREOPT) failed.";
    exit(99);
}

}  // namespace installd
}  // namespace android

int main(const int argc, char *argv[]) {
    return android::installd::otapreopt_chroot(argc, argv);
}
