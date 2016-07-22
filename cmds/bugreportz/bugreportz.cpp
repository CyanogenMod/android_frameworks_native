/*
 * Copyright (C) 2016 The Android Open Source Project
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bugreportz.h"

int bugreportz(int s) {
    while (1) {
        char buffer[65536];
        ssize_t bytes_read = TEMP_FAILURE_RETRY(read(s, buffer, sizeof(buffer)));
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            // EAGAIN really means time out, so change the errno.
            if (errno == EAGAIN) {
                errno = ETIMEDOUT;
            }
            printf("FAIL:Bugreport read terminated abnormally (%s)\n", strerror(errno));
            break;
        }

        ssize_t bytes_to_send = bytes_read;
        ssize_t bytes_written;
        do {
            bytes_written = TEMP_FAILURE_RETRY(
                write(STDOUT_FILENO, buffer + bytes_read - bytes_to_send, bytes_to_send));
            if (bytes_written == -1) {
                fprintf(stderr,
                        "Failed to write data to stdout: read %zd, trying to send %zd "
                        "(%s)\n",
                        bytes_read, bytes_to_send, strerror(errno));
                break;
            }
            bytes_to_send -= bytes_written;
        } while (bytes_written != 0 && bytes_to_send > 0);
    }

    if (close(s) == -1) {
        fprintf(stderr, "WARNING: error closing socket: %s\n", strerror(errno));
    }
    return EXIT_SUCCESS;
}
