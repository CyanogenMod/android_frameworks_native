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

#ifndef _DUMPSTATE_H_
#define _DUMPSTATE_H_

/* When defined, skips the real dumps and just print the section headers.
   Useful when debugging dumpstate itself. */
//#define _DUMPSTATE_DRY_RUN_

#ifdef _DUMPSTATE_DRY_RUN_
#define ON_DRY_RUN_RETURN(X) return X
#define ON_DRY_RUN(code) code
#else
#define ON_DRY_RUN_RETURN(X)
#define ON_DRY_RUN(code)
#endif

#ifndef MYLOGD
#define MYLOGD(...) fprintf(stderr, __VA_ARGS__); ALOGD(__VA_ARGS__);
#endif

#ifndef MYLOGI
#define MYLOGI(...) fprintf(stderr, __VA_ARGS__); ALOGI(__VA_ARGS__);
#endif

#ifndef MYLOGE
#define MYLOGE(...) fprintf(stderr, __VA_ARGS__); ALOGE(__VA_ARGS__);
#endif

#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <vector>

#define SU_PATH "/system/xbin/su"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (for_each_pid_func)(int, const char *);
typedef void (for_each_tid_func)(int, int, const char *);

/* Estimated total weight of bugreport generation.
 *
 * Each section contributes to the total weight by an individual weight, so the overall progress
 * can be calculated by dividing the all completed weight by the total weight.
 *
 * This value is defined empirically and it need to be adjusted as more sections are added.
 *
 * It does not need to match the exact sum of all sections, but ideally it should to be slight more
 * than such sum: a value too high will cause the bugreport to finish before the user expected (for
 * example, jumping from 70% to 100%), while a value too low will cause the progress to get stuck
 * at an almost-finished value (like 99%) for a while.
 */
static const int WEIGHT_TOTAL = 6500;

/* Most simple commands have 10 as timeout, so 5 is a good estimate */
static const int WEIGHT_FILE = 5;

/*
 * TODO: the dumpstate internal state is getting fragile; for example, this variable is defined
 * here, declared at utils.cpp, and used on utils.cpp and dumpstate.cpp.
 * It would be better to take advantage of the C++ migration and encapsulate the state in an object,
 * but that will be better handled in a major C++ refactoring, which would also get rid of other C
 * idioms (like using std::string instead of char*, removing varargs, etc...) */
extern int do_update_progress, progress, weight_total, control_socket_fd;

/* full path of the directory where the bugreport files will be written */
extern std::string bugreport_dir;

/* root dir for all files copied as-is into the bugreport. */
extern const std::string ZIP_ROOT_DIR;

/* Checkes whether dumpstate is generating a zipped bugreport. */
bool is_zipping();

/* adds a new entry to the existing zip file. */
bool add_zip_entry(const std::string& entry_name, const std::string& entry_path);

/* adds a new entry to the existing zip file. */
bool add_zip_entry_from_fd(const std::string& entry_name, int fd);

/* adds all files from a directory to the zipped bugreport file */
void add_dir(const char *dir, bool recursive);

/* prints the contents of a file */
int dump_file(const char *title, const char *path);

/* saves the the contents of a file as a long */
int read_file_as_long(const char *path, long int *output);

/* prints the contents of the fd
 * fd must have been opened with the flag O_NONBLOCK.
 */
int dump_file_from_fd(const char *title, const char *path, int fd);

/* calls skip to gate calling dump_from_fd recursively
 * in the specified directory. dump_from_fd defaults to
 * dump_file_from_fd above when set to NULL. skip defaults
 * to false when set to NULL. dump_from_fd will always be
 * called with title NULL.
 */
int dump_files(const char *title, const char *dir,
        bool (*skip)(const char *path),
        int (*dump_from_fd)(const char *title, const char *path, int fd));

// TODO: need to refactor all those run_command variations; there shold be just one, receiving an
// optional CommandOptions objects with values such as run_always, drop_root, etc...

/* forks a command and waits for it to finish -- terminate args with NULL */
int run_command_as_shell(const char *title, int timeout_seconds, const char *command, ...);
int run_command(const char *title, int timeout_seconds, const char *command, ...);

enum RootMode { DROP_ROOT, DONT_DROP_ROOT };
enum StdoutMode { NORMAL_STDOUT, REDIRECT_TO_STDERR };

/* forks a command and waits for it to finish
   first element of args is the command, and last must be NULL.
   command is always ran, even when _DUMPSTATE_DRY_RUN_ is defined. */
int run_command_always(const char *title, RootMode root_mode, StdoutMode stdout_mode,
        int timeout_seconds, const char *args[]);

/* switch to non-root user and group */
bool drop_root_user();

/* sends a broadcast using Activity Manager */
void send_broadcast(const std::string& action, const std::vector<std::string>& args);

/* updates the overall progress of dumpstate by the given weight increment */
void update_progress(int weight);

/* prints all the system properties */
void print_properties();

/** opens a socket and returns its file descriptor */
int open_socket(const char *service);

/* redirect output to a service control socket */
void redirect_to_socket(FILE *redirect, const char *service);

/* redirect output to a file */
void redirect_to_file(FILE *redirect, char *path);

/* create leading directories, if necessary */
void create_parent_dirs(const char *path);

/* dump Dalvik and native stack traces, return the trace file location (NULL if none) */
const char *dump_traces();

/* for each process in the system, run the specified function */
void for_each_pid(for_each_pid_func func, const char *header);

/* for each thread in the system, run the specified function */
void for_each_tid(for_each_tid_func func, const char *header);

/* Displays a blocked processes in-kernel wait channel */
void show_wchan(int pid, int tid, const char *name);

/* Displays a processes times */
void show_showtime(int pid, const char *name);

/* Runs "showmap" for a process */
void do_showmap(int pid, const char *name);

/* Gets the dmesg output for the kernel */
void do_dmesg();

/* Prints the contents of all the routing tables, both IPv4 and IPv6. */
void dump_route_tables();

/* Play a sound via Stagefright */
void play_sound(const char *path);

/* Implemented by libdumpstate_board to dump board-specific info */
void dumpstate_board();

/* Takes a screenshot and save it to the given file */
void take_screenshot(const std::string& path);

/* Vibrates for a given durating (in milliseconds). */
void vibrate(FILE* vibrator, int ms);

/* Checks if a given path is a directory. */
bool is_dir(const char* pathname);

/** Gets the last modification time of a file, or default time if file is not found. */
time_t get_mtime(int fd, time_t default_mtime);

/* Dumps eMMC Extended CSD data. */
void dump_emmc_ecsd(const char *ext_csd_path);

/** Gets command-line arguments. */
void format_args(int argc, const char *argv[], std::string *args);

/** Tells if the device is running a user build. */
bool is_user_build();

/*
 * Helper class used to report how long it takes for a section to finish.
 *
 * Typical usage:
 *
 *    DurationReporter duration_reporter(title);
 *
 */
class DurationReporter {
public:
    DurationReporter(const char *title);
    DurationReporter(const char *title, FILE* out);

    ~DurationReporter();

    static uint64_t nanotime();

private:
    const char* title_;
    FILE* out_;
    uint64_t started_;
};

#ifdef __cplusplus
}
#endif

#endif /* _DUMPSTATE_H_ */
