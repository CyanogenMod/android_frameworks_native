/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include <time.h>
#include <zlib.h>

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

/* Command line options */
static int g_traceDurationSeconds = 5;
static bool g_traceSchedSwitch = false;
static bool g_traceCpuFrequency = false;
static bool g_traceCpuIdle = false;
static bool g_traceDisk = false;
static bool g_traceGovernorLoad = false;
static bool g_traceWorkqueue = false;
static bool g_traceOverwrite = false;
static int g_traceBufferSizeKB = 2048;
static bool g_compress = false;

/* Global state */
static bool g_traceAborted = false;

/* Sys file paths */
static const char* k_traceClockPath =
    "/sys/kernel/debug/tracing/trace_clock";

static const char* k_traceBufferSizePath =
    "/sys/kernel/debug/tracing/buffer_size_kb";

static const char* k_tracingOverwriteEnablePath =
    "/sys/kernel/debug/tracing/options/overwrite";

static const char* k_schedSwitchEnablePath =
    "/sys/kernel/debug/tracing/events/sched/sched_switch/enable";

static const char* k_schedWakeupEnablePath =
    "/sys/kernel/debug/tracing/events/sched/sched_wakeup/enable";

static const char* k_cpuFreqEnablePath =
    "/sys/kernel/debug/tracing/events/power/cpu_frequency/enable";

static const char* k_cpuIdleEnablePath =
    "/sys/kernel/debug/tracing/events/power/cpu_idle/enable";

static const char* k_governorLoadEnablePath =
    "/sys/kernel/debug/tracing/events/cpufreq_interactive/enable";

static const char* k_workqueueEnablePath =
    "/sys/kernel/debug/tracing/events/workqueue/enable";

static const char* k_diskEnablePaths[] = {
        "/sys/kernel/debug/tracing/events/ext4/ext4_sync_file_enter/enable",
        "/sys/kernel/debug/tracing/events/ext4/ext4_sync_file_exit/enable",
        "/sys/kernel/debug/tracing/events/block/block_rq_issue/enable",
        "/sys/kernel/debug/tracing/events/block/block_rq_complete/enable",
};

static const char* k_tracingOnPath =
    "/sys/kernel/debug/tracing/tracing_on";

static const char* k_tracePath =
    "/sys/kernel/debug/tracing/trace";

static const char* k_traceMarkerPath =
    "/sys/kernel/debug/tracing/trace_marker";

// Write a string to a file, returning true if the write was successful.
bool writeStr(const char* filename, const char* str)
{
    int fd = open(filename, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "error opening %s: %s (%d)\n", filename,
                strerror(errno), errno);
        return false;
    }

    bool ok = true;
    ssize_t len = strlen(str);
    if (write(fd, str, len) != len) {
        fprintf(stderr, "error writing to %s: %s (%d)\n", filename,
                strerror(errno), errno);
        ok = false;
    }

    close(fd);

    return ok;
}

// Enable or disable a kernel option by writing a "1" or a "0" into a /sys file.
static bool setKernelOptionEnable(const char* filename, bool enable)
{
    return writeStr(filename, enable ? "1" : "0");
}

// Enable or disable a collection of kernel options by writing a "1" or a "0" into each /sys file.
static bool setMultipleKernelOptionsEnable(const char** filenames, size_t count, bool enable)
{
    bool result = true;
    for (size_t i = 0; i < count; i++) {
        result &= setKernelOptionEnable(filenames[i], enable);
    }
    return result;
}

// Enable or disable overwriting of the kernel trace buffers.  Disabling this
// will cause tracing to stop once the trace buffers have filled up.
static bool setTraceOverwriteEnable(bool enable)
{
    return setKernelOptionEnable(k_tracingOverwriteEnablePath, enable);
}

// Enable or disable tracing of the kernel scheduler switching.
static bool setSchedSwitchTracingEnable(bool enable)
{
    bool ok = true;
    ok &= setKernelOptionEnable(k_schedSwitchEnablePath, enable);
    ok &= setKernelOptionEnable(k_schedWakeupEnablePath, enable);
    return ok;
}

// Enable or disable tracing of the CPU clock frequency.
static bool setCpuFrequencyTracingEnable(bool enable)
{
    return setKernelOptionEnable(k_cpuFreqEnablePath, enable);
}

// Enable or disable tracing of CPU idle events.
static bool setCpuIdleTracingEnable(bool enable)
{
    return setKernelOptionEnable(k_cpuIdleEnablePath, enable);
}

// Enable or disable tracing of the interactive CPU frequency governor's idea of
// the CPU load.
static bool setGovernorLoadTracingEnable(bool enable)
{
    return setKernelOptionEnable(k_governorLoadEnablePath, enable);
}

// Enable or disable tracing of the kernel workqueues.
static bool setWorkqueueTracingEnabled(bool enable)
{
    return setKernelOptionEnable(k_workqueueEnablePath, enable);
}

// Enable or disable tracing of disk I/O.
static bool setDiskTracingEnabled(bool enable)
{
    return setMultipleKernelOptionsEnable(k_diskEnablePaths, NELEM(k_diskEnablePaths), enable);
}

// Enable or disable kernel tracing.
static bool setTracingEnabled(bool enable)
{
    return setKernelOptionEnable(k_tracingOnPath, enable);
}

// Clear the contents of the kernel trace.
static bool clearTrace()
{
    int traceFD = creat(k_tracePath, 0);
    if (traceFD == -1) {
        fprintf(stderr, "error truncating %s: %s (%d)\n", k_tracePath,
                strerror(errno), errno);
        return false;
    }

    close(traceFD);

    return true;
}

// Set the size of the kernel's trace buffer in kilobytes.
static bool setTraceBufferSizeKB(int size)
{
    char str[32] = "1";
    int len;
    if (size < 1) {
        size = 1;
    }
    snprintf(str, 32, "%d", size);
    return writeStr(k_traceBufferSizePath, str);
}

// Enable or disable the kernel's use of the global clock.  Disabling the global
// clock will result in the kernel using a per-CPU local clock.
static bool setGlobalClockEnable(bool enable)
{
    return writeStr(k_traceClockPath, enable ? "global" : "local");
}

// Check whether a file exists.
static bool fileExists(const char* filename) {
    return access(filename, F_OK) != -1;
}

// Enable tracing in the kernel.
static bool startTrace(bool isRoot)
{
    bool ok = true;

    // Set up the tracing options that don't require root.
    ok &= setTraceOverwriteEnable(g_traceOverwrite);
    ok &= setSchedSwitchTracingEnable(g_traceSchedSwitch);
    ok &= setCpuFrequencyTracingEnable(g_traceCpuFrequency);
    ok &= setCpuIdleTracingEnable(g_traceCpuIdle);
    if (fileExists(k_governorLoadEnablePath) || g_traceGovernorLoad) {
        ok &= setGovernorLoadTracingEnable(g_traceGovernorLoad);
    }
    ok &= setTraceBufferSizeKB(g_traceBufferSizeKB);
    ok &= setGlobalClockEnable(true);

    // Set up the tracing options that do require root.  The options that
    // require root should have errored out earlier if we're not running as
    // root.
    if (isRoot) {
        ok &= setWorkqueueTracingEnabled(g_traceWorkqueue);
        ok &= setDiskTracingEnabled(g_traceDisk);
    }

    // Enable tracing.
    ok &= setTracingEnabled(true);

    if (!ok) {
        fprintf(stderr, "error: unable to start trace\n");
    }

    return ok;
}

// Disable tracing in the kernel.
static void stopTrace(bool isRoot)
{
    // Disable tracing.
    setTracingEnabled(false);

    // Set the options back to their defaults.
    setTraceOverwriteEnable(true);
    setSchedSwitchTracingEnable(false);
    setCpuFrequencyTracingEnable(false);
    if (fileExists(k_governorLoadEnablePath)) {
        setGovernorLoadTracingEnable(false);
    }
    setGlobalClockEnable(false);

    if (isRoot) {
        setWorkqueueTracingEnabled(false);
        setDiskTracingEnabled(false);
    }

    // Note that we can't reset the trace buffer size here because that would
    // clear the trace before we've read it.
}

// Read the current kernel trace and write it to stdout.
static void dumpTrace()
{
    int traceFD = open(k_tracePath, O_RDWR);
    if (traceFD == -1) {
        fprintf(stderr, "error opening %s: %s (%d)\n", k_tracePath,
                strerror(errno), errno);
        return;
    }

    if (g_compress) {
        z_stream zs;
        uint8_t *in, *out;
        int result, flush;

        bzero(&zs, sizeof(zs));
        result = deflateInit(&zs, Z_DEFAULT_COMPRESSION);
        if (result != Z_OK) {
            fprintf(stderr, "error initializing zlib: %d\n", result);
            close(traceFD);
            return;
        }

        const size_t bufSize = 64*1024;
        in = (uint8_t*)malloc(bufSize);
        out = (uint8_t*)malloc(bufSize);
        flush = Z_NO_FLUSH;

        zs.next_out = out;
        zs.avail_out = bufSize;

        do {

            if (zs.avail_in == 0) {
                // More input is needed.
                result = read(traceFD, in, bufSize);
                if (result < 0) {
                    fprintf(stderr, "error reading trace: %s (%d)\n",
                            strerror(errno), errno);
                    result = Z_STREAM_END;
                    break;
                } else if (result == 0) {
                    flush = Z_FINISH;
                } else {
                    zs.next_in = in;
                    zs.avail_in = result;
                }
            }

            if (zs.avail_out == 0) {
                // Need to write the output.
                result = write(STDOUT_FILENO, out, bufSize);
                if ((size_t)result < bufSize) {
                    fprintf(stderr, "error writing deflated trace: %s (%d)\n",
                            strerror(errno), errno);
                    result = Z_STREAM_END; // skip deflate error message
                    zs.avail_out = bufSize; // skip the final write
                    break;
                }
                zs.next_out = out;
                zs.avail_out = bufSize;
            }

        } while ((result = deflate(&zs, flush)) == Z_OK);

        if (result != Z_STREAM_END) {
            fprintf(stderr, "error deflating trace: %s\n", zs.msg);
        }

        if (zs.avail_out < bufSize) {
            size_t bytes = bufSize - zs.avail_out;
            result = write(STDOUT_FILENO, out, bytes);
            if ((size_t)result < bytes) {
                fprintf(stderr, "error writing deflated trace: %s (%d)\n",
                        strerror(errno), errno);
            }
        }

        result = deflateEnd(&zs);
        if (result != Z_OK) {
            fprintf(stderr, "error cleaning up zlib: %d\n", result);
        }

        free(in);
        free(out);
    } else {
        ssize_t sent = 0;
        while ((sent = sendfile(STDOUT_FILENO, traceFD, NULL, 64*1024*1024)) > 0);
        if (sent == -1) {
            fprintf(stderr, "error dumping trace: %s (%d)\n", strerror(errno),
                    errno);
        }
    }

    close(traceFD);
}

// Print the command usage help to stderr.
static void showHelp(const char *cmd)
{
    fprintf(stderr, "usage: %s [options]\n", cmd);
    fprintf(stderr, "options include:\n"
                    "  -b N            use a trace buffer size of N KB\n"
                    "  -c              trace into a circular buffer\n"
                    "  -d              trace disk I/O\n"
                    "  -f              trace CPU frequency changes\n"
                    "  -l              trace CPU frequency governor load\n"
                    "  -s              trace the kernel scheduler switches\n"
                    "  -t N            trace for N seconds [defualt 5]\n"
                    "  -w              trace the kernel workqueue\n"
                    "  -z              compress the trace dump\n");
}

static void handleSignal(int signo) {
    g_traceAborted = true;
}

static void registerSigHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handleSignal;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main(int argc, char **argv)
{
    bool isRoot = (getuid() == 0);

    if (argc == 2 && 0 == strcmp(argv[1], "--help")) {
        showHelp(argv[0]);
        exit(0);
    }

    for (;;) {
        int ret;

        ret = getopt(argc, argv, "b:cidflst:wz");

        if (ret < 0) {
            break;
        }

        switch(ret) {
            case 'b':
                g_traceBufferSizeKB = atoi(optarg);
            break;

            case 'c':
                g_traceOverwrite = true;
            break;

            case 'i':
                g_traceCpuIdle = true;
            break;

            case 'l':
                g_traceGovernorLoad = true;
            break;

            case 'd':
                if (!isRoot) {
                    fprintf(stderr, "error: tracing disk activity requires root privileges\n");
                    exit(1);
                }
                g_traceDisk = true;
            break;

            case 'f':
                g_traceCpuFrequency = true;
            break;

            case 's':
                g_traceSchedSwitch = true;
            break;

            case 't':
                g_traceDurationSeconds = atoi(optarg);
            break;

            case 'w':
                if (!isRoot) {
                    fprintf(stderr, "error: tracing kernel work queues requires root privileges\n");
                    exit(1);
                }
                g_traceWorkqueue = true;
            break;

            case 'z':
                g_compress = true;
            break;

            default:
                fprintf(stderr, "\n");
                showHelp(argv[0]);
                exit(-1);
            break;
        }
    }

    registerSigHandler();

    bool ok = startTrace(isRoot);

    if (ok) {
        printf("capturing trace...");
        fflush(stdout);

        // We clear the trace after starting it because tracing gets enabled for
        // each CPU individually in the kernel. Having the beginning of the trace
        // contain entries from only one CPU can cause "begin" entries without a
        // matching "end" entry to show up if a task gets migrated from one CPU to
        // another.
        ok = clearTrace();

        if (ok) {
            // Sleep to allow the trace to be captured.
            struct timespec timeLeft;
            timeLeft.tv_sec = g_traceDurationSeconds;
            timeLeft.tv_nsec = 0;
            do {
                if (g_traceAborted) {
                    break;
                }
            } while (nanosleep(&timeLeft, &timeLeft) == -1 && errno == EINTR);
        }
    }

    // Stop the trace and restore the default settings.
    stopTrace(isRoot);

    if (ok) {
        if (!g_traceAborted) {
            printf(" done\nTRACE:\n");
            fflush(stdout);
            dumpTrace();
        } else {
            printf("\ntrace aborted.\n");
            fflush(stdout);
        }
        clearTrace();
    } else {
        fprintf(stderr, "unable to start tracing\n");
    }

    // Reset the trace buffer size to 1.
    setTraceBufferSizeKB(1);

    return g_traceAborted ? 1 : 0;
}
