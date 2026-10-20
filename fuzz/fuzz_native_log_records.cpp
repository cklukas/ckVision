// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fuzz_common.hpp"
#include "../examples/logmonitor/native_json.hpp"
#include "../examples/logmonitor/windows_event_record.hpp"
#include "cvision/core/utf8.hpp"
#include <algorithm>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ckv; using namespace ckv::logmonitor;
    size = std::min<std::size_t>(size, kLogBatchBytes + 1); const std::string_view text(reinterpret_cast<const char*>(data), size);
    NativeJsonDecoder decoder; LogMessage message; message.source = 1; message.epoch = 1;
    for (const auto schema : {NativeJsonSchema::Apple, NativeJsonSchema::Journal}) {
        const auto result = decoder.decode(text, schema, message);
        if (result == NativeJsonResult::Record) {
            fuzz::require(message.valid() && message.bytes().size() == size);
            for (const auto& field : message.metadata().fields) fuzz::require(field.value.size() <= 384 && utf8::is_valid(field.value));
        }
    }
    native_timestamp(text);
    const WindowsEventRecord event{.xml = text, .channel = "Application", .provider = "fuzz", .rendered_message = text,
        .event_id = size ? data[0] : 0U, .filetime = size ? std::optional<std::uint64_t>{116444736000000000ULL + data[0]} : std::nullopt,
        .level = size ? std::optional<std::uint8_t>{data[0]} : std::nullopt};
    const auto encoded = encode_windows_event(event, message);
    if (encoded.accepted) fuzz::require(message.valid() && message.bytes().size() <= kLogBatchBytes);
    return 0;
}
