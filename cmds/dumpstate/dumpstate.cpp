/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <memory>
#include <regex>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <android-base/stringprintf.h>
#include <cutils/properties.h>

#include "private/android_filesystem_config.h"

#define LOG_TAG "dumpstate"
#include <cutils/log.h>

#include "dumpstate.h"
#include "ScopedFd.h"
#include "ziparchive/zip_writer.h"

using android::base::StringPrintf;

/* read before root is shed */
static char cmdline_buf[16384] = "(unknown)";
static const char *dump_traces_path = NULL;

// TODO: should be part of dumpstate object
static char build_type[PROPERTY_VALUE_MAX];
static time_t now;
static std::unique_ptr<ZipWriter> zip_writer;

#define PSTORE_LAST_KMSG "/sys/fs/pstore/console-ramoops"

#define RAFT_DIR "/data/misc/raft/"
#define RECOVERY_DIR "/cache/recovery"
#define TOMBSTONE_DIR "/data/tombstones"
#define TOMBSTONE_FILE_PREFIX TOMBSTONE_DIR "/tombstone_"
/* Can accomodate a tombstone number up to 9999. */
#define TOMBSTONE_MAX_LEN (sizeof(TOMBSTONE_FILE_PREFIX) + 4)
#define NUM_TOMBSTONES  10

typedef struct {
  char name[TOMBSTONE_MAX_LEN];
  int fd;
} tombstone_data_t;

static tombstone_data_t tombstone_data[NUM_TOMBSTONES];

// Root dir for all files copied as-is into the bugreport
const std::string& ZIP_ROOT_DIR = "FS";

/* gets the tombstone data, according to the bugreport type: if zipped gets all tombstones,
 * otherwise gets just those modified in the last half an hour. */
static void get_tombstone_fds(tombstone_data_t data[NUM_TOMBSTONES]) {
    time_t thirty_minutes_ago = now - 60*30;
    for (size_t i = 0; i < NUM_TOMBSTONES; i++) {
        snprintf(data[i].name, sizeof(data[i].name), "%s%02zu", TOMBSTONE_FILE_PREFIX, i);
        int fd = TEMP_FAILURE_RETRY(open(data[i].name,
                                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        struct stat st;
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
            (zip_writer || (time_t) st.st_mtime >= thirty_minutes_ago)) {
        data[i].fd = fd;
        } else {
        close(fd);
            data[i].fd = -1;
        }
    }
}

static void dump_dev_files(const char *title, const char *driverpath, const char *filename)
{
    DIR *d;
    struct dirent *de;
    char path[PATH_MAX];

    d = opendir(driverpath);
    if (d == NULL) {
        return;
    }

    while ((de = readdir(d))) {
        if (de->d_type != DT_LNK) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s/%s", driverpath, de->d_name, filename);
        dump_file(title, path);
    }

    closedir(d);
}

static bool skip_not_stat(const char *path) {
    static const char stat[] = "/stat";
    size_t len = strlen(path);
    if (path[len - 1] == '/') { /* Directory? */
        return false;
    }
    return strcmp(path + len - sizeof(stat) + 1, stat); /* .../stat? */
}

static bool skip_none(const char *path) {
    return false;
}

static const char mmcblk0[] = "/sys/block/mmcblk0/";
unsigned long worst_write_perf = 20000; /* in KB/s */

static int dump_stat_from_fd(const char *title __unused, const char *path, int fd) {
    unsigned long fields[11], read_perf, write_perf;
    bool z;
    char *cp, *buffer = NULL;
    size_t i = 0;
    FILE *fp = fdopen(fd, "rb");
    getline(&buffer, &i, fp);
    fclose(fp);
    if (!buffer) {
        return -errno;
    }
    i = strlen(buffer);
    while ((i > 0) && (buffer[i - 1] == '\n')) {
        buffer[--i] = '\0';
    }
    if (!*buffer) {
        free(buffer);
        return 0;
    }
    z = true;
    for (cp = buffer, i = 0; i < (sizeof(fields) / sizeof(fields[0])); ++i) {
        fields[i] = strtol(cp, &cp, 0);
        if (fields[i] != 0) {
            z = false;
        }
    }
    if (z) { /* never accessed */
        free(buffer);
        return 0;
    }

    if (!strncmp(path, mmcblk0, sizeof(mmcblk0) - 1)) {
        path += sizeof(mmcblk0) - 1;
    }

    printf("%s: %s\n", path, buffer);
    free(buffer);

    read_perf = 0;
    if (fields[3]) {
        read_perf = 512 * fields[2] / fields[3];
    }
    write_perf = 0;
    if (fields[7]) {
        write_perf = 512 * fields[6] / fields[7];
    }
    printf("%s: read: %luKB/s write: %luKB/s\n", path, read_perf, write_perf);
    if ((write_perf > 1) && (write_perf < worst_write_perf)) {
        worst_write_perf = write_perf;
    }
    return 0;
}

/* Copied policy from system/core/logd/LogBuffer.cpp */

#define LOG_BUFFER_SIZE (256 * 1024)
#define LOG_BUFFER_MIN_SIZE (64 * 1024UL)
#define LOG_BUFFER_MAX_SIZE (256 * 1024 * 1024UL)

static bool valid_size(unsigned long value) {
    if ((value < LOG_BUFFER_MIN_SIZE) || (LOG_BUFFER_MAX_SIZE < value)) {
        return false;
    }

    long pages = sysconf(_SC_PHYS_PAGES);
    if (pages < 1) {
        return true;
    }

    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize <= 1) {
        pagesize = PAGE_SIZE;
    }

    // maximum memory impact a somewhat arbitrary ~3%
    pages = (pages + 31) / 32;
    unsigned long maximum = pages * pagesize;

    if ((maximum < LOG_BUFFER_MIN_SIZE) || (LOG_BUFFER_MAX_SIZE < maximum)) {
        return true;
    }

    return value <= maximum;
}

static unsigned long property_get_size(const char *key) {
    unsigned long value;
    char *cp, property[PROPERTY_VALUE_MAX];

    property_get(key, property, "");
    value = strtoul(property, &cp, 10);

    switch(*cp) {
    case 'm':
    case 'M':
        value *= 1024;
    /* FALLTHRU */
    case 'k':
    case 'K':
        value *= 1024;
    /* FALLTHRU */
    case '\0':
        break;

    default:
        value = 0;
    }

    if (!valid_size(value)) {
        value = 0;
    }

    return value;
}

/* timeout in ms */
static unsigned long logcat_timeout(const char *name) {
    static const char global_tuneable[] = "persist.logd.size"; // Settings App
    static const char global_default[] = "ro.logd.size";       // BoardConfig.mk
    char key[PROP_NAME_MAX];
    unsigned long property_size, default_size;

    default_size = property_get_size(global_tuneable);
    if (!default_size) {
        default_size = property_get_size(global_default);
    }

    snprintf(key, sizeof(key), "%s.%s", global_tuneable, name);
    property_size = property_get_size(key);

    if (!property_size) {
        snprintf(key, sizeof(key), "%s.%s", global_default, name);
        property_size = property_get_size(key);
    }

    if (!property_size) {
        property_size = default_size;
    }

    if (!property_size) {
        property_size = LOG_BUFFER_SIZE;
    }

    /* Engineering margin is ten-fold our guess */
    return 10 * (property_size + worst_write_perf) / worst_write_perf;
}

/* End copy from system/core/logd/LogBuffer.cpp */

/* dumps the current system state to stdout */
static void print_header() {
    char build[PROPERTY_VALUE_MAX], fingerprint[PROPERTY_VALUE_MAX];
    char radio[PROPERTY_VALUE_MAX], bootloader[PROPERTY_VALUE_MAX];
    char network[PROPERTY_VALUE_MAX], date[80];

    property_get("ro.build.display.id", build, "(unknown)");
    property_get("ro.build.fingerprint", fingerprint, "(unknown)");
    property_get("ro.build.type", build_type, "(unknown)");
    property_get("ro.baseband", radio, "(unknown)");
    property_get("ro.bootloader", bootloader, "(unknown)");
    property_get("gsm.operator.alpha", network, "(unknown)");
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("========================================================\n");
    printf("== dumpstate: %s\n", date);
    printf("========================================================\n");

    printf("\n");
    printf("Build: %s\n", build);
    printf("Build fingerprint: '%s'\n", fingerprint); /* format is important for other tools */
    printf("Bootloader: %s\n", bootloader);
    printf("Radio: %s\n", radio);
    printf("Network: %s\n", network);

    printf("Kernel: ");
    dump_file(NULL, "/proc/version");
    printf("Command line: %s\n", strtok(cmdline_buf, "\n"));
    printf("\n");
}

/* adds a new entry to the existing zip file. */
static bool add_zip_entry_from_fd(const std::string& entry_name, int fd) {
    int32_t err = zip_writer->StartEntryWithTime(entry_name.c_str(),
            ZipWriter::kCompress, get_mtime(fd, now));
    if (err) {
        ALOGE("zip_writer->StartEntryWithTime(%s): %s\n", entry_name.c_str(), ZipWriter::ErrorCodeString(err));
        return false;
    }

    while (1) {
        std::vector<uint8_t> buffer(65536);
        ssize_t bytes_read = TEMP_FAILURE_RETRY(read(fd, buffer.data(), sizeof(buffer)));
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            ALOGE("read(%s): %s\n", entry_name.c_str(), strerror(errno));
            return false;
        }
        err = zip_writer->WriteBytes(buffer.data(), bytes_read);
        if (err) {
            ALOGE("zip_writer->WriteBytes(): %s\n", ZipWriter::ErrorCodeString(err));
            return false;
        }
    }

    err = zip_writer->FinishEntry();
    if (err) {
        ALOGE("zip_writer->FinishEntry(): %s\n", ZipWriter::ErrorCodeString(err));
        return false;
    }

    return true;
}

/* adds a new entry to the existing zip file. */
static bool add_zip_entry(const std::string& entry_name, const std::string& entry_path) {
    ScopedFd fd(TEMP_FAILURE_RETRY(open(entry_path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)));
    if (fd.get() == -1) {
        ALOGE("open(%s): %s\n", entry_path.c_str(), strerror(errno));
        return false;
    }

    return add_zip_entry_from_fd(entry_name, fd.get());
}

/* adds a file to the existing zipped bugreport */
static int _add_file_from_fd(const char *title, const char *path, int fd) {
    return add_zip_entry_from_fd(ZIP_ROOT_DIR + path, fd) ? 0 : 1;
}

/* adds all files from a directory to the zipped bugreport file */
void add_dir(const char *dir, bool recursive) {
    if (!zip_writer) return;
    DurationReporter duration_reporter(dir);
    dump_files(NULL, dir, recursive ? skip_none : is_dir, _add_file_from_fd);
}

static void dumpstate(const std::string& screenshot_path) {
    std::unique_ptr<DurationReporter> duration_reporter(new DurationReporter("DUMPSTATE"));
    unsigned long timeout;

    dump_dev_files("TRUSTY VERSION", "/sys/bus/platform/drivers/trusty", "trusty_version");
    run_command("UPTIME", 10, "uptime", NULL);
    dump_files("UPTIME MMC PERF", mmcblk0, skip_not_stat, dump_stat_from_fd);
    dump_emmc_ecsd("/d/mmc0/mmc0:0001/ext_csd");
    dump_file("MEMORY INFO", "/proc/meminfo");
    run_command("CPU INFO", 10, "top", "-n", "1", "-d", "1", "-m", "30", "-H", NULL);
    run_command("PROCRANK", 20, SU_PATH, "root", "procrank", NULL);
    dump_file("VIRTUAL MEMORY STATS", "/proc/vmstat");
    dump_file("VMALLOC INFO", "/proc/vmallocinfo");
    dump_file("SLAB INFO", "/proc/slabinfo");
    dump_file("ZONEINFO", "/proc/zoneinfo");
    dump_file("PAGETYPEINFO", "/proc/pagetypeinfo");
    dump_file("BUDDYINFO", "/proc/buddyinfo");
    dump_file("FRAGMENTATION INFO", "/d/extfrag/unusable_index");

    dump_file("KERNEL WAKELOCKS", "/proc/wakelocks");
    dump_file("KERNEL WAKE SOURCES", "/d/wakeup_sources");
    dump_file("KERNEL CPUFREQ", "/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state");
    dump_file("KERNEL SYNC", "/d/sync");

    run_command("PROCESSES AND THREADS", 10, "ps", "-Z", "-t", "-p", "-P", NULL);
    run_command("LIBRANK", 10, SU_PATH, "root", "librank", NULL);

    do_dmesg();

    run_command("LIST OF OPEN FILES", 10, SU_PATH, "root", "lsof", NULL);
    for_each_pid(do_showmap, "SMAPS OF ALL PROCESSES");
    for_each_tid(show_wchan, "BLOCKED PROCESS WAIT-CHANNELS");

    if (!screenshot_path.empty()) {
        ALOGI("taking late screenshot\n");
        take_screenshot(screenshot_path);
        ALOGI("wrote screenshot: %s\n", screenshot_path.c_str());
    }

    // dump_file("EVENT LOG TAGS", "/etc/event-log-tags");
    // calculate timeout
    timeout = logcat_timeout("main") + logcat_timeout("system") + logcat_timeout("crash");
    if (timeout < 20000) {
        timeout = 20000;
    }
    run_command("SYSTEM LOG", timeout / 1000, "logcat", "-v", "threadtime",
                                                        "-v", "printable",
                                                        "-d",
                                                        "*:v", NULL);
    timeout = logcat_timeout("events") + logcat_timeout("security");
    if (timeout < 20000) {
        timeout = 20000;
    }
    run_command("EVENT LOG", timeout / 1000, "logcat", "-b", "events",
                                                       "-b", "security",
                                                       "-v", "threadtime",
                                                       "-v", "printable",
                                                       "-d",
                                                       "*:v", NULL);
    timeout = logcat_timeout("radio");
    if (timeout < 20000) {
        timeout = 20000;
    }
    run_command("RADIO LOG", timeout / 1000, "logcat", "-b", "radio",
                                                       "-v", "threadtime",
                                                       "-v", "printable",
                                                       "-d",
                                                       "*:v", NULL);

    run_command("LOG STATISTICS", 10, "logcat", "-b", "all", "-S", NULL);

    run_command("RAFT LOGS", 600, SU_PATH, "root", "logcompressor", "-r", RAFT_DIR, NULL);

    /* show the traces we collected in main(), if that was done */
    if (dump_traces_path != NULL) {
        dump_file("VM TRACES JUST NOW", dump_traces_path);
    }

    /* only show ANR traces if they're less than 15 minutes old */
    struct stat st;
    char anr_traces_path[PATH_MAX];
    property_get("dalvik.vm.stack-trace-file", anr_traces_path, "");
    if (!anr_traces_path[0]) {
        printf("*** NO VM TRACES FILE DEFINED (dalvik.vm.stack-trace-file)\n\n");
    } else {
      int fd = TEMP_FAILURE_RETRY(open(anr_traces_path,
                                       O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
      if (fd < 0) {
          printf("*** NO ANR VM TRACES FILE (%s): %s\n\n", anr_traces_path, strerror(errno));
      } else {
          dump_file_from_fd("VM TRACES AT LAST ANR", anr_traces_path, fd);
      }
    }

    /* slow traces for slow operations */
    if (anr_traces_path[0] != 0) {
        int tail = strlen(anr_traces_path)-1;
        while (tail > 0 && anr_traces_path[tail] != '/') {
            tail--;
        }
        int i = 0;
        while (1) {
            sprintf(anr_traces_path+tail+1, "slow%02d.txt", i);
            if (stat(anr_traces_path, &st)) {
                // No traces file at this index, done with the files.
                break;
            }
            dump_file("VM TRACES WHEN SLOW", anr_traces_path);
            i++;
        }
    }

    int dumped = 0;
    for (size_t i = 0; i < NUM_TOMBSTONES; i++) {
        if (tombstone_data[i].fd != -1) {
            const char *name = tombstone_data[i].name;
            int fd = tombstone_data[i].fd;
            dumped = 1;
            if (zip_writer) {
                if (!add_zip_entry_from_fd(ZIP_ROOT_DIR + name, fd)) {
                    ALOGE("Unable to add tombstone %s to zip file\n", name);
                }
            } else {
                dump_file_from_fd("TOMBSTONE", name, fd);
            }
            close(fd);
            tombstone_data[i].fd = -1;
        }
    }
    if (!dumped) {
        printf("*** NO TOMBSTONES to dump in %s\n\n", TOMBSTONE_DIR);
    }

    dump_file("NETWORK DEV INFO", "/proc/net/dev");
    dump_file("QTAGUID NETWORK INTERFACES INFO", "/proc/net/xt_qtaguid/iface_stat_all");
    dump_file("QTAGUID NETWORK INTERFACES INFO (xt)", "/proc/net/xt_qtaguid/iface_stat_fmt");
    dump_file("QTAGUID CTRL INFO", "/proc/net/xt_qtaguid/ctrl");
    dump_file("QTAGUID STATS INFO", "/proc/net/xt_qtaguid/stats");

    if (!stat(PSTORE_LAST_KMSG, &st)) {
        /* Also TODO: Make console-ramoops CAP_SYSLOG protected. */
        dump_file("LAST KMSG", PSTORE_LAST_KMSG);
    } else {
        /* TODO: Make last_kmsg CAP_SYSLOG protected. b/5555691 */
        dump_file("LAST KMSG", "/proc/last_kmsg");
    }

    /* kernels must set CONFIG_PSTORE_PMSG, slice up pstore with device tree */
    run_command("LAST LOGCAT", 10, "logcat", "-L",
                                             "-b", "all",
                                             "-v", "threadtime",
                                             "-v", "printable",
                                             "-d",
                                             "*:v", NULL);

    /* The following have a tendency to get wedged when wifi drivers/fw goes belly-up. */

    run_command("NETWORK INTERFACES", 10, "ip", "link", NULL);

    run_command("IPv4 ADDRESSES", 10, "ip", "-4", "addr", "show", NULL);
    run_command("IPv6 ADDRESSES", 10, "ip", "-6", "addr", "show", NULL);

    run_command("IP RULES", 10, "ip", "rule", "show", NULL);
    run_command("IP RULES v6", 10, "ip", "-6", "rule", "show", NULL);

    dump_route_tables();

    run_command("ARP CACHE", 10, "ip", "-4", "neigh", "show", NULL);
    run_command("IPv6 ND CACHE", 10, "ip", "-6", "neigh", "show", NULL);

    run_command("IPTABLES", 10, SU_PATH, "root", "iptables", "-L", "-nvx", NULL);
    run_command("IP6TABLES", 10, SU_PATH, "root", "ip6tables", "-L", "-nvx", NULL);
    run_command("IPTABLE NAT", 10, SU_PATH, "root", "iptables", "-t", "nat", "-L", "-nvx", NULL);
    /* no ip6 nat */
    run_command("IPTABLE RAW", 10, SU_PATH, "root", "iptables", "-t", "raw", "-L", "-nvx", NULL);
    run_command("IP6TABLE RAW", 10, SU_PATH, "root", "ip6tables", "-t", "raw", "-L", "-nvx", NULL);

    run_command("WIFI NETWORKS", 20,
            SU_PATH, "root", "wpa_cli", "IFNAME=wlan0", "list_networks", NULL);

#ifdef FWDUMP_bcmdhd
    run_command("ND OFFLOAD TABLE", 5,
            SU_PATH, "root", "wlutil", "nd_hostip", NULL);

    run_command("DUMP WIFI INTERNAL COUNTERS (1)", 20,
            SU_PATH, "root", "wlutil", "counters", NULL);

    run_command("ND OFFLOAD STATUS (1)", 5,
            SU_PATH, "root", "wlutil", "nd_status", NULL);

#endif
    dump_file("INTERRUPTS (1)", "/proc/interrupts");

    run_command("NETWORK DIAGNOSTICS", 10, "dumpsys", "connectivity", "--diag", NULL);

#ifdef FWDUMP_bcmdhd
    run_command("DUMP WIFI STATUS", 20,
            SU_PATH, "root", "dhdutil", "-i", "wlan0", "dump", NULL);

    run_command("DUMP WIFI INTERNAL COUNTERS (2)", 20,
            SU_PATH, "root", "wlutil", "counters", NULL);

    run_command("ND OFFLOAD STATUS (2)", 5,
            SU_PATH, "root", "wlutil", "nd_status", NULL);
#endif
    dump_file("INTERRUPTS (2)", "/proc/interrupts");

    print_properties();

    run_command("VOLD DUMP", 10, "vdc", "dump", NULL);
    run_command("SECURE CONTAINERS", 10, "vdc", "asec", "list", NULL);

    run_command("FILESYSTEMS & FREE SPACE", 10, "df", NULL);

    run_command("LAST RADIO LOG", 10, "parse_radio_log", "/proc/last_radio_log", NULL);

    printf("------ BACKLIGHTS ------\n");
    printf("LCD brightness=");
    dump_file(NULL, "/sys/class/leds/lcd-backlight/brightness");
    printf("Button brightness=");
    dump_file(NULL, "/sys/class/leds/button-backlight/brightness");
    printf("Keyboard brightness=");
    dump_file(NULL, "/sys/class/leds/keyboard-backlight/brightness");
    printf("ALS mode=");
    dump_file(NULL, "/sys/class/leds/lcd-backlight/als");
    printf("LCD driver registers:\n");
    dump_file(NULL, "/sys/class/leds/lcd-backlight/registers");
    printf("\n");

    /* Binder state is expensive to look at as it uses a lot of memory. */
    dump_file("BINDER FAILED TRANSACTION LOG", "/sys/kernel/debug/binder/failed_transaction_log");
    dump_file("BINDER TRANSACTION LOG", "/sys/kernel/debug/binder/transaction_log");
    dump_file("BINDER TRANSACTIONS", "/sys/kernel/debug/binder/transactions");
    dump_file("BINDER STATS", "/sys/kernel/debug/binder/stats");
    dump_file("BINDER STATE", "/sys/kernel/debug/binder/state");

    printf("========================================================\n");
    printf("== Board\n");
    printf("========================================================\n");

    dumpstate_board();
    printf("\n");

    /* Migrate the ril_dumpstate to a dumpstate_board()? */
    char ril_dumpstate_timeout[PROPERTY_VALUE_MAX] = {0};
    property_get("ril.dumpstate.timeout", ril_dumpstate_timeout, "30");
    if (strnlen(ril_dumpstate_timeout, PROPERTY_VALUE_MAX - 1) > 0) {
        if (0 == strncmp(build_type, "user", PROPERTY_VALUE_MAX - 1)) {
            // su does not exist on user builds, so try running without it.
            // This way any implementations of vril-dump that do not require
            // root can run on user builds.
            run_command("DUMP VENDOR RIL LOGS", atoi(ril_dumpstate_timeout),
                    "vril-dump", NULL);
        } else {
            run_command("DUMP VENDOR RIL LOGS", atoi(ril_dumpstate_timeout),
                    SU_PATH, "root", "vril-dump", NULL);
        }
    }

    printf("========================================================\n");
    printf("== Android Framework Services\n");
    printf("========================================================\n");

    /* the full dumpsys is starting to take a long time, so we need
       to increase its timeout.  we really need to do the timeouts in
       dumpsys itself... */
    run_command("DUMPSYS", 60, "dumpsys", NULL);

    printf("========================================================\n");
    printf("== Checkins\n");
    printf("========================================================\n");

    run_command("CHECKIN BATTERYSTATS", 30, "dumpsys", "batterystats", "-c", NULL);
    run_command("CHECKIN MEMINFO", 30, "dumpsys", "meminfo", "--checkin", NULL);
    run_command("CHECKIN NETSTATS", 30, "dumpsys", "netstats", "--checkin", NULL);
    run_command("CHECKIN PROCSTATS", 30, "dumpsys", "procstats", "-c", NULL);
    run_command("CHECKIN USAGESTATS", 30, "dumpsys", "usagestats", "-c", NULL);
    run_command("CHECKIN PACKAGE", 30, "dumpsys", "package", "--checkin", NULL);

    printf("========================================================\n");
    printf("== Running Application Activities\n");
    printf("========================================================\n");

    run_command("APP ACTIVITIES", 30, "dumpsys", "activity", "all", NULL);

    printf("========================================================\n");
    printf("== Running Application Services\n");
    printf("========================================================\n");

    run_command("APP SERVICES", 30, "dumpsys", "activity", "service", "all", NULL);

    printf("========================================================\n");
    printf("== Running Application Providers\n");
    printf("========================================================\n");

    run_command("APP SERVICES", 30, "dumpsys", "activity", "provider", "all", NULL);


    printf("========================================================\n");
    printf("== dumpstate: done\n");
    printf("========================================================\n");
}

static void usage() {
    fprintf(stderr, "usage: dumpstate [-b soundfile] [-e soundfile] [-o file [-d] [-p] [-z]] [-s] [-q]\n"
            "  -o: write to file (instead of stdout)\n"
            "  -d: append date to filename (requires -o)\n"
            "  -z: generates zipped file (requires -o)\n"
            "  -p: capture screenshot to filename.png (requires -o)\n"
            "  -s: write output to control socket (for init)\n"
            "  -b: play sound file instead of vibrate, at beginning of job\n"
            "  -e: play sound file instead of vibrate, at end of job\n"
            "  -q: disable vibrate\n"
            "  -B: send broadcast when finished (requires -o)\n"
            "  -P: send broadacast when started and update system properties on progress (requires -o and -B)\n"
                );
}

static void sigpipe_handler(int n) {
    // don't complain to stderr or stdout
    _exit(EXIT_FAILURE);
}

/* adds the temporary report to the existing .zip file, closes the .zip file, and removes the
   temporary file.
 */
static bool finish_zip_file(const std::string& bugreport_name, const std::string& bugreport_path,
        time_t now) {
    if (!add_zip_entry(bugreport_name, bugreport_path)) {
        ALOGE("Failed to add text entry to .zip file\n");
        return false;
    }

    int32_t err = zip_writer->Finish();
    if (err) {
        ALOGE("zip_writer->Finish(): %s\n", ZipWriter::ErrorCodeString(err));
        return false;
    }

    if (remove(bugreport_path.c_str())) {
        ALOGW("remove(%s): %s\n", bugreport_path.c_str(), strerror(errno));
    }

    return true;
}

int main(int argc, char *argv[]) {
    struct sigaction sigact;
    int do_add_date = 0;
    int do_zip_file = 0;
    int do_vibrate = 1;
    char* use_outfile = 0;
    int use_socket = 0;
    int do_fb = 0;
    int do_broadcast = 0;
    int do_early_screenshot = 0;

    now = time(NULL);

    if (getuid() != 0) {
        // Old versions of the adb client would call the
        // dumpstate command directly. Newer clients
        // call /system/bin/bugreport instead. If we detect
        // we're being called incorrectly, then exec the
        // correct program.
        return execl("/system/bin/bugreport", "/system/bin/bugreport", NULL);
    }

    ALOGI("begin\n");

    /* clear SIGPIPE handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigpipe_handler;
    sigaction(SIGPIPE, &sigact, NULL);

    /* set as high priority, and protect from OOM killer */
    setpriority(PRIO_PROCESS, 0, -20);
    FILE *oom_adj = fopen("/proc/self/oom_adj", "we");
    if (oom_adj) {
        fputs("-17", oom_adj);
        fclose(oom_adj);
    }

    /* parse arguments */
    int c;
    while ((c = getopt(argc, argv, "dho:svqzpPB")) != -1) {
        switch (c) {
            case 'd': do_add_date = 1;          break;
            case 'z': do_zip_file = 1;          break;
            case 'o': use_outfile = optarg;     break;
            case 's': use_socket = 1;           break;
            case 'v': break;  // compatibility no-op
            case 'q': do_vibrate = 0;           break;
            case 'p': do_fb = 1;                break;
            case 'P': do_update_progress = 1;   break;
            case 'B': do_broadcast = 1;         break;
            case '?': printf("\n");
            case 'h':
                usage();
                exit(1);
        }
    }

    if ((do_zip_file || do_add_date || do_update_progress || do_broadcast) && !use_outfile) {
        usage();
        exit(1);
    }

    if (do_update_progress && !do_broadcast) {
        usage();
        exit(1);
    }

    do_early_screenshot = do_update_progress;

    // If we are going to use a socket, do it as early as possible
    // to avoid timeouts from bugreport.
    if (use_socket) {
        redirect_to_socket(stdout, "dumpstate");
    }

    /* full path of the directory where the bug report files will be written */
    std::string bugreport_dir;

    /* full path of the temporary file containing the bug report */
    std::string tmp_path;

    /* full path of the temporary file containing the screenshot (when requested) */
    std::string screenshot_path;

    /* base name (without suffix or extensions) of the bug report files */
    std::string base_name;

    /* suffix of the bug report files - it's typically the date (when invoked with -d),
     * although it could be changed by the user using a system property */
    std::string suffix;

    /* pointer to the actual path, be it zip or text */
    std::string path;

    /* pointers to the zipped file file */
    std::unique_ptr<FILE, int(*)(FILE*)> zip_file(NULL, fclose);

    /* redirect output if needed */
    bool is_redirecting = !use_socket && use_outfile;

    if (is_redirecting) {
        bugreport_dir = dirname(use_outfile);
        base_name = basename(use_outfile);
        if (do_add_date) {
            char date[80];
            strftime(date, sizeof(date), "%Y-%m-%d-%H-%M-%S", localtime(&now));
            suffix = date;
        } else {
            suffix = "undated";
        }
        if (do_fb) {
            // TODO: if dumpstate was an object, the paths could be internal variables and then
            // we could have a function to calculate the derived values, such as:
            //     screenshot_path = GetPath(".png");
            screenshot_path = bugreport_dir + "/" + base_name + "-" + suffix + ".png";
        }
        tmp_path = bugreport_dir + "/" + base_name + "-" + suffix + ".tmp";

        ALOGD("Bugreport dir: %s\nBase name: %s\nSuffix: %s\nTemporary path: %s\n"
                "Screenshot path: %s\n", bugreport_dir.c_str(), base_name.c_str(), suffix.c_str(),
                tmp_path.c_str(), screenshot_path.c_str());

        if (do_zip_file) {
            ALOGD("Creating initial .zip file");
            path = bugreport_dir + "/" + base_name + "-" + suffix + ".zip";
            zip_file.reset(fopen(path.c_str(), "wb"));
            if (!zip_file) {
                ALOGE("fopen(%s, 'wb'): %s\n", path.c_str(), strerror(errno));
                do_zip_file = 0;
            } else {
                zip_writer.reset(new ZipWriter(zip_file.get()));
            }
        }

        if (do_update_progress) {
            std::vector<std::string> am_args = {
                 "--receiver-permission", "android.permission.DUMP",
                 "--es", "android.intent.extra.NAME", suffix,
                 "--ei", "android.intent.extra.PID", std::to_string(getpid()),
                 "--ei", "android.intent.extra.MAX", std::to_string(WEIGHT_TOTAL),
            };
            send_broadcast("android.intent.action.BUGREPORT_STARTED", am_args);
        }
    }

    print_header();

    /* open the vibrator before dropping root */
    std::unique_ptr<FILE, int(*)(FILE*)> vibrator(NULL, fclose);
    if (do_vibrate) {
        vibrator.reset(fopen("/sys/class/timed_output/vibrator/enable", "we"));
        if (vibrator) {
            vibrate(vibrator.get(), 150);
        }
    }

    if (do_fb && do_early_screenshot) {
        if (screenshot_path.empty()) {
            // should not have happened
            ALOGE("INTERNAL ERROR: skipping early screenshot because path was not set");
        } else {
            ALOGI("taking early screenshot\n");
            take_screenshot(screenshot_path);
            ALOGI("wrote screenshot: %s\n", screenshot_path.c_str());
            if (chown(screenshot_path.c_str(), AID_SHELL, AID_SHELL)) {
                ALOGE("Unable to change ownership of screenshot file %s: %s\n",
                        screenshot_path.c_str(), strerror(errno));
            }
        }
    }

    if (do_zip_file) {
        if (chown(path.c_str(), AID_SHELL, AID_SHELL)) {
            ALOGE("Unable to change ownership of zip file %s: %s\n", path.c_str(), strerror(errno));
        }
    }

    /* read /proc/cmdline before dropping root */
    FILE *cmdline = fopen("/proc/cmdline", "re");
    if (cmdline) {
        fgets(cmdline_buf, sizeof(cmdline_buf), cmdline);
        fclose(cmdline);
    }

    /* collect stack traces from Dalvik and native processes (needs root) */
    dump_traces_path = dump_traces();

    /* Get the tombstone fds and recovery files here while we are running as root. */
    get_tombstone_fds(tombstone_data);
    add_dir(RECOVERY_DIR, true);

    /* ensure we will keep capabilities when we drop root */
    if (prctl(PR_SET_KEEPCAPS, 1) < 0) {
        ALOGE("prctl(PR_SET_KEEPCAPS) failed: %s\n", strerror(errno));
        return -1;
    }

    /* switch to non-root user and group */
    gid_t groups[] = { AID_LOG, AID_SDCARD_R, AID_SDCARD_RW,
            AID_MOUNT, AID_INET, AID_NET_BW_STATS, AID_READPROC };
    if (setgroups(sizeof(groups)/sizeof(groups[0]), groups) != 0) {
        ALOGE("Unable to setgroups, aborting: %s\n", strerror(errno));
        return -1;
    }
    if (setgid(AID_SHELL) != 0) {
        ALOGE("Unable to setgid, aborting: %s\n", strerror(errno));
        return -1;
    }
    if (setuid(AID_SHELL) != 0) {
        ALOGE("Unable to setuid, aborting: %s\n", strerror(errno));
        return -1;
    }

    struct __user_cap_header_struct capheader;
    struct __user_cap_data_struct capdata[2];
    memset(&capheader, 0, sizeof(capheader));
    memset(&capdata, 0, sizeof(capdata));
    capheader.version = _LINUX_CAPABILITY_VERSION_3;
    capheader.pid = 0;

    capdata[CAP_TO_INDEX(CAP_SYSLOG)].permitted = CAP_TO_MASK(CAP_SYSLOG);
    capdata[CAP_TO_INDEX(CAP_SYSLOG)].effective = CAP_TO_MASK(CAP_SYSLOG);
    capdata[0].inheritable = 0;
    capdata[1].inheritable = 0;

    if (capset(&capheader, &capdata[0]) < 0) {
        ALOGE("capset failed: %s\n", strerror(errno));
        return -1;
    }

    if (is_redirecting) {
        /* TODO: rather than generating a text file now and zipping it later,
           it would be more efficient to redirect stdout to the zip entry
           directly, but the libziparchive doesn't support that option yet. */
        redirect_to_file(stdout, const_cast<char*>(tmp_path.c_str()));
    }

    dumpstate(do_early_screenshot ? "": screenshot_path);

    /* done */
    if (vibrator) {
        for (int i = 0; i < 3; i++) {
            vibrate(vibrator.get(), 75);
            usleep((75 + 50) * 1000);
        }
    }

    /* close output if needed */
    if (is_redirecting) {
        fclose(stdout);
    }

    /* rename or zip the (now complete) .tmp file to its final location */
    if (use_outfile) {

        /* check if user changed the suffix using system properties */
        char key[PROPERTY_KEY_MAX];
        char value[PROPERTY_VALUE_MAX];
        sprintf(key, "dumpstate.%d.name", getpid());
        property_get(key, value, "");
        bool change_suffix= false;
        if (value[0]) {
            /* must whitelist which characters are allowed, otherwise it could cross directories */
            std::regex valid_regex("^[-_a-zA-Z0-9]+$");
            if (std::regex_match(value, valid_regex)) {
                change_suffix = true;
            } else {
                ALOGE("invalid suffix provided by user: %s", value);
            }
        }
        if (change_suffix) {
            ALOGI("changing suffix from %s to %s", suffix.c_str(), value);
            suffix = value;
            if (!screenshot_path.empty()) {
                std::string new_screenshot_path =
                        bugreport_dir + "/" + base_name + "-" + suffix + ".png";
                if (rename(screenshot_path.c_str(), new_screenshot_path.c_str())) {
                    ALOGE("rename(%s, %s): %s\n", screenshot_path.c_str(),
                            new_screenshot_path.c_str(), strerror(errno));
                } else {
                    screenshot_path = new_screenshot_path;
                }
            }
        }

        bool do_text_file = true;
        if (do_zip_file) {
            ALOGD("Adding text entry to .zip bugreport");
            if (!finish_zip_file(base_name + "-" + suffix + ".txt", tmp_path, now)) {
                ALOGE("Failed to finish zip file; sending text bugreport instead\n");
                do_text_file = true;
            } else {
                do_text_file = false;
            }
        }
        if (do_text_file) {
            ALOGD("Generating .txt bugreport");
            path = bugreport_dir + "/" + base_name + "-" + suffix + ".txt";
            if (rename(tmp_path.c_str(), path.c_str())) {
                ALOGE("rename(%s, %s): %s\n", tmp_path.c_str(), path.c_str(), strerror(errno));
                path.clear();
            }
        }
    }

    /* tell activity manager we're done */
    if (do_broadcast) {
        if (!path.empty()) {
            ALOGI("Final bugreport path: %s\n", path.c_str());
            std::vector<std::string> am_args = {
                 "--receiver-permission", "android.permission.DUMP",
                 "--ei", "android.intent.extra.PID", std::to_string(getpid()),
                 "--es", "android.intent.extra.BUGREPORT", path
            };
            if (do_fb) {
                am_args.push_back("--es");
                am_args.push_back("android.intent.extra.SCREENSHOT");
                am_args.push_back(screenshot_path);
            }
            send_broadcast("android.intent.action.BUGREPORT_FINISHED", am_args);
        } else {
            ALOGE("Skipping finished broadcast because bugreport could not be generated\n");
        }
    }

    ALOGD("Final progress: %d/%d (originally %d)\n", progress, weight_total, WEIGHT_TOTAL);
    ALOGI("done\n");

    return 0;
}
