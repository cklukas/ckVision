// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "windows_event_record.hpp"
#include "cvision/core/utf8.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace ckv::logmonitor {
WindowsRecordResult encode_windows_event(const WindowsEventRecord& event, LogMessage& output) noexcept {
    WindowsRecordResult result;
    if (event.xml.empty() || event.xml.size() > kLogBatchBytes || !utf8::is_valid(event.xml) ||
        !utf8::is_valid(event.channel) || !utf8::is_valid(event.provider) || !utf8::is_valid(event.bookmark) || !utf8::is_valid(event.rendered_message)) return result;
    constexpr std::array levels{LogSeverity::Unknown, LogSeverity::Critical, LogSeverity::Error, LogSeverity::Warning, LogSeverity::Info, LogSeverity::Debug};
    const auto severity = event.level && *event.level < levels.size() ? levels[*event.level] : LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp;
    constexpr std::uint64_t unix_epoch = 116444736000000000ULL;
    if (event.filetime && *event.filetime >= unix_epoch && *event.filetime - unix_epoch <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 100))
        timestamp = static_cast<std::int64_t>(*event.filetime - unix_epoch) * 100;
    std::array<LogField, 8> fields{}; std::size_t count = 0;
    const auto add = [&](std::string_view name, std::string_view value) {
        if (value.size() > 384) {
            value = value.substr(0, 384); while (!value.empty() && !utf8::is_valid(value)) value.remove_suffix(1);
            result.metadata_clipped = true;
        }
        fields[count++] = {name, value};
    };
    std::array<std::array<char, 32>, 3> numbers{}; std::size_t index = 0;
    const auto number = [&](std::string_view name, std::uint64_t value) {
        auto& text = numbers[index++]; const auto encoded = std::to_chars(text.data(), text.data() + text.size(), value);
        add(name, {text.data(), static_cast<std::size_t>(encoded.ptr - text.data())});
    };
    add("channel", event.channel); add("provider", event.provider);
    if (event.event_id) number("event_id", *event.event_id);
    if (event.record_id) number("record_id", *event.record_id);
    if (!event.bookmark.empty()) add("bookmark", event.bookmark);
    if (!event.rendered_message.empty()) add("rendered_message", event.rendered_message);
    if (event.render_error) number("render_error", event.render_error);
    result.accepted = output.set_record(event.xml, {severity, timestamp, {fields.data(), count}});
    return result;
}
}  // namespace ckv::logmonitor
