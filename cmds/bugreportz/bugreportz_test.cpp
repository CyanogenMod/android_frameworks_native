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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "bugreportz.h"

using ::testing::StrEq;
using ::testing::internal::CaptureStdout;
using ::testing::internal::GetCapturedStdout;

class BugreportzTest : public ::testing::Test {
  public:
    // Creates the pipe used to communicate with bugreportz()
    void SetUp() {
        int fds[2];
        ASSERT_EQ(0, pipe(fds));
        read_fd_ = fds[0];
        write_fd_ = fds[1];
    }

    // Closes the pipe FDs.
    // If a FD is closed manually during a test, set it to -1 to prevent TearDown() trying to close
    // it again.
    void TearDown() {
        for (int fd : {read_fd_, write_fd_}) {
            if (fd >= 0) {
                close(fd);
            }
        }
    }

    // Emulates dumpstate output by writing to the socket passed to bugreportz()
    void WriteToSocket(const std::string& data) {
        if (write_fd_ < 0) {
            ADD_FAILURE() << "cannot write '" << data << "' because socket is already closed";
            return;
        }
        int expected = data.length();
        int actual = write(write_fd_, data.data(), data.length());
        ASSERT_EQ(expected, actual) << "wrong number of bytes written to socket";
    }

    void AssertStdoutEquals(const std::string& expected) {
        ASSERT_THAT(stdout_, StrEq(expected)) << "wrong stdout output";
    }

    // Calls bugreportz() using the internal pipe.
    //
    // Tests must call WriteToSocket() to set what's written prior to calling it, since the writing
    // end of the pipe will be closed before calling bugreportz() (otherwise that function would
    // hang).
    void Bugreportz() {
        close(write_fd_);
        write_fd_ = -1;

        CaptureStdout();
        int status = bugreportz(read_fd_);

        close(read_fd_);
        read_fd_ = -1;
        stdout_ = GetCapturedStdout();

        ASSERT_EQ(0, status) << "bugrepotz() call failed (stdout: " << stdout_ << ")";
    }

  private:
    int read_fd_;
    int write_fd_;
    std::string stdout_;
};

// Tests 'bugreportz', without any argument - it will just echo dumpstate's output to stdout.
TEST_F(BugreportzTest, NoArgument) {
    WriteToSocket("What happens on 'dumpstate',");
    WriteToSocket("stays on 'bugreportz'.\n");

    Bugreportz();

    AssertStdoutEquals("What happens on 'dumpstate',stays on 'bugreportz'.\n");
}
