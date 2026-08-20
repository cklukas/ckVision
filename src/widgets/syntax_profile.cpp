// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/syntax_profile.hpp"

#include <algorithm>
#include <string_view>

namespace ckv::widgets {
namespace {

bool ends_with(std::string_view value, std::string_view suffix) noexcept {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

bool ascii_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

bool ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

bool ascii_alpha(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

bool ascii_alnum(char value) noexcept { return ascii_alpha(value) || ascii_digit(value); }

std::string_view trim_ascii_space(std::string_view value) noexcept {
    std::size_t begin = 0;
    while (begin < value.size() && ascii_space(value[begin])) ++begin;
    return value.substr(begin);
}

bool word_at(std::string_view value, std::size_t begin, std::size_t end, std::initializer_list<std::string_view> words) {
    const std::string_view token = value.substr(begin, end - begin);
    return std::find(words.begin(), words.end(), token) != words.end();
}

void add(std::vector<SyntaxSpan>& spans, std::size_t begin, std::size_t end, SyntaxTokenKind kind) {
    if (begin < end) spans.push_back(SyntaxSpan{begin, end, kind});
}

SyntaxLineResult json_line(std::string_view line, std::string_view) {
    SyntaxLineResult result;
    for (std::size_t i = 0; i < line.size();) {
        const char ch = line[i];
        if (ascii_space(ch)) {
            ++i;
        } else if (line[i] == '"') {
            const std::size_t begin = i++;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) i += 2;
                else if (line[i] == '"') {
                    ++i;
                    closed = true;
                    break;
                } else {
                    ++i;
                }
            }
            std::size_t after = i;
            while (after < line.size() && ascii_space(line[after])) ++after;
            add(result.spans, begin, i, closed && after < line.size() && line[after] == ':' ? SyntaxTokenKind::Property
                                                                                              : (closed ? SyntaxTokenKind::String : SyntaxTokenKind::Error));
        } else if (ascii_digit(ch) || line[i] == '-') {
            const std::size_t begin = i++;
            while (i < line.size() && (ascii_digit(line[i]) || line[i] == '.' ||
                                       line[i] == 'e' || line[i] == 'E' || line[i] == '+' || line[i] == '-'))
                ++i;
            add(result.spans, begin, i, SyntaxTokenKind::Number);
        } else if (ascii_alpha(ch)) {
            const std::size_t begin = i++;
            while (i < line.size() && ascii_alpha(line[i])) ++i;
            add(result.spans, begin, i, word_at(line, begin, i, {"true", "false", "null"}) ? SyntaxTokenKind::Keyword
                                                                                                  : SyntaxTokenKind::Error);
        } else {
            add(result.spans, i, i + 1, (line[i] == '{' || line[i] == '}' || line[i] == '[' || line[i] == ']' ||
                                         line[i] == ':' || line[i] == ',') ? SyntaxTokenKind::Operator : SyntaxTokenKind::Error);
            ++i;
        }
    }
    return result;
}

SyntaxLineResult yaml_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    result.next_state = std::string(incoming);
    const std::size_t comment = line.find('#');
    const std::size_t content_end = comment == std::string_view::npos ? line.size() : comment;
    if (comment != std::string_view::npos) add(result.spans, comment, line.size(), SyntaxTokenKind::Comment);
    std::size_t begin = 0;
    while (begin < content_end && ascii_space(line[begin])) ++begin;
    if (begin < content_end && line[begin] == '%') add(result.spans, begin, content_end, SyntaxTokenKind::Keyword);
    if (begin < content_end && line[begin] == '-') add(result.spans, begin, begin + 1U, SyntaxTokenKind::Operator);
    for (std::size_t token = begin; token < content_end;) {
        if (line[token] != '!' && line[token] != '&' && line[token] != '*') {
            ++token;
            continue;
        }
        const std::size_t token_begin = token++;
        while (token < content_end && (ascii_alnum(line[token]) || line[token] == '_' || line[token] == '-')) ++token;
        add(result.spans, token_begin, token, SyntaxTokenKind::Type);
    }
    const std::size_t colon = line.substr(begin, content_end - begin).find(':');
    if (colon != std::string_view::npos) {
        const std::size_t key_end = begin + colon;
        add(result.spans, begin, key_end, SyntaxTokenKind::Property);
        add(result.spans, key_end, key_end + 1, SyntaxTokenKind::Operator);
        std::size_t value = key_end + 1;
        while (value < content_end && ascii_space(line[value])) ++value;
        if (value < content_end && (line[value] == '\'' || line[value] == '"'))
            add(result.spans, value, content_end, SyntaxTokenKind::String);
        else if (value < content_end)
            add(result.spans, value, content_end, SyntaxTokenKind::Plain);
    }
    return result;
}

SyntaxLineResult bash_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    constexpr std::string_view heredoc_prefix = "heredoc:";
    if (incoming.starts_with(heredoc_prefix)) {
        const std::string_view delimiter = incoming.substr(heredoc_prefix.size());
        add(result.spans, 0, line.size(), line == delimiter ? SyntaxTokenKind::Operator : SyntaxTokenKind::String);
        if (line != delimiter) result.next_state = std::string(incoming);
        return result;
    }
    bool in_single = incoming == "single";
    bool in_double = incoming == "double";
    std::size_t segment = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (!in_single && !in_double && line[i] == '#') {
            add(result.spans, segment, i, SyntaxTokenKind::Plain);
            add(result.spans, i, line.size(), SyntaxTokenKind::Comment);
            return result;
        }
        if (!in_double && line[i] == '\'') {
            if (!in_single) segment = i;
            in_single = !in_single;
            if (!in_single) { add(result.spans, segment, i + 1, SyntaxTokenKind::String); segment = i + 1; }
        } else if (!in_single && line[i] == '"') {
            if (!in_double) segment = i;
            in_double = !in_double;
            if (!in_double) { add(result.spans, segment, i + 1, SyntaxTokenKind::String); segment = i + 1; }
        } else if (!in_single && !in_double && line[i] == '$') {
            std::size_t end = i + 1;
            while (end < line.size() && (ascii_alnum(line[end]) || line[end] == '_')) ++end;
            add(result.spans, i, end, SyntaxTokenKind::Property);
            i = end == 0 ? i : end - 1;
            segment = end;
        } else if (!in_single && !in_double && (line[i] == '|' || line[i] == ';' || line[i] == '&' ||
                                                line[i] == '<' || line[i] == '>')) {
            add(result.spans, i, i + 1, SyntaxTokenKind::Operator);
        }
    }
    if (in_single || in_double) {
        add(result.spans, segment, line.size(), SyntaxTokenKind::String);
        result.next_state = in_single ? "single" : "double";
    } else {
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && !ascii_alpha(line[i])) ++i;
            const std::size_t begin = i;
            while (i < line.size() && (ascii_alnum(line[i]) || line[i] == '_')) ++i;
            if (word_at(line, begin, i, {"if", "then", "fi", "for", "in", "do", "done", "case", "esac", "while", "function"}))
                add(result.spans, begin, i, SyntaxTokenKind::Keyword);
        }
        std::size_t command = 0;
        while (command < line.size() && ascii_space(line[command])) ++command;
        const std::size_t command_begin = command;
        while (command < line.size() && (ascii_alnum(line[command]) || line[command] == '_' || line[command] == '-' || line[command] == '.')) ++command;
        if (command > command_begin && !word_at(line, command_begin, command,
                                                  {"if", "then", "fi", "for", "in", "do", "done", "case", "esac", "while", "function"}))
            add(result.spans, command_begin, command, SyntaxTokenKind::Command);

        const std::size_t heredoc = line.find("<<");
        if (heredoc != std::string_view::npos) {
            std::size_t begin = heredoc + 2U;
            if (begin < line.size() && line[begin] == '-') ++begin;
            while (begin < line.size() && ascii_space(line[begin])) ++begin;
            std::size_t end = begin;
            while (end < line.size() && (ascii_alnum(line[end]) || line[end] == '_')) ++end;
            if (end > begin) {
                add(result.spans, heredoc, heredoc + 2U, SyntaxTokenKind::Operator);
                add(result.spans, begin, end, SyntaxTokenKind::String);
                result.next_state = std::string(heredoc_prefix) + std::string(line.substr(begin, end - begin));
            }
        }
    }
    return result;
}

LanguageProfile plain_profile() {
    return LanguageProfile{"plain", "Plain text", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
                           [](std::string_view, std::string_view state) { return SyntaxLineResult{{}, std::string(state)}; }};
}

}  // namespace

bool SyntaxProfileRegistry::register_profile(LanguageProfile profile) {
    if (profile.id.empty() || !profile.detect || !profile.highlight_line || find(profile.id) != nullptr) return false;
    profiles_.push_back(std::move(profile));
    return true;
}

const LanguageProfile* SyntaxProfileRegistry::find(std::string_view id) const noexcept {
    const auto match = std::find_if(profiles_.begin(), profiles_.end(), [id](const LanguageProfile& profile) { return profile.id == id; });
    return match == profiles_.end() ? nullptr : &*match;
}

const LanguageProfile& SyntaxProfileRegistry::plain_text() const noexcept {
    if (const auto* profile = find("plain")) return *profile;
    static const LanguageProfile fallback = plain_profile();
    return fallback;
}

const LanguageProfile& SyntaxProfileRegistry::detect(const LanguageDetectionInput& input) const noexcept {
    if (input.requested_profile) {
        if (const auto* profile = find(*input.requested_profile)) return *profile;
    }
    const LanguageProfile* best = &plain_text();
    int best_score = 0;
    for (const LanguageProfile& profile : profiles_) {
        const LanguageDetection candidate = profile.detect(input);
        if (candidate.score > best_score || (candidate.score == best_score && candidate.score > 0 && profile.id < best->id)) {
            best = &profile;
            best_score = candidate.score;
        }
    }
    return *best;
}

void register_standard_syntax_profiles(SyntaxProfileRegistry& registry) {
    (void)registry.register_profile(plain_profile());
    (void)registry.register_profile(LanguageProfile{
        "json", "JSON",
        [](const LanguageDetectionInput& input) {
            if (ends_with(input.file_name, ".json") || ends_with(input.file_name, ".jsonc")) return LanguageDetection{80, "file suffix"};
            const std::string_view prefix = trim_ascii_space(input.content_prefix);
            if (!prefix.empty() && (prefix.front() == '{' || prefix.front() == '[')) return LanguageDetection{50, "content prefix"};
            return LanguageDetection{};
        },
        json_line});
    (void)registry.register_profile(LanguageProfile{
        "yaml", "YAML",
        [](const LanguageDetectionInput& input) {
            if (ends_with(input.file_name, ".yaml") || ends_with(input.file_name, ".yml")) return LanguageDetection{80, "file suffix"};
            const std::string_view prefix = trim_ascii_space(input.content_prefix);
            if (prefix.starts_with("---") || prefix.find(":") != std::string_view::npos) return LanguageDetection{30, "content prefix"};
            return LanguageDetection{};
        },
        yaml_line});
    (void)registry.register_profile(LanguageProfile{
        "bash", "Bash",
        [](const LanguageDetectionInput& input) {
            if (ends_with(input.file_name, ".sh") || ends_with(input.file_name, ".bash")) return LanguageDetection{80, "file suffix"};
            if (input.shebang.find("bash") != std::string::npos) return LanguageDetection{70, "shebang"};
            if (trim_ascii_space(input.content_prefix).starts_with("#!") && input.content_prefix.find("bash") != std::string::npos)
                return LanguageDetection{70, "content prefix shebang"};
            return LanguageDetection{};
        },
        bash_line});
}

}  // namespace ckv::widgets
