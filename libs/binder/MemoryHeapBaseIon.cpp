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
 * \file MemoryHeapBaseIon.cpp
 * \brief source file for MemoryHeapBaseIon
 * \author MinGu, Jeon(mingu85.jeon)
 * \date 2011/11/20
 *
 * <b>Revision History: </b>
 * - 2011/11/20 : MinGu, Jeon(mingu85.jeon)) \n
 * Initial version
 */

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cutils/log.h>
#include <binder/MemoryHeapBase.h>
#include <binder/IMemory.h>
#include <binder/MemoryHeapBaseIon.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "ion.h"

namespace android {

MemoryHeapBaseIon::MemoryHeapBaseIon(size_t size, uint32_t flags, char const *name)
{
    mIonClient = ion_client_create();
    if (mIonClient < 0) {
        mIonClient = -1;
        ALOGE("MemoryHeapBaseIon : ION client creation failed");
    }
    void* base = NULL;
    int fd = ion_alloc(mIonClient, size, 0, ION_HEAP_EXYNOS_MASK);

    if (fd < 0) {
        ALOGE("MemoryHeapBaseIon : ION memory allocation failed");
    } else {
        flags |= USE_ION_FD;
        base = ion_map(fd, size, 0);
        if (base != MAP_FAILED)
            init(fd, base, size, flags, NULL);
        else
            ALOGE("MemoryHeapBaseIon : mmap failed");
    }
}

MemoryHeapBaseIon::MemoryHeapBaseIon(int fd, size_t size, uint32_t flags, uint32_t offset)
{
    ALOGE_IF(fd < 0, "MemoryHeapBaseIon : file discriptor error. fd is not for ION Memory");
    mIonClient = ion_client_create();
    if (mIonClient < 0) {
        mIonClient = -1;
        ALOGE("MemoryHeapBaseIon : ION client creation failed");
    }
    void* base = NULL;
    if (fd >= 0) {
        int dup_fd = dup(fd);
        flags |= USE_ION_FD;
        base = ion_map(dup_fd, size, 0);
        if (base != MAP_FAILED)
            init(dup_fd, base, size, flags, NULL);
        else
            ALOGE("MemoryHeapBaseIon : mmap failed");
    }
}

MemoryHeapBaseIon::~MemoryHeapBaseIon()
{
    if (mIonClient != -1) {
        ion_unmap(getBase(), getSize());
        ion_free(getHeapID());
        ion_client_destroy(mIonClient);
        mIonClient = -1;
    }
}

};
