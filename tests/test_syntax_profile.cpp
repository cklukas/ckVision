// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/core/text.hpp"
#include "cvision/widgets/syntax_profile.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using ckv::widgets::LanguageDetectionInput;
using ckv::widgets::SyntaxProfileRegistry;
using ckv::widgets::SyntaxTokenKind;

CK_TEST(standard_syntax_profiles_detect_json_yaml_and_bash_deterministically) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "settings.json", {}, {}}).id == "json");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "config.yml", {}, {}}).id == "yaml");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "script", "#!/usr/bin/env bash", "#!/usr/bin/env bash"}).id == "bash");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "notes.txt", {}, {}}).id == "plain");
}

CK_TEST(standard_syntax_profiles_detect_explicit_content_prefixes_without_host_state) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "  {\"name\": \"ckVision\"}", {}}).id == "json");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "---\nname: ckVision", {}}).id == "yaml");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "#!/usr/bin/env bash\necho ok", {}}).id == "bash");
}

CK_TEST(json_profile_marks_property_string_number_and_keyword) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& json = *registry.find("json");
    const auto result = json.highlight_line("{\"name\": \"ckv\", \"count\": 2, \"ok\": true}", "");
    bool property = false;
    bool string = false;
    bool number = false;
    bool keyword = false;
    for (const auto& span : result.spans) {
        property = property || span.kind == SyntaxTokenKind::Property;
        string = string || span.kind == SyntaxTokenKind::String;
        number = number || span.kind == SyntaxTokenKind::Number;
        keyword = keyword || span.kind == SyntaxTokenKind::Keyword;
    }
    CK_CHECK(property);
    CK_CHECK(string);
    CK_CHECK(number);
    CK_CHECK(keyword);
}

CK_TEST(yaml_profile_marks_sequence_punctuation_tags_and_anchors) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& yaml = *registry.find("yaml");
    const auto result = yaml.highlight_line("- !widget &primary title: *primary # comment", "");
    bool type = false;
    bool op = false;
    bool comment = false;
    for (const auto& span : result.spans) {
        type = type || span.kind == SyntaxTokenKind::Type;
        op = op || span.kind == SyntaxTokenKind::Operator;
        comment = comment || span.kind == SyntaxTokenKind::Comment;
    }
    CK_CHECK(type);
    CK_CHECK(op);
    CK_CHECK(comment);
}

CK_TEST(profile_registration_is_instance_owned_and_rejects_duplicate_ids) {
    SyntaxProfileRegistry first;
    SyntaxProfileRegistry second;
    ckv::widgets::register_standard_syntax_profiles(first);
    CK_CHECK(first.find("json") != nullptr);
    CK_CHECK(second.find("json") == nullptr);
    CK_CHECK(!first.register_profile(*first.find("json")));
}

CK_TEST(bash_profile_carries_quote_state_across_lines_and_recovers_at_the_closing_quote) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& bash = *registry.find("bash");
    const auto first = bash.highlight_line("echo \"unterminated", "");
    CK_CHECK(first.next_state == "double");
    const auto second = bash.highlight_line("continued\" # comment", first.next_state);
    CK_CHECK(second.next_state.empty());
    bool has_comment = false;
    for (const auto& span : second.spans) has_comment = has_comment || span.kind == SyntaxTokenKind::Comment;
    CK_CHECK(has_comment);
}

CK_TEST(bash_profile_marks_commands_operators_and_multiline_heredoc_content) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& bash = *registry.find("bash");
    const auto opening = bash.highlight_line("printf '%s\\n' ok <<EOF | sed s/o/x/", "");
    bool command = false;
    bool op = false;
    for (const auto& span : opening.spans) {
        command = command || span.kind == SyntaxTokenKind::Command;
        op = op || span.kind == SyntaxTokenKind::Operator;
    }
    CK_CHECK(command);
    CK_CHECK(op);
    CK_CHECK(opening.next_state == "heredoc:EOF");
    const auto body = bash.highlight_line("literal $content", opening.next_state);
    CK_CHECK(body.next_state == "heredoc:EOF");
    CK_CHECK(body.spans.size() == 1U && body.spans.front().kind == SyntaxTokenKind::String);
    const auto closing = bash.highlight_line("EOF", body.next_state);
    CK_CHECK(closing.next_state.empty());
    CK_CHECK(closing.spans.size() == 1U && closing.spans.front().kind == SyntaxTokenKind::Operator);
}

CK_TEST(profile_lexing_uses_explicit_ascii_source_grammar_without_locale_classification) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& json = *registry.find("json");
    const std::string malformed{"{\xC2\xA0}", 4};
    const auto first = json.highlight_line(malformed, "");
    const auto second = json.highlight_line(malformed, "");
    CK_CHECK(first.spans == second.spans);
    bool error = false;
    for (const auto& span : first.spans) error = error || span.kind == SyntaxTokenKind::Error;
    CK_CHECK(error);
}

namespace {

using ckv::widgets::SyntaxSpan;

std::vector<SyntaxSpan> markdown_spans(std::string_view line, std::string_view state = "body") {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    return registry.find("markdown")->highlight_line(line, state).spans;
}

}  // namespace

CK_TEST(markdown_profile_is_detected_by_file_suffix_and_by_content) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "talk.md", {}, {}}).id == "markdown");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "README.markdown", {}, {}}).id == "markdown");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "\n# Release notes\n\n- item\n", {}}).id == "markdown");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled",
                                                    "---\ntitle: Field Report\nformat: slides\n---\n\n# Field Report\n", {}}).id == "markdown");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled",
                                                    "---\ntitle: Field Report\n# reviewed\ntags:\n  - field\n---\n", {}}).id == "markdown");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "#hashtag first\n", {}}).id == "plain");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "#!/usr/bin/env bash\n# heading?\n", {}}).id == "bash");
}

CK_TEST(markdown_profile_leaves_yaml_that_only_looks_like_front_matter_to_yaml) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    // A document that opens with `---` but never closes the block is YAML.
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "---\nname: ckVision\nversion: 1\n", {}}).id == "yaml");
    // A line that is not `key: value` before the closing `---` ends the claim.
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "---\nname: ckVision\nnot a key\n---\n", {}}).id == "yaml");
    // The file suffix outranks a front-matter-shaped prefix.
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "deck.yml", "---\ntitle: Field Report\n---\n", {}}).id == "yaml");
}

CK_TEST(markdown_profile_marks_atx_headings_whole) {
    const std::vector<SyntaxSpan> expected{{0, 22, SyntaxTokenKind::Keyword}};
    CK_CHECK(markdown_spans("# Field Report {a=b}  ") == expected);
    CK_CHECK(markdown_spans("###### Deep") == std::vector<SyntaxSpan>({{0, 11, SyntaxTokenKind::Keyword}}));
    CK_CHECK(markdown_spans("####### Not a heading").empty());
    CK_CHECK(markdown_spans("#hashtag").empty());
}

CK_TEST(markdown_profile_marks_emphasis_strong_emphasis_and_escapes) {
    const std::vector<SyntaxSpan> expected{{6, 17, SyntaxTokenKind::Type}, {31, 42, SyntaxTokenKind::Keyword}};
    CK_CHECK(markdown_spans("Costs *warehouse* fell; margin **doubled**.") == expected);
    CK_CHECK(markdown_spans("__strong__ and _em_") ==
             std::vector<SyntaxSpan>({{0, 10, SyntaxTokenKind::Keyword}, {15, 19, SyntaxTokenKind::Type}}));
    // Intraword underscores and unbalanced stars are text.
    CK_CHECK(markdown_spans("snake_case_name * 2").empty());
    CK_CHECK(markdown_spans("a \\* literal star") == std::vector<SyntaxSpan>({{2, 4, SyntaxTokenKind::Escape}}));
}

CK_TEST(markdown_profile_marks_inline_code_by_matching_backtick_runs) {
    CK_CHECK(markdown_spans("run `make` now") == std::vector<SyntaxSpan>({{4, 10, SyntaxTokenKind::String}}));
    CK_CHECK(markdown_spans("`` a ` b `` tail") == std::vector<SyntaxSpan>({{0, 11, SyntaxTokenKind::String}}));
    CK_CHECK(markdown_spans("an unmatched ` stays text").empty());
}

CK_TEST(markdown_profile_carries_the_fence_state_until_the_closing_fence) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& markdown = *registry.find("markdown");
    const auto opening = markdown.highlight_line("```cpp", "body");
    CK_CHECK(opening.next_state == "fence:```");
    CK_CHECK(opening.spans == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}, {3, 6, SyntaxTokenKind::Type}}));
    const auto body = markdown.highlight_line("# not a heading, *not* emphasis", opening.next_state);
    CK_CHECK(body.next_state == "fence:```");
    CK_CHECK(body.spans == std::vector<SyntaxSpan>({{0, 31, SyntaxTokenKind::String}}));
    const auto shorter = markdown.highlight_line("``", body.next_state);
    CK_CHECK(shorter.next_state == "fence:```");
    const auto closing = markdown.highlight_line("````  ", shorter.next_state);
    CK_CHECK(closing.next_state == "body");
    CK_CHECK(closing.spans == std::vector<SyntaxSpan>({{0, 4, SyntaxTokenKind::Operator}}));
    // A backtick fence's info string may not contain a backtick: that line is inline code.
    CK_CHECK(markdown.highlight_line("``` a ` b ```", "body").next_state == "body");
    CK_CHECK(markdown.highlight_line("~~~", "body").next_state == "fence:~~~");
}

CK_TEST(markdown_profile_marks_links_and_images) {
    CK_CHECK(markdown_spans("see [the report](https://example.org/r?a=(1)) now") ==
             std::vector<SyntaxSpan>({{4, 16, SyntaxTokenKind::Property}, {16, 45, SyntaxTokenKind::String}}));
    CK_CHECK(markdown_spans("![alt][ref]") == std::vector<SyntaxSpan>({{0, 11, SyntaxTokenKind::Property}}));
    CK_CHECK(markdown_spans("- [ ] a task").size() == 1U);
    CK_CHECK(markdown_spans("[bare] brackets").empty());
}

CK_TEST(markdown_profile_marks_block_quotes_list_markers_and_rules) {
    CK_CHECK(markdown_spans("> quoted **text**") ==
             std::vector<SyntaxSpan>({{0, 1, SyntaxTokenKind::Operator}, {1, 17, SyntaxTokenKind::Comment}}));
    CK_CHECK(markdown_spans("- Revenue grew *fast*") ==
             std::vector<SyntaxSpan>({{0, 1, SyntaxTokenKind::Operator}, {15, 21, SyntaxTokenKind::Type}}));
    CK_CHECK(markdown_spans("  12. twelfth") ==
             std::vector<SyntaxSpan>({{2, 5, SyntaxTokenKind::Number}}));
    CK_CHECK(markdown_spans("-no space, not a list").empty());
    CK_CHECK(markdown_spans("* * *") == std::vector<SyntaxSpan>({{0, 5, SyntaxTokenKind::Operator}}));
    CK_CHECK(markdown_spans("===") == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}}));
}

CK_TEST(markdown_profile_opens_front_matter_only_at_the_document_start) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& markdown = *registry.find("markdown");
    const auto opening = markdown.highlight_line("---", "");
    CK_CHECK(opening.next_state == "front");
    CK_CHECK(opening.spans == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}}));
    const auto key = markdown.highlight_line("title: Field Report", opening.next_state);
    CK_CHECK(key.next_state == "front");
    CK_CHECK(key.spans == std::vector<SyntaxSpan>({{0, 5, SyntaxTokenKind::Property},
                                                   {5, 6, SyntaxTokenKind::Operator},
                                                   {7, 19, SyntaxTokenKind::Plain}}));
    const auto closing = markdown.highlight_line("---", key.next_state);
    CK_CHECK(closing.next_state == "body");
    CK_CHECK(closing.spans == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}}));
    // The same `---` after body text is a thematic break, and the body state is kept.
    const auto rule = markdown.highlight_line("---", closing.next_state);
    CK_CHECK(rule.next_state == "body");
    CK_CHECK(rule.spans == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}}));
    // A document that does not start with `---` is body from its first line.
    CK_CHECK(markdown.highlight_line("# Title", "").next_state == "body");
}

CK_TEST(markdown_profile_marks_directive_lines) {
    CK_CHECK(markdown_spans("::steps") == std::vector<SyntaxSpan>({{0, 7, SyntaxTokenKind::Command}}));
    CK_CHECK(markdown_spans("::step{at=2-3}") ==
             std::vector<SyntaxSpan>({{0, 6, SyntaxTokenKind::Command}, {6, 14, SyntaxTokenKind::Property}}));
    CK_CHECK(markdown_spans("::") == std::vector<SyntaxSpan>({{0, 2, SyntaxTokenKind::Command}}));
    CK_CHECK(markdown_spans("::figure{src=a.png} *caption*") ==
             std::vector<SyntaxSpan>({{0, 8, SyntaxTokenKind::Command}, {8, 19, SyntaxTokenKind::Property},
                                      {20, 29, SyntaxTokenKind::Type}}));
    CK_CHECK(markdown_spans("::step{unclosed") ==
             std::vector<SyntaxSpan>({{0, 6, SyntaxTokenKind::Command}, {6, 15, SyntaxTokenKind::Property}}));
}

namespace {

std::vector<SyntaxSpan> sql_spans(std::string_view line, std::string_view state = "") {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    return registry.find("sql")->highlight_line(line, state).spans;
}

/// The kind covering `text` in `line`, or Plain when nothing covers it.
SyntaxTokenKind sql_kind_of(std::string_view line, std::string_view text) {
    const std::size_t at = line.find(text);
    if (at == std::string_view::npos) return SyntaxTokenKind::Error;
    for (const SyntaxSpan& span : sql_spans(line))
        if (span.begin_byte <= at && at + text.size() <= span.end_byte) return span.kind;
    return SyntaxTokenKind::Plain;
}

}  // namespace

CK_TEST(sql_profile_is_detected_by_file_suffix_and_by_a_leading_statement_word) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "report.sql", {}, {}}).id == "sql");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "select 1", {}}).id == "sql");
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled", "  CREATE TABLE t (id INTEGER)", {}}).id == "sql");
    // A statement's bound parameters put a colon in most of them, which is
    // all YAML's content rule asks for; the statement word outranks it.
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "untitled",
                                                     "SELECT company FROM customers WHERE revenue > :least", {}})
                 .id == "sql");
    // Prose that merely mentions a table is nobody's source.
    CK_CHECK(registry.detect(LanguageDetectionInput{std::nullopt, "notes.txt", "the customers table", {}}).id == "plain");
}

CK_TEST(sql_profile_marks_keywords_types_numbers_calls_and_operators) {
    const std::string_view line = "SELECT count(id), price * 1.5e2, 0xFF FROM items WHERE ok = true;";
    CK_CHECK(sql_kind_of(line, "SELECT") == SyntaxTokenKind::Keyword);
    CK_CHECK(sql_kind_of(line, "FROM") == SyntaxTokenKind::Keyword);
    // A word before '(' is a call, whoever defined it.
    CK_CHECK(sql_kind_of(line, "count") == SyntaxTokenKind::Command);
    CK_CHECK(sql_kind_of(line, "1.5e2") == SyntaxTokenKind::Number);
    CK_CHECK(sql_kind_of(line, "0xFF") == SyntaxTokenKind::Number);
    CK_CHECK(sql_kind_of(line, "true") == SyntaxTokenKind::Number);
    CK_CHECK(sql_kind_of(line, "*") == SyntaxTokenKind::Operator);
    // An ordinary name is left alone, so the marked words stand out.
    CK_CHECK(sql_kind_of(line, "items") == SyntaxTokenKind::Plain);
    const std::string_view ddl = "create table t (id integer primary key, name TEXT not null)";
    CK_CHECK(sql_kind_of(ddl, "integer") == SyntaxTokenKind::Type);
    CK_CHECK(sql_kind_of(ddl, "TEXT") == SyntaxTokenKind::Type);
    CK_CHECK(sql_kind_of(ddl, "create") == SyntaxTokenKind::Keyword);
}

CK_TEST(sql_profile_paints_a_quoted_name_as_a_name_and_only_single_quotes_as_text) {
    // SQL's own trap: "abc" is a NAME, not the text abc, and an engine that
    // accepts it where no such column exists changes what the statement means.
    const std::string_view line = "SELECT \"company\", [order], `qty` FROM t WHERE name = 'it''s'";
    CK_CHECK(sql_kind_of(line, "\"company\"") == SyntaxTokenKind::Property);
    CK_CHECK(sql_kind_of(line, "[order]") == SyntaxTokenKind::Property);
    CK_CHECK(sql_kind_of(line, "`qty`") == SyntaxTokenKind::Property);
    // The doubled quote is one quote inside the text, not its end.
    CK_CHECK(sql_kind_of(line, "'it''s'") == SyntaxTokenKind::String);
}

CK_TEST(sql_profile_marks_every_spelling_of_a_bound_parameter) {
    const std::string_view line = "SELECT * FROM t WHERE a = :least AND b = @name AND c = $id AND d = ? AND e = ?1";
    for (const std::string_view parameter : {":least", "@name", "$id", "?1"})
        CK_CHECK(sql_kind_of(line, parameter) == SyntaxTokenKind::Property);
    // A lone '?' is a parameter too; a lone ':' is punctuation.
    CK_CHECK(sql_kind_of("SELECT ?", "?") == SyntaxTokenKind::Property);
    CK_CHECK(sql_kind_of("SELECT a : b", ":") == SyntaxTokenKind::Operator);
}

CK_TEST(sql_profile_carries_a_block_comment_and_an_unclosed_string_across_lines) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& sql = *registry.find("sql");
    const auto opened = sql.highlight_line("SELECT 1 /* why this", "");
    CK_CHECK(opened.next_state == "comment");
    const auto inside = sql.highlight_line("still the comment", opened.next_state);
    CK_CHECK(inside.next_state == "comment");
    CK_CHECK(inside.spans.size() == 1U && inside.spans.front().kind == SyntaxTokenKind::Comment);
    const auto closed = sql.highlight_line("ends */ FROM t", inside.next_state);
    CK_CHECK(closed.next_state.empty());
    bool keyword = false;
    for (const auto& span : closed.spans) keyword = keyword || span.kind == SyntaxTokenKind::Keyword;
    CK_CHECK(keyword);

    const auto open_text = sql.highlight_line("SELECT 'unterminated", "");
    CK_CHECK(open_text.next_state == "string");
    const auto end_text = sql.highlight_line("still text' FROM t", open_text.next_state);
    CK_CHECK(end_text.next_state.empty());
    CK_CHECK(end_text.spans.front().kind == SyntaxTokenKind::String);

    // A line comment ends at the line, whatever follows it.
    const auto commented = sql.highlight_line("SELECT 1 -- a note with 'quotes' and /*", "");
    CK_CHECK(commented.next_state.empty());
    CK_CHECK(commented.spans.back().kind == SyntaxTokenKind::Comment);
}

// SyntaxSpan requires both ends of every span on grapheme-cluster boundaries,
// and SyntaxCache drops a span that has an end inside a cluster. The standard
// profiles recognise ASCII syntax by the first byte of a cluster and step and
// end their tokens by whole clusters, so no text can make them split one.
namespace {

// Whether each byte offset of `line`, and its end, is a grapheme-cluster boundary.
std::vector<bool> cluster_boundaries(std::string_view line) {
    std::vector<bool> boundaries(line.size() + 1U, false);
    boundaries.front() = true;
    for (std::size_t position = 0; position < line.size();) {
        position = ckv::text::grapheme_end(line, position);
        boundaries[position] = true;
    }
    return boundaries;
}

// The incoming states a standard profile gives meaning to, and the empty state
// of a document's first line.
std::vector<std::string_view> probe_states(std::string_view id) {
    if (id == "bash") return {"", "single", "double", "heredoc:EOF"};
    if (id == "markdown") return {"", "body", "front", "fence:```", "fence:~~~"};
    if (id == "sql") return {"", "comment", "string"};
    return {""};
}

std::size_t misaligned_in_line(const ckv::widgets::LanguageProfile& profile, std::string_view line,
                               std::string_view state, const std::vector<bool>& boundaries) {
    std::size_t misaligned = 0;
    for (const SyntaxSpan& span : profile.highlight_line(line, state).spans)
        if (!(span.begin_byte < span.end_byte && span.end_byte <= line.size() && boundaries[span.begin_byte] &&
              boundaries[span.end_byte]))
            ++misaligned;
    return misaligned;
}

// How many spans the standard profiles emit for `text` that are empty, leave
// their line or have an end inside a grapheme cluster. Each profile lexes the
// text as a document, carrying its state from line to line, and lexes every
// line again in each probe state.
std::size_t misaligned_spans(std::string_view text) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    std::size_t misaligned = 0;
    for (const std::string_view id : {"plain", "json", "yaml", "bash", "markdown", "sql"}) {
        const ckv::widgets::LanguageProfile& profile = *registry.find(id);
        const std::vector<std::string_view> probes = probe_states(id);
        std::string state;
        for (std::size_t start = 0;;) {
            const std::size_t newline = text.find('\n', start);
            const std::string_view line = text.substr(start, newline == std::string_view::npos ? newline : newline - start);
            const std::vector<bool> boundaries = cluster_boundaries(line);
            for (const std::string_view probe : probes)
                misaligned += misaligned_in_line(profile, line, probe, boundaries);
            if (std::find(probes.begin(), probes.end(), state) == probes.end())
                misaligned += misaligned_in_line(profile, line, state, boundaries);
            state = profile.highlight_line(line, state).next_state;
            if (newline == std::string_view::npos) break;
            start = newline + 1U;
        }
    }
    return misaligned;
}

// Lines that reach every construct of the standard profiles.
constexpr std::string_view construct_samples[] = {
    R"({"name": "ck\"V\\ision", "count": -1.5e+3, "ok": true, "none": null, "list": [1, 2]} x)",
    R"(- !widget &primary title: *primary # comment)",
    R"(%YAML 1.2)",
    R"(key: 'quoted value')",
    R"(if [ "$name" = 'ckVision' ]; then echo "$name" | cat >out; fi # done)",
    R"(cat <<-EOF && printf '%s\n' ok)",
    R"(# Heading {#id})",
    R"(> quoted **text**)",
    R"(  12. twelfth *em* __strong__ \* `code` ``a ` b`` [link](https://x.org/(1)) ![alt][ref])",
    R"(``` cpp  )",
    R"(~~~)",
    R"(* * *)",
    R"(::figure{src=a.png} *caption* snake_case_name)",
    R"(SELECT count(id), 'it''s', "name", [order], `qty`, :least, @at, $id, ?1, ? FROM t WHERE x >= 0x1F)",
    R"(select 1.5e-2, .5 /* block */ -- line)",
    R"(still a comment */ and 'an open string)",
};

// A code point that joins the cluster before it (U+0301 COMBINING ACUTE
// ACCENT, an Extend) and one that joins the cluster after it (U+0600 ARABIC
// NUMBER SIGN, a Prepend). Inserted at every offset of a line, they put
// ASCII syntax at the start, the end and inside a multi-byte cluster.
constexpr std::string_view joiners[] = {"\xCC\x81", "\xD8\x80"};

}  // namespace

CK_TEST(standard_profiles_keep_every_span_on_cluster_boundaries_for_the_fuzz_found_input) {
    // A libFuzzer campaign found this input: the JSON profile's fallback emitted
    // one Error span per byte and so split U+07E4 (DF A4) into two spans.
    const std::string input{"#sr/\0in/env baSh\xF6\x96\x99\xDF\xA4\xDF\xDD\xE2namname\" = \"ckVi\0\0\0G\" ]; "
                            "then echo \"$name\"; fi\n",
                            71};
    CK_CHECK(misaligned_spans(input) == 0U);
    CK_CHECK(misaligned_spans("\xDF\xA4") == 0U);
}

CK_TEST(standard_profiles_step_and_end_tokens_by_whole_grapheme_clusters) {
    for (const std::string_view sample : construct_samples) {
        CK_CHECK(misaligned_spans(sample) == 0U);
        for (const std::string_view joiner : joiners) {
            std::size_t misaligned = 0;
            for (std::size_t at = 0; at <= sample.size(); ++at) {
                std::string line{sample.substr(0, at)};
                line += joiner;
                line += sample.substr(at);
                misaligned += misaligned_spans(line);
            }
            CK_CHECK(misaligned == 0U);
        }
    }
}

CK_TEST(standard_profiles_recognise_ascii_syntax_by_the_first_byte_of_a_cluster) {
    SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    const auto& json = *registry.find("json");
    // A brace, a digit, a quote and an escaped quote keep their meaning with a
    // combining mark on them, and the token takes the mark with it; a keyword
    // with a mark on it is another word.
    CK_CHECK(json.highlight_line("{\xCC\x81", "").spans == std::vector<SyntaxSpan>({{0, 3, SyntaxTokenKind::Operator}}));
    CK_CHECK(json.highlight_line("12\xCC\x81" "3", "").spans ==
             std::vector<SyntaxSpan>({{0, 5, SyntaxTokenKind::Number}}));
    CK_CHECK(json.highlight_line("\"a\"\xCC\x81", "").spans ==
             std::vector<SyntaxSpan>({{0, 5, SyntaxTokenKind::String}}));
    CK_CHECK(json.highlight_line("\"\\\"\xCC\x81\"", "").spans ==
             std::vector<SyntaxSpan>({{0, 6, SyntaxTokenKind::String}}));
    CK_CHECK(json.highlight_line("true\xCC\x81", "").spans ==
             std::vector<SyntaxSpan>({{0, 6, SyntaxTokenKind::Error}}));
    // A character the grammar does not know is one Error span, however many
    // bytes encode it.
    CK_CHECK(json.highlight_line("\xDF\xA4", "").spans == std::vector<SyntaxSpan>({{0, 2, SyntaxTokenKind::Error}}));
    const auto& sql = *registry.find("sql");
    CK_CHECK(sql.highlight_line("x -\xCC\x81- note", "").spans ==
             std::vector<SyntaxSpan>({{2, 11, SyntaxTokenKind::Comment}}));
    const auto& markdown = *registry.find("markdown");
    CK_CHECK(markdown.highlight_line("``\xCC\x81`", "body").next_state == "fence:```");
    CK_CHECK(markdown.highlight_line("#\xCC\x81 Title", "body").spans ==
             std::vector<SyntaxSpan>({{0, 9, SyntaxTokenKind::Keyword}}));
}

CK_TEST(standard_profiles_keep_spans_on_cluster_boundaries_in_malformed_utf8_and_nul_bytes) {
    const std::string samples[] = {
        std::string{"{\"a\xFF\": \xC3}", 9},
        std::string{"\xE2\x80", 2},
        std::string{"[\xC3\xA9\xCC", 4},
        std::string{"-\x80\x80 key\xF0\x9F: \xED\xA0\x80", 14},
        std::string{"a\0b # \0", 7},
        std::string{"\0{\0}\0", 5},
        std::string{"echo \"\xFE\" '\xC0\xAF' $\xE0\x80 | cat", 23},
        std::string{"``\xF8`\0 \xC2", 7},
        std::string{"SELECT '\xC3' \"\xE2\x82\" -\xFF- /\x80*", 23},
    };
    for (const std::string& sample : samples) {
        CK_CHECK(misaligned_spans(sample) == 0U);
        for (const std::string_view joiner : joiners) {
            std::size_t misaligned = 0;
            for (std::size_t at = 0; at <= sample.size(); ++at)
                misaligned += misaligned_spans(sample.substr(0, at) + std::string(joiner) + sample.substr(at));
            CK_CHECK(misaligned == 0U);
        }
    }
}
