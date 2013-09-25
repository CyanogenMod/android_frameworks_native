/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright 2011, Samsung Electronics Co. LTD
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
/*!
 * \file MemoryHeapIon.h
 * \brief header file for MemoryHeapIon
 * \author MinGu, Jeon(mingu85.jeon)
 * \date 2011/11/20
 *
 * <b>Revision History: </b>
 * - 2011/11/21 : MinGu, Jeon(mingu85.jeon)) \n
 * Initial version
 * - 2012/11/29 : MinGu, Jeon(mingu85.jeon)) \n
 * Change name
 */

#ifndef ANDROID_MEMORY_HEAP_ION_H
#define ANDROID_MEMORY_HEAP_ION_H

#include <binder/IMemory.h>
#include <binder/MemoryHeapBase.h>
#include <stdlib.h>

namespace android {

class MemoryHeapIon : public MemoryHeapBase
{
public:
    enum {
        USE_ION_FD = IMemoryHeap::USE_ION_FD
    };
    MemoryHeapIon(size_t size, uint32_t flags = 0, char const* name = NULL);
    MemoryHeapIon(int fd, size_t size, uint32_t flags = 0, uint32_t offset = 0);
    ~MemoryHeapIon();
private:
    int mIonClient;
};

};
#endif
