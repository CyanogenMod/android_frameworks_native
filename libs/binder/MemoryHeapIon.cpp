/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*!
 * \file MemoryHeapIon.cpp
 * \brief source file for MemoryHeapIon
 * \author MinGu, Jeon(mingu85.jeon)
 * \date 2011/11/20
 *
 * <b>Revision History: </b>
 * - 2011/11/20 : MinGu, Jeon(mingu85.jeon)) \n
 * Initial version
 * - 2012/11/29 : MinGu, Jeon(mingu85.jeon)) \n
 * Change name
 */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cutils/log.h>
#include <binder/MemoryHeapBase.h>
#include <binder/IMemory.h>
#include <binder/MemoryHeapIon.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "ion.h"

namespace android {

MemoryHeapIon::MemoryHeapIon(size_t size, uint32_t flags, char const *name):MemoryHeapBase()
{
    void* base = NULL;
    int fd = -1;

    mIonClient = ion_client_create();

    if (mIonClient < 0) {
        ALOGE("MemoryHeapIon : ION client creation failed : %s", strerror(errno));
        mIonClient = -1;
    } else {
        fd = ion_alloc(mIonClient, size, 0, ION_HEAP_SYSTEM_MASK, ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_PRESERVE_KMAP);
        if (fd < 0) {
            ALOGE("MemoryHeapIon : ION memory allocation failed(size[%u]) : %s", size, strerror(errno));
        } else {
            flags |= USE_ION_FD;
            base = ion_map(fd, size, 0);
            if (base != MAP_FAILED) {
                init(fd, base, size, flags, NULL);
            } else {
                ALOGE("MemoryHeapIon : mmap failed(size[%u], fd[%d]) : %s", size, fd, strerror(errno));
                ion_free(fd);
            }
        }
    }
}

MemoryHeapIon::MemoryHeapIon(int fd, size_t size, uint32_t flags, uint32_t offset):MemoryHeapBase()
{
    void* base = NULL;
    int dup_fd = -1;

    mIonClient = ion_client_create();

    if (mIonClient < 0) {
        ALOGE("MemoryHeapIon : ION client creation failed : %s", strerror(errno));
        mIonClient = -1;
    } else {
        if (fd >= 0) {
            dup_fd = dup(fd);
            if (dup_fd == -1) {
                ALOGE("MemoryHeapIon : cannot dup fd (size[%u], fd[%d]) : %s", size, fd, strerror(errno));
            } else {
                flags |= USE_ION_FD;
                base = ion_map(dup_fd, size, 0);
                if (base != MAP_FAILED) {
                    init(dup_fd, base, size, flags, NULL);
                } else {
                    ALOGE("MemoryHeapIon : ION mmap failed(size[%u], fd[%d]): %s", size, fd, strerror(errno));
                    ion_free(dup_fd);
                }
            }
        } else {
            ALOGE("MemoryHeapIon : fd parameter error(fd : %d)", fd);
        }
    }
}

MemoryHeapIon::~MemoryHeapIon()
{
    if (mIonClient != -1) {
        ion_unmap(getBase(), getSize());
        ion_client_destroy(mIonClient);
        mIonClient = -1;
    }
}

};
