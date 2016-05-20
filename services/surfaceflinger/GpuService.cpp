/*
 * Copyright 2016 The Android Open Source Project
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

#include "GpuService.h"

#include <binder/Parcel.h>
#include <utils/String8.h>
#include <vkjson.h>

namespace android {

// ----------------------------------------------------------------------------

class BpGpuService : public BpInterface<IGpuService>
{
public:
    BpGpuService(const sp<IBinder>& impl) : BpInterface<IGpuService>(impl) {}
};

IMPLEMENT_META_INTERFACE(GpuService, "android.ui.IGpuService");

status_t BnGpuService::onTransact(uint32_t code, const Parcel& data,
        Parcel* reply, uint32_t flags)
{
    switch (code) {
    case SHELL_COMMAND_TRANSACTION: {
        int in = data.readFileDescriptor();
        int out = data.readFileDescriptor();
        int err = data.readFileDescriptor();
        int argc = data.readInt32();
        Vector<String16> args;
        for (int i = 0; i < argc && data.dataAvail() > 0; i++) {
           args.add(data.readString16());
        }
        return shellCommand(in, out, err, args);
    }

    default:
        return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

namespace {
    status_t cmd_help(int out);
    status_t cmd_vkjson(int out, int err);
}

const char* const GpuService::SERVICE_NAME = "gpu";

GpuService::GpuService() {}

status_t GpuService::shellCommand(int /*in*/, int out, int err,
        Vector<String16>& args)
{
    ALOGV("GpuService::shellCommand");
    for (size_t i = 0, n = args.size(); i < n; i++)
        ALOGV("  arg[%zu]: '%s'", i, String8(args[i]).string());

    if (args[0] == String16("vkjson"))
        return cmd_vkjson(out, err);
    else if (args[0] == String16("help"))
        return cmd_help(out);

    return NO_ERROR;
}

// ----------------------------------------------------------------------------

namespace {

status_t cmd_help(int out) {
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        ALOGE("vkjson: failed to create out stream: %s (%d)", strerror(errno),
            errno);
        return BAD_VALUE;
    }
    fprintf(outs,
        "GPU Service commands:\n"
        "  vkjson   dump Vulkan device capabilities as JSON\n");
    fclose(outs);
    return NO_ERROR;
}

VkResult vkjsonPrint(FILE* out, FILE* err) {
    VkResult result;

    const VkApplicationInfo app_info = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr,
        "vkjson", 1,    /* app name, version */
        "", 0,          /* engine name, version */
        VK_API_VERSION_1_0
    };
    const VkInstanceCreateInfo instance_info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr,
        0,              /* flags */
        &app_info,
        0, nullptr,     /* layers */
        0, nullptr,     /* extensions */
    };
    VkInstance instance;
    result = vkCreateInstance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        fprintf(err, "vkCreateInstance failed: %d\n", result);
        return result;
    }

    uint32_t ngpu = 0;
    result = vkEnumeratePhysicalDevices(instance, &ngpu, nullptr);
    if (result != VK_SUCCESS) {
        fprintf(err, "vkEnumeratePhysicalDevices failed: %d\n", result);
        return result;
    }
    std::vector<VkPhysicalDevice> gpus(ngpu, VK_NULL_HANDLE);
    result = vkEnumeratePhysicalDevices(instance, &ngpu, gpus.data());
    if (result != VK_SUCCESS) {
        fprintf(err, "vkEnumeratePhysicalDevices failed: %d\n", result);
        return result;
    }

    for (size_t i = 0, n = gpus.size(); i < n; i++) {
        auto props = VkJsonGetAllProperties(gpus[i]);
        std::string json = VkJsonAllPropertiesToJson(props);
        fwrite(json.data(), 1, json.size(), out);
        if (i < n - 1)
            fputc(',', out);
        fputc('\n', out);
    }

    vkDestroyInstance(instance, nullptr);

    return VK_SUCCESS;
}

status_t cmd_vkjson(int out, int err) {
    int errnum;
    FILE* outs = fdopen(out, "w");
    if (!outs) {
        errnum = errno;
        ALOGE("vkjson: failed to create output stream: %s", strerror(errnum));
        return -errnum;
    }
    FILE* errs = fdopen(err, "w");
    if (!errs) {
        errnum = errno;
        ALOGE("vkjson: failed to create error stream: %s", strerror(errnum));
        fclose(outs);
        return -errnum;
    }
    fprintf(outs, "[\n");
    VkResult result = vkjsonPrint(outs, errs);
    fprintf(outs, "]\n");
    fclose(errs);
    fclose(outs);
    return result >= 0 ? NO_ERROR : UNKNOWN_ERROR;
}

} // anonymous namespace

} // namespace android
