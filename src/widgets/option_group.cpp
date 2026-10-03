// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/option_group.hpp"

#include <algorithm>
#include <vector>
#include <span>

#include "cvision/core/ascii.hpp"
#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {

// The marker before a choice — "[X] " or "(•) " — and the room between two
// columns of choices.
constexpr int kMarkerWidth = 4;
constexpr int kColumnGap = 2;

bool is_mnemonic_request(const KeyEvent& event) noexcept {
    return event.chord.key == Key::Char && !event.chord.text.empty() &&
           !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
           !has_modifier(event.chord.modifiers, Modifier::Super);
}

bool is_space_request(const KeyEvent& event) noexcept {
    return event.chord.key == Key::Char && event.chord.text == " " &&
           event.chord.modifiers == Modifier::None;
}

bool state_as_bool(CheckState state) noexcept { return state == CheckState::Checked; }

Style option_mnemonic_style(Style base, Style accent) {
    Style result = accent_style(base, accent);
    if (result.fg == result.bg) result.fg = base.fg;
    return result;
}

Style group_label_style(const ui::Theme& theme, ui::RoleId label_role, ui::RoleId focused_option_role,
                        ui::RoleId label_disabled_role, bool enabled, bool focused) {
    const Style label = theme.resolve(label_role);
    if (!enabled) return accent_style(label, theme.resolve(label_disabled_role));
    if (!focused) return label;
    // The caption must remain on the dialog/window surface; only its
    // foreground communicates that this option group owns keyboard focus.
    const auto foreground = theme.resolve(focused_option_role).fg;
    // A focused face may invert foreground/background. Preserve contrast on
    // the caption's surface, with an attribute cue when that foreground vanishes.
    return foreground == label.bg ? Style{label.fg, label.bg, label.attrs | Attr::Underline}
                                  : Style{foreground, label.bg, label.attrs};
}

CheckState toggled_state(CheckState state, bool tristate) noexcept {
    if (!tristate) return state == CheckState::Checked ? CheckState::Unchecked : CheckState::Checked;
    switch (state) {
        case CheckState::Unchecked:
            return CheckState::Checked;
        case CheckState::Checked:
            return CheckState::Mixed;
        case CheckState::Mixed:
            return CheckState::Unchecked;
    }
    CKV_ASSERT(false);
    return CheckState::Unchecked;
}

// Where a group's choices sit: row-major in `columns` columns under the
// caption's row, indented one cell under a caption, each column as wide as
// the widest choice in it — so a single row packs its choices, and several
// rows align as a table. Both groups lay out alike; only their markers
// differ.
struct GroupLayout {
    int caption_rows = 0;
    int indent = 0;
    int columns = 1;
    int rows = 0;
    int row_height = 1;
    bool classic = true;
    std::span<const int> column_x;      // where each column's marker starts
    std::span<const int> column_width;  // marker and the widest choice in it

    int natural_width(int caption_width) const noexcept {
        if (rows == 0) return caption_width;
        return std::max(caption_width, column_x.back() + column_width.back());
    }
    int height() const noexcept { return caption_rows + rows * row_height; }

    int row_of(std::size_t index) const noexcept { return caption_rows + (static_cast<int>(index) / columns) * row_height; }
    int column_of(std::size_t index) const noexcept { return static_cast<int>(index) % columns; }
    // The cell a choice's marker starts in.
    Point origin(std::size_t index) const noexcept {
        return Point{column_x[static_cast<std::size_t>(column_of(index))], row_of(index)};
    }
    // The cells a choice owns on its row, highlight and press alike: from
    // its marker (from the edge, for the first column) to the next column's
    // marker, or the right edge for the last column. One column therefore
    // owns the whole row, as a stacked group always did.
    int span_start(std::size_t index) const noexcept { return classic && column_of(index) == 0 ? 0 : origin(index).x; }
    int span_end(std::size_t index, int width) const noexcept {
        const int column = column_of(index);
        if (!classic) return std::min(width, origin(index).x + column_width[static_cast<std::size_t>(column)]);
        return column == columns - 1 ? width : column_x[static_cast<std::size_t>(column) + 1];
    }
    std::optional<std::size_t> index_at(Point local, std::size_t count, int width) const noexcept {
        const int offset = local.y - caption_rows;
        if (offset < 0) return std::nullopt;
        const int row = offset / row_height;
        if (row >= rows) return std::nullopt;
        for (int column = 0; column < columns; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) +
                                      static_cast<std::size_t>(column);
            if (index >= count) return std::nullopt;
            if (local.x >= span_start(index) && local.x < span_end(index, width)) return index;
        }
        return std::nullopt;
    }
};

GroupLayout layout_of(const std::vector<MnemonicText>& labels, const MnemonicText& caption, int columns,
                      OptionPresentation presentation, std::vector<int>& positions, std::vector<int>& extents) {
    GroupLayout layout;
    layout.classic = presentation == OptionPresentation::Classic;
    layout.row_height = presentation == OptionPresentation::BoxedRows ? 3 : 1;
    const int chrome = presentation == OptionPresentation::BoxedRows ? 6 : presentation == OptionPresentation::Buttons ? 5 : kMarkerWidth;
    layout.caption_rows = caption.display.empty() ? 0 : 1;
    layout.indent = layout.caption_rows;
    const int count = static_cast<int>(labels.size());
    layout.columns = std::max(1, std::min(columns, std::max(1, count)));
    layout.rows = (count + layout.columns - 1) / layout.columns;
    extents.assign(static_cast<std::size_t>(layout.columns), 0);
    positions.clear();
    for (std::size_t index = 0; index < labels.size(); ++index) {
        int& width = extents[static_cast<std::size_t>(layout.column_of(index))];
        width = std::max(width, chrome + text::text_width(labels[index].display));
    }
    int x = layout.indent;
    for (const int width : extents) {
        positions.push_back(x);
        x += width + kColumnGap;
    }
    layout.column_x = positions;
    layout.column_width = extents;
    return layout;
}

// The choice an arrow moves the cursor to, or nothing when the arrow would
// not move it: Left and Right step through the sequence and wrap, Up and
// Down move within the cursor's column and wrap there. A choice alone in its
// column has no use for Up or Down, and a group of one for any arrow.
std::optional<std::size_t> arrow_target(Key key, std::size_t cursor, std::size_t count, const GroupLayout& layout) {
    if (count < 2) return std::nullopt;
    const int n = static_cast<int>(count);
    const int at = static_cast<int>(cursor);
    switch (key) {
        case Key::Left:
            return static_cast<std::size_t>((at - 1 + n) % n);
        case Key::Right:
            return static_cast<std::size_t>((at + 1) % n);
        case Key::Up:
        case Key::Down: {
            const int step = layout.columns;
            const int column = layout.column_of(cursor);
            // The choices of this column, in order.
            const int in_column = (n - 1 - column) / step + 1;
            if (in_column < 2) return std::nullopt;
            const int position = (at - column) / step;
            const int next = key == Key::Down ? (position + 1) % in_column : (position - 1 + in_column) % in_column;
            return static_cast<std::size_t>(column + next * step);
        }
        default:
            return std::nullopt;
    }
}

void draw_caption(scene::Painter& painter, const MnemonicText& caption, int width, Style title_style,
                  Style accent) {
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", title_style));
    draw_mnemonic(painter, Point{0, 0}, caption, width, title_style, accent);
}

}  // namespace

// --- CheckGroup ------------------------------------------------------

CheckGroup::CheckGroup(std::vector<std::string> labels)
    : labels_(std::move(labels)), states_(labels_.size(), CheckState::Unchecked) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    parsed_labels_.reserve(labels_.size());
    column_positions_.reserve(std::max<std::size_t>(1, labels_.size()));
    column_extents_.reserve(std::max<std::size_t>(1, labels_.size()));
    for (const auto& label : labels_) parsed_labels_.push_back(parse_mnemonic(label));
}

void CheckGroup::set_presentation(OptionPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    size_hint_changed();
    invalidate();
}
std::optional<PointerShape> CheckGroup::pointer_shape_at(Point local) const {
    if (local.x < 0 || local.y < 0 || local.x >= bounds().width || local.y >= bounds().height) return std::nullopt;
    if (!layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).index_at(local, labels_.size(), bounds().width)) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}

void CheckGroup::set_group_label(std::string label) {
    if (caption_raw_ == label) return;
    caption_raw_ = std::move(label);
    caption_ = parse_mnemonic(caption_raw_);
    invalidate();
    size_hint_changed();
}

void CheckGroup::set_columns(int columns) {
    columns = std::max(1, columns);
    if (columns_ == columns) return;
    columns_ = columns;
    invalidate();
    size_hint_changed();
}

void CheckGroup::set_column_width(int columns) {
    CKV_ASSERT(columns >= 0);
    if (column_width_ == columns) return;
    column_width_ = columns;
    invalidate();
    size_hint_changed();
}

void CheckGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
    if (mnemonic_role_ == ui::kInvalidRole)
        mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
    if (group_label_role_ == ui::kInvalidRole)
        group_label_role_ = context().roles->find("ckv.label.text");
    if (disabled_role_ == ui::kInvalidRole)
        disabled_role_ = context().roles->find("ckv.option.disabled");
    if (label_disabled_role_ == ui::kInvalidRole)
        label_disabled_role_ = context().roles->find("ckv.label.disabled");
}

bool CheckGroup::checked(std::size_t index) const {
    return state_as_bool(check_state(index));
}

void CheckGroup::set_checked(std::size_t index, bool value) {
    set_check_state(index, value ? CheckState::Checked : CheckState::Unchecked);
}

CheckState CheckGroup::check_state(std::size_t index) const {
    CKV_ASSERT(index < states_.size());
    return states_[index];
}

void CheckGroup::set_check_state(std::size_t index, CheckState state) {
    CKV_ASSERT(index < states_.size());
    if (states_[index] == state) return;
    states_[index] = state;
    invalidate();
    if (on_state_changed) on_state_changed(index, state);
    if (on_changed) on_changed(index, state_as_bool(state));
}

void CheckGroup::toggle(std::size_t index) { set_check_state(index, toggled_state(states_[index], tristate_)); }

SizeHint CheckGroup::horizontal_size_hint() const {
    int width = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).natural_width(text::text_width(caption_.display));
    if (column_width_ != 0) width = column_width_;
    return SizeHint{width, width, width};
}
SizeHint CheckGroup::vertical_size_hint() const {
    const int h = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).height();
    return SizeHint{h, h, h};
}

bool CheckGroup::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    switch (event.chord.key) {
        case Key::Up:
        case Key::Left:
        case Key::Down:
        case Key::Right: {
            const auto target = arrow_target(event.chord.key, cursor_, labels_.size(), layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_));
            if (!target) return false;
            cursor_ = *target;
            invalidate();
            return true;
        }
        // Enter is deliberately not handled. Space is what ticks a box —
        // in this toolkit and in every other — and Enter belongs to the
        // form: it presses the dialog's default button. A group that
        // swallowed it left OK unreachable from the keyboard for as long as
        // any box had focus, which in a settings dialog is from the moment
        // it opens.
        case Key::Char:
            if (is_space_request(event)) {
                if (!labels_.empty()) toggle(cursor_);
                return true;
            }
            if (!is_mnemonic_request(event)) return false;
            for (std::size_t i = 0; i < labels_.size(); ++i) {
                const auto& parsed = parsed_labels_[i];
                if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, event.chord.text)) {
                    cursor_ = i;
                    toggle(i);
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool CheckGroup::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    if (!absolute_bounds().contains(event.cell)) return false;
    const Rect abs = absolute_bounds();
    const auto index = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_)
                           .index_at(Point{event.cell.x - abs.x, event.cell.y - abs.y}, labels_.size(), bounds().width);
    if (!index) return false;
    cursor_ = *index;
    toggle(cursor_);
    return true;
}

void CheckGroup::on_focus(const FocusEvent&) { invalidate(); }

void CheckGroup::draw(scene::Painter& target) {
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const ui::Theme& theme = *context().theme;
    const GroupLayout layout = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_);
    // Disabled (D-076): every choice keeps its mark on the disabled face, and
    // nothing says where the keyboard is or which letter would reach it.
    const bool enabled = enabled_in_tree();
    const Style normal = theme.resolve(enabled ? normal_role_ : disabled_role_);
    const Style mnemonic = theme.resolve(mnemonic_role_);
    if (layout.caption_rows != 0) {
        const Style title_style = group_label_style(theme, group_label_role_, focused_role_,
                                                    label_disabled_role_, enabled, has_focus());
        draw_caption(painter, caption_, bounds().width, title_style,
                     enabled ? option_mnemonic_style(title_style, mnemonic) : title_style);
    }
    for (int row = 0; row < layout.rows; ++row)
        painter.fill(Rect{0, layout.caption_rows + row * layout.row_height, bounds().width, layout.row_height}, Cell::from_grapheme(" ", normal));
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        Style style = (enabled && has_focus() && i == cursor_) ? theme.resolve(focused_role_) : normal;
        Point origin = layout.origin(i);
        const int start = layout.span_start(i);
        const int end = layout.span_end(i, bounds().width);
        const Rect choice{start, origin.y, std::max(0, end - start), layout.row_height};
        auto choice_painter = painter.clipped(choice);
        choice_painter.fill(choice, Cell::from_grapheme(" ", style));
        if (presentation_ == OptionPresentation::BoxedRows) {
            choice_painter.draw_box(choice, scene::LineStyle::Single, style);
            ++origin.x;
            ++origin.y;
        } else if (presentation_ == OptionPresentation::Buttons) ++origin.x;
        if (presentation_ != OptionPresentation::Classic && enabled && has_focus() && i == cursor_) style.attrs |= Attr::Underline;
        if (presentation_ != OptionPresentation::Classic && enabled && states_[i] != CheckState::Unchecked) style.attrs |= Attr::Bold;
        const std::string_view marker = presentation_ == OptionPresentation::Buttons ? (states_[i] == CheckState::Checked ? "✓  " : states_[i] == CheckState::Mixed ? "~  " : "   ")
                                 : states_[i] == CheckState::Checked ? "[X] "
                                 : states_[i] == CheckState::Mixed   ? "[~] "
                                                                     : "[ ] ";
        choice_painter.draw_text(origin, marker, style);
        const int marker_width = presentation_ == OptionPresentation::Buttons ? 3 : kMarkerWidth;
        draw_mnemonic(choice_painter, Point{origin.x + marker_width, origin.y}, parsed_labels_[i],
                      end - origin.x - marker_width - (presentation_ == OptionPresentation::Classic ? 0 : 1), style, enabled ? option_mnemonic_style(style, mnemonic) : style);
    }
}

// --- RadioGroup --------------------------------------------------------

RadioGroup::RadioGroup(std::vector<std::string> labels) : labels_(std::move(labels)) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    parsed_labels_.reserve(labels_.size());
    column_positions_.reserve(std::max<std::size_t>(1, labels_.size()));
    column_extents_.reserve(std::max<std::size_t>(1, labels_.size()));
    for (const auto& label : labels_) parsed_labels_.push_back(parse_mnemonic(label));
}

void RadioGroup::set_presentation(OptionPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    size_hint_changed();
    invalidate();
}
std::optional<PointerShape> RadioGroup::pointer_shape_at(Point local) const {
    if (local.x < 0 || local.y < 0 || local.x >= bounds().width || local.y >= bounds().height) return std::nullopt;
    if (!layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).index_at(local, labels_.size(), bounds().width)) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}

void RadioGroup::set_group_label(std::string label) {
    if (caption_raw_ == label) return;
    caption_raw_ = std::move(label);
    caption_ = parse_mnemonic(caption_raw_);
    invalidate();
    size_hint_changed();
}

void RadioGroup::set_columns(int columns) {
    columns = std::max(1, columns);
    if (columns_ == columns) return;
    columns_ = columns;
    invalidate();
    size_hint_changed();
}

void RadioGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
    if (mnemonic_role_ == ui::kInvalidRole)
        mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
    if (group_label_role_ == ui::kInvalidRole)
        group_label_role_ = context().roles->find("ckv.label.text");
    if (disabled_role_ == ui::kInvalidRole)
        disabled_role_ = context().roles->find("ckv.option.disabled");
    if (label_disabled_role_ == ui::kInvalidRole)
        label_disabled_role_ = context().roles->find("ckv.label.disabled");
}

void RadioGroup::set_selected(int index) {
    if (index != -1 && (index < 0 || static_cast<std::size_t>(index) >= labels_.size())) return;  // out of range
    // The keyboard cursor follows a programmatic selection, exactly as it
    // follows a mouse click (on_mouse sets cursor_ before toggling). Without
    // this, a dialog whose radio group opens with row N selected still has its
    // cursor on row 0 — and the reader's first Up, wrapping from 0 to the last
    // row, re-selects the very row they were trying to arrow away from.
    if (index >= 0) cursor_ = static_cast<std::size_t>(index);
    if (index == selected_) return;
    selected_ = index;
    invalidate();
    if (on_changed) on_changed(selected_);
}

void RadioGroup::set_column_width(int columns) {
    CKV_ASSERT(columns >= 0);
    if (column_width_ == columns) return;
    column_width_ = columns;
    invalidate();
    size_hint_changed();
}

SizeHint RadioGroup::horizontal_size_hint() const {
    int width = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).natural_width(text::text_width(caption_.display));
    if (column_width_ != 0) width = column_width_;
    return SizeHint{width, width, width};
}
SizeHint RadioGroup::vertical_size_hint() const {
    const int h = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_).height();
    return SizeHint{h, h, h};
}

bool RadioGroup::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    switch (event.chord.key) {
        case Key::Up:
        case Key::Left:
        case Key::Down:
        case Key::Right: {
            const auto target = arrow_target(event.chord.key, cursor_, labels_.size(), layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_));
            if (!target) return false;
            cursor_ = *target;
            set_selected(static_cast<int>(cursor_));  // arrow navigation also selects, matching classic radio groups
            invalidate();
            return true;
        }
        // Enter is deliberately not handled, for the reason CheckGroup gives
        // above. A radio group is doubly safe here: its arrow keys already
        // select as they move, so there is nothing Enter was needed for.
        case Key::Char:
            if (is_space_request(event)) {
                if (!labels_.empty()) set_selected(static_cast<int>(cursor_));
                return true;
            }
            if (!is_mnemonic_request(event)) return false;
            for (std::size_t i = 0; i < labels_.size(); ++i) {
                const auto& parsed = parsed_labels_[i];
                if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, event.chord.text)) {
                    cursor_ = i;
                    set_selected(static_cast<int>(i));
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool RadioGroup::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    if (!absolute_bounds().contains(event.cell)) return false;
    const Rect abs = absolute_bounds();
    const auto index = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_)
                           .index_at(Point{event.cell.x - abs.x, event.cell.y - abs.y}, labels_.size(), bounds().width);
    if (!index) return false;
    cursor_ = *index;
    set_selected(static_cast<int>(*index));
    return true;
}

void RadioGroup::on_focus(const FocusEvent&) { invalidate(); }

void RadioGroup::draw(scene::Painter& target) {
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const ui::Theme& theme = *context().theme;
    const GroupLayout layout = layout_of(parsed_labels_, caption_, columns_, presentation_, column_positions_, column_extents_);
    // Disabled (D-076): every choice keeps its mark on the disabled face, and
    // nothing says where the keyboard is or which letter would reach it.
    const bool enabled = enabled_in_tree();
    const Style normal = theme.resolve(enabled ? normal_role_ : disabled_role_);
    const Style mnemonic = theme.resolve(mnemonic_role_);
    if (layout.caption_rows != 0) {
        const Style title_style = group_label_style(theme, group_label_role_, focused_role_,
                                                    label_disabled_role_, enabled, has_focus());
        draw_caption(painter, caption_, bounds().width, title_style,
                     enabled ? option_mnemonic_style(title_style, mnemonic) : title_style);
    }
    for (int row = 0; row < layout.rows; ++row)
        painter.fill(Rect{0, layout.caption_rows + row * layout.row_height, bounds().width, layout.row_height}, Cell::from_grapheme(" ", normal));
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        Style style = (enabled && has_focus() && i == cursor_) ? theme.resolve(focused_role_) : normal;
        Point origin = layout.origin(i);
        const int start = layout.span_start(i);
        const int end = layout.span_end(i, bounds().width);
        const Rect choice{start, origin.y, std::max(0, end - start), layout.row_height};
        auto choice_painter = painter.clipped(choice);
        choice_painter.fill(choice, Cell::from_grapheme(" ", style));
        if (presentation_ == OptionPresentation::BoxedRows) {
            choice_painter.draw_box(choice, scene::LineStyle::Single, style);
            ++origin.x;
            ++origin.y;
        } else if (presentation_ == OptionPresentation::Buttons) ++origin.x;
        if (presentation_ != OptionPresentation::Classic && enabled && has_focus() && i == cursor_) style.attrs |= Attr::Underline;
        if (presentation_ != OptionPresentation::Classic && enabled && static_cast<int>(i) == selected_) style.attrs |= Attr::Bold;
        const std::string_view marker = presentation_ == OptionPresentation::Buttons ? (static_cast<int>(i) == selected_ ? "●  " : "   ") : (static_cast<int>(i) == selected_) ? "(•) " : "( ) ";
        choice_painter.draw_text(origin, marker, style);
        const int marker_width = presentation_ == OptionPresentation::Buttons ? 3 : kMarkerWidth;
        draw_mnemonic(choice_painter, Point{origin.x + marker_width, origin.y}, parsed_labels_[i],
                      end - origin.x - marker_width - (presentation_ == OptionPresentation::Classic ? 0 : 1), style, enabled ? option_mnemonic_style(style, mnemonic) : style);
    }
}

}  // namespace ckv::widgets
