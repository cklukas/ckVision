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

// The lexical category a highlighter assigns to a run of text. The categories are
// language-neutral so every profile shares one palette: TextEditor maps each kind to its
// own theme role, "ckv.editor.syntax." followed by the kind's lower-case name, and draws
// text that no span covers as Plain.
enum class SyntaxTokenKind {
    // Ordinary text with no lexical meaning; also the kind of any uncovered byte.
    Plain,
    // A reserved word or literal of the language, such as JSON's true, false and null.
    Keyword,
    // A type name or type-like annotation, such as a SQL column type or a YAML tag.
    Type,
    // A key naming a value, such as a JSON object key or a YAML mapping key.
    Property,
    // A quoted string literal.
    String,
    // A numeric literal.
    Number,
    // A comment.
    Comment,
    // An invoked command or function name, such as a shell command word.
    Command,
    // Punctuation and operators that structure the text.
    Operator,
    // An escape sequence.
    Escape,
    // Text the highlighter recognises as malformed, such as an unterminated JSON string.
    Error,
};

// One highlighted run within a single line: the half-open byte range [begin_byte, end_byte)
// of the line's UTF-8 text, and its kind. SyntaxCache keeps a span only when it is
// non-empty, ends within the line and both ends fall on grapheme-cluster boundaries; it
// drops any other span rather than repairing it.
struct SyntaxSpan {
    // The byte range, relative to the start of the line, and the category it is drawn as.
    std::size_t begin_byte = 0;
    std::size_t end_byte = 0;
    SyntaxTokenKind kind = SyntaxTokenKind::Plain;

    // Memberwise equality; SyntaxCache compares spans to detect that relexing has
    // converged.
    friend bool operator==(const SyntaxSpan&, const SyntaxSpan&) = default;
};

// What a highlighter returns for one line.
struct SyntaxLineResult {
    // The line's highlighted runs, in any order; SyntaxCache sorts them by begin_byte.
    // Where spans overlap, TextEditor draws a byte with the first span in that order that
    // covers it.
    std::vector<SyntaxSpan> spans;
    // The lexer state in effect at the end of the line (inside a block comment, a heredoc,
    // a fenced block...), handed to the next line as its incoming state. The string is
    // opaque to ckVision and only compared for equality; a profile with no multi-line
    // constructs returns its incoming state unchanged.
    std::string next_state;
};

// The facts a LanguageDetector may base its guess on. TextEditor fills it from its file
// name and the start of its document.
struct LanguageDetectionInput {
    // A profile id the caller asks for explicitly. When it names a registered profile,
    // SyntaxProfileRegistry::detect returns that profile without consulting any detector;
    // an unknown id falls through to detection.
    std::optional<std::string> requested_profile;
    // The document's file name; the standard detectors test its suffix. Empty when the
    // document has none.
    std::string file_name;
    // The start of the document's text; TextEditor passes at most its first 512 bytes.
    std::string content_prefix;
    // The document's first line, without its newline. TextEditor passes that line whether
    // or not it begins with "#!", so a detector checks for the marker itself.
    std::string shebang;
};

// A detector's verdict: how strongly a profile claims the input. The standard profiles
// score 80 for a matching file suffix, 70 for a shebang and 30 to 60 for content evidence.
struct LanguageDetection {
    // Zero or less makes no claim; a higher score beats a lower one.
    int score = 0;
    // A short human-readable account of the evidence ("file suffix", "content prefix").
    // Diagnostic only: detection never reads it.
    std::string reason;
};

// Highlights one line, without its newline, given the state the previous line ended in
// (empty for the first line). It must be a pure function of its two arguments, because
// SyntaxCache stops relexing once a line's result repeats and keeps the cached results
// for the lines after it.
using SyntaxLineHighlighter = std::function<SyntaxLineResult(std::string_view line, std::string_view incoming_state)>;
// Scores how well a profile fits a document. Called for every registered profile on each
// detection, so it should be cheap.
using LanguageDetector = std::function<LanguageDetection(const LanguageDetectionInput&)>;

// A language a TextEditor can highlight: identity, a detector and a line highlighter.
// Profiles are plain values; the registry that holds one owns its copy.
struct LanguageProfile {
    // The unique, non-empty key a registry and LanguageDetectionInput::requested_profile
    // use ("json", "markdown"). Equal detection scores go to the lexicographically smaller
    // id.
    std::string id;
    // The name shown to a reader ("JSON", "Plain text").
    std::string display_name;
    // The detector and the line highlighter; the registry refuses a profile in which
    // either is empty.
    LanguageDetector detect;
    SyntaxLineHighlighter highlight_line;
};

// No global registration: applications own a registry and pass it to editors.
class SyntaxProfileRegistry {
public:
    // Adds `profile` and returns true. Refuses it, returning false and leaving the registry
    // unchanged, when its id is empty, either callable is empty, or a profile with the same
    // id is already registered.
    bool register_profile(LanguageProfile profile);
    // The profile registered under `id`, or nullptr. Pointers and references into the
    // registry stay valid only until the next successful register_profile, which may
    // reallocate its storage.
    const LanguageProfile* find(std::string_view id) const noexcept;
    // The profile for `input`: the requested profile when one is named and registered;
    // otherwise the profile whose detector scores highest, equal scores going to the
    // smaller id. When no detector scores above zero, the result is plain_text().
    const LanguageProfile& detect(const LanguageDetectionInput& input) const noexcept;
    // The profile registered as "plain" or, when there is none, a built-in plain-text
    // profile that highlights nothing and passes its state through unchanged.
    const LanguageProfile& plain_text() const noexcept;
    // How many profiles are registered.
    std::size_t size() const noexcept { return profiles_.size(); }

private:
    std::vector<LanguageProfile> profiles_;
};

// Registers ckVision's built-in profiles into `registry`: "plain", "json", "yaml", "bash",
// "markdown" and "sql". An id that is already registered is skipped, so an application can
// substitute its own profile for one of these by registering it first. A TextEditor
// constructed without a registry builds a private one this way.
void register_standard_syntax_profiles(SyntaxProfileRegistry& registry);

}  // namespace ckv::widgets
