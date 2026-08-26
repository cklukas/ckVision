// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_editor.hpp"

#include "cvision/widgets/text_layout.hpp"

#include <algorithm>
#include <array>
#include <charconv>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"

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

std::size_t word_end(std::string_view value, std::size_t byte) {
    while (byte < value.size() && word_byte(value[byte])) byte = text::grapheme_end(value, byte);
    return byte;
}

}  // namespace

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
    profile_ = &profiles_->plain_text();
    profile_id_ = profile_->id;
    cursor_ = document_->begin();
    observer_ = document_->subscribe([this](const DocumentChange& change) {
        clamp_cursor();
        rebuild_lines(change.first_affected_line);
        refresh_search();
        notify_status_changed();
        invalidate();
    });
    set_focus_policy(ui::FocusPolicy::TabStop);
    refresh_syntax();
}

TextEditor::~TextEditor() {
    if (document_ && observer_ != 0U) document_->unsubscribe(observer_);
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
    if (active_search_match_ && *active_search_match_ >= search_matches_.size()) active_search_match_.reset();
}

bool TextEditor::use_selection_as_search_query() {
    const auto selected = selection();
    if (!selected) return false;
    const std::string text = document_->text(*selected);
    if (text.empty()) return false;
    set_search_query(EditorSearchQuery{std::move(text), true, false});
    return !search_matches_.empty();
}

bool TextEditor::activate_search_match(std::size_t index) {
    if (index >= search_matches_.size()) return false;
    const DocumentRange range = search_matches_[index].range;
    if (range.begin.revision != document_->revision() || range.end.revision != document_->revision()) return false;
    selection_anchor_ = range.begin;
    cursor_ = range.end;
    active_search_match_ = index;
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
            if (search_matches_[index].range.begin.byte >= pivot) return activate_search_match(index);
        return activate_search_match(0);
    }
    for (std::size_t index = search_matches_.size(); index-- > 0;) {
        if (search_matches_[index].range.end.byte <= pivot) return activate_search_match(index);
    }
    return activate_search_match(search_matches_.size() - 1U);
}

bool TextEditor::replace_current_search_match(std::string replacement) {
    refresh_search();
    if (!active_search_match_ || *active_search_match_ >= search_matches_.size() || read_only_) return false;
    const DocumentRange range = search_matches_[*active_search_match_].range;
    const DocumentEditResult result = document_->replace(range, std::move(replacement));
    if (!result || !result.change) return false;
    cursor_ = document_->position_at_byte(result.change->replaced_begin_byte + result.change->inserted_bytes).value_or(document_->end());
    selection_anchor_.reset();
    active_search_match_.reset();
    ensure_cursor_visible();
    notify_status_changed();
    return true;
}

DocumentEditResult TextEditor::replace_all_search_matches(const std::string& replacement) {
    if (read_only_) return DocumentEditResult{};
    const DocumentEditResult result = EditorSearch::replace_all(*document_, search_query_, replacement);
    if (result) active_search_match_.reset();
    return result;
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
    profile_ = &profiles_->detect(input);
    profile_id_ = profile_->id;
    refresh_syntax();
    invalidate();
}

void TextEditor::refresh_syntax() {
    lines_.clear();  // profile selection changes invalidate every lexical state.
    rebuild_lines();
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

    // The gutter is chrome, not content: it is subtracted before the text
    // area is measured, and it never scrolls sideways with the text.
    const ScrollGeometry geometry = resolve_scroll_geometry(
        Size{std::max(0, bounds().width - gutter), bounds().height}, v_scrollbar_->policy(),
        h_scrollbar_->policy(), [this](int viewport_width) {
            const auto& rows = display_rows(std::max(1, viewport_width));
            int widest = 0;
            for (const DisplayRow& row : rows) widest = std::max(widest, column_x(row, row.end_byte));
            return Size{widest, static_cast<int>(rows.size())};
        });

    viewport_width_ = geometry.viewport_width;
    viewport_height_ = geometry.viewport_height;
    const auto& rows = display_rows(std::max(1, viewport_width_));
    content_width_ = 0;
    for (const DisplayRow& row : rows) content_width_ = std::max(content_width_, column_x(row, row.end_byte));

    const int v_width = geometry.show_vertical ? std::min(1, bounds().width) : 0;
    const int h_height = geometry.show_horizontal ? std::min(1, bounds().height) : 0;
    v_scrollbar_->set_range(static_cast<int>(rows.size()), std::max(1, geometry.viewport_height));
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
    const int cursor_row = static_cast<int>(cursor_display_row(rows));
    const int height = std::max(1, viewport_height_ > 0 ? viewport_height_ : bounds().height);
    if (cursor_row < top_display_row_) top_display_row_ = cursor_row;
    if (cursor_row >= top_display_row_ + height) top_display_row_ = cursor_row - height + 1;
    top_display_row_ = std::clamp(top_display_row_, 0, std::max(0, static_cast<int>(rows.size()) - height));
    if (v_scrollbar_ != nullptr) v_scrollbar_->set_position(top_display_row_);

    // And sideways: without wrapping, a cursor walking along a long line
    // would otherwise leave the viewport and keep going unseen.
    if (h_scrollbar_ == nullptr || cursor_row < 0 ||
        static_cast<std::size_t>(cursor_row) >= rows.size())
        return;
    const DisplayRow& row = rows[static_cast<std::size_t>(cursor_row)];
    if (row.line >= lines_.size()) return;
    // The same line-relative byte cursor_display_row() works in.
    const std::size_t within = cursor_.byte - std::min(cursor_.byte, lines_[row.line].start_byte);
    const int cursor_x = column_x(row, within);
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
    if (profile_ != nullptr) {
        (void)syntax_cache_.update(*profile_, source_lines);
        for (std::size_t index = 0; index < lines_.size(); ++index) {
            const SyntaxCacheLine* cached = syntax_cache_.line(index);
            if (cached == nullptr) continue;
            lines_[index].spans = cached->spans;
            lines_[index].incoming_state = cached->incoming_state;
            lines_[index].outgoing_state = cached->outgoing_state;
        }
    } else {
        syntax_cache_.clear();
    }
    if (lines_.empty()) lines_.push_back(Line{});
    ensure_cursor_visible();
}

std::optional<DocumentRange> TextEditor::selection() const noexcept {
    if (!selection_anchor_ || selection_anchor_->revision != cursor_.revision || *selection_anchor_ == cursor_) return std::nullopt;
    return less(*selection_anchor_, cursor_) ? DocumentRange{*selection_anchor_, cursor_} : DocumentRange{cursor_, *selection_anchor_};
}

EditorStatus TextEditor::status() const {
    EditorStatus result;
    if (const auto line_column = document_->line_column(cursor_)) {
        result.line = line_column->line + 1U;
        result.column = line_column->column + 1U;
    }
    if (const auto selected = selection()) result.selection_bytes = selected->end.byte - selected->begin.byte;
    result.modified = document_->modified();
    result.overwrite = overwrite_;
    result.profile_id = profile_id_;
    result.encoding = document_->encoding();
    result.newline = document_->preferred_newline();
    return result;
}

void TextEditor::clamp_cursor() {
    const std::size_t byte = std::min(cursor_.byte, document_->byte_size());
    cursor_ = document_->position_at_byte(byte).value_or(document_->end());
    if (selection_anchor_) selection_anchor_ = document_->position_at_byte(std::min(selection_anchor_->byte, document_->byte_size()));
}

void TextEditor::move_cursor(DocumentPosition target, bool extend) {
    if (target.revision != document_->revision()) return;
    if (extend) {
        if (!selection_anchor_) selection_anchor_ = cursor_;
    } else {
        selection_anchor_.reset();
    }
    cursor_ = target;
    ensure_cursor_visible();
    notify_status_changed();
    invalidate();
}

bool TextEditor::replace_selection(std::string value) {
    if (read_only_) return false;
    DocumentRange target = selection().value_or(DocumentRange{cursor_, cursor_});
    // Overwrite replaces complete following graphemes on the current logical
    // line. Newline-bearing input retains ordinary insertion semantics: it
    // must never silently consume a line boundary.
    if (!selection() && overwrite_ && value.find('\n') == std::string::npos && cursor_.byte < document_->byte_size()) {
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
            if (const auto position = document_->position_at_byte(end)) target.end = *position;
        }
    }
    const DocumentEditResult result = document_->replace(target, std::move(value));
    if (!result || !result.change) return false;
    cursor_ = document_->position_at_byte(result.change->replaced_begin_byte + result.change->inserted_bytes).value_or(document_->end());
    selection_anchor_.reset();
    ensure_cursor_visible();
    notify_status_changed();
    return true;
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
    if (read_only_ || !copy_selection_to_clipboard()) return false;
    return replace_selection({});
}

bool TextEditor::paste_from_clipboard() {
    return context().app != nullptr && !context().app->clipboard_text().empty() && replace_selection(context().app->clipboard_text());
}

bool TextEditor::erase_backward() {
    if (read_only_) return false;
    if (selection()) return replace_selection({});
    if (cursor_.byte == 0U) return false;
    const std::string value = document_->text();
    const std::size_t begin = previous_grapheme(value, cursor_.byte);
    const auto first = document_->position_at_byte(begin);
    if (!first) return false;
    const DocumentEditResult result = document_->replace(DocumentRange{*first, cursor_}, "");
    if (!result || !result.change) return false;
    cursor_ = document_->position_at_byte(begin).value_or(document_->begin());
    selection_anchor_.reset();
    ensure_cursor_visible();
    notify_status_changed();
    return true;
}

bool TextEditor::erase_forward() {
    if (read_only_) return false;
    if (selection()) return replace_selection({});
    if (cursor_.byte >= document_->byte_size()) return false;
    const std::string value = document_->text();
    const std::size_t end = text::grapheme_end(value, cursor_.byte);
    const auto last = document_->position_at_byte(end);
    if (!last) return false;
    const DocumentEditResult result = document_->replace(DocumentRange{cursor_, *last}, "");
    return static_cast<bool>(result);
}

bool TextEditor::erase_backward_word() {
    if (read_only_) return false;
    if (selection()) return replace_selection({});
    if (cursor_.byte == 0U) return false;
    const std::string value = document_->text();
    const auto begin = document_->position_at_byte(previous_word(value, cursor_.byte));
    if (!begin) return false;
    const DocumentEditResult result = document_->replace(DocumentRange{*begin, cursor_}, "");
    if (!result || !result.change) return false;
    cursor_ = *begin;
    selection_anchor_.reset();
    ensure_cursor_visible();
    notify_status_changed();
    return true;
}

bool TextEditor::erase_forward_word() {
    if (read_only_) return false;
    if (selection()) return replace_selection({});
    if (cursor_.byte >= document_->byte_size()) return false;
    const std::string value = document_->text();
    const auto end = document_->position_at_byte(next_word(value, cursor_.byte));
    if (!end) return false;
    const DocumentEditResult result = document_->replace(DocumentRange{cursor_, *end}, "");
    return static_cast<bool>(result);
}

std::size_t TextEditor::gutter_width() const {
    return show_line_numbers_ ? decimal_width(lines_.size()) + 1U : 0U;
}

Style TextEditor::style_for(std::size_t line, std::size_t line_byte, bool selected) const {
    if (selected) return context().theme->resolve(selected_role_);
    const std::size_t absolute = line < lines_.size() ? lines_[line].start_byte + line_byte : 0U;
    for (const EditorSearchMatch& match : search_matches_)
        if (match.range.begin.revision == document_->revision() && absolute >= match.range.begin.byte && absolute < match.range.end.byte)
            return context().theme->resolve(search_role_);
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
    const int max_top = std::max(0, static_cast<int>(rows.size()) - std::max(1, rows_shown));
    top_display_row_ = std::clamp(top_display_row_, 0, max_top);
    constexpr std::string_view reflow_marker = "\xE2\x86\xAA";  // U+21AA ↪
    const Style marker_style = context().theme->resolve(gutter_role_);
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
            if (x + width <= content_x) continue;               // off to the left
            if (x >= content_x + content_width) break;          // past the right edge
            if (x < content_x || x + width > content_x + content_width) continue;
            painter.draw_text(Point{x, row}, grapheme, style_for(display.line, start_byte, highlighted));
        }
        if (display.continues && content_width > text::text_width(reflow_marker))
            painter.draw_text(Point{content_x + content_width - text::text_width(reflow_marker), row}, reflow_marker, marker_style);
    }
}

bool TextEditor::on_key(const KeyEvent& event) {
    if (!enabled()) return false;
    if (event.action == KeyAction::Release) return false;
    const bool has_ctrl = has_modifier(event.chord.modifiers, Modifier::Ctrl);
    const bool has_shift = has_modifier(event.chord.modifiers, Modifier::Shift);
    const bool has_alt_or_super = has_modifier(event.chord.modifiers, Modifier::Alt) ||
                                  has_modifier(event.chord.modifiers, Modifier::Super);
    if (event.chord.key == Key::Insert && !has_alt_or_super) {
        if (has_ctrl) return copy_selection_to_clipboard();
        if (has_shift) return paste_from_clipboard();
    }
    if (event.chord.key == Key::Delete && has_shift && !has_ctrl && !has_alt_or_super)
        return cut_selection_to_clipboard();
    const bool extend = has_modifier(event.chord.modifiers, Modifier::Shift);
    if (has_modifier(event.chord.modifiers, Modifier::Ctrl) && event.chord.key == Key::Char) {
        if (event.chord.text == "z" || event.chord.text == "Z") return document_->undo();
        if (event.chord.text == "y" || event.chord.text == "Y") return document_->redo();
        if (event.chord.text == "c" || event.chord.text == "C") return copy_selection_to_clipboard();
        if (event.chord.text == "x" || event.chord.text == "X") return cut_selection_to_clipboard();
        if (event.chord.text == "v" || event.chord.text == "V") return paste_from_clipboard();
        if (event.chord.text == "f" || event.chord.text == "F") return use_selection_as_search_query();
    }
    if (event.chord.key == Key::F3) return find_next(!has_modifier(event.chord.modifiers, Modifier::Shift));
    // Shift participates in producing ordinary text; it does not turn that
    // text into a command chord. This is the same editing-boundary contract
    // as InputLine and Memo. Alt/Ctrl/Super remain available to command
    // routing, while the terminal-provided text stays authoritative for the
    // shifted or composed character.
    if (event.chord.key == Key::Char && !event.chord.text.empty() && !has_alt_or_super && !has_ctrl)
        return replace_selection(event.chord.text);
    const auto line_column = document_->line_column(cursor_);
    if (!line_column) return false;
    const bool control = has_modifier(event.chord.modifiers, Modifier::Ctrl);
    if (event.chord.key == Key::Tab && !has_modifier(event.chord.modifiers, Modifier::Shift)) return replace_selection("    ");
    switch (event.chord.key) {
        case Key::Left: {
            if (cursor_.byte == 0U) return true;
            const std::string value = document_->text();
            move_cursor(*document_->position_at_byte(control ? previous_word(value, cursor_.byte) : previous_grapheme(value, cursor_.byte)), extend);
            return true;
        }
        case Key::Right: {
            if (cursor_.byte == document_->byte_size()) return true;
            const std::string value = document_->text();
            move_cursor(*document_->position_at_byte(control ? next_word(value, cursor_.byte) : text::grapheme_end(value, cursor_.byte)), extend);
            return true;
        }
        case Key::Up:
            if (line_column->line > 0U) {
                const std::size_t line = line_column->line - 1U;
                const auto target = document_->position_at_line_column(line, std::min(line_column->column, grapheme_count(lines_[line].text)));
                if (target) move_cursor(*target, extend);
            }
            return true;
        case Key::Down:
            if (line_column->line + 1U < document_->line_count()) {
                const std::size_t line = line_column->line + 1U;
                const auto target = document_->position_at_line_column(line, std::min(line_column->column, grapheme_count(lines_[line].text)));
                if (target) move_cursor(*target, extend);
            }
            return true;
        case Key::Home: move_cursor(control ? document_->begin() : *document_->position_at_line_column(line_column->line, 0), extend); return true;
        case Key::End: {
            if (control) { move_cursor(document_->end(), extend); return true; }
            const Line& line = lines_[line_column->line];
            const auto target = document_->position_at_byte(line.start_byte + line.text.size());
            if (target) move_cursor(*target, extend);
            return true;
        }
        case Key::PageUp: top_display_row_ = std::max(0, top_display_row_ - std::max(1, bounds().height)); invalidate(); return true;
        case Key::PageDown: {
            const int width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
            const int maximum = std::max(0, static_cast<int>(display_rows(width).size()) - 1);
            top_display_row_ = std::min(maximum, top_display_row_ + std::max(1, bounds().height));
            invalidate();
            return true;
        }
        case Key::Backspace: return control ? erase_backward_word() : erase_backward();
        case Key::Delete: return control ? erase_forward_word() : erase_forward();
        case Key::Enter: return replace_selection("\n");
        // Through set_overwrite, so that the mode reaches the status observers
        // with the keystroke that changed it: a toggle that only invalidated
        // left the frame reading INS until the next cursor move republished it.
        case Key::Insert: set_overwrite(!overwrite_); return true;
        default: return false;
    }
}

bool TextEditor::on_text(const TextEvent& event) {
    return enabled() && !event.text.empty() && replace_selection(event.text);
}

std::optional<DocumentPosition> TextEditor::position_for_screen_cell(Point cell) const {
    const Rect absolute = absolute_bounds();
    const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
    const auto& rows = display_rows(content_width);
    const int row = cell.y - absolute.y + top_display_row_;
    if (row < 0 || static_cast<std::size_t>(row) >= rows.size()) return std::nullopt;
    const int x = cell.x - absolute.x - static_cast<int>(gutter_width()) + left_column();
    if (x < 0) return std::nullopt;
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

bool TextEditor::on_mouse(const MouseEvent& event) {
    if (!enabled()) return false;
    if (event.action == MouseAction::Wheel) {
        const int content_width = std::max(1, bounds().width - static_cast<int>(gutter_width()));
        const int maximum = std::max(0, static_cast<int>(display_rows(content_width).size()) - 1);
        top_display_row_ = std::clamp(top_display_row_ + (event.button == MouseButton::WheelDown ? 1 : -1), 0, maximum);
        invalidate();
        return true;
    }
    if (event.button != MouseButton::Left) return false;
    if (event.action == MouseAction::DoubleClick) {
        const auto target = position_for_screen_cell(event.cell);
        if (!target) return false;
        const std::string value = document_->text();
        if (target->byte >= value.size()) { move_cursor(*target, false); return true; }
        const std::size_t begin = word_byte(value[target->byte]) ? previous_word(value, target->byte) : target->byte;
        const std::size_t end = word_byte(value[target->byte]) ? word_end(value, target->byte)
                                                               : text::grapheme_end(value, target->byte);
        const auto first = document_->position_at_byte(begin);
        const auto last = document_->position_at_byte(end);
        if (!first || !last) return false;
        selection_anchor_ = *first;
        cursor_ = *last;
        ensure_cursor_visible();
        notify_status_changed();
        invalidate();
        return true;
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

void TextEditor::on_focus(const FocusEvent& event) { has_focus_ = event.gained; notify_status_changed(); invalidate(); }
void TextEditor::on_resized() {
    rebuild_lines();
    relayout_scrollbars();
}

std::optional<CursorState> TextEditor::cursor_state() const {
    if (!has_focus_ || bounds().width <= 0 || bounds().height <= 0) return std::nullopt;
    const int gutter = static_cast<int>(gutter_width());
    const int content_width = bounds().width - gutter;
    if (content_width <= 0) return std::nullopt;
    const auto& rows = display_rows(content_width);
    const std::size_t display_row = cursor_display_row(rows);
    const int local_y = static_cast<int>(display_row) - top_display_row_;
    if (local_y < 0 || local_y >= bounds().height) return std::nullopt;
    const DisplayRow& display = rows[display_row];
    if (display.line >= lines_.size()) return std::nullopt;
    const Line& line = lines_[display.line];
    const std::size_t local_byte = cursor_.byte - std::min(cursor_.byte, line.start_byte);
    int local_x = gutter;
    for (std::size_t byte = display.begin_byte; byte < display.end_byte && byte < local_byte;) {
        const std::size_t end = text::grapheme_end(line.text, byte);
        local_x += text::grapheme_width(std::string_view(line.text).substr(byte, end - byte));
        byte = end;
    }
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
