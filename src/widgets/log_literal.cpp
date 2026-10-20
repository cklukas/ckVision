// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_literal.hpp"

#include "cvision/core/utf8.hpp"

namespace ckv::widgets {
LogLiteral::LogLiteral() {
    pattern_.reserve(kLogLiteralBytes);
    prefix_.reserve(kLogLiteralBytes);
}
unsigned char LogLiteral::fold(unsigned char value) const noexcept {
    return !sensitive_ && value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}
bool LogLiteral::valid(std::string_view value) noexcept {
    if (value.size() > kLogLiteralBytes) return false;
    while (!value.empty()) {
        std::size_t consumed = 0;
        const auto scalar = utf8::decode(value, consumed);
        if (scalar == utf8::replacement_char && value.substr(0, consumed) != "\xEF\xBF\xBD") return false;
        if ((scalar < 0x20 && scalar != U'\n') || (scalar >= 0x7F && scalar <= 0x9F)) return false;
        value.remove_prefix(consumed);
    }
    return true;
}
bool LogLiteral::set(std::string_view value, bool sensitive) noexcept {
    if (!valid(value)) return false;
    pattern_.assign(value);
    sensitive_ = sensitive;
    prefix_.resize(value.size());
    std::size_t length = 0;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i != 0) {
            while (length && fold(static_cast<unsigned char>(value[i])) != fold(static_cast<unsigned char>(value[length])))
                length = prefix_[length - 1];
            if (fold(static_cast<unsigned char>(value[i])) == fold(static_cast<unsigned char>(value[length]))) ++length;
        }
        prefix_[i] = length;
    }
    return true;
}
LogLiteralStep LogLiteral::scan(std::string_view value, LogLiteralCursor& cursor,
                                std::size_t& operations, LogTextRange& match) const noexcept {
    if (pattern_.empty() || cursor.byte >= value.size()) return LogLiteralStep::End;
    // A cursor belongs to this unchanged pattern/value. Defensively retire
    // invalid offsets instead of indexing a different pattern after misuse.
    if (cursor.matched >= pattern_.size()) return LogLiteralStep::End;
    while (cursor.byte < value.size()) {
        if (operations == 0) return LogLiteralStep::Pending;
        --operations;
        const auto byte = fold(static_cast<unsigned char>(value[cursor.byte]));
        if (byte == fold(static_cast<unsigned char>(pattern_[cursor.matched]))) {
            ++cursor.byte;
            ++cursor.matched;
            if (cursor.matched == pattern_.size()) {
                match = {cursor.byte - pattern_.size(), cursor.byte};
                cursor.matched = prefix_[cursor.matched - 1];
                return LogLiteralStep::Match;
            }
        } else if (cursor.matched) cursor.matched = prefix_[cursor.matched - 1];
        else ++cursor.byte;
    }
    return LogLiteralStep::End;
}
}  // namespace ckv::widgets
