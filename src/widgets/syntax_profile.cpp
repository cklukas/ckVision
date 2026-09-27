// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/syntax_profile.hpp"

#include <algorithm>
#include <string_view>

#include "cvision/core/text.hpp"

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

// A character of a YAML anchor or tag name, or of a Markdown directive name.
bool ascii_name_char(char value) noexcept { return ascii_alnum(value) || value == '_' || value == '-'; }

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

// ---- Grapheme clusters ------------------------------------------------------
//
// SyntaxSpan requires both ends of a span on grapheme-cluster boundaries, so no
// lexer here steps by bytes. Each walks its line one cluster at a time from the
// line's start, and every position it stands on, starts a token at or ends one
// at is a cluster boundary. The grammars stay ASCII: a cluster is recognised by
// its first byte, so `{` with a combining mark on it is still an opening brace,
// a digit with one still continues a number, and a character outside the
// grammar is one token however many bytes encode it. Searches walk clusters
// too, because a byte search can land inside a cluster: a prepended mark such
// as U+0600 joins the character after it.

// The end of the cluster that begins at `at`, or the end of the line when `at`
// is there.
std::size_t next_cluster(std::string_view line, std::size_t at) noexcept {
    return at < line.size() ? text::grapheme_end(line, at) : line.size();
}

// The first cluster boundary at or after `at` whose cluster does not begin with
// a byte `accept` takes, or the end of the line.
template <typename Accept>
std::size_t skip_clusters(std::string_view line, std::size_t at, Accept accept) {
    while (at < line.size() && accept(line[at])) at = next_cluster(line, at);
    return at;
}

// The end of the clusters from `at` whose first bytes spell `pattern`, or npos
// when they do not.
std::size_t match_clusters(std::string_view line, std::size_t at, std::string_view pattern) noexcept {
    for (const char ch : pattern) {
        if (at >= line.size() || line[at] != ch) return std::string_view::npos;
        at = next_cluster(line, at);
    }
    return at;
}

// Where clusters spelling a pattern begin and end.
struct ClusterMatch {
    std::size_t begin = std::string_view::npos;
    std::size_t end = std::string_view::npos;

    bool found() const noexcept { return begin != std::string_view::npos; }
};

// The first clusters at or after the boundary `from` whose first bytes spell
// `pattern`; not found() when there are none.
ClusterMatch find_clusters(std::string_view line, std::size_t from, std::string_view pattern) noexcept {
    for (std::size_t at = from; at < line.size(); at = next_cluster(line, at))
        if (const std::size_t end = match_clusters(line, at, pattern); end != std::string_view::npos)
            return ClusterMatch{at, end};
    return {};
}

// A run of clusters that all begin with one byte: how many, and where it ends.
struct ClusterRun {
    std::size_t count = 0;
    std::size_t end = 0;
};

ClusterRun cluster_run(std::string_view line, std::size_t at, char mark) noexcept {
    ClusterRun run{0, at};
    while (run.end < line.size() && line[run.end] == mark) {
        run.end = next_cluster(line, run.end);
        ++run.count;
    }
    return run;
}

// The end of the last cluster at or after the boundary `from` that does not
// begin with ASCII space, or `from` when there is none.
std::size_t end_before_trailing_space(std::string_view line, std::size_t from) noexcept {
    std::size_t end = from;
    for (std::size_t at = from; at < line.size();) {
        const char ch = line[at];
        at = next_cluster(line, at);
        if (!ascii_space(ch)) end = at;
    }
    return end;
}

bool json_number_char(char value) noexcept {
    return ascii_digit(value) || value == '.' || value == 'e' || value == 'E' || value == '+' || value == '-';
}

SyntaxLineResult json_line(std::string_view line, std::string_view) {
    SyntaxLineResult result;
    for (std::size_t i = 0; i < line.size();) {
        const char ch = line[i];
        const std::size_t begin = i;
        i = next_cluster(line, i);
        if (ascii_space(ch)) continue;
        if (ch == '"') {
            bool closed = false;
            while (i < line.size() && !closed) {
                const char inner = line[i];
                i = next_cluster(line, i);
                if (inner == '\\') i = next_cluster(line, i);
                closed = inner == '"';
            }
            const std::size_t after = skip_clusters(line, i, ascii_space);
            add(result.spans, begin, i, closed && after < line.size() && line[after] == ':' ? SyntaxTokenKind::Property
                                                                                              : (closed ? SyntaxTokenKind::String : SyntaxTokenKind::Error));
        } else if (ascii_digit(ch) || ch == '-') {
            i = skip_clusters(line, i, json_number_char);
            add(result.spans, begin, i, SyntaxTokenKind::Number);
        } else if (ascii_alpha(ch)) {
            i = skip_clusters(line, i, ascii_alpha);
            add(result.spans, begin, i, word_at(line, begin, i, {"true", "false", "null"}) ? SyntaxTokenKind::Keyword
                                                                                                  : SyntaxTokenKind::Error);
        } else {
            add(result.spans, begin, i, (ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == ':' || ch == ',')
                                            ? SyntaxTokenKind::Operator : SyntaxTokenKind::Error);
        }
    }
    return result;
}

SyntaxLineResult yaml_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    result.next_state = std::string(incoming);
    // The comment's start is a cluster boundary, so the content before it is
    // walked by the same clusters as the whole line.
    const ClusterMatch comment = find_clusters(line, 0, "#");
    if (comment.found()) add(result.spans, comment.begin, line.size(), SyntaxTokenKind::Comment);
    const std::string_view content = line.substr(0, comment.begin);
    const std::size_t begin = skip_clusters(content, 0, ascii_space);
    if (begin < content.size() && content[begin] == '%') add(result.spans, begin, content.size(), SyntaxTokenKind::Keyword);
    if (begin < content.size() && content[begin] == '-')
        add(result.spans, begin, next_cluster(content, begin), SyntaxTokenKind::Operator);
    for (std::size_t token = begin; token < content.size();) {
        const char ch = content[token];
        const std::size_t token_begin = token;
        token = next_cluster(content, token);
        if (ch != '!' && ch != '&' && ch != '*') continue;
        token = skip_clusters(content, token, ascii_name_char);
        add(result.spans, token_begin, token, SyntaxTokenKind::Type);
    }
    if (const ClusterMatch colon = find_clusters(content, begin, ":"); colon.found()) {
        add(result.spans, begin, colon.begin, SyntaxTokenKind::Property);
        add(result.spans, colon.begin, colon.end, SyntaxTokenKind::Operator);
        const std::size_t value = skip_clusters(content, colon.end, ascii_space);
        if (value < content.size() && (content[value] == '\'' || content[value] == '"'))
            add(result.spans, value, content.size(), SyntaxTokenKind::String);
        else if (value < content.size())
            add(result.spans, value, content.size(), SyntaxTokenKind::Plain);
    }
    return result;
}

bool bash_word_char(char value) noexcept { return ascii_alnum(value) || value == '_'; }

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
    for (std::size_t i = 0; i < line.size();) {
        const char ch = line[i];
        const std::size_t at = i;
        i = next_cluster(line, i);
        if (!in_single && !in_double && ch == '#') {
            add(result.spans, segment, at, SyntaxTokenKind::Plain);
            add(result.spans, at, line.size(), SyntaxTokenKind::Comment);
            return result;
        }
        if (!in_double && ch == '\'') {
            if (!in_single) segment = at;
            in_single = !in_single;
            if (!in_single) { add(result.spans, segment, i, SyntaxTokenKind::String); segment = i; }
        } else if (!in_single && ch == '"') {
            if (!in_double) segment = at;
            in_double = !in_double;
            if (!in_double) { add(result.spans, segment, i, SyntaxTokenKind::String); segment = i; }
        } else if (!in_single && !in_double && ch == '$') {
            i = skip_clusters(line, i, bash_word_char);
            add(result.spans, at, i, SyntaxTokenKind::Property);
            segment = i;
        } else if (!in_single && !in_double && (ch == '|' || ch == ';' || ch == '&' || ch == '<' || ch == '>')) {
            add(result.spans, at, i, SyntaxTokenKind::Operator);
        }
    }
    if (in_single || in_double) {
        add(result.spans, segment, line.size(), SyntaxTokenKind::String);
        result.next_state = in_single ? "single" : "double";
    } else {
        for (std::size_t i = 0; i < line.size();) {
            const std::size_t begin = skip_clusters(line, i, [](char value) { return !ascii_alpha(value); });
            i = skip_clusters(line, begin, bash_word_char);
            if (word_at(line, begin, i, {"if", "then", "fi", "for", "in", "do", "done", "case", "esac", "while", "function"}))
                add(result.spans, begin, i, SyntaxTokenKind::Keyword);
        }
        const std::size_t command_begin = skip_clusters(line, 0, ascii_space);
        const std::size_t command = skip_clusters(line, command_begin, [](char value) {
            return bash_word_char(value) || value == '-' || value == '.';
        });
        if (command > command_begin && !word_at(line, command_begin, command,
                                                  {"if", "then", "fi", "for", "in", "do", "done", "case", "esac", "while", "function"}))
            add(result.spans, command_begin, command, SyntaxTokenKind::Command);

        if (const ClusterMatch heredoc = find_clusters(line, 0, "<<"); heredoc.found()) {
            std::size_t begin = heredoc.end;
            if (begin < line.size() && line[begin] == '-') begin = next_cluster(line, begin);
            begin = skip_clusters(line, begin, ascii_space);
            const std::size_t end = skip_clusters(line, begin, bash_word_char);
            if (end > begin) {
                add(result.spans, heredoc.begin, heredoc.end, SyntaxTokenKind::Operator);
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

// The end of the spaces and tabs that indent the line.
std::size_t leading_indent(std::string_view line) noexcept {
    return skip_clusters(line, 0, [](char value) { return value == ' ' || value == '\t'; });
}

// Whether the content opens with an ATX heading: up to six `#` marks followed
// by a space or the end of the line, so a `#hashtag` stays text.
bool atx_heading(std::string_view content) noexcept {
    const ClusterRun marks = cluster_run(content, 0, '#');
    return marks.count > 0 && marks.count <= 6 && (marks.end == content.size() || ascii_space(content[marks.end]));
}

// The fence run at `at` that opens or closes a fenced code block: three or
// more backticks or tildes; an empty run when there is none.
ClusterRun fence_run(std::string_view line, std::size_t at) noexcept {
    if (at >= line.size() || (line[at] != '`' && line[at] != '~')) return ClusterRun{0, at};
    const ClusterRun run = cluster_run(line, at, line[at]);
    return run.count >= 3 ? run : ClusterRun{0, at};
}

// A line made only of three or more `-`, `*`, `_` or `=` and spaces: a
// thematic break, or the underline of a setext heading.
bool markup_rule(std::string_view content) noexcept {
    if (content.empty()) return false;
    const char mark = content.front();
    if (mark != '-' && mark != '*' && mark != '_' && mark != '=') return false;
    std::size_t count = 0;
    for (std::size_t at = 0; at < content.size(); at = next_cluster(content, at)) {
        if (content[at] == mark) ++count;
        else if (!ascii_space(content[at])) return false;
    }
    return count >= 3;
}

// Where a list marker at `at` ends and the kind that paints it: `-`, `+` or
// `*` before a space, or up to nine digits and `.` or `)` before a space. A
// marker that is not there ends at `at`.
struct ListMarker {
    std::size_t end = 0;
    SyntaxTokenKind kind = SyntaxTokenKind::Plain;
};

ListMarker list_marker(std::string_view line, std::size_t at) noexcept {
    if (at >= line.size()) return ListMarker{at};
    const auto ends_item = [line](std::size_t after) { return after == line.size() || ascii_space(line[after]); };
    const std::size_t first_end = next_cluster(line, at);
    if ((line[at] == '-' || line[at] == '+' || line[at] == '*') && ends_item(first_end))
        return ListMarker{first_end, SyntaxTokenKind::Operator};
    std::size_t digits = 0;
    std::size_t end = at;
    for (; end < line.size() && digits < 9U && ascii_digit(line[end]); ++digits) end = next_cluster(line, end);
    if (digits > 0 && end < line.size() && (line[end] == '.' || line[end] == ')') && ends_item(next_cluster(line, end)))
        return ListMarker{next_cluster(line, end), SyntaxTokenKind::Number};
    return ListMarker{at};
}

// The end of the bracket that closes the opener at `open`, honouring nesting
// and backslash escapes; npos when the line has none.
std::size_t matching_bracket(std::string_view line, std::size_t open, char opener, char closer) noexcept {
    std::size_t depth = 0;
    for (std::size_t i = open; i < line.size();) {
        const char ch = line[i];
        i = next_cluster(line, i);
        if (ch == '\\') i = next_cluster(line, i);
        else if (ch == opener) ++depth;
        else if (ch == closer && --depth == 0) return i;
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
        const std::size_t next = next_cluster(line, i);
        if (ch == '\\') {
            if (next < size && ascii_punct(line[next])) {
                const std::size_t end = next_cluster(line, next);
                add(spans, i, end, SyntaxTokenKind::Escape);
                i = end;
            } else {
                i = next;
            }
            continue;
        }
        if (ch == '`') {
            const ClusterRun run = cluster_run(line, i, '`');
            std::size_t close = std::string_view::npos;
            for (std::size_t j = run.end; j < size;) {
                if (line[j] != '`') { j = next_cluster(line, j); continue; }
                const ClusterRun candidate = cluster_run(line, j, '`');
                if (candidate.count == run.count) { close = candidate.end; break; }
                j = candidate.end;
            }
            if (close == std::string_view::npos) { i = run.end; continue; }
            add(spans, i, close, SyntaxTokenKind::String);
            i = close;
            continue;
        }
        if (ch == '*' || ch == '_') {
            const ClusterRun run = cluster_run(line, i, ch);
            const bool intraword_opener = ch == '_' && i > 0 && ascii_alnum(line[i - 1U]);
            const bool opens = !intraword_opener && run.end < size && !ascii_space(line[run.end]);
            std::size_t close = std::string_view::npos;
            for (std::size_t j = run.end; opens && j < size;) {
                if (line[j] == '\\') { j = next_cluster(line, next_cluster(line, j)); continue; }
                if (line[j] != ch) { j = next_cluster(line, j); continue; }
                const ClusterRun candidate = cluster_run(line, j, ch);
                const bool intraword_closer = ch == '_' && candidate.end < size && ascii_alnum(line[candidate.end]);
                if (candidate.count == run.count && !ascii_space(line[j - 1U]) && !intraword_closer) {
                    close = candidate.end;
                    break;
                }
                j = candidate.end;
            }
            if (close == std::string_view::npos) { i = run.end; continue; }
            add(spans, i, close, run.count >= 2U ? SyntaxTokenKind::Keyword : SyntaxTokenKind::Type);
            i = close;
            continue;
        }
        if (ch == '[' || (ch == '!' && next < size && line[next] == '[')) {
            const std::size_t open = ch == '!' ? next : i;
            const std::size_t after = matching_bracket(line, open, '[', ']');
            if (after == std::string_view::npos) { i = next_cluster(line, open); continue; }
            if (after < size && line[after] == '(') {
                const std::size_t target_end = matching_bracket(line, after, '(', ')');
                if (target_end != std::string_view::npos) {
                    add(spans, i, after, SyntaxTokenKind::Property);
                    add(spans, after, target_end, SyntaxTokenKind::String);
                    i = target_end;
                    continue;
                }
            } else if (after < size && line[after] == '[') {
                const std::size_t label_end = matching_bracket(line, after, '[', ']');
                if (label_end != std::string_view::npos) {
                    add(spans, i, label_end, SyntaxTokenKind::Property);
                    i = label_end;
                    continue;
                }
            }
            i = next_cluster(line, open);
            continue;
        }
        i = next;
    }
}

// -- SQL ---------------------------------------------------------------------
//
// One line of SQL, in the dialect every embedded engine agrees on. Two rules
// are worth stating because they are where SQL misleads a reader:
//
//  * '...' is the only string literal. "..." , [...] and `...` are QUOTED
//    NAMES, and are painted as names — so a reader sees at a glance that
//    "abc" is not the text abc, which is the mistake SQL's own permissiveness
//    invites (an engine may accept a double-quoted name as text where no such
//    column exists, and the statement then means something else entirely).
//  * A word immediately before '(' is a call. Naming no functions keeps the
//    profile true for every engine's own set and for the ones an application
//    registers itself.
//
// A bound parameter (:name, @name, $name, ? and ?1) is a name too: it is what
// a saved query leaves for its caller to fill in, and it must be visible.

constexpr std::string_view sql_comment_state = "comment";
constexpr std::string_view sql_string_state = "string";

bool sql_word_char(char value) noexcept { return ascii_alnum(value) || value == '_' || value == '$'; }

/// Case-insensitive membership, so `select` and `SELECT` are one word.
bool sql_word_is(std::string_view token, std::initializer_list<std::string_view> words) {
    std::string folded;
    folded.reserve(token.size());
    for (const char value : token)
        folded.push_back(value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value);
    return std::find(words.begin(), words.end(), std::string_view(folded)) != words.end();
}

bool sql_keyword(std::string_view token) {
    return sql_word_is(token, {"abort", "action", "add", "after", "all", "alter", "always", "analyze", "and", "as",
                               "asc", "attach", "autoincrement", "before", "begin", "between", "by", "cascade",
                               "case", "cast", "check", "collate", "column", "commit", "conflict", "constraint",
                               "create", "cross", "current", "database", "default", "deferrable", "deferred",
                               "delete", "desc", "detach", "distinct", "do", "drop", "each", "else", "end",
                               "escape", "except", "exclusive", "exists", "explain", "fail", "filter", "first",
                               "following", "for", "foreign", "from", "full", "generated", "glob", "group",
                               "groups", "having", "if", "ignore", "immediate", "in", "index", "indexed",
                               "initially", "inner", "insert", "instead", "intersect", "into", "is", "isnull",
                               "join", "key", "last", "left", "like", "limit", "match", "materialized",
                               "natural", "no", "not", "nothing", "notnull", "null", "nulls", "of", "offset",
                               "on", "or", "order", "others", "outer", "over", "partition", "plan", "pragma",
                               "preceding", "primary", "query", "raise", "range", "recursive", "references",
                               "regexp", "reindex", "release", "rename", "replace", "restrict", "returning",
                               "right", "rollback", "row", "rows", "savepoint", "select", "set", "table", "temp",
                               "temporary", "then", "ties", "to", "transaction", "trigger", "unbounded", "union",
                               "unique", "update", "using", "vacuum", "values", "view", "virtual", "when",
                               "where", "window", "with", "without"});
}

bool sql_type(std::string_view token) {
    return sql_word_is(token, {"bigint", "blob", "boolean", "char", "character", "date", "datetime", "decimal",
                               "double", "float", "int", "int2", "int8", "integer", "mediumint", "numeric",
                               "nvarchar", "precision", "real", "smallint", "text", "time", "timestamp",
                               "tinyint", "varchar", "varying"});
}

bool sql_literal_word(std::string_view token) {
    return sql_word_is(token, {"true", "false", "null", "current_date", "current_time", "current_timestamp"});
}

/// The quoted-name closer for an opener, or '\0' when the character opens no
/// name. The three spellings are one rule with three brackets.
char sql_name_closer(char opener) noexcept {
    if (opener == '"') return '"';
    if (opener == '[') return ']';
    if (opener == '`') return '`';
    return '\0';
}

bool ascii_hex_digit(char value) noexcept {
    return ascii_digit(value) || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
}

// Where a string literal whose text starts at `at` ends: past its closing
// quote, or at the end of the line, where it stays open for the next.
struct SqlStringEnd {
    std::size_t end = 0;
    bool closed = false;
};

SqlStringEnd sql_string_end(std::string_view line, std::size_t at) noexcept {
    while (at < line.size()) {
        const char ch = line[at];
        at = next_cluster(line, at);
        if (ch != '\'') continue;
        if (at < line.size() && line[at] == '\'') {
            at = next_cluster(line, at);  // '' is one quote inside the text, not its end
            continue;
        }
        return SqlStringEnd{at, true};
    }
    return SqlStringEnd{line.size(), false};
}

SyntaxLineResult sql_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    std::size_t i = 0;
    // A block comment and a string literal both carry across lines, and the
    // rest of the line belongs to whichever is open.
    if (incoming == sql_comment_state) {
        const ClusterMatch close = find_clusters(line, 0, "*/");
        if (!close.found()) {
            add(result.spans, 0, line.size(), SyntaxTokenKind::Comment);
            result.next_state = std::string(sql_comment_state);
            return result;
        }
        add(result.spans, 0, close.end, SyntaxTokenKind::Comment);
        i = close.end;
    } else if (incoming == sql_string_state) {
        const SqlStringEnd text = sql_string_end(line, 0);
        add(result.spans, 0, text.end, SyntaxTokenKind::String);
        if (!text.closed) {
            result.next_state = std::string(sql_string_state);
            return result;
        }
        i = text.end;
    }

    while (i < line.size()) {
        const char ch = line[i];
        const std::size_t next = next_cluster(line, i);
        // The first byte of the next cluster, NUL past the end of the line;
        // nothing below accepts a NUL as the second byte of a token.
        const char following = next < line.size() ? line[next] : '\0';
        if (ascii_space(ch)) {
            i = next;
            continue;
        }
        if (ch == '-' && following == '-') {
            add(result.spans, i, line.size(), SyntaxTokenKind::Comment);
            return result;
        }
        if (ch == '/' && following == '*') {
            const ClusterMatch close = find_clusters(line, next_cluster(line, next), "*/");
            if (!close.found()) {
                add(result.spans, i, line.size(), SyntaxTokenKind::Comment);
                result.next_state = std::string(sql_comment_state);
                return result;
            }
            add(result.spans, i, close.end, SyntaxTokenKind::Comment);
            i = close.end;
            continue;
        }
        if (ch == '\'') {
            const SqlStringEnd text = sql_string_end(line, next);
            add(result.spans, i, text.end, SyntaxTokenKind::String);
            if (!text.closed) {
                result.next_state = std::string(sql_string_state);
                return result;
            }
            i = text.end;
            continue;
        }
        if (const char closer = sql_name_closer(ch); closer != '\0') {
            std::size_t end = skip_clusters(line, next, [closer](char value) { return value != closer; });
            end = next_cluster(line, end);
            add(result.spans, i, end, SyntaxTokenKind::Property);
            i = end;
            continue;
        }
        if (ch == ':' || ch == '@' || ch == '?' || (ch == '$' && sql_word_char(following))) {
            const std::size_t end = skip_clusters(line, next, sql_word_char);
            // A lone ':' or '@' is punctuation; '?' alone IS a parameter.
            add(result.spans, i, end, end > next || ch == '?' ? SyntaxTokenKind::Property : SyntaxTokenKind::Operator);
            i = end;
            continue;
        }
        if (ascii_digit(ch) || (ch == '.' && ascii_digit(following))) {
            std::size_t end = 0;
            if (ch == '0' && (following == 'x' || following == 'X')) {
                end = skip_clusters(line, next_cluster(line, next), ascii_hex_digit);
            } else {
                end = skip_clusters(line, i, [](char value) { return ascii_digit(value) || value == '.'; });
                if (end < line.size() && (line[end] == 'e' || line[end] == 'E')) {
                    std::size_t exponent = next_cluster(line, end);
                    if (exponent < line.size() && (line[exponent] == '+' || line[exponent] == '-'))
                        exponent = next_cluster(line, exponent);
                    if (exponent < line.size() && ascii_digit(line[exponent])) end = skip_clusters(line, exponent, ascii_digit);
                }
            }
            add(result.spans, i, end, SyntaxTokenKind::Number);
            i = end;
            continue;
        }
        if (ascii_alpha(ch) || ch == '_') {
            const std::size_t end = skip_clusters(line, i, sql_word_char);
            const std::string_view token = line.substr(i, end - i);
            const std::size_t after = skip_clusters(line, end, ascii_space);
            const bool call = after < line.size() && line[after] == '(';
            if (sql_keyword(token)) add(result.spans, i, end, SyntaxTokenKind::Keyword);
            else if (sql_literal_word(token)) add(result.spans, i, end, SyntaxTokenKind::Number);
            else if (sql_type(token)) add(result.spans, i, end, SyntaxTokenKind::Type);
            else if (call) add(result.spans, i, end, SyntaxTokenKind::Command);
            i = end;
            continue;
        }
        if (std::string_view("=<>!+-*/%|&~^,;().").find(ch) != std::string_view::npos)
            add(result.spans, i, next, SyntaxTokenKind::Operator);
        i = next;
    }
    return result;
}

SyntaxLineResult markdown_line(std::string_view line, std::string_view incoming) {
    SyntaxLineResult result;
    if (incoming.starts_with(markdown_fence_prefix)) {
        const std::string_view opener = incoming.substr(markdown_fence_prefix.size());
        const std::size_t indent = leading_indent(line);
        const ClusterRun run = fence_run(line, indent);
        if (run.count > 0 && run.count >= opener.size() && opener.starts_with(line[indent]) &&
            skip_clusters(line, run.end, ascii_space) == line.size()) {
            add(result.spans, indent, run.end, SyntaxTokenKind::Operator);
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
        const std::size_t mark_end = next_cluster(line, indent);
        add(result.spans, indent, mark_end, SyntaxTokenKind::Operator);
        add(result.spans, mark_end, line.size(), SyntaxTokenKind::Comment);
        return result;
    }
    if (const ClusterRun run = fence_run(line, indent);
        run.count > 0 && (content.front() == '~' || !find_clusters(line, run.end, "`").found())) {
        add(result.spans, indent, run.end, SyntaxTokenKind::Operator);
        add(result.spans, run.end, end_before_trailing_space(line, run.end), SyntaxTokenKind::Type);
        result.next_state = std::string(markdown_fence_prefix) + std::string(run.count, content.front());
        return result;
    }
    if (atx_heading(content)) {
        add(result.spans, indent, line.size(), SyntaxTokenKind::Keyword);
        return result;
    }
    if (markup_rule(content)) {
        add(result.spans, indent, line.size(), SyntaxTokenKind::Operator);
        return result;
    }
    if (const std::size_t marks_end = match_clusters(line, indent, "::"); marks_end != std::string_view::npos) {
        std::size_t rest = skip_clusters(line, marks_end, ascii_name_char);
        add(result.spans, indent, rest, SyntaxTokenKind::Command);
        if (rest < line.size() && line[rest] == '{') {
            const std::size_t close = matching_bracket(line, rest, '{', '}');
            const std::size_t end = close == std::string_view::npos ? line.size() : close;
            add(result.spans, rest, end, SyntaxTokenKind::Property);
            rest = end;
        }
        markdown_inline(line, rest, result.spans);
        return result;
    }
    const ListMarker marker = list_marker(line, indent);
    add(result.spans, indent, marker.end, marker.kind);
    markdown_inline(line, marker.end, result.spans);
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
    if (atx_heading(opening)) return LanguageDetection{40, "content heading"};
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
    (void)registry.register_profile(LanguageProfile{
        "sql", "SQL",
        [](const LanguageDetectionInput& input) {
            if (ends_with(input.file_name, ".sql")) return LanguageDetection{80, "file suffix"};
            // A statement word at the start is the only content SQL claims,
            // and it outranks YAML's bare "there is a colon somewhere" — a
            // statement's bound parameters put colons in most of them.
            const std::string_view prefix = trim_ascii_space(input.content_prefix);
            std::size_t end = 0;
            while (end < prefix.size() && (ascii_alpha(prefix[end]) || prefix[end] == '_')) ++end;
            if (end > 0 && sql_word_is(prefix.substr(0, end),
                                       {"select", "insert", "update", "delete", "create", "alter", "drop", "with",
                                        "explain", "pragma", "begin", "vacuum", "replace"}))
                return LanguageDetection{50, "content prefix"};
            return LanguageDetection{};
        },
        sql_line});
}

}  // namespace ckv::widgets
