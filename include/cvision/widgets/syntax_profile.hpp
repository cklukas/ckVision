// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Instance-owned language profiles and line-state highlighters for TextEditor.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::widgets {

enum class SyntaxTokenKind {
    Plain,
    Keyword,
    Type,
    Property,
    String,
    Number,
    Comment,
    Command,
    Operator,
    Escape,
    Error,
};

struct SyntaxSpan {
    std::size_t begin_byte = 0;
    std::size_t end_byte = 0;
    SyntaxTokenKind kind = SyntaxTokenKind::Plain;

    friend bool operator==(const SyntaxSpan&, const SyntaxSpan&) = default;
};

struct SyntaxLineResult {
    std::vector<SyntaxSpan> spans;
    std::string next_state;
};

struct LanguageDetectionInput {
    std::optional<std::string> requested_profile;
    std::string file_name;
    std::string content_prefix;
    std::string shebang;
};

struct LanguageDetection {
    int score = 0;
    std::string reason;
};

using SyntaxLineHighlighter = std::function<SyntaxLineResult(std::string_view line, std::string_view incoming_state)>;
using LanguageDetector = std::function<LanguageDetection(const LanguageDetectionInput&)>;

struct LanguageProfile {
    std::string id;
    std::string display_name;
    LanguageDetector detect;
    SyntaxLineHighlighter highlight_line;
};

// No global registration: applications own a registry and pass it to editors.
class SyntaxProfileRegistry {
public:
    bool register_profile(LanguageProfile profile);
    const LanguageProfile* find(std::string_view id) const noexcept;
    const LanguageProfile& detect(const LanguageDetectionInput& input) const noexcept;
    const LanguageProfile& plain_text() const noexcept;
    std::size_t size() const noexcept { return profiles_.size(); }

private:
    std::vector<LanguageProfile> profiles_;
};

void register_standard_syntax_profiles(SyntaxProfileRegistry& registry);

}  // namespace ckv::widgets
