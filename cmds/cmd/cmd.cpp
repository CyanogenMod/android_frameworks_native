/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "cmd"

#include <utils/Log.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IResultReceiver.h>
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

class MyResultReceiver : public BnResultReceiver
{
public:
    virtual void send(int32_t /*resultCode*/) {
    }
};

int main(int argc, char* const argv[])
{
    signal(SIGPIPE, SIG_IGN);
    sp<ProcessState> proc = ProcessState::self();
    proc->startThreadPool();

    sp<IServiceManager> sm = defaultServiceManager();
    fflush(stdout);
    if (sm == NULL) {
        ALOGE("Unable to get default service manager!");
        aerr << "cmd: Unable to get default service manager!" << endl;
        return 20;
    }

    if (argc == 1) {
        aout << "cmd: no service specified; use -l to list all services" << endl;
        return 20;
    }

    if ((argc == 2) && (strcmp(argv[1], "-l") == 0)) {
        Vector<String16> services = sm->listServices();
        services.sort(sort_func);
        aout << "Currently running services:" << endl;

        for (size_t i=0; i<services.size(); i++) {
            sp<IBinder> service = sm->checkService(services[i]);
            if (service != NULL) {
                aout << "  " << services[i] << endl;
            }
        }
        return 0;
    }

    Vector<String16> args;
    for (int i=2; i<argc; i++) {
        args.add(String16(argv[i]));
    }
    String16 cmd = String16(argv[1]);
    sp<IBinder> service = sm->checkService(cmd);
    if (service == NULL) {
        aerr << "Can't find service: " << argv[1] << endl;
        return 20;
    }

    // TODO: block until a result is returned to MyResultReceiver.
    IBinder::shellCommand(service, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, args,
            new MyResultReceiver());
    return 0;
}
