/*
 * Command that dumps interesting system state to the log.
 *
 */

#define LOG_TAG "dumpsys"

#include <utils/Log.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/TextOutput.h>
#include <utils/Vector.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

using namespace android;

static int sort_func(const String16* lhs, const String16* rhs)
{
    return lhs->compare(*rhs);
}

static void usage() {
    fprintf(stderr,
        "usage: dumpsys\n"
            "         To dump all services.\n"
            "or:\n"
            "       dumpsys [--help | -l | --skip SERVICES | SERVICE [ARGS]]\n"
            "         --help: shows this help\n"
            "         -l: only list services, do not dump them\n"
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
    if (argc == 2) {
        // 1 argument: check for special cases (-l or --help)
        if (strcmp(argv[1], "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[1], "-l") == 0) {
            showListOnly = true;
        }
    }
    if (argc == 3) {
        // 2 arguments: check for special cases (--skip SERVICES)
        if (strcmp(argv[1], "--skip") == 0) {
            char* token = strtok(argv[2], ",");
            while (token != NULL) {
                skippedServices.add(String16(token));
                token = strtok(NULL, ",");
            }
        }
    }
    bool dumpAll = argc == 1;
    if (dumpAll || !skippedServices.empty() || showListOnly) {
        // gets all services
        services = sm->listServices();
        services.sort(sort_func);
        args.add(String16("-a"));
    } else {
        // gets a specific service:
        // first check if its name is not a special argument...
        if (strcmp(argv[1], "--skip") == 0 || strcmp(argv[1], "-l") == 0) {
            usage();
            return -1;
        }
        // ...then gets its arguments
        services.add(String16(argv[1]));
        for (int i=2; i<argc; i++) {
            args.add(String16(argv[i]));
        }
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

    for (size_t i=0; i<N; i++) {
        if (IsSkipped(skippedServices, services[i])) continue;

        sp<IBinder> service = sm->checkService(services[i]);
        if (service != NULL) {
            if (N > 1) {
                aout << "------------------------------------------------------------"
                        "-------------------" << endl;
                aout << "DUMP OF SERVICE " << services[i] << ":" << endl;
            }
            int err = service->dump(STDOUT_FILENO, args);
            if (err != 0) {
                aerr << "Error dumping service info: (" << strerror(err)
                        << ") " << services[i] << endl;
            }
        } else {
            aerr << "Can't find service: " << services[i] << endl;
        }
    }

    return 0;
}
