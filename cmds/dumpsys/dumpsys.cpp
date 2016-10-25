/*
 * Command that dumps interesting system state to the log.
 *
 */

#define LOG_TAG "dumpsys"

#include <algorithm>
#include <chrono>
#include <thread>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/TextOutput.h>
#include <utils/Log.h>
#include <utils/Vector.h>

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace android;
using android::base::StringPrintf;
using android::base::unique_fd;
using android::base::WriteFully;

static int sort_func(const String16* lhs, const String16* rhs)
{
    return lhs->compare(*rhs);
}

static void usage() {
    fprintf(stderr,
        "usage: dumpsys\n"
            "         To dump all services.\n"
            "or:\n"
            "       dumpsys [-t TIMEOUT] [--help | -l | --skip SERVICES | SERVICE [ARGS]]\n"
            "         --help: shows this help\n"
            "         -l: only list services, do not dump them\n"
            "         -t TIMEOUT: TIMEOUT to use in seconds instead of default 10 seconds\n"
            "         --skip SERVICES: dumps all services but SERVICES (comma-separated list)\n"
            "         SERVICE [ARGS]: dumps only service SERVICE, optionally passing ARGS to it\n");
}

bool IsSkipped(const Vector<String16>& skipped, const String16& service) {
    for (const auto& candidate : skipped) {
        if (candidate == service) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* const argv[])
{
    signal(SIGPIPE, SIG_IGN);
    sp<IServiceManager> sm = defaultServiceManager();
    fflush(stdout);
    if (sm == NULL) {
        ALOGE("Unable to get default service manager!");
        aerr << "dumpsys: Unable to get default service manager!" << endl;
        return 20;
    }

    Vector<String16> services;
    Vector<String16> args;
    Vector<String16> skippedServices;
    bool showListOnly = false;
    bool skipServices = false;
    int timeoutArg = 10;
    static struct option longOptions[] = {
        {"skip", no_argument, 0,  0 },
        {"help", no_argument, 0,  0 },
        {     0,           0, 0,  0 }
    };

    while (1) {
        int c;
        int optionIndex = 0;

        c = getopt_long(argc, argv, "+t:l", longOptions, &optionIndex);

        if (c == -1) {
            break;
        }

        switch (c) {
        case 0:
            if (!strcmp(longOptions[optionIndex].name, "skip")) {
                skipServices = true;
            } else if (!strcmp(longOptions[optionIndex].name, "help")) {
                usage();
                return 0;
            }
            break;

        case 't':
            {
                char *endptr;
                timeoutArg = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || timeoutArg <= 0) {
                    fprintf(stderr, "Error: invalid timeout number: '%s'\n", optarg);
                    return -1;
                }
            }
            break;

        case 'l':
            showListOnly = true;
            break;

        default:
            fprintf(stderr, "\n");
            usage();
            return -1;
        }
    }

    for (int i = optind; i < argc; i++) {
        if (skipServices) {
            skippedServices.add(String16(argv[i]));
        } else {
            if (i == optind) {
                services.add(String16(argv[i]));
            } else {
                args.add(String16(argv[i]));
            }
        }
    }

    if ((skipServices && skippedServices.empty()) ||
            (showListOnly && (!services.empty() || !skippedServices.empty()))) {
        usage();
        return -1;
    }

    if (services.empty() || showListOnly) {
        // gets all services
        services = sm->listServices();
        services.sort(sort_func);
        args.add(String16("-a"));
    }

    const size_t N = services.size();

    if (N > 1) {
        // first print a list of the current services
        aout << "Currently running services:" << endl;

        for (size_t i=0; i<N; i++) {
            sp<IBinder> service = sm->checkService(services[i]);
            if (service != NULL) {
                bool skipped = IsSkipped(skippedServices, services[i]);
                aout << "  " << services[i] << (skipped ? " (skipped)" : "") << endl;
            }
        }
    }

    if (showListOnly) {
        return 0;
    }

    for (size_t i = 0; i < N; i++) {
        String16 service_name = std::move(services[i]);
        if (IsSkipped(skippedServices, service_name)) continue;

        sp<IBinder> service = sm->checkService(service_name);
        if (service != NULL) {
            int sfd[2];

            if (pipe(sfd) != 0) {
                aerr << "Failed to create pipe to dump service info for " << service_name
                     << ": " << strerror(errno) << endl;
                continue;
            }

            unique_fd local_end(sfd[0]);
            unique_fd remote_end(sfd[1]);
            sfd[0] = sfd[1] = -1;

            if (N > 1) {
                aout << "------------------------------------------------------------"
                        "-------------------" << endl;
                aout << "DUMP OF SERVICE " << service_name << ":" << endl;
            }

            // dump blocks until completion, so spawn a thread..
            std::thread dump_thread([=, remote_end { std::move(remote_end) }]() mutable {
                int err = service->dump(remote_end.get(), args);

                // It'd be nice to be able to close the remote end of the socketpair before the dump
                // call returns, to terminate our reads if the other end closes their copy of the
                // file descriptor, but then hangs for some reason. There doesn't seem to be a good
                // way to do this, though.
                remote_end.clear();

                if (err != 0) {
                    aerr << "Error dumping service info: (" << strerror(err) << ") " << service_name
                         << endl;
                }
            });

            auto timeout = std::chrono::seconds(timeoutArg);
            auto start = std::chrono::steady_clock::now();
            auto end = start + timeout;

            struct pollfd pfd = {
                .fd = local_end.get(),
                .events = POLLIN
            };

            bool timed_out = false;
            bool error = false;
            while (true) {
                // Wrap this in a lambda so that TEMP_FAILURE_RETRY recalculates the timeout.
                auto time_left_ms = [end]() {
                    auto now = std::chrono::steady_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
                    return std::max(diff.count(), 0ll);
                };

                int rc = TEMP_FAILURE_RETRY(poll(&pfd, 1, time_left_ms()));
                if (rc < 0) {
                    aerr << "Error in poll while dumping service " << service_name << " : "
                         << strerror(errno) << endl;
                    error = true;
                    break;
                } else if (rc == 0) {
                    timed_out = true;
                    break;
                }

                char buf[4096];
                rc = TEMP_FAILURE_RETRY(read(local_end.get(), buf, sizeof(buf)));
                if (rc < 0) {
                    aerr << "Failed to read while dumping service " << service_name << ": "
                         << strerror(errno) << endl;
                    error = true;
                    break;
                } else if (rc == 0) {
                    // EOF.
                    break;
                }

                if (!WriteFully(STDOUT_FILENO, buf, rc)) {
                    aerr << "Failed to write while dumping service " << service_name << ": "
                         << strerror(errno) << endl;
                    error = true;
                    break;
                }
            }

            if (timed_out) {
                aout << endl << "*** SERVICE DUMP TIMEOUT EXPIRED ***" << endl << endl;
            }

            if (timed_out || error) {
                dump_thread.detach();
            } else {
                dump_thread.join();
            }

            if (N > 1) {
              std::chrono::duration<double> elapsed_seconds =
                  std::chrono::steady_clock::now() - start;
              aout << StringPrintf("--------- %.3fs ", elapsed_seconds.count()).c_str()
                   << "was the duration of dumpsys " << service_name << endl;
            }
        } else {
            aerr << "Can't find service: " << service_name << endl;
        }
    }

    return 0;
}
