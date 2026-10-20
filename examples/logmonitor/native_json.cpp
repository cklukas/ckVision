// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "native_json.hpp"
#include "cvision/core/utf8.hpp"
#include <algorithm>
#include <charconv>
#include <limits>

namespace ckv::logmonitor {
namespace {
constexpr std::array<std::string_view, 8> apple_keys{
    "processImagePath", "processID", "subsystem", "category", "messageType", "timestamp", "activityIdentifier", "eventMessage"};
constexpr std::array<std::string_view, 8> journal_keys{
    "_SYSTEMD_UNIT", "_COMM", "_PID", "__CURSOR", "PRIORITY", "__REALTIME_TIMESTAMP", "SYSLOG_IDENTIFIER", "MESSAGE"};
class Reader {
public:
    explicit Reader(std::string_view text) : text_(text) {}
    void whitespace() { while (position < text_.size() && (text_[position] == ' ' || text_[position] == '\t' || text_[position] == '\r' || text_[position] == '\n')) ++position; }
    bool eat(char byte) { whitespace(); if (position >= text_.size() || text_[position] != byte) return false; ++position; return true; }
    bool string(std::span<char> target, std::size_t& size, bool& clipped) {
        if (!eat('"')) return false;
        size = 0; clipped = false;
        const auto put = [&](char byte) { if (size < target.size()) target[size++] = byte; else clipped = true; };
        while (position < text_.size()) {
            const auto byte = static_cast<unsigned char>(text_[position++]);
            if (byte == '"') return true;
            if (byte < 32) return false;
            if (byte != '\\') { put(static_cast<char>(byte)); continue; }
            if (position == text_.size()) return false;
            const char escape = text_[position++];
            switch (escape) {
                case '"': case '\\': case '/': put(escape); break;
                case 'b': put('\b'); break; case 'f': put('\f'); break;
                case 'n': put('\n'); break; case 'r': put('\r'); break; case 't': put('\t'); break;
                case 'u': {
                    auto code = hex4(); if (!code) return false; std::uint32_t scalar = *code;
                    if (scalar >= 0xD800 && scalar <= 0xDBFF) {
                        if (text_.substr(position, 2) != "\\u") return false;
                        position += 2;
                        code = hex4(); if (!code || *code < 0xDC00 || *code > 0xDFFF) return false;
                        scalar = 0x10000 + ((scalar - 0xD800) << 10U) + (*code - 0xDC00);
                    } else if (scalar >= 0xDC00 && scalar <= 0xDFFF) return false;
                    if (scalar < 0x80) put(static_cast<char>(scalar));
                    else {
                        const int count = scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
                        put(static_cast<char>((count == 2 ? 0xC0U : count == 3 ? 0xE0U : 0xF0U) | (scalar >> (6U * static_cast<unsigned>(count - 1)))));
                        for (int shift = count - 2; shift >= 0; --shift) put(static_cast<char>(0x80U | ((scalar >> (6U * static_cast<unsigned>(shift))) & 0x3FU)));
                    }
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool value(unsigned depth) {
        whitespace(); if (position == text_.size()) return false;
        if (depth > 16) { limited = true; return false; }
        if (text_[position] == '"') { std::size_t size = 0; bool clipped = false; return string({}, size, clipped); }
        if (text_[position] == '{' || text_[position] == '[') {
            const bool object = text_[position++] == '{'; const char end = object ? '}' : ']';
            if (eat(end)) return true;
            do {
                if (object) { std::size_t size = 0; bool clipped = false; if (!string({}, size, clipped) || !eat(':')) return false; }
                if (!value(depth + 1)) return false;
                if (eat(end)) return true;
            } while (eat(','));
            return false;
        }
        for (auto literal : {std::string_view("null"), std::string_view("true"), std::string_view("false")})
            if (text_.substr(position, literal.size()) == literal) { position += literal.size(); return true; }
        const auto start = position;
        if (text_[position] == '-') ++position;
        if (position == text_.size()) return false;
        if (text_[position] == '0') ++position;
        else { if (text_[position] < '1' || text_[position] > '9') return false; digits(); }
        if (position < text_.size() && text_[position] == '.') { ++position; if (!digits()) return false; }
        if (position < text_.size() && (text_[position] == 'e' || text_[position] == 'E')) {
            ++position; if (position < text_.size() && (text_[position] == '+' || text_[position] == '-')) ++position;
            if (!digits()) return false;
        }
        return position > start;
    }
    std::size_t position = 0; bool limited = false;
private:
    bool digits() { const auto start = position; while (position < text_.size() && text_[position] >= '0' && text_[position] <= '9') ++position; return start != position; }
    std::optional<std::uint32_t> hex4() {
        if (text_.size() - position < 4) return {};
        std::uint32_t result = 0;
        for (int index = 0; index < 4; ++index) {
            const char byte = text_[position++]; unsigned digit;
            if (byte >= '0' && byte <= '9') digit = static_cast<unsigned>(byte - '0');
            else if (byte >= 'a' && byte <= 'f') digit = static_cast<unsigned>(byte - 'a' + 10);
            else if (byte >= 'A' && byte <= 'F') digit = static_cast<unsigned>(byte - 'A' + 10);
            else return {};
            result = result * 16 + digit;
        }
        return result;
    }
    std::string_view text_;
};
std::optional<std::int64_t> integer(std::string_view text) {
    std::int64_t number = 0; const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return {};
    return number;
}
}
std::optional<std::int64_t> native_timestamp(std::string_view text) noexcept {
    if (text.size() < 20 || text[4] != '-' || text[7] != '-' || (text[10] != 'T' && text[10] != ' ') || text[13] != ':' || text[16] != ':') return {};
    const auto component = [&](std::size_t at, std::size_t count) { if (at > text.size() || count > text.size() - at) return std::optional<std::int64_t>{}; const auto part = text.substr(at, count); if (part.find_first_not_of("0123456789") != std::string_view::npos) return std::optional<std::int64_t>{}; return integer(part); };
    const auto year = component(0, 4), month = component(5, 2), day = component(8, 2), hour = component(11, 2), minute = component(14, 2), second = component(17, 2);
    if (!year || !month || !day || !hour || !minute || !second || *year < 1970 || *year > 2262 || *month < 1 || *month > 12 || *hour > 23 || *minute > 59 || *second > 59) return {};
    const bool leap = *year % 4 == 0 && (*year % 100 != 0 || *year % 400 == 0);
    constexpr std::array<int, 12> month_days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (*day < 1 || *day > month_days[static_cast<std::size_t>(*month - 1)] + (*month == 2 && leap ? 1 : 0)) return {};
    std::int64_t days = 0;
    for (std::int64_t current = 1970; current < *year; ++current) days += 365 + (current % 4 == 0 && (current % 100 != 0 || current % 400 == 0) ? 1 : 0);
    for (std::int64_t current = 1; current < *month; ++current) days += month_days[static_cast<std::size_t>(current - 1)] + (current == 2 && leap ? 1 : 0);
    days += *day - 1;
    std::size_t position = 19; std::int64_t fraction = 0, factor = 100'000'000;
    if (position < text.size() && text[position] == '.') {
        const auto start = ++position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
            if (position - start >= 9) return {};
            fraction += (text[position++] - '0') * factor; factor /= 10;
        }
        if (position == start) return {};
    }
    std::int64_t offset = 0;
    if (position < text.size() && text[position] == 'Z') ++position;
    else {
        if (position >= text.size() || (text[position] != '+' && text[position] != '-')) return {};
        const bool positive = text[position++] == '+'; const auto hours = component(position, 2); position += 2;
        if (position < text.size() && text[position] == ':') ++position;
        const auto minutes = component(position, 2); position += 2;
        if (!hours || !minutes || *hours > 23 || *minutes > 59) return {};
        offset = (*hours * 60 + *minutes) * 60 * (positive ? 1 : -1);
    }
    if (position != text.size()) return {};
    const auto seconds = days * 86400 + *hour * 3600 + *minute * 60 + *second - offset;
    if (seconds < 0 || seconds > (std::numeric_limits<std::int64_t>::max() - fraction) / 1'000'000'000) return {};
    return seconds * 1'000'000'000 + fraction;
}
NativeJsonResult NativeJsonDecoder::decode(std::string_view envelope, NativeJsonSchema schema, LogMessage& output) noexcept {
    metadata_clipped_ = false;
    if (envelope.size() > kLogBatchBytes) return NativeJsonResult::Limit;
    if (!utf8::is_valid(envelope)) return NativeJsonResult::Invalid;
    sizes_.fill(0); present_.fill(false); const auto& keys = schema == NativeJsonSchema::Apple ? apple_keys : journal_keys;
    Reader reader(envelope); if (!reader.eat('{')) return NativeJsonResult::Invalid;
    if (!reader.eat('}')) {
        do {
            std::array<char, 128> key{}; std::size_t size = 0; bool clipped = false;
            if (!reader.string(key, size, clipped) || !reader.eat(':')) return NativeJsonResult::Invalid;
            const auto found = clipped ? keys.end() : std::find(keys.begin(), keys.end(), std::string_view(key.data(), size));
            reader.whitespace(); const auto start = reader.position;
            if (found == keys.end()) { if (!reader.value(1)) return reader.limited ? NativeJsonResult::Limit : NativeJsonResult::Invalid; }
            else {
                const auto index = static_cast<std::size_t>(found - keys.begin()); if (present_[index]) return NativeJsonResult::Invalid; present_[index] = true;
                if (start < envelope.size() && envelope[start] == '"') {
                    if (!reader.string(values_[index], sizes_[index], clipped)) return NativeJsonResult::Invalid;
                    // Preserve valid UTF-8 in scalar prefixes, including escapes.
                    if (clipped) { while (sizes_[index] && !utf8::is_valid({values_[index].data(), sizes_[index]})) --sizes_[index]; metadata_clipped_ = true; }
                } else {
                    if (!reader.value(1)) return reader.limited ? NativeJsonResult::Limit : NativeJsonResult::Invalid;
                    const auto raw = envelope.substr(start, reader.position - start);
                    sizes_[index] = std::min(raw.size(), values_[index].size()); std::copy_n(raw.data(), sizes_[index], values_[index].data());
                    if (raw.size() > sizes_[index]) { while (sizes_[index] && !utf8::is_valid({values_[index].data(), sizes_[index]})) --sizes_[index]; metadata_clipped_ = true; }
                }
            }
            if (reader.eat('}')) break;
            if (!reader.eat(',')) return NativeJsonResult::Invalid;
        } while (true);
    }
    reader.whitespace(); if (reader.position != envelope.size()) return NativeJsonResult::Invalid;
    // Require the event payload field; helper banners are not log events.
    if (!present_[7]) return NativeJsonResult::Invalid;
    std::array<LogField, 8> fields{}; std::size_t count = 0;
    for (std::size_t index = 0; index < 8; ++index) if (present_[index]) fields[count++] = {keys[index], {values_[index].data(), sizes_[index]}};
    const auto scalar = [&](std::size_t index) { return std::string_view(values_[index].data(), sizes_[index]); };
    LogSeverity severity = LogSeverity::Unknown; std::optional<std::int64_t> stamp;
    if (schema == NativeJsonSchema::Apple) {
        if (scalar(4) == "Debug") severity = LogSeverity::Debug;
        else if (scalar(4) == "Info") severity = LogSeverity::Info;
        else if (scalar(4) == "Default") severity = LogSeverity::Notice;
        else if (scalar(4) == "Error") severity = LogSeverity::Error;
        else if (scalar(4) == "Fault") severity = LogSeverity::Critical;
        stamp = native_timestamp(scalar(5));
    } else {
        constexpr std::array levels{LogSeverity::Critical, LogSeverity::Critical, LogSeverity::Critical, LogSeverity::Error, LogSeverity::Warning, LogSeverity::Notice, LogSeverity::Info, LogSeverity::Debug};
        const auto priority = integer(scalar(4)); if (priority && *priority >= 0 && *priority < 8) severity = levels[static_cast<std::size_t>(*priority)];
        const auto micros = integer(scalar(5)); if (micros && *micros >= 0 && *micros <= std::numeric_limits<std::int64_t>::max() / 1000) stamp = *micros * 1000;
    }
    // Attr::None is intentionally not assumed; existing default is zero.
    if (!output.set_record(envelope, {severity, stamp, {fields.data(), count}})) return NativeJsonResult::Limit;
    return NativeJsonResult::Record;
}
}  // namespace ckv::logmonitor
