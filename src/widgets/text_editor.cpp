// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_editor.hpp"

#include "cvision/widgets/text_layout.hpp"

#include <algorithm>
#include <array>
#include <charconv>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/menu.hpp"

namespace ckv::widgets {
namespace {

bool less(DocumentPosition left, DocumentPosition right) noexcept { return left.byte < right.byte; }

std::size_t previous_grapheme(std::string_view value, std::size_t byte) {
    if (byte == 0U) return 0U;
    std::size_t previous = 0;
    for (std::size_t cursor = 0; cursor < byte;) {
        previous = cursor;
        cursor = text::grapheme_end(value, cursor);
    }
    return previous;
}

std::size_t decimal_width(std::size_t value) noexcept {
    std::size_t width = 1;
    while (value >= 10U) { value /= 10U; ++width; }
    return width;
}

std::size_t syntax_index(SyntaxTokenKind kind) noexcept { return static_cast<std::size_t>(kind); }

std::size_t grapheme_count(std::string_view value) {
    std::size_t count = 0;
    for (std::size_t byte = 0; byte < value.size();) {
        byte = text::grapheme_end(value, byte);
        ++count;
    }
    return count;
}

bool word_byte(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}

std::size_t previous_word(std::string_view value, std::size_t byte) {
    while (byte > 0U && !word_byte(value[previous_grapheme(value, byte)])) byte = previous_grapheme(value, byte);
    while (byte > 0U && word_byte(value[previous_grapheme(value, byte)])) byte = previous_grapheme(value, byte);
    return byte;
}

std::size_t next_word(std::string_view value, std::size_t byte) {
    while (byte < value.size() && word_byte(value[byte])) byte = text::grapheme_end(value, byte);
    while (byte < value.size() && !word_byte(value[byte])) byte = text::grapheme_end(value, byte);
    return byte;
}

// The start of the word that contains the grapheme at `byte`, which must be a word grapheme.
std::size_t word_start(std::string_view value, std::size_t byte) {
    while (byte > 0U && word_byte(value[previous_grapheme(value, byte)])) byte = previous_grapheme(value, byte);
    return byte;
}

// The grapheme boundary at or before `byte`: `byte` itself when it is one, else the start of the
// cluster it falls inside. A cluster never spans a line break, so the walk starts at its line.
std::size_t grapheme_floor(std::string_view value, std::size_t byte) {
    byte = std::min(byte, value.size());
    const std::size_t newline = byte == 0U ? std::string_view::npos : value.rfind('\n', byte - 1U);
    std::size_t boundary = newline == std::string_view::npos ? 0U : newline + 1U;
    for (std::size_t next = boundary; next < byte;) {
        next = text::grapheme_end(value, next);
        if (next <= byte) boundary = next;
    }
    return boundary;
}

std::size_t word_end(std::string_view value, std::size_t byte) {
    while (byte < value.size() && word_byte(value[byte])) byte = text::grapheme_end(value, byte);
    return byte;
}

KeyChord chord(Key key, Modifier modifiers = Modifier::None, std::string text = {}) {
    return KeyChord{key, modifiers, std::move(text)};
}

}  // namespace

std::vector<EditorKeyBinding> default_editor_key_bindings() {
    return {
        {chord(Key::Char, Modifier::Ctrl, "z"), EditorCommand::Undo},
        {chord(Key::Char, Modifier::Ctrl, "y"), EditorCommand::Redo},
        {chord(Key::Char, Modifier::Ctrl, "x"), EditorCommand::Cut},
        {chord(Key::Delete, Modifier::Shift), EditorCommand::Cut},
        {chord(Key::Char, Modifier::Ctrl, "c"), EditorCommand::Copy},
        {chord(Key::Insert, Modifier::Ctrl), EditorCommand::Copy},
        {chord(Key::Char, Modifier::Ctrl, "v"), EditorCommand::Paste},
        {chord(Key::Insert, Modifier::Shift), EditorCommand::Paste},
        {chord(Key::Char, Modifier::Ctrl, "a"), EditorCommand::SelectAll},
        {chord(Key::Char, Modifier::Ctrl, "f"), EditorCommand::FindSelection},
        {chord(Key::F3), EditorCommand::FindNext},
        {chord(Key::F3, Modifier::Shift), EditorCommand::FindPrevious},
        {chord(Key::Insert), EditorCommand::ToggleOverwrite},
    };
}

TextEditor::TextEditor(std::shared_ptr<EditorDocument> document, SyntaxProfileRegistry* profiles)
    : document_(std::move(document)), profiles_(profiles) {
    if (!document_) document_ = std::make_shared<EditorDocument>();
    if (profiles_ == nullptr) {
        register_standard_syntax_profiles(fallback_profiles_);
        profiles_ = &fallback_profiles_;
    }
    v_scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    v_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    profile_ = profiles_->plain_text();
    cursor_ = document_->begin();
    observer_ = document_->subscribe([this](const DocumentChange& change) {
        // The caret stays on the same text through an edit this editor did
        // not make — a host writing the document from elsewhere, another
        // view's change mirrored in: bytes inserted or removed before it
        // carry it along, and a replacement that swallows it leaves it at
        // the replacement's end. The editor's own edits set the caret
        // explicitly afterwards, so this costs them nothing.
        const auto carry = [&change](DocumentPosition& position) {
            const std::size_t removed = change.replaced_end_byte - change.replaced_begin_byte;
            if (change.replaced_end_byte <= position.byte)
                position.byte = position.byte - removed + change.inserted_bytes;
            else if (change.replaced_begin_byte < position.byte)
                position.byte = change.replaced_begin_byte + change.inserted_bytes;
        };
        carry(cursor_);
        if (selection_anchor_) carry(*selection_anchor_);
        carry_search_match(change);
        // A provisional caret names a place relative to the text as it was;
        // once the text has changed underneath it that place may be text.
        virtual_caret_.reset();
        desired_column_.reset();
        carry_highlights(change);
        clamp_cursor();
        rebuild_lines(change.first_affected_line);
        refresh_search();
        notify_status_changed();
        invalidate();
    });
    // Clean state and format metadata are part of the status without being text.
    state_observer_ = document_->subscribe_state([this] { notify_status_changed(); });
    set_focus_policy(ui::FocusPolicy::TabStop);
    refresh_syntax();
}

TextEditor::~TextEditor() {
    if (document_ && observer_ != 0U) document_->unsubscribe(observer_);
    if (document_ && state_observer_ != 0U) document_->unsubscribe(state_observer_);
}

void TextEditor::on_attached() {
    const auto role = [this](std::string_view name, Style fallback) {
        const ui::RoleId existing = context().roles->find(name);
        return existing == ui::kInvalidRole ? context().roles->intern(name, fallback) : existing;
    };
    text_role_ = role("ckv.editor.text", Style{});
    gutter_role_ = role("ckv.editor.gutter", Style{Color::rgb(120, 120, 120), Color::default_color()});
    selected_role_ = role("ckv.editor.selection", Style{Color::default_color(), Color::default_color(), Attr::Reverse});
    search_role_ = role("ckv.editor.search", Style{Color::rgb(30, 30, 30), Color::rgb(230, 210, 70)});
    static constexpr std::array<std::string_view, 11> names = {"plain", "keyword", "type", "property", "string", "number",
                                                                "comment", "command", "operator", "escape", "error"};
    static constexpr std::array<Color, 11> colors = {Color::default_color(), Color::rgb(80, 160, 255), Color::rgb(80, 200, 220),
                                                       Color::rgb(220, 180, 80), Color::rgb(100, 200, 120), Color::rgb(220, 130, 210),
                                                       Color::rgb(120, 150, 120), Color::rgb(130, 190, 255), Color::rgb(220, 220, 220),
                                                       Color::rgb(240, 180, 80), Color::rgb(255, 90, 90)};
    syntax_roles_.clear();
    for (std::size_t i = 0; i < names.size(); ++i)
        syntax_roles_.push_back(role("ckv.editor.syntax." + std::string(names[i]), Style{colors[i], Color::default_color()}));
}

void TextEditor::set_show_line_numbers(bool enabled) {
    if (show_line_numbers_ == enabled) return;
    show_line_numbers_ = enabled;
    on_resized();
    invalidate();
}

void TextEditor::set_wrap_mode(WrapMode mode) {
    if (wrap_mode_ == mode) return;
    wrap_mode_ = mode;
    display_rows_dirty_ = true;
    // Rewrapping changes both how many rows there are and how wide the widest
    // is, so the bars have to be settled again before the cursor is chased.
    relayout_scrollbars();
    ensure_cursor_visible();
    invalidate();
}

void TextEditor::set_overwrite(bool value) {
    if (overwrite_ == value) return;
    overwrite_ = value;
    notify_status_changed();
    invalidate();
}

void TextEditor::set_tab_width(int spaces) { tab_width_ = std::max(1, spaces); }

void TextEditor::set_key_bindings(std::vector<EditorKeyBinding> bindings) { key_bindings_ = std::move(bindings); }

bool TextEditor::perform(EditorCommand command) {
    switch (command) {
        case EditorCommand::Undo:
        case EditorCommand::Redo: {
            // Walking the history changes the text as much as typing does.
            if (read_only_ || !enabled_in_tree()) return false;
            const bool redo = command == EditorCommand::Redo;
            if (history_handler_) return history_handler_(*this, redo);
            // Every view carries its selection through the step as through any change; the
            // view that asked for it then takes the selection the step restores.
            const std::optional<DocumentChange> change = redo ? document_->redo() : document_->undo();
            if (!change) return false;
            if (change->selection) restore_selection(*change->selection);
            return true;
        }
        case EditorCommand::Cut: return cut_selection_to_clipboard();
        case EditorCommand::Copy: return copy_selection_to_clipboard();
        case EditorCommand::Paste: return paste_from_clipboard();
        case EditorCommand::SelectAll: return select_all();
        case EditorCommand::FindSelection: return use_selection_as_search_query();
        case EditorCommand::FindNext: return find_next(true);
        case EditorCommand::FindPrevious: return find_next(false);
        // Through set_overwrite, so that the mode reaches the status observers
        // with the keystroke that changed it: a toggle that only invalidated
        // left the frame reading INS until the next cursor move republished it.
        case EditorCommand::ToggleOverwrite: set_overwrite(!overwrite_); return true;
    }
    return false;
}

void TextEditor::set_status_changed_handler(std::function<void(const EditorStatus&)> handler) {
    status_changed_ = std::move(handler);
    notify_status_changed();
}

TextEditor::StatusObserverId TextEditor::subscribe_status(StatusObserver observer) {
    const StatusObserverId identifier = next_status_observer_id_++;
    status_observers_.push_back({identifier, std::move(observer)});
    return identifier;
}

void TextEditor::unsubscribe_status(StatusObserverId observer) noexcept {
    status_observers_.erase(std::remove_if(status_observers_.begin(), status_observers_.end(), [observer](const auto& candidate) {
                                return candidate.first == observer;
                            }),
                            status_observers_.end());
}

void TextEditor::set_search_query(EditorSearchQuery query) {
    search_query_ = std::move(query);
    refresh_search();
    invalidate();
}

void TextEditor::clear_search() {
    if (search_query_.text.empty() && search_matches_.empty()) return;
    search_query_ = EditorSearchQuery{};
    search_matches_.clear();
    active_search_match_.reset();
    invalidate();
}

void TextEditor::refresh_search() {
    search_matches_ = EditorSearch::find_all(*document_, search_query_);
    // The current match survives only as a match: text that no longer satisfies the query (a
    // whole-word match that gained a letter beside it) ends it.
    if (active_search_match_ &&
        std::none_of(search_matches_.begin(), search_matches_.end(),
                     [this](const EditorSearchMatch& match) { return match.range == *active_search_match_; }))
        active_search_match_.reset();
}

void TextEditor::carry_search_match(const DocumentChange& change) {
    if (!active_search_match_) return;
    DocumentRange& match = *active_search_match_;
    if (change.replaced_end_byte <= match.begin.byte) {
        // Wholly before the match: the match's text moves with the difference in length.
        const std::size_t length = match.end.byte - match.begin.byte;
        match.begin.byte = match.begin.byte - change.replaced_end_byte + change.replaced_begin_byte + change.inserted_bytes;
        match.end.byte = match.begin.byte + length;
    } else if (change.replaced_begin_byte < match.end.byte) {
        // It rewrote some of the match's text.
        active_search_match_.reset();
        return;
    }
    match.begin.revision = change.revision;
    match.end.revision = change.revision;
}

bool TextEditor::use_selection_as_search_query() {
    const auto selected = selection();
    if (!selected) return false;
    const std::string text = document_->text(*selected);
    if (text.empty()) return false;
    set_search_query(EditorSearchQuery{std::move(text), true, false});
    return !search_matches_.empty();
}

bool TextEditor::activate_search_match(const DocumentRange& range) {
    if (range.begin.revision != document_->revision() || range.end.revision != document_->revision()) return false;
    selection_anchor_ = range.begin;
    cursor_ = range.end;
    virtual_caret_.reset();
    desired_column_.reset();
    active_search_match_ = range;
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
    return true;
}

bool TextEditor::find_next(bool forward) {
    refresh_search();
    if (search_matches_.empty()) return false;
    const auto selected = selection();
    const std::size_t pivot = selected ? (forward ? selected->end.byte : selected->begin.byte) : cursor_.byte;
    if (forward) {
        for (std::size_t index = 0; index < search_matches_.size(); ++index)
            if (search_matches_[index].range.begin.byte >= pivot) return activate_search_match(search_matches_[index].range);
        return activate_search_match(search_matches_.front().range);
    }
    for (std::size_t index = search_matches_.size(); index-- > 0;) {
        if (search_matches_[index].range.end.byte <= pivot) return activate_search_match(search_matches_[index].range);
    }
    return activate_search_match(search_matches_.back().range);
}

bool TextEditor::replace_current_search_match(std::string replacement) {
    if (!active_search_match_) return false;
    EditRequest request;
    request.kind = EditKind::Replace;
    request.edits.push_back(DocumentTextEdit{*active_search_match_, replacement});
    request.text = std::move(replacement);
    if (!submit_edit(std::move(request))) return false;
    active_search_match_.reset();
    return true;
}

bool TextEditor::replace_all_search_matches(const std::string& replacement) {
    if (search_matches_.empty()) return false;
    EditRequest request;
    request.kind = EditKind::ReplaceAll;
    request.text = replacement;
    request.edits.reserve(search_matches_.size());
    for (const EditorSearchMatch& match : search_matches_) request.edits.push_back(DocumentTextEdit{match.range, replacement});
    if (!submit_edit(std::move(request))) return false;
    active_search_match_.reset();
    return true;
}

void TextEditor::set_file_name(std::string name) {
    file_name_ = std::move(name);
    set_profile(std::nullopt);
}

void TextEditor::set_profile(std::optional<std::string> id) {
    LanguageDetectionInput input;
    input.requested_profile = std::move(id);
    input.file_name = file_name_;
    const std::string document_text = document_->text();
    input.content_prefix = document_text.substr(0, std::min<std::size_t>(document_text.size(), 512U));
    const std::size_t newline = input.content_prefix.find('\n');
    input.shebang = input.content_prefix.substr(0, newline);
    const LanguageProfile& detected = profiles_->detect(input);
    const bool changed = detected.id != profile_.id;
    profile_ = detected;
    refresh_syntax();
    if (changed) notify_status_changed();
    invalidate();
}

void TextEditor::refresh_syntax() {
    lines_.clear();  // profile selection changes invalidate every lexical state.
    rebuild_lines();
}

bool TextEditor::set_highlights(DocumentRevision revision, std::vector<HighlightSpan> spans) {
    if (revision != document_->revision()) return false;
    const std::size_t size = document_->byte_size();
    for (std::size_t index = 0; index < spans.size(); ++index) {
        const HighlightSpan& span = spans[index];
        if (span.begin_byte > span.end_byte || span.end_byte > size) return false;
        if (index > 0U && span.begin_byte < spans[index - 1U].end_byte) return false;
    }
    highlights_ = std::move(spans);
    highlights_active_ = true;
    invalidate();
    return true;
}

void TextEditor::clear_highlights() {
    if (!highlights_active_) return;
    highlights_active_ = false;
    highlights_.clear();
    invalidate();
}

void TextEditor::carry_highlights(const DocumentChange& change) {
    if (!highlights_active_) return;
    // Spans the edit did not touch keep colouring the same text; a span the
    // edit overlaps described text that is gone, so it goes with it. The
    // host's next set_highlights() replaces the lot.
    std::vector<HighlightSpan> carried;
    carried.reserve(highlights_.size());
    for (const HighlightSpan& span : highlights_) {
        if (span.end_byte <= change.replaced_begin_byte) {
            carried.push_back(span);
        } else if (span.begin_byte >= change.replaced_end_byte) {
            const std::size_t begin = span.begin_byte - change.replaced_end_byte;
            const std::size_t length = span.end_byte - span.begin_byte;
            const std::size_t shifted = change.replaced_begin_byte + change.inserted_bytes + begin;
            carried.push_back(HighlightSpan{shifted, shifted + length, span.role});
        }
    }
    highlights_ = std::move(carried);
}

const std::vector<TextEditor::DisplayRow>& TextEditor::display_rows(int content_width) const {
    const int width = std::max(content_width, 1);
    if (!display_rows_dirty_ && display_rows_width_ == width) return display_rows_cache_;
    display_rows_cache_.clear();
    display_rows_width_ = width;
    display_rows_dirty_ = false;
    constexpr std::string_view reflow_marker = "\xE2\x86\xAA";  // U+21AA ↪
    const int marker_width = text::text_width(reflow_marker);
    for (std::size_t line_index = 0; line_index < lines_.size(); ++line_index) {
        const Line& line = lines_[line_index];
        // One wrap rule for the whole library. A continued row reserves a
        // cell for the reflow marker, which is what the shared reserve is for.
        const std::vector<WrapSegment> segments =
            wrap_text(line.text, WrapOptions{width, wrap_mode_, marker_width});
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const bool continues = i + 1 < segments.size();
            display_rows_cache_.push_back(
                DisplayRow{line_index, segments[i].begin, segments[i].end, continues});
        }
    }
    if (display_rows_cache_.empty()) display_rows_cache_.push_back(DisplayRow{});
    return display_rows_cache_;
}

std::size_t TextEditor::cursor_display_row(const std::vector<DisplayRow>& rows) const {
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const DisplayRow& row = rows[index];
        if (row.line >= lines_.size()) continue;
        const std::size_t byte = cursor_.byte - std::min(cursor_.byte, lines_[row.line].start_byte);
        if (byte < row.end_byte || (byte == row.end_byte && !row.continues)) return index;
    }
    return rows.empty() ? 0U : rows.size() - 1U;
}

std::pair<int, int> TextEditor::caret_display_cell(const std::vector<DisplayRow>& rows) const {
    if (!virtual_caret_) {
        const std::size_t index = cursor_display_row(rows);
        if (index >= rows.size()) return {0, 0};
        const DisplayRow& row = rows[index];
        const std::size_t within =
            row.line < lines_.size() ? cursor_.byte - std::min(cursor_.byte, lines_[row.line].start_byte) : 0U;
        return {static_cast<int>(index), column_x(row, within)};
    }
    const VirtualCaret& caret = *virtual_caret_;
    if (lines_.empty() || rows.empty()) return {0, 0};
    const std::size_t last_line = lines_.size() - 1U;
    if (caret.line > last_line) {
        const int below = static_cast<int>(caret.line - last_line);
        return {static_cast<int>(rows.size()) - 1 + below, static_cast<int>(caret.column)};
    }
    // Past a line's end: on that line's last display row, counted from where
    // that row starts.
    for (std::size_t index = rows.size(); index-- > 0;) {
        const DisplayRow& row = rows[index];
        if (row.line != caret.line) continue;
        const DisplayRow from_line_start{row.line, 0U, row.begin_byte, false};
        const int before = column_x(from_line_start, row.begin_byte);
        return {static_cast<int>(index), static_cast<int>(caret.column) - before};
    }
    return {0, 0};
}

int TextEditor::left_column() const noexcept {
    return h_scrollbar_ != nullptr ? h_scrollbar_->position() : 0;
}

int TextEditor::column_x(const DisplayRow& row, std::size_t byte) const {
    if (row.line >= lines_.size()) return 0;
    const Line& line = lines_[row.line];
    int columns = 0;
    for (std::size_t at = row.begin_byte; at < byte && at < line.text.size();) {
        const std::size_t end = text::grapheme_end(line.text, at);
        columns += text::grapheme_width(std::string_view(line.text).substr(at, end - at));
        at = end;
    }
    return columns;
}

void TextEditor::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

void TextEditor::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

ScrollbarPolicy TextEditor::vertical_scrollbar_policy() const noexcept {
    return v_scrollbar_ != nullptr ? v_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

ScrollbarPolicy TextEditor::horizontal_scrollbar_policy() const noexcept {
    return h_scrollbar_ != nullptr ? h_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

void TextEditor::relayout_scrollbars() {
    if (v_scrollbar_ == nullptr || h_scrollbar_ == nullptr) return;
    const int gutter = static_cast<int>(gutter_width());

    // The extent a virtual caret reaches is content too: a caret the reader
    // placed below the text or past a line's end must be scrollable into view.
    const auto extent = [this](const std::vector<DisplayRow>& rows) {
        int widest = 0;
        for (const DisplayRow& row : rows) widest = std::max(widest, column_x(row, row.end_byte));
        int height = static_cast<int>(rows.size());
        if (virtual_caret_) {
            const auto [row, column] = caret_display_cell(rows);
            widest = std::max(widest, column + 1);
            height = std::max(height, row + 1);
        }
        return Size{widest, height};
    };

    // The gutter is chrome, not content: it is subtracted before the text
    // area is measured, and it never scrolls sideways with the text.
    const ScrollGeometry geometry = resolve_scroll_geometry(
        Size{std::max(0, bounds().width - gutter), bounds().height}, v_scrollbar_->policy(),
        h_scrollbar_->policy(), [this, &extent](int viewport_width) {
            return extent(display_rows(std::max(1, viewport_width)));
        });

    viewport_width_ = geometry.viewport_width;
    viewport_height_ = geometry.viewport_height;
    const Size content = extent(display_rows(std::max(1, viewport_width_)));
    content_width_ = content.width;

    const int v_width = geometry.show_vertical ? std::min(1, bounds().width) : 0;
    const int h_height = geometry.show_horizontal ? std::min(1, bounds().height) : 0;
    v_scrollbar_->set_range(content.height, std::max(1, geometry.viewport_height));
    h_scrollbar_->set_range(content_width_, std::max(1, geometry.viewport_width));
    v_scrollbar_->set_bounds(Rect{std::max(0, bounds().width - v_width), 0, v_width,
                                  std::max(0, bounds().height - h_height)});
    h_scrollbar_->set_bounds(Rect{gutter, std::max(0, bounds().height - h_height),
                                  std::max(0, bounds().width - gutter - v_width), h_height});
    v_scrollbar_->set_visible(geometry.show_vertical);
    h_scrollbar_->set_visible(geometry.show_horizontal);
    v_scrollbar_->set_position(top_display_row_);
}

void TextEditor::ensure_cursor_visible() {
    const int content_width = std::max(1, viewport_width_ > 0
                                              ? viewport_width_
                                              : bounds().width - static_cast<int>(gutter_width()));
    const auto& rows = display_rows(content_width);
    const auto [cursor_row, cursor_x] = caret_display_cell(rows);
    const int height = std::max(1, viewport_height_ > 0 ? viewport_height_ : bounds().height);
    const int extent = std::max(static_cast<int>(rows.size()), cursor_row + 1);
    if (cursor_row < top_display_row_) top_display_row_ = cursor_row;
    if (cursor_row >= top_display_row_ + height) top_display_row_ = cursor_row - height + 1;
    top_display_row_ = std::clamp(top_display_row_, 0, std::max(0, extent - height));
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_position(top_display_row_);

    // And sideways: without wrapping, a cursor walking along a long line
    // would otherwise leave the viewport and keep going unseen.
    if (h_scrollbar_ == nullptr) return;
    if (cursor_x < h_scrollbar_->position()) {
        h_scrollbar_->set_position(cursor_x);
    } else if (cursor_x >= h_scrollbar_->position() + content_width) {
        h_scrollbar_->set_position(cursor_x - content_width + 1);
    }
}

void TextEditor::notify_status_changed() {
    const EditorStatus current = status();
    if (status_changed_) status_changed_(current);
    const auto observers = status_observers_;
    for (const auto& [identifier, observer] : observers) {
        (void)identifier;
        if (observer) observer(current);
    }
}

void TextEditor::rebuild_lines(std::size_t first_dirty_line) {
    (void)first_dirty_line;
    lines_.clear();
    display_rows_dirty_ = true;
    const std::string value = document_->text();
    std::vector<std::string> source_lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t newline = value.find('\n', start);
        const std::size_t end = newline == std::string::npos ? value.size() : newline;
        Line line;
        line.start_byte = start;
        line.text = value.substr(start, end - start);
        lines_.push_back(std::move(line));
        source_lines.push_back(lines_.back().text);
        if (newline == std::string::npos) break;
        start = newline + 1U;
    }
    (void)syntax_cache_.update(profile_, source_lines);
    for (std::size_t index = 0; index < lines_.size(); ++index) {
        const SyntaxCacheLine* cached = syntax_cache_.line(index);
        if (cached == nullptr) continue;
        lines_[index].spans = cached->spans;
        lines_[index].incoming_state = cached->incoming_state;
        lines_[index].outgoing_state = cached->outgoing_state;
    }
    if (lines_.empty()) lines_.push_back(Line{});
    ensure_cursor_visible();
}

std::optional<DocumentRange> TextEditor::selection() const noexcept {
    if (!selection_anchor_ || selection_anchor_->revision != cursor_.revision || *selection_anchor_ == cursor_) return std::nullopt;
    return less(*selection_anchor_, cursor_) ? DocumentRange{*selection_anchor_, cursor_} : DocumentRange{cursor_, *selection_anchor_};
}

bool TextEditor::set_selection(DocumentRange range) {
    if (range.begin.revision != document_->revision() || range.end.revision != document_->revision() ||
        range.begin.byte > range.end.byte || !document_->position_at_byte(range.begin.byte) ||
        !document_->position_at_byte(range.end.byte))
        return false;

    selection_anchor_ = range.begin;
    cursor_ = range.end;
    virtual_caret_.reset();
    desired_column_.reset();
    relayout_scrollbars();
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
    return true;
}

void TextEditor::restore_selection(DocumentSelection selection) {
    const auto anchor = document_->position_at_byte(selection.anchor_byte);
    const auto caret = document_->position_at_byte(selection.caret_byte);
    if (!anchor || !caret) return;
    cursor_ = *caret;
    if (*anchor == *caret) selection_anchor_.reset();
    else selection_anchor_ = *anchor;
    virtual_caret_.reset();
    desired_column_.reset();
    relayout_scrollbars();
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
}

bool TextEditor::select_all() {
    if (document_->byte_size() == 0U) return false;
    selection_anchor_ = document_->begin();
    cursor_ = document_->end();
    virtual_caret_.reset();
    desired_column_.reset();
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
    return true;
}

EditorStatus TextEditor::status() const {
    EditorStatus result;
    if (virtual_caret_) {
        result.virtual_caret = true;
        result.line = virtual_caret_->line + 1U;
        std::size_t column = virtual_caret_->column;
        if (virtual_caret_->line < lines_.size()) {
            // Graphemes up to the line's end, then one column per blank cell.
            const std::string& text = lines_[virtual_caret_->line].text;
            const auto width = static_cast<std::size_t>(text::text_width(text));
            column = grapheme_count(text) + (column - std::min(column, width));
        }
        result.column = column + 1U;
    } else if (const auto line_column = document_->line_column(cursor_)) {
        result.line = line_column->line + 1U;
        result.column = line_column->column + 1U;
    }
    if (const auto selected = selection()) result.selection_bytes = selected->end.byte - selected->begin.byte;
    result.modified = document_->modified();
    result.overwrite = overwrite_;
    result.profile_id = profile_.id;
    result.encoding = document_->encoding();
    result.newline = document_->preferred_newline();
    return result;
}

void TextEditor::clamp_cursor() {
    const auto current = [this](DocumentPosition position) {
        const std::size_t byte = std::min(position.byte, document_->byte_size());
        if (const auto exact = document_->position_at_byte(byte)) return *exact;
        // Rare: the change joined graphemes around it, so it now falls inside a cluster.
        const std::string value = document_->text();
        return document_->position_at_byte(grapheme_floor(value, byte)).value_or(document_->begin());
    };
    cursor_ = current(cursor_);
    if (selection_anchor_) selection_anchor_ = current(*selection_anchor_);
}

void TextEditor::move_cursor(DocumentPosition target, bool extend, bool keep_desired_column) {
    if (target.revision != document_->revision()) return;
    const bool had_virtual_caret = virtual_caret_.has_value();
    virtual_caret_.reset();
    if (!keep_desired_column) desired_column_.reset();
    if (extend) {
        if (!selection_anchor_) selection_anchor_ = cursor_;
    } else {
        selection_anchor_.reset();
    }
    cursor_ = target;
    if (had_virtual_caret) relayout_scrollbars();
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
}

void TextEditor::move_vertically(std::ptrdiff_t lines, bool extend) {
    const auto line_column = document_->line_column(cursor_);
    if (!line_column) return;
    if (!desired_column_) desired_column_ = line_column->column;
    const std::ptrdiff_t last = static_cast<std::ptrdiff_t>(document_->line_count()) - 1;
    const std::size_t target_line = static_cast<std::size_t>(
        std::clamp(static_cast<std::ptrdiff_t>(line_column->line) + lines, std::ptrdiff_t{0}, last));
    const std::size_t column = std::min(*desired_column_, grapheme_count(lines_[target_line].text));
    const auto target = document_->position_at_line_column(target_line, column);
    if (target) move_cursor(*target, extend, /*keep_desired_column=*/true);
}

DocumentPosition TextEditor::virtual_caret_anchor(const VirtualCaret& caret) const {
    if (caret.line < lines_.size()) {
        const Line& line = lines_[caret.line];
        return document_->position_at_byte(line.start_byte + line.text.size()).value_or(document_->end());
    }
    return document_->end();
}

std::string TextEditor::virtual_caret_padding(const VirtualCaret& caret) const {
    if (lines_.empty()) return {};
    const std::size_t last_line = lines_.size() - 1U;
    if (caret.line <= last_line) {
        const auto width = static_cast<std::size_t>(text::text_width(lines_[caret.line].text));
        return std::string(caret.column - std::min(caret.column, width), ' ');
    }
    return std::string(caret.line - last_line, '\n') + std::string(caret.column, ' ');
}

void TextEditor::set_virtual_space(bool enabled) {
    if (virtual_space_ == enabled) return;
    virtual_space_ = enabled;
    if (!enabled && virtual_caret_) {
        virtual_caret_.reset();
        relayout_scrollbars();
        ensure_cursor_visible();
        notify_status_changed();
        invalidate();
    }
}

bool TextEditor::set_virtual_caret(VirtualCaret caret) {
    if (!virtual_space_ || lines_.empty()) return false;
    if (caret.line < lines_.size()) {
        const auto width = static_cast<std::size_t>(text::text_width(lines_[caret.line].text));
        if (caret.column <= width) return false;
    }
    selection_anchor_.reset();
    desired_column_.reset();
    cursor_ = virtual_caret_anchor(caret);
    virtual_caret_ = caret;
    relayout_scrollbars();
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
    return true;
}

bool TextEditor::submit_edit(EditRequest request) {
    if (read_only_ || !enabled_in_tree()) return false;
    // The request carries the provisional caret; whatever happens next, the
    // editor no longer holds it — a host that wants one after its own
    // transaction places it again.
    const bool had_virtual_caret = virtual_caret_.has_value();
    virtual_caret_.reset();
    desired_column_.reset();
    if (edit_handler_ && edit_handler_(*this, request)) {
        if (had_virtual_caret && !virtual_caret_) relayout_scrollbars();
        ensure_cursor_visible();
        notify_status_changed();
        invalidate();
        return true;
    }
    if (request.kind == EditKind::Cut) {
        if (context().app == nullptr) return false;
        context().app->set_clipboard_text(request.text);
    }
    DocumentTransaction transaction = document_->transaction();
    for (DocumentTextEdit& edit : request.edits) transaction.replace(edit.range, std::move(edit.text));
    // What an undo of this edit gives back: the selection, or the caret, it was made at.
    transaction.set_selection_before(
        DocumentSelection{selection_anchor_.value_or(cursor_).byte, cursor_.byte});
    const DocumentEditResult result = document_->commit(std::move(transaction));
    if (!result || !result.change) return false;
    cursor_ = document_->position_at_byte(result.change->replaced_begin_byte + result.change->inserted_bytes).value_or(document_->end());
    selection_anchor_.reset();
    if (had_virtual_caret) relayout_scrollbars();
    ensure_cursor_visible();
    notify_status_changed();
    return true;
}

bool TextEditor::submit_text(EditKind kind, std::string value) {
    if (read_only_ || !enabled_in_tree()) return false;
    EditRequest request;
    request.kind = kind;
    if (virtual_caret_) {
        const DocumentPosition anchor = virtual_caret_anchor(*virtual_caret_);
        request.edits.push_back(DocumentTextEdit{DocumentRange{anchor, anchor}, virtual_caret_padding(*virtual_caret_) + value});
        request.virtual_caret = virtual_caret_;
        request.text = std::move(value);
        return submit_edit(std::move(request));
    }
    DocumentRange range = selection().value_or(DocumentRange{cursor_, cursor_});
    // Overwrite replaces complete following graphemes on the current logical
    // line, for typing only. Newline-bearing input retains ordinary insertion
    // semantics: it must never silently consume a line boundary.
    if (kind == EditKind::Insert && !selection() && overwrite_ && value.find('\n') == std::string::npos &&
        cursor_.byte < document_->byte_size()) {
        const auto line_column = document_->line_column(cursor_);
        if (line_column && line_column->line < lines_.size()) {
            const std::size_t line_end = lines_[line_column->line].start_byte + lines_[line_column->line].text.size();
            const std::size_t count = grapheme_count(value);
            const std::string current = document_->text();
            std::size_t end = cursor_.byte;
            for (std::size_t replaced = 0; replaced < count && end < line_end;) {
                end = text::grapheme_end(current, end);
                ++replaced;
            }
            if (const auto position = document_->position_at_byte(end)) range.end = *position;
        }
    }
    request.edits.push_back(DocumentTextEdit{range, value});
    request.text = std::move(value);
    return submit_edit(std::move(request));
}

bool TextEditor::erase(EditKind kind) {
    if (read_only_ || !enabled_in_tree()) return false;
    // Erasing at a provisional caret has nothing to erase: it abandons the
    // caret and leaves the text as it was.
    if (virtual_caret_) {
        virtual_caret_.reset();
        relayout_scrollbars();
        ensure_cursor_visible();
        notify_status_changed();
        invalidate();
        return true;
    }
    EditRequest request;
    request.kind = kind;
    if (const auto selected = selection()) {
        request.edits.push_back(DocumentTextEdit{*selected, {}});
        return submit_edit(std::move(request));
    }
    const std::string value = document_->text();
    std::size_t begin = cursor_.byte;
    std::size_t end = cursor_.byte;
    switch (kind) {
        case EditKind::DeleteBackward: begin = previous_grapheme(value, cursor_.byte); break;
        case EditKind::DeleteWordBackward: begin = previous_word(value, cursor_.byte); break;
        case EditKind::DeleteForward:
            end = cursor_.byte < value.size() ? text::grapheme_end(value, cursor_.byte) : cursor_.byte;
            break;
        case EditKind::DeleteWordForward: end = next_word(value, cursor_.byte); break;
        default: return false;
    }
    if (begin == end) return false;
    const auto first = document_->position_at_byte(begin);
    const auto last = document_->position_at_byte(end);
    if (!first || !last) return false;
    request.edits.push_back(DocumentTextEdit{DocumentRange{*first, *last}, {}});
    return submit_edit(std::move(request));
}

bool TextEditor::copy_selection_to_clipboard() {
    if (context().app == nullptr) return false;
    const auto target = selection();
    if (!target) return false;
    const std::string copied = document_->text(*target);
    if (copied.empty()) return false;
    context().app->set_clipboard_text(copied);
    return true;
}

bool TextEditor::cut_selection_to_clipboard() {
    const auto target = selection();
    if (!target) return false;
    EditRequest request;
    request.kind = EditKind::Cut;
    request.text = document_->text(*target);
    request.edits.push_back(DocumentTextEdit{*target, {}});
    if (request.text.empty()) return false;
    return submit_edit(std::move(request));
}

bool TextEditor::paste_from_clipboard() {
    if (context().app == nullptr || context().app->clipboard_text().empty()) return false;
    return submit_text(EditKind::Paste, context().app->clipboard_text());
}

std::size_t TextEditor::gutter_width() const {
    return show_line_numbers_ ? decimal_width(lines_.size()) + 1U : 0U;
}

Style TextEditor::style_for(std::size_t line, std::size_t line_byte, bool selected, ui::RoleId highlight) const {
    if (selected) return context().theme->resolve(selected_role_);
    const std::size_t absolute = line < lines_.size() ? lines_[line].start_byte + line_byte : 0U;
    for (const EditorSearchMatch& match : search_matches_)
        if (match.range.begin.revision == document_->revision() && absolute >= match.range.begin.byte && absolute < match.range.end.byte)
            return context().theme->resolve(search_role_);
    // Host colouring replaces the profile's entirely: text it leaves
    // unmarked is plain text, not whatever a line lexer would have made of it.
    if (highlights_active_)
        return context().theme->resolve(highlight != ui::kInvalidRole ? highlight : text_role_);
    SyntaxTokenKind kind = SyntaxTokenKind::Plain;
    if (line < lines_.size())
        for (const SyntaxSpan& span : lines_[line].spans)
            if (line_byte >= span.begin_byte && line_byte < span.end_byte) { kind = span.kind; break; }
    const std::size_t index = syntax_index(kind);
    if (index < syntax_roles_.size()) return context().theme->resolve(syntax_roles_[index]);
    return context().theme->resolve(text_role_);
}

void TextEditor::draw(scene::Painter& painter) {
    const Style base = context().theme->resolve(text_role_);
    const std::size_t gutter = gutter_width();
    const int content_x = static_cast<int>(std::min<std::size_t>(gutter, static_cast<std::size_t>(std::max(0, bounds().width))));
    const int content_width = viewport_width_ > 0 ? viewport_width_ : std::max(0, bounds().width - content_x);
    const int rows_shown = viewport_height_ > 0 ? viewport_height_ : bounds().height;
    const int left = left_column();
    const auto selected = selection();
    const auto& rows = display_rows(std::max(1, content_width));
    const int extent = std::max(static_cast<int>(rows.size()),
                                virtual_caret_ ? caret_display_cell(rows).first + 1 : 0);
    const int max_top = std::max(0, extent - std::max(1, rows_shown));
    top_display_row_ = std::clamp(top_display_row_, 0, max_top);
    constexpr std::string_view reflow_marker = "\xE2\x86\xAA";  // U+21AA ↪
    const Style marker_style = context().theme->resolve(gutter_role_);
    // The text is drawn through a painter clipped to the content area, so a
    // glyph the left edge cuts in half shows as a blank in its own style
    // rather than spilling into the gutter, and nothing after it moves.
    scene::Painter content = painter.clipped(Rect{content_x, 0, content_width, rows_shown});
    for (int row = 0; row < rows_shown; ++row) {
        painter.fill(Rect{0, row, bounds().width, 1}, Cell::from_grapheme(" ", base));
        const int display_index = top_display_row_ + row;
        if (display_index < 0 || static_cast<std::size_t>(display_index) >= rows.size()) continue;
        const DisplayRow& display = rows[static_cast<std::size_t>(display_index)];
        const Line& line = lines_[display.line];
        if (show_line_numbers_) {
            char number[32]{};
            const auto converted = std::to_chars(number, number + sizeof(number), display.line + 1U);
            const std::string digits(number, converted.ptr);
            const std::string padded = display.begin_byte == 0U
                ? std::string(gutter - 1U - digits.size(), ' ') + digits + " "
                : std::string(gutter, ' ');
            painter.draw_text(Point{0, row}, padded, context().theme->resolve(gutter_role_));
        }
        // The first host span that could colour this row; the walk below only
        // ever advances it, so a row costs one search however many spans the
        // document has.
        auto highlight = highlights_.cbegin();
        if (highlights_active_) {
            const std::size_t row_begin = line.start_byte + display.begin_byte;
            highlight = std::partition_point(highlights_.cbegin(), highlights_.cend(),
                                             [row_begin](const HighlightSpan& span) { return span.end_byte <= row_begin; });
        }
        // Walk the row in its own columns and subtract the scroll offset, so
        // a horizontally scrolled row starts part-way through rather than
        // being dropped. The gutter is drawn above at column 0 regardless:
        // line numbers that slid away with the text would stop indexing it.
        int cell_x = 0;
        for (std::size_t byte = display.begin_byte; byte < display.end_byte;) {
            const std::size_t start_byte = byte;
            const std::size_t end = text::grapheme_end(line.text, start_byte);
            const std::size_t absolute = line.start_byte + start_byte;
            const bool highlighted = selected && absolute >= selected->begin.byte && absolute < selected->end.byte;
            const std::string_view grapheme(line.text.data() + start_byte, end - start_byte);
            const int width = text::grapheme_width(grapheme);
            const int x = content_x + cell_x - left;
            cell_x += width;
            byte = end;
            ui::RoleId role = ui::kInvalidRole;
            if (highlights_active_) {
                while (highlight != highlights_.cend() && highlight->end_byte <= absolute) ++highlight;
                if (highlight != highlights_.cend() && highlight->begin_byte <= absolute) role = highlight->role;
            }
            if (x + width <= content_x) continue;               // off to the left
            if (x >= content_x + content_width) break;          // past the right edge
            content.draw_text(Point{x, row}, grapheme, style_for(display.line, start_byte, highlighted, role));
        }
        if (display.continues && content_width > text::text_width(reflow_marker))
            painter.draw_text(Point{content_x + content_width - text::text_width(reflow_marker), row}, reflow_marker, marker_style);
    }
}

bool TextEditor::on_key(const KeyEvent& event) {
    if (!enabled_in_tree()) return false;
    if (event.action == KeyAction::Release) return false;
    if (context_menu_handler_ && is_keyboard_context_menu_request(event)) {
        const Rect absolute = absolute_bounds();
        const std::optional<CursorState> caret = cursor_state();
        context_menu_handler_(*this, caret ? caret->position : Point{absolute.x, absolute.y});
        return true;
    }
    for (const EditorKeyBinding& binding : key_bindings_)
        if (binding.chord == event.chord) return perform(binding.command);
    const bool control = has_modifier(event.chord.modifiers, Modifier::Ctrl);
    const bool extend = has_modifier(event.chord.modifiers, Modifier::Shift);
    const bool has_alt_or_super = has_modifier(event.chord.modifiers, Modifier::Alt) ||
                                  has_modifier(event.chord.modifiers, Modifier::Super);
    // Shift participates in producing ordinary text; it does not turn that
    // text into a command chord. This is the same editing-boundary contract
    // as InputLine and Memo. Alt/Ctrl/Super remain available to command
    // routing, while the terminal-provided text stays authoritative for the
    // shifted or composed character.
    if (event.chord.key == Key::Char && !event.chord.text.empty() && !has_alt_or_super && !control)
        return submit_text(EditKind::Insert, event.chord.text);
    if (event.chord.key == Key::Tab && !extend && !control && !has_alt_or_super)
        return submit_text(EditKind::Indent, std::string(static_cast<std::size_t>(tab_width_), ' '));
    const auto line_column = document_->line_column(cursor_);
    if (!line_column) return false;
    switch (event.chord.key) {
        case Key::Left: {
            const std::string value = document_->text();
            const std::size_t target = cursor_.byte == 0U ? 0U
                : control ? previous_word(value, cursor_.byte) : previous_grapheme(value, cursor_.byte);
            move_cursor(*document_->position_at_byte(target), extend);
            return true;
        }
        case Key::Right: {
            const std::string value = document_->text();
            const std::size_t target = cursor_.byte == document_->byte_size() ? cursor_.byte
                : control ? next_word(value, cursor_.byte) : text::grapheme_end(value, cursor_.byte);
            move_cursor(*document_->position_at_byte(target), extend);
            return true;
        }
        case Key::Up: move_vertically(-1, extend); return true;
        case Key::Down: move_vertically(1, extend); return true;
        case Key::Home: move_cursor(control ? document_->begin() : *document_->position_at_line_column(line_column->line, 0), extend); return true;
        case Key::End: {
            if (control) { move_cursor(document_->end(), extend); return true; }
            const Line& line = lines_[line_column->line];
            const auto target = document_->position_at_byte(line.start_byte + line.text.size());
            if (target) move_cursor(*target, extend);
            return true;
        }
        case Key::PageUp:
        case Key::PageDown: {
            // A page is the viewport less the row that stays in sight as the
            // reader's anchor. The view scrolls by the same amount, so the
            // caret keeps its place on screen while the text moves under it.
            const int height = viewport_height_ > 0 ? viewport_height_ : bounds().height;
            const int page = std::max(1, height - 1);
            const int direction = event.chord.key == Key::PageUp ? -1 : 1;
            const auto& rows = display_rows(std::max(1, viewport_width_ > 0 ? viewport_width_
                                                                            : bounds().width - static_cast<int>(gutter_width())));
            top_display_row_ = std::clamp(top_display_row_ + direction * page, 0,
                                          std::max(0, static_cast<int>(rows.size()) - std::max(1, height)));
            move_vertically(direction * page, extend);
            return true;
        }
        case Key::Backspace: return erase(control ? EditKind::DeleteWordBackward : EditKind::DeleteBackward);
        case Key::Delete: return erase(control ? EditKind::DeleteWordForward : EditKind::DeleteForward);
        case Key::Enter:
            if (read_only_) return false;
            return submit_text(EditKind::LineBreak, "\n");
        default: return false;
    }
}

bool TextEditor::on_text(const TextEvent& event) {
    return enabled_in_tree() && !event.text.empty() &&
           submit_text(event.from_paste ? EditKind::Paste : EditKind::Insert, event.text);
}

std::optional<DocumentPosition> TextEditor::position_for_screen_cell(Point cell) const {
    const Rect absolute = absolute_bounds();
    const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
    const auto& rows = display_rows(content_width);
    if (rows.empty()) return std::nullopt;
    // A cell below the text lands on its last row, and one in the gutter on a
    // row's start: a click anywhere in the editor places the caret somewhere.
    const int row = std::clamp(cell.y - absolute.y + top_display_row_, 0, static_cast<int>(rows.size()) - 1);
    const int x = std::max(0, cell.x - absolute.x - static_cast<int>(gutter_width()) + left_column());
    const DisplayRow& display = rows[static_cast<std::size_t>(row)];
    const Line& line = lines_[display.line];
    int columns = 0;
    for (std::size_t byte = display.begin_byte; byte < display.end_byte;) {
        const std::size_t end = text::grapheme_end(line.text, byte);
        const int width = text::grapheme_width(std::string_view(line.text).substr(byte, end - byte));
        if (x < columns + width) return document_->position_at_byte(line.start_byte + byte);
        columns += width;
        byte = end;
    }
    return document_->position_at_byte(line.start_byte + display.end_byte);
}

std::optional<VirtualCaret> TextEditor::virtual_caret_for_screen_cell(Point cell) const {
    if (!virtual_space_ || lines_.empty()) return std::nullopt;
    const Rect absolute = absolute_bounds();
    const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
    const auto& rows = display_rows(content_width);
    const int row = cell.y - absolute.y + top_display_row_;
    const int x = cell.x - absolute.x - static_cast<int>(gutter_width()) + left_column();
    if (row < 0 || x < 0 || rows.empty()) return std::nullopt;
    if (static_cast<std::size_t>(row) >= rows.size()) {
        const std::size_t below = static_cast<std::size_t>(row) - (rows.size() - 1U);
        return VirtualCaret{lines_.size() - 1U + below, static_cast<std::size_t>(x)};
    }
    const DisplayRow& display = rows[static_cast<std::size_t>(row)];
    // Past the end of a row that wraps onward is still inside its line.
    if (display.continues || x <= column_x(display, display.end_byte)) return std::nullopt;
    const DisplayRow from_line_start{display.line, 0U, display.begin_byte, false};
    const int before = column_x(from_line_start, display.begin_byte);
    return VirtualCaret{display.line, static_cast<std::size_t>(before + x)};
}

bool TextEditor::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) return false;
    if (const int rows = ui::wheel_scroll_rows(event); rows != 0) {
        const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
        const int maximum = std::max(0, static_cast<int>(display_rows(content_width).size()) - 1);
        top_display_row_ = std::clamp(top_display_row_ + rows, 0, maximum);
        invalidate();
        return true;
    }
    const bool context_request = event.action == MouseAction::Down &&
        (event.button == MouseButton::Right ||
         (event.button == MouseButton::Left && has_modifier(event.modifiers, Modifier::Ctrl)));
    if (context_request && context_menu_handler_) {
        if (const auto target = position_for_screen_cell(event.cell)) {
            const auto selected = selection();
            const bool inside = selected && target->byte >= selected->begin.byte && target->byte < selected->end.byte;
            if (!inside) move_cursor(*target, false);
        }
        context_menu_handler_(*this, event.cell);
        return true;
    }
    if (event.button != MouseButton::Left) return false;
    // The second press of a double click selects rather than placing the
    // caret, and starts no drag: its first press has already placed the caret
    // and its release ended that drag.
    if (event.action == MouseAction::Down && event.click_count == 2) {
        dragging_ = false;
        if (const auto caret = virtual_caret_for_screen_cell(event.cell)) return set_virtual_caret(*caret);
        const auto target = position_for_screen_cell(event.cell);
        if (!target) return false;
        const std::string value = document_->text();
        if (target->byte >= value.size()) { move_cursor(*target, false); return true; }
        const std::size_t begin = word_byte(value[target->byte]) ? word_start(value, target->byte) : target->byte;
        const std::size_t end = word_byte(value[target->byte]) ? word_end(value, target->byte)
                                                               : text::grapheme_end(value, target->byte);
        const auto first = document_->position_at_byte(begin);
        const auto last = document_->position_at_byte(end);
        if (!first || !last) return false;
        return set_selection(DocumentRange{*first, *last});
    }
    if (event.action == MouseAction::Down) {
        const auto target = position_for_screen_cell(event.cell);
        if (!target) return false;
        move_cursor(*target, has_modifier(event.modifiers, Modifier::Shift));
        dragging_ = true;
        return true;
    }
    if (event.action == MouseAction::Move && dragging_) {
        const Rect absolute = absolute_bounds();
        const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
        const int maximum = std::max(0, static_cast<int>(display_rows(content_width).size()) - 1);
        Point cell = event.cell;
        if (cell.y < absolute.y) {
            top_display_row_ = std::max(0, top_display_row_ - 1);
            cell.y = absolute.y;
        } else if (cell.y >= absolute.y + bounds().height) {
            top_display_row_ = std::min(maximum, top_display_row_ + 1);
            cell.y = absolute.y + bounds().height - 1;
        }
        if (const auto target = position_for_screen_cell(cell)) move_cursor(*target, true);
        return true;
    }
    if (event.action == MouseAction::Up && dragging_) { dragging_ = false; return true; }
    return false;
}

void TextEditor::on_focus(const FocusEvent&) {
    notify_status_changed();
    invalidate();
}
void TextEditor::on_resized() {
    rebuild_lines();
    relayout_scrollbars();
}

std::optional<CursorState> TextEditor::cursor_state() const {
    if (!has_focus() || bounds().width <= 0 || bounds().height <= 0) return std::nullopt;
    const int gutter = static_cast<int>(gutter_width());
    const int content_width = bounds().width - gutter;
    if (content_width <= 0) return std::nullopt;
    const auto& rows = display_rows(content_width);
    const auto [display_row, cell_x] = caret_display_cell(rows);
    const int local_y = display_row - top_display_row_;
    if (local_y < 0 || local_y >= bounds().height) return std::nullopt;
    // Measured in the row's own cells and then shifted by the horizontal
    // scroll, exactly as draw() places the text it sits in.
    const int local_x = gutter + cell_x - left_column();
    if (local_x < gutter || local_x >= bounds().width) return std::nullopt;
    const Rect absolute = absolute_bounds();
    return CursorState{true, Point{absolute.x + local_x, absolute.y + local_y},
                       overwrite_ ? CursorShape::Block : CursorShape::Bar};
}

EditorStatusModel::EditorStatusModel(TextEditor& editor) : editor_(&editor), value_(editor.status()) {
    editor_observer_ = editor_->subscribe_status([this](const EditorStatus& value) { update(value); });
}

EditorStatusModel::~EditorStatusModel() {
    if (editor_ != nullptr && editor_observer_ != 0U) editor_->unsubscribe_status(editor_observer_);
}

EditorStatusModel::ObserverId EditorStatusModel::subscribe(Observer observer) {
    const ObserverId identifier = next_observer_id_++;
    observers_.push_back({identifier, std::move(observer)});
    return identifier;
}

void EditorStatusModel::unsubscribe(ObserverId observer) noexcept {
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(), [observer](const auto& candidate) {
                         return candidate.first == observer;
                     }),
                     observers_.end());
}

void EditorStatusModel::update(const EditorStatus& value) {
    value_ = value;
    const auto observers = observers_;
    for (const auto& [identifier, observer] : observers) {
        (void)identifier;
        if (observer) observer(value_);
    }
}

}  // namespace ckv::widgets
