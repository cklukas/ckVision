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

// ---- Markdown -------------------------------------------------------------
//
// Line states. The cache hands the first line an empty incoming state and
// every later line the state its predecessor returned; this profile never
// returns an empty state, so an empty incoming state means "document start"
// and a `---` there opens the front matter while a `---` in the body is a
// thematic break. "front" is inside the front matter, "body" is ordinary
// Markdown, "fence:<run>" is inside a fenced code block opened by that run.
constexpr std::string_view markdown_body_state = "body";
constexpr std::string_view markdown_front_state = "front";
constexpr std::string_view markdown_fence_prefix = "fence:";

bool ascii_punct(char value) noexcept {
    return (value >= '!' && value <= '/') || (value >= ':' && value <= '@') || (value >= '[' && value <= '`') ||
           (value >= '{' && value <= '~');
}

std::string_view trim_ascii_space_right(std::string_view value) noexcept {
    std::size_t end = value.size();
    while (end > 0 && ascii_space(value[end - 1U])) --end;
    return value.substr(0, end);
}

bool front_matter_delimiter(std::string_view line) noexcept { return trim_ascii_space_right(line) == "---"; }

std::size_t leading_indent(std::string_view line) noexcept {
    std::size_t indent = 0;
    while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) ++indent;
    return indent;
}

// Byte count of the `#` run of an ATX heading, zero when the content is not
// one: up to six marks followed by a space or the end of the line, so a
// `#hashtag` stays text.
std::size_t atx_heading_marks(std::string_view content) noexcept {
    std::size_t marks = 0;
    while (marks < content.size() && content[marks] == '#') ++marks;
    if (marks == 0 || marks > 6) return 0;
    if (marks < content.size() && !ascii_space(content[marks])) return 0;
    return marks;
}

// Byte count of the fence run that opens or closes a fenced code block: three
// or more backticks or tildes.
std::size_t fence_run(std::string_view content) noexcept {
    if (content.empty() || (content.front() != '`' && content.front() != '~')) return 0;
    std::size_t run = 0;
    while (run < content.size() && content[run] == content.front()) ++run;
    return run >= 3 ? run : 0;
}

// A line made only of three or more `-`, `*`, `_` or `=` and spaces: a
// thematic break, or the underline of a setext heading.
bool markup_rule(std::string_view content) noexcept {
    if (content.empty()) return false;
    const char mark = content.front();
    if (mark != '-' && mark != '*' && mark != '_' && mark != '=') return false;
    std::size_t count = 0;
    for (const char value : content) {
        if (value == mark) ++count;
        else if (!ascii_space(value)) return false;
    }
    return count >= 3;
}

// Byte count of a list marker at the start of the content and the kind that
// paints it: `-`, `+` or `*` before a space, or up to nine digits and `.`
// or `)` before a space.
struct ListMarker {
    std::size_t size = 0;
    SyntaxTokenKind kind = SyntaxTokenKind::Plain;
};

ListMarker list_marker(std::string_view content) noexcept {
    if (content.empty()) return {};
    const auto ends_item = [content](std::size_t at) { return at == content.size() || ascii_space(content[at]); };
    if ((content.front() == '-' || content.front() == '+' || content.front() == '*') && ends_item(1U))
        return ListMarker{1U, SyntaxTokenKind::Operator};
    std::size_t digits = 0;
    while (digits < content.size() && digits < 9U && ascii_digit(content[digits])) ++digits;
    if (digits > 0 && digits < content.size() && (content[digits] == '.' || content[digits] == ')') && ends_item(digits + 1U))
        return ListMarker{digits + 1U, SyntaxTokenKind::Number};
    return {};
}

// The closing bracket matching an opener at `open`, honouring nesting and
// backslash escapes; npos when the line has none.
std::size_t matching_bracket(std::string_view line, std::size_t open, char opener, char closer) noexcept {
    std::size_t depth = 0;
    for (std::size_t i = open; i < line.size(); ++i) {
        if (line[i] == '\\') { ++i; continue; }
        if (line[i] == opener) ++depth;
        else if (line[i] == closer && --depth == 0) return i;
    }
    return std::string_view::npos;
}

// Inline constructs from `begin` to the end of the line: backslash escapes,
// code spans, emphasis and strong emphasis, and links. The scan is left to
// right and every construct it paints is skipped whole, so spans never
// overlap.
void markdown_inline(std::string_view line, std::size_t begin, std::vector<SyntaxSpan>& spans) {
    const std::size_t size = line.size();
    std::size_t i = begin;
    while (i < size) {
        const char ch = line[i];
        if (ch == '\\') {
            if (i + 1U < size && ascii_punct(line[i + 1U])) {
                add(spans, i, i + 2U, SyntaxTokenKind::Escape);
                i += 2U;
            } else {
                ++i;
            }
            continue;
        }
        if (ch == '`') {
            std::size_t run = 0;
            while (i + run < size && line[i + run] == '`') ++run;
            std::size_t close = std::string_view::npos;
            for (std::size_t j = i + run; j < size;) {
                if (line[j] != '`') { ++j; continue; }
                std::size_t candidate = 0;
                while (j + candidate < size && line[j + candidate] == '`') ++candidate;
                if (candidate == run) { close = j + candidate; break; }
                j += candidate;
            }
            if (close == std::string_view::npos) { i += run; continue; }
            add(spans, i, close, SyntaxTokenKind::String);
            i = close;
            continue;
        }
        if (ch == '*' || ch == '_') {
            std::size_t run = 0;
            while (i + run < size && line[i + run] == ch) ++run;
            const bool intraword_opener = ch == '_' && i > 0 && ascii_alnum(line[i - 1U]);
            const bool opens = !intraword_opener && i + run < size && !ascii_space(line[i + run]);
            std::size_t close = std::string_view::npos;
            for (std::size_t j = i + run; opens && j < size;) {
                if (line[j] == '\\') { j += 2U; continue; }
                if (line[j] != ch) { ++j; continue; }
                std::size_t candidate = 0;
                while (j + candidate < size && line[j + candidate] == ch) ++candidate;
                const bool intraword_closer = ch == '_' && j + candidate < size && ascii_alnum(line[j + candidate]);
                if (candidate == run && !ascii_space(line[j - 1U]) && !intraword_closer) { close = j + candidate; break; }
                j += candidate;
            }
            if (close == std::string_view::npos) { i += run; continue; }
            add(spans, i, close, run >= 2U ? SyntaxTokenKind::Keyword : SyntaxTokenKind::Type);
            i = close;
            continue;
        }
        if (ch == '[' || (ch == '!' && i + 1U < size && line[i + 1U] == '[')) {
            const std::size_t open = ch == '!' ? i + 1U : i;
            const std::size_t text_close = matching_bracket(line, open, '[', ']');
            if (text_close == std::string_view::npos) { i = open + 1U; continue; }
            const std::size_t after = text_close + 1U;
            if (after < size && line[after] == '(') {
                const std::size_t target_close = matching_bracket(line, after, '(', ')');
                if (target_close != std::string_view::npos) {
                    add(spans, i, after, SyntaxTokenKind::Property);
                    add(spans, after, target_close + 1U, SyntaxTokenKind::String);
                    i = target_close + 1U;
                    continue;
                }
            } else if (after < size && line[after] == '[') {
                const std::size_t label_close = matching_bracket(line, after, '[', ']');
                if (label_close != std::string_view::npos) {
                    add(spans, i, label_close + 1U, SyntaxTokenKind::Property);
                    i = label_close + 1U;
                    continue;
                }
            }
            i = open + 1U;
            continue;
        }
        ++i;
    }
}

SyntaxLineResult markdown_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    if (incoming.starts_with(markdown_fence_prefix)) {
        const std::string_view opener = incoming.substr(markdown_fence_prefix.size());
        const std::size_t indent = leading_indent(line);
        const std::string_view content = trim_ascii_space_right(line.substr(indent));
        const std::size_t run = fence_run(content);
        if (run >= opener.size() && run == content.size() && content.front() == opener.front()) {
            add(result.spans, indent, indent + run, SyntaxTokenKind::Operator);
            result.next_state = std::string(markdown_body_state);
        } else {
            add(result.spans, 0, line.size(), SyntaxTokenKind::String);
            result.next_state = std::string(incoming);
        }
        return result;
    }
    if (incoming == markdown_front_state) {
        if (front_matter_delimiter(line)) {
            add(result.spans, 0, line.size(), SyntaxTokenKind::Operator);
            result.next_state = std::string(markdown_body_state);
        } else {
            result.spans = yaml_line(line, {}).spans;
            result.next_state = std::string(markdown_front_state);
        }
        return result;
    }
    result.next_state = std::string(markdown_body_state);
    if (incoming.empty() && front_matter_delimiter(line)) {
        add(result.spans, 0, line.size(), SyntaxTokenKind::Operator);
        result.next_state = std::string(markdown_front_state);
        return result;
    }
    const std::size_t indent = leading_indent(line);
    const std::string_view content = line.substr(indent);
    if (content.empty()) return result;
    if (content.front() == '>') {
        add(result.spans, indent, indent + 1U, SyntaxTokenKind::Operator);
        add(result.spans, indent + 1U, line.size(), SyntaxTokenKind::Comment);
        return result;
    }
    if (const std::size_t run = fence_run(content); run > 0 &&
        (content.front() == '~' || content.find('`', run) == std::string_view::npos)) {
        add(result.spans, indent, indent + run, SyntaxTokenKind::Operator);
        const std::size_t info = indent + run;
        add(result.spans, info, trim_ascii_space_right(line).size(), SyntaxTokenKind::Type);
        result.next_state = std::string(markdown_fence_prefix) + std::string(content.substr(0, run));
        return result;
    }
    if (atx_heading_marks(content) > 0) {
        add(result.spans, indent, line.size(), SyntaxTokenKind::Keyword);
        return result;
    }
    if (markup_rule(content)) {
        add(result.spans, indent, line.size(), SyntaxTokenKind::Operator);
        return result;
    }
    if (content.starts_with("::")) {
        std::size_t name_end = 2U;
        while (name_end < content.size() && (ascii_alnum(content[name_end]) || content[name_end] == '_' || content[name_end] == '-'))
            ++name_end;
        add(result.spans, indent, indent + name_end, SyntaxTokenKind::Command);
        std::size_t rest = indent + name_end;
        if (rest < line.size() && line[rest] == '{') {
            const std::size_t close = matching_bracket(line, rest, '{', '}');
            const std::size_t end = close == std::string_view::npos ? line.size() : close + 1U;
            add(result.spans, rest, end, SyntaxTokenKind::Property);
            rest = end;
        }
        markdown_inline(line, rest, result.spans);
        return result;
    }
    std::size_t text = indent;
    if (const ListMarker marker = list_marker(content); marker.size > 0) {
        add(result.spans, indent, indent + marker.size, marker.kind);
        text = indent + marker.size;
    }
    markdown_inline(line, text, result.spans);
    return result;
}

// True when the prefix opens with a YAML front matter block: a `---` first
// line, then `key: value` lines (blank lines, comments, and indented or
// sequence continuation lines between them), then a closing `---`.
bool markdown_front_matter(std::string_view prefix) noexcept {
    std::size_t line_begin = 0;
    bool first = true;
    bool keys = false;
    while (line_begin < prefix.size()) {
        const std::size_t newline = prefix.find('\n', line_begin);
        const std::string_view line = prefix.substr(line_begin, newline == std::string_view::npos ? std::string_view::npos
                                                                                                    : newline - line_begin);
        line_begin = newline == std::string_view::npos ? prefix.size() : newline + 1U;
        const std::string_view content = trim_ascii_space_right(line);
        if (first) {
            if (!front_matter_delimiter(content)) return false;
            first = false;
            continue;
        }
        if (front_matter_delimiter(content)) return keys;
        if (content.empty() || content.front() == ' ' || content.front() == '\t' || content.front() == '#' ||
            content.starts_with("- "))
            continue;
        std::size_t key_end = 0;
        while (key_end < content.size() && (ascii_alnum(content[key_end]) || content[key_end] == '_' ||
                                            content[key_end] == '-' || content[key_end] == '.'))
            ++key_end;
        if (key_end == 0 || key_end >= content.size() || content[key_end] != ':' ||
            (key_end + 1U < content.size() && !ascii_space(content[key_end + 1U])))
            return false;
        keys = true;
    }
    return false;
}

std::string_view first_non_blank_line(std::string_view prefix) noexcept {
    std::size_t line_begin = 0;
    while (line_begin < prefix.size()) {
        const std::size_t newline = prefix.find('\n', line_begin);
        const std::string_view line = prefix.substr(line_begin, newline == std::string_view::npos ? std::string_view::npos
                                                                                                    : newline - line_begin);
        line_begin = newline == std::string_view::npos ? prefix.size() : newline + 1U;
        if (!trim_ascii_space(line).empty()) return line;
    }
    return {};
}

LanguageDetection markdown_detect(const LanguageDetectionInput& input) {
    if (ends_with(input.file_name, ".md") || ends_with(input.file_name, ".markdown")) return LanguageDetection{80, "file suffix"};
    if (markdown_front_matter(input.content_prefix)) return LanguageDetection{60, "content front matter"};
    const std::string_view opening = trim_ascii_space(first_non_blank_line(input.content_prefix));
    if (atx_heading_marks(opening) > 0) return LanguageDetection{40, "content heading"};
    return LanguageDetection{};
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
    (void)registry.register_profile(LanguageProfile{"markdown", "Markdown", markdown_detect, markdown_line});
}

}  // namespace ckv::widgets
