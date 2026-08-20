// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"

#include "cvision/widgets/syntax_profile.hpp"

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
