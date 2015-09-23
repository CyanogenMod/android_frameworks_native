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

#include <stdint.h>
#include <sys/types.h>

#define LOG_TAG "InputDriver"

#define LOG_NDEBUG 0

#include "InputDriver.h"
#include "InputHost.h"

#include <hardware/input.h>
#include <utils/Log.h>
#include <utils/String8.h>

#define INDENT2 "    "

namespace android {

static input_host_callbacks_t kCallbacks = {
    .create_device_identifier = create_device_identifier,
    .create_device_definition = create_device_definition,
    .create_input_report_definition = create_input_report_definition,
    .create_output_report_definition = create_output_report_definition,
    .input_device_definition_add_report = input_device_definition_add_report,
    .input_report_definition_add_collection = input_report_definition_add_collection,
    .input_report_definition_declare_usage_int = input_report_definition_declare_usage_int,
    .input_report_definition_declare_usages_bool = input_report_definition_declare_usages_bool,
    .register_device = register_device,
    .input_allocate_report = input_allocate_report,
    .input_report_set_usage_int = input_report_set_usage_int,
    .input_report_set_usage_bool = input_report_set_usage_bool,
    .report_event = report_event,
    .input_get_device_property_map = input_get_device_property_map,
    .input_get_device_property = input_get_device_property,
    .input_get_property_key = input_get_property_key,
    .input_get_property_value = input_get_property_value,
    .input_free_device_property = input_free_device_property,
    .input_free_device_property_map = input_free_device_property_map,
};

InputDriver::InputDriver(const char* name) : mName(String8(name)) {
    const hw_module_t* module;
    int err = input_open(&module, name);
    LOG_ALWAYS_FATAL_IF(err != 0, "Input module %s not found", name);
    mHal = reinterpret_cast<const input_module_t*>(module);
}

void InputDriver::init(InputHostInterface* host) {
    mHal->init(mHal, static_cast<input_host_t*>(host), kCallbacks);
}

void InputDriver::dump(String8& result) {
    result.appendFormat(INDENT2 "HAL Input Driver (%s)\n", mName.string());
}


// HAL wrapper functions

input_device_identifier_t* create_device_identifier(input_host_t* host,
        const char* name, int32_t product_id, int32_t vendor_id,
        input_bus_t bus, const char* unique_id) {
    return nullptr;
}

input_device_definition_t* create_device_definition(input_host_t* host) {
    return nullptr;
}

input_report_definition_t* create_input_report_definition(input_host_t* host) {
    return nullptr;
}

input_report_definition_t* create_output_report_definition(input_host_t* host) {
    return nullptr;
}

void input_device_definition_add_report(input_host_t* host,
        input_device_definition_t* d, input_report_definition_t* r) { }

void input_report_definition_add_collection(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id, int32_t arity) { }

void input_report_definition_declare_usage_int(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id,
        input_usage_t usage, int32_t min, int32_t max, float resolution) { }

void input_report_definition_declare_usages_bool(input_host_t* host,
        input_report_definition_t* report, input_collection_id_t id,
        input_usage_t* usage, size_t usage_count) { }


input_device_handle_t* register_device(input_host_t* host,
        input_device_identifier_t* id, input_device_definition_t* d) {
    return nullptr;
}

input_report_t* input_allocate_report(input_host_t* host, input_report_definition_t* r) {
    return nullptr;
}
void input_report_set_usage_int(input_host_t* host, input_report_t* r,
        input_collection_id_t id, input_usage_t usage, int32_t value, int32_t arity_index) { }

void input_report_set_usage_bool(input_host_t* host, input_report_t* r,
        input_collection_id_t id, input_usage_t usage, bool value, int32_t arity_index) { }

void report_event(input_host_t* host, input_device_handle_t* d, input_report_t* report) { }

input_property_map_t* input_get_device_property_map(input_host_t* host,
        input_device_identifier_t* id) {
    return nullptr;
}

input_property_t* input_get_device_property(input_host_t* host, input_property_map_t* map,
        const char* key) {
    return nullptr;
}

const char* input_get_property_key(input_host_t* host, input_property_t* property) {
    return nullptr;
}

const char* input_get_property_value(input_host_t* host, input_property_t* property) {
    return nullptr;
}

void input_free_device_property(input_host_t* host, input_property_t* property) { }

void input_free_device_property_map(input_host_t* host, input_property_map_t* map) { }

} // namespace android
