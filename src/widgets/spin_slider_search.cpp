// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/spin_box.hpp"
#include "cvision/widgets/slider.hpp"
#include "cvision/widgets/search_box.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include "cvision/core/ascii.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
using detail::is_press;
using detail::stepped;
using detail::enclosing_desktop;
using detail::tail_to_width;
using detail::without_last_grapheme;
using detail::trim_spaces;
using detail::typed_text;
using detail::entry_caret;

namespace {

// The English reasons a SpinBox gives when nothing else was asked for. The
// bounds are written with std::to_string, which formats an int in plain
// ASCII digits whatever the process locale.
std::string english_refusal_text(SpinBoxRefusal refusal, int minimum, int maximum) {
    if (refusal == SpinBoxRefusal::NotANumber) return "Not a whole number.";
    return "Enter a number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".";
}

}  // namespace

SpinBox::SpinBox() : refusal_text_(english_refusal_text) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{10, 1});
}
void SpinBox::set_range(int minimum, int maximum) {
    minimum_ = std::min(minimum, maximum);
    maximum_ = std::max(minimum, maximum);
    invalidate();  // Stepper availability can change without the value moving.
    assign(value_);
}
void SpinBox::set_step(int step) { step_ = std::max(1, step); }
void SpinBox::set_value(int value) {
    cancel_entry();
    assign(value);
}
void SpinBox::assign(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    if (on_change) on_change(value_);
}
void SpinBox::set_editable(bool editable) {
    if (editable_ == editable) return;
    editable_ = editable;
    invalidate();
    if (!editable_) cancel_entry();
}
void SpinBox::set_refusal_text(std::function<std::string(SpinBoxRefusal, int, int)> text) {
    refusal_text_ = text ? std::move(text) : english_refusal_text;
}
void SpinBox::edit_entry(std::string entry) {
    // An edit supersedes the verdict on what was there before it: the reader
    // is correcting it, and the next commit judges it again.
    entry_ = std::move(entry);
    refusal_.clear();
    invalidate();
}
bool SpinBox::commit_entry() {
    if (!entry_) return true;
    std::string_view text = trim_spaces(*entry_);
    // An optional sign, then digits and nothing else: "12 or so" is not 12,
    // and "+-3" is not -3.
    bool signed_plus = false;
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        signed_plus = true;
    }
    int parsed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    std::optional<SpinBoxRefusal> refusal;
    if (text.empty() || (signed_plus && text.front() == '-') || error == std::errc::invalid_argument ||
        end != text.data() + text.size())
        refusal = SpinBoxRefusal::NotANumber;
    else if (error == std::errc::result_out_of_range || parsed < minimum_ || parsed > maximum_)
        refusal = SpinBoxRefusal::OutOfRange;
    if (refusal) {
        // Refused, and never clamped: a number the reader did not type would
        // be a value nobody chose. The entry stays as typed for them to fix.
        refusal_ = refusal_text_(*refusal, minimum_, maximum_);
        invalidate();
        if (on_invalid) {
            const std::string reason = refusal_;
            on_invalid(reason);
        }
        return false;
    }
    entry_.reset();
    refusal_.clear();
    invalidate();
    assign(parsed);
    return true;
}
void SpinBox::cancel_entry() {
    if (!entry_ && refusal_.empty()) return;
    entry_.reset();
    refusal_.clear();
    invalidate();
}
void SpinBox::adjust(int delta) {
    // A step moves the value, so a typed entry becomes the value first -- and
    // one that is refused stays, and nothing moves.
    if (!commit_entry()) return;
    assign(stepped(value_, delta, step_, minimum_, maximum_));
}
void SpinBox::set_presentation(SpinBoxPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    hover_position_.reset();
    size_hint_changed();
    invalidate();
}
ui::SizeHint SpinBox::horizontal_size_hint() const {
    if (preferred_size().width > 0) return ui::View::horizontal_size_hint();
    std::array<char, 32> buffer{};
    const auto digits = [&](int v) {
        return static_cast<int>(std::to_chars(buffer.data(), buffer.data() + buffer.size(), v).ptr - buffer.data());
    };
    const int reserve = presentation_ == SpinBoxPresentation::Compact ? 2
                      : presentation_ == SpinBoxPresentation::Separate ? 7
                                                                       : 3;
    const int field_cols = std::max({3, digits(minimum_), digits(maximum_)});
    const int pref = field_cols + reserve;
    return {1 + reserve, pref, pref};
}
void SpinBox::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}
ui::SizeHint SpinBox::vertical_size_hint() const {
    const int height = presentation_ == SpinBoxPresentation::Stacked ? 2 : 1;
    return {height, height, height};
}
Rect SpinBox::field_bounds() const noexcept {
    const int width = std::max(0, bounds().width);
    const int reserve = presentation_ == SpinBoxPresentation::Compact ? 2 : presentation_ == SpinBoxPresentation::Separate ? 7 : 3;
    return {0, 0, width == 0 ? 0 : std::max(1, width - reserve), std::min(std::max(0, bounds().height), presentation_ == SpinBoxPresentation::Stacked ? 2 : 1)};
}
Rect SpinBox::decrement_bounds() const noexcept {
    const int start = field_bounds().width;
    const Rect wanted = presentation_ == SpinBoxPresentation::Compact ? Rect{start, 0, 1, 1} : presentation_ == SpinBoxPresentation::Separate ? Rect{start + 1, 0, 3, 1} : Rect{start, 1, 3, 1};
    return wanted.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect SpinBox::increment_bounds() const noexcept {
    const int start = field_bounds().width;
    const Rect wanted = presentation_ == SpinBoxPresentation::Compact ? Rect{start + 1, 0, 1, 1} : presentation_ == SpinBoxPresentation::Separate ? Rect{start + 4, 0, 3, 1} : Rect{start, 0, 3, 1};
    return wanted.intersected(Rect{0, 0, bounds().width, bounds().height});
}
SpinBox::Shown SpinBox::shown_entry(int width) const {
    if (!entry_ || width <= 0) return {};
    const std::string_view entry(*entry_);
    int remaining = text::text_width(entry);
    const int available = std::max(0, width - 1);
    std::size_t first = 0;
    while (first < entry.size() && remaining > available) {
        const auto end = text::grapheme_end(entry, first);
        remaining -= text::grapheme_width(entry.substr(first, end - first));
        first = end;
    }
    return {entry.substr(first), remaining};
}
bool SpinBox::can_step(int delta) const noexcept {
    return enabled_in_tree() && valid() && (entry_.has_value() || (delta > 0 ? value_ < maximum_ : value_ > minimum_));
}
std::optional<PointerShape> SpinBox::pointer_shape_at(Point local) const {
    if (decrement_bounds().contains(local)) return can_step(-1) ? PointerShape::Pointer : PointerShape::NotAllowed;
    if (increment_bounds().contains(local)) return can_step(1) ? PointerShape::Pointer : PointerShape::NotAllowed;
    if (field_bounds().contains(local)) return !enabled_in_tree() ? PointerShape::NotAllowed : editable_ ? PointerShape::Text : PointerShape::Default;
    return std::nullopt;
}
void SpinBox::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}
void SpinBox::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    const ui::RoleId normal_role = role_override_ != ui::kInvalidRole ? role_override_ : role_;
    const Style style = context().theme->resolve(!enabled ? disabled_role_ : !valid() ? invalid_role_ : has_focus() ? focused_role_ : normal_role);
    const Rect field = field_bounds();
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", context().theme->resolve(surface_role_)));
    painter.fill(field, Cell::from_grapheme(" ", style));
    Style text_style = style;
    if (enabled && has_focus()) text_style.attrs |= Attr::Underline;
    if (entry_) painter.draw_text(Point{field.x, field.y}, shown_entry(field.width).text, text_style);
    else {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value_);
        const std::string_view number(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
        const bool elided = static_cast<int>(number.size()) > field.width;
        const auto prefix = text::clip_to_width_view(number, elided ? field.width - 1 : field.width);
        painter.draw_text(Point{field.x, field.y}, prefix, text_style);
        if (elided) painter.draw_text(Point{field.x + static_cast<int>(prefix.size()), field.y}, "…", text_style);
    }
    const Style accessory = context().theme->resolve(accessory_role_);
    const auto control = [&](Rect rect, std::string_view glyph, int delta) {
        if (rect.empty()) return;
        auto clipped = painter.clipped(rect);
        Style shown = can_step(delta) ? accessory : accent_style(accessory, context().theme->resolve(disabled_role_));
        if (can_step(delta) && hover_position_ && rect.contains(*hover_position_)) shown = accent_style(shown, context().theme->resolve(accessory_hovered_role_));
        clipped.fill(rect, Cell::from_grapheme(" ", shown));
        clipped.draw_text(Point{rect.x + rect.width / 2, rect.y}, glyph, shown);
    };
    control(decrement_bounds(), presentation_ == SpinBoxPresentation::Stacked ? "▼" : "−", -1);
    control(increment_bounds(), presentation_ == SpinBoxPresentation::Stacked ? "▲" : "+", 1);
}
std::optional<CursorState> SpinBox::cursor_state() const {
    if (!entry_ || !has_focus() || !enabled_in_tree()) return std::nullopt;
    return entry_caret(*this, 0, shown_entry(field_bounds().width).text_end, field_bounds().width);
}
bool SpinBox::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    const Key key = event.chord.key;
    if (entry_) {
        if (key == Key::Enter) {
            commit_entry();
            return true;
        }
        if (key == Key::Escape) {
            cancel_entry();
            return true;
        }
        if (key == Key::Backspace) {
            edit_entry(without_last_grapheme(*entry_));
            return true;
        }
    }
    if (editable_) {
        if (const std::optional<std::string_view> text = typed_text(event)) {
            edit_entry(entry_.value_or(std::string{}) + std::string(*text));
            return true;
        }
        if (key == Key::Backspace) {
            edit_entry(without_last_grapheme(std::to_string(value_)));
            return true;
        }
    }
    if (key == Key::Up || key == Key::Right) { adjust(1); return true; }
    if (key == Key::Down || key == Key::Left) { adjust(-1); return true; }
    return false;
}
bool SpinBox::on_text(const TextEvent& event) {
    if (!enabled_in_tree() || !editable_) return false;
    edit_entry(entry_.value_or(std::string{}) + event.text);
    return true;
}
bool SpinBox::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || !absolute_bounds().contains(event.cell)) return false;
    if (event.action == MouseAction::Move) {
        const Rect abs = absolute_bounds();
        hover_position_ = Point{event.cell.x - abs.x, event.cell.y - abs.y};
        invalidate();
        return false;
    }
    // The wheel steps the value the way the date picker's segments step:
    // away from the reader is up.
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelUp) {
        adjust(1);
        return true;
    }
    if (event.action == MouseAction::Wheel && event.button == MouseButton::WheelDown) {
        adjust(-1);
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (increment_bounds().contains(local)) { adjust(1); return true; }
    if (decrement_bounds().contains(local)) { adjust(-1); return true; }
    return false;
}
void SpinBox::on_focus(const FocusEvent& event) {
    // Leaving the box commits what was typed, as Enter does; a refused entry
    // stays, marked, for the reader to come back to.
    if (!event.gained && entry_) commit_entry();
    invalidate();
}
void SpinBox::on_attached() {
    accessory_role_ = context().roles->find("ckv.input.accessory");
    accessory_hovered_role_ = context().roles->find("ckv.input.accessory.hovered");
    surface_role_ = context().roles->find("ckv.label.text");
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.input.disabled");
}

Slider::Slider() {
    set_focus_policy(ui::FocusPolicy::TabStop);
}
void Slider::set_orientation(Orientation orientation) {
    if (orientation_ == orientation) return;
    orientation_ = orientation;
    size_hint_changed();
    invalidate();
}
void Slider::set_role_override(ui::RoleId role) {
    if (role_override_ == role) return;
    role_override_ = role;
    invalidate();
}
void Slider::set_presentation(SliderPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    invalidate();
}
void Slider::set_show_value(bool show) {
    if (show_value_ == show) return;
    show_value_ = show;
    size_hint_changed();
    invalidate();
}
int Slider::track_width() const noexcept {
    const int width = std::max(0, bounds().width);
    if (!show_value_) return width;
    std::array<char, 32> buffer{};
    const auto digits = [&](int value) { return static_cast<int>(std::to_chars(buffer.data(), buffer.data() + buffer.size(), value).ptr - buffer.data()); };
    const int reserved = std::max(digits(minimum_), digits(maximum_)) + 1;
    return width >= reserved + 2 ? width - reserved : width;
}
Rect Slider::track_bounds() const noexcept {
    if (orientation_ == Orientation::Vertical) {
        return Rect{0, 0, std::min(1, std::max(0, bounds().width)), std::max(0, bounds().height)};
    }
    return Rect{0, 0, track_width(), std::min(1, std::max(0, bounds().height))};
}
void Slider::set_range(int minimum, int maximum) { minimum_ = std::min(minimum, maximum); maximum_ = std::max(minimum, maximum); set_value(value_); invalidate(); }
void Slider::set_step(int step) { step_ = std::max(1, step); }
void Slider::set_value(int value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value_ == value) return;
    value_ = value;
    invalidate();
    const auto cb = on_change;
    if (cb) cb(value_);
}
void Slider::set_ticks(std::vector<SliderTick> ticks) {
    const bool rows_change = ticks_.empty() != ticks.empty();
    ticks_ = std::move(ticks);
    placed_labels_.reserve(ticks_.size());
    invalidate();
    if (rows_change) size_hint_changed();  // the label row comes or goes
}
ui::SizeHint Slider::horizontal_size_hint() const {
    if (preferred_size().width > 0 && orientation_ == Orientation::Horizontal)
        return ui::View::horizontal_size_hint();
    if (orientation_ == Orientation::Vertical) return ui::SizeHint{1, 1, 1};
    return ui::SizeHint{8, 20, ui::kUnboundedExtent};
}
ui::SizeHint Slider::vertical_size_hint() const {
    if (orientation_ == Orientation::Vertical) return ui::SizeHint{4, 10, ui::kUnboundedExtent};
    if (ticks_.empty()) return ui::SizeHint{1, 1, 1};
    return ui::SizeHint{2, 2, 2};
}
void Slider::adjust(int delta) { set_value(stepped(value_, delta, step_, minimum_, maximum_)); }
int Slider::value_from_x(int x) const {
    if (track_width() <= 1 || maximum_ == minimum_) return minimum_;
    // In 64 bits: the span of an int range needs 32, and times a column 63.
    const std::int64_t span = std::int64_t{maximum_} - minimum_;
    const std::int64_t offset = span * std::clamp(x, 0, track_width() - 1) / (track_width() - 1);
    return static_cast<int>(minimum_ + offset);
}
int Slider::x_from_value(int value) const {
    const int last = std::max(0, track_width() - 1);
    if (maximum_ == minimum_) return 0;
    const std::int64_t span = std::int64_t{maximum_} - minimum_;
    return static_cast<int>((std::int64_t{value} - minimum_) * last / span);
}
void Slider::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    // Disabled (D-076): the track and its filled part keep their surfaces,
    // so the value still shows, in the disabled foreground.
    const bool enabled = enabled_in_tree();
    const Style inert = context().theme->resolve(disabled_role_);
    const auto shown = [&](Style style) { return enabled ? style : accent_style(style, inert); };
    const Style track = shown(context().theme->resolve(role_override_ != ui::kInvalidRole ? role_override_ : role_));
    const Style fill = shown(context().theme->resolve(fill_role_));
    if (orientation_ == Orientation::Vertical) {
        const int h = std::max(0, bounds().height);
        const int last = std::max(0, h - 1);
        const int thumb_y = (maximum_ == minimum_) ? last
            : last - static_cast<int>((std::int64_t{value_} - minimum_) * last / (std::int64_t{maximum_} - minimum_));
        painter.fill(Rect{0, 0, 1, h}, Cell::from_grapheme(presentation_ == SliderPresentation::Block ? "░" : "│", track));
        for (int y = thumb_y; y < h; ++y) {
            painter.draw_text(Point{0, y}, presentation_ == SliderPresentation::Block ? "█" : "┃", fill);
        }
        painter.draw_text(Point{0, thumb_y}, enabled && has_focus() ? "◆" : "●", fill);
        return;
    }
    const int width = track_width();
    painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(presentation_ == SliderPresentation::Block ? "░" : "─", track));
    const int pos = x_from_value(value_);
    painter.fill(Rect{0, 0, pos, 1}, Cell::from_grapheme(presentation_ == SliderPresentation::Block ? "█" : "━", fill));
    const auto in_range = [this](const SliderTick& tick) { return tick.value >= minimum_ && tick.value <= maximum_; };
    // Each tick marks its column on the track, in the weight of the part it
    // falls on; the thumb, drawn last, stands over a mark in its own column.
    for (const SliderTick& tick : ticks_) {
        if (!in_range(tick)) continue;
        const int x = x_from_value(tick.value);
        painter.draw_text(Point{x, 0}, x < pos ? "┯" : "┬", x < pos ? fill : track);
    }
    if (presentation_ == SliderPresentation::ProminentThumb) {
        painter.fill(Rect{std::max(0, pos - 1), 0, std::min(width, pos + 2) - std::max(0, pos - 1), 1}, Cell::from_grapheme(" ", fill));
    }
    painter.draw_text(Point{pos, 0}, enabled && has_focus() ? "◆" : "●", fill);
    if (show_value_ && width < bounds().width) {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value_);
        const std::string_view label(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
        painter.fill(Rect{width, 0, bounds().width - width, 1}, Cell::from_grapheme(" ", track));
        painter.draw_text(Point{bounds().width - static_cast<int>(label.size()), 0}, label, track);
    }
    if (ticks_.empty() || bounds().height < 2) return;
    // The labels, in the order given: each centred on its mark, moved inward
    // to stay inside the slider, and left out when it would touch one that
    // is already down -- the rule that makes the same slider label the same
    // ticks every time.
    painter.fill(Rect{0, 1, bounds().width, 1}, Cell::from_grapheme(" ", track));
    placed_labels_.clear();  // Retained [left, right) ranges, prepared by set_ticks.
    for (const SliderTick& tick : ticks_) {
        if (!in_range(tick)) continue;
        const bool elided = text::text_width(tick.label) > width;
        const std::string_view label = text::clip_to_width_view(tick.label, elided ? width - 1 : width);
        const int prefix_width = text::text_width(label);
        const int label_width = prefix_width + (elided ? 1 : 0);
        if (label_width == 0) continue;
        const int left = std::clamp(x_from_value(tick.value) - label_width / 2, 0, std::max(0, width - label_width));
        const int right = left + label_width;
        const bool collides = std::any_of(placed_labels_.begin(), placed_labels_.end(), [left, right](const auto& other) {
            return left < other.second + 1 && other.first < right + 1;
        });
        if (collides) continue;
        placed_labels_.emplace_back(left, right);
        painter.draw_text(Point{left, 1}, label, track);
        if (elided) painter.draw_text(Point{left + prefix_width, 1}, "…", track);
    }
}
bool Slider::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event)) return false;
    if (orientation_ == Orientation::Vertical) {
        if (event.chord.key == Key::Down) { adjust(-1); return true; }
        if (event.chord.key == Key::Up) { adjust(1); return true; }
    } else {
        if (event.chord.key == Key::Left) { adjust(-1); return true; }
        if (event.chord.key == Key::Right) { adjust(1); return true; }
    }
    if (event.chord.key == Key::Home) { set_value(minimum_); return true; }
    if (event.chord.key == Key::End) { set_value(maximum_); return true; }
    return false;
}
bool Slider::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!track_bounds().contains(local)) return false;
    if (orientation_ == Orientation::Vertical) {
        const int h = std::max(1, bounds().height);
        if (h <= 1 || maximum_ == minimum_) { set_value(minimum_); return true; }
        const std::int64_t span = std::int64_t{maximum_} - minimum_;
        const int inv_y = (h - 1) - std::clamp(local.y, 0, h - 1);
        set_value(static_cast<int>(minimum_ + span * inv_y / (h - 1)));
        return true;
    }
    set_value(value_from_x(local.x));
    return true;
}
std::optional<PointerShape> Slider::pointer_shape_at(Point local) const {
    if (!enabled_in_tree() || !track_bounds().contains(local)) return std::nullopt;
    return PointerShape::Pointer;
}
void Slider::on_focus(const FocusEvent&) { invalidate(); }
void Slider::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (fill_role_ == ui::kInvalidRole) fill_role_ = context().roles->find("ckv.menu.bar.active");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.list.disabled");
}

SearchBox::SearchBox() {
    set_preferred_size(Size{20, 1});
    // The query is typed into a real InputLine, the box's one focus stop, so
    // it edits exactly as every other one-line field does.
    field_ = make<InputLine>();
    field_->on_change = [this] { query_changed(); };
}
void SearchBox::set_prompt(std::string prompt) {
    prompt = text::sanitize_display_text(prompt);
    if (prompt_ == prompt) return;
    prompt_ = std::move(prompt);
    prompt_columns_ = text::text_width(prompt_);
    place_field();
    invalidate();
}
void SearchBox::set_presentation(InputPresentation presentation) {
    if (field_->presentation() == presentation) return;
    field_->set_presentation(presentation);
    hover_position_.reset();
    place_field();
    size_hint_changed();
    invalidate();
}

ui::SizeHint SearchBox::horizontal_size_hint() const {
    const int padding = presentation() == InputPresentation::Flat ? 0 : 2;
    const int preferred = 20 + padding;
    return ui::SizeHint{10 + padding, preferred, ui::kUnboundedExtent};
}

ui::SizeHint SearchBox::vertical_size_hint() const {
    return field_->vertical_size_hint();
}

void SearchBox::set_query(std::string query) {
    if (field_->text() == query) return;
    field_->set_text(std::move(query));
    query_changed();
}
void SearchBox::query_changed() {
    place_field();
    invalidate();
    if (on_change) on_change(query());
}
void SearchBox::commit_to_history() {
    if (field_->empty()) return;
    field_->commit_to_history();
}
void SearchBox::clear() {
    set_query({});
    if (on_clear) on_clear();
}
void SearchBox::set_status(std::string status) {
    if (status_ == status) return;
    status_ = std::move(status);
    place_field();
    invalidate();
}

SearchBox::Layout SearchBox::layout() const {
    Layout parts;
    const int width = std::max(0, bounds().width);
    parts.prompt_width = std::min(prompt_columns_, width);
    // The clear control only exists while there is something to clear, and
    // only at the right edge, where it is drawn.
    parts.clear_width = field_->empty() ? 0 : std::min(kClearControlWidth, width - parts.prompt_width);
    parts.clear_x = width - parts.clear_width;
    parts.field_x = parts.prompt_width;
    const int between = std::max(0, parts.clear_x - parts.field_x);
    // The status takes what the field can spare, the field keeping its
    // minimum; a status squeezed below two columns (its blank and one cell)
    // says nothing and is left out.
    if (!status_.empty()) {
        const int wanted = text::text_width(status_) + 1;
        const int spare = std::max(0, between - (kMinimumQueryColumns + (presentation() == InputPresentation::Flat ? 0 : 2)));
        parts.status_width = std::min(wanted, spare);
        if (parts.status_width < 2) parts.status_width = 0;
    }
    parts.field_width = between - parts.status_width;
    return parts;
}

void SearchBox::place_field() {
    const Layout parts = layout();
    field_->set_bounds(Rect{parts.field_x, 0, parts.field_width,
                           std::min(input_presentation_height(presentation()), std::max(0, bounds().height))});
    shown_status_ = parts.status_width > 0 ? text::elide_to_width(status_, parts.status_width - 1) : std::string{};
    status_x_ = parts.clear_x - text::text_width(shown_status_);
}

void SearchBox::on_resized() { hover_position_.reset(); place_field(); }

void SearchBox::draw(scene::Painter& painter) {
    // A search box has to look like something you can type into: the field
    // draws itself, in the input surface's own colours, and the box draws the
    // prompt, the status and the clear control around it in the label's.
    const bool enabled = enabled_in_tree();
    const Style label = enabled ? context().theme->resolve(label_role_)
                                : accent_style(context().theme->resolve(label_role_),
                                               context().theme->resolve(label_disabled_role_));
    const int width = bounds().width;
    if (width <= 0 || bounds().height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, width, std::min(bounds().height, input_presentation_height(presentation()))});
    clipped.fill(Rect{0, 0, width, std::min(bounds().height, input_presentation_height(presentation()))}, Cell::from_grapheme(" ", label));

    const Layout parts = layout();
    clipped.draw_text(Point{0, 0}, text::clip_to_width_view(prompt_, parts.prompt_width), label);
    // Right-aligned against the clear control, so a count that changes width
    // as the reader types does not make the clear control jump.
    if (!shown_status_.empty()) clipped.draw_text(Point{status_x_, 0}, shown_status_, label);
    if (parts.clear_width > 0) {
        const bool hovered = hover_position_ && hover_position_->y == 0 &&
            hover_position_->x >= parts.clear_x && hover_position_->x < parts.clear_x + parts.clear_width;
        const Style accessory = enabled ? context().theme->resolve(hovered ? accessory_hovered_role_ : accessory_role_) : label;
        clipped.fill(Rect{parts.clear_x, 0, parts.clear_width, 1}, Cell::from_grapheme(" ", accessory));
        clipped.draw_text(Point{parts.clear_x + (parts.clear_width - 1) / 2, 0}, "x", accessory);
    }
}

bool SearchBox::on_key(const KeyEvent& event) {
    if (!is_press(event)) return false;
    // Escape clears a query; with nothing to clear it is left for whatever
    // encloses the box -- a dialog's cancel, a popup's dismissal.
    if (event.chord.key == Key::Escape) {
        if (field_->empty()) return false;
        clear();
        return true;
    }
    // Enter is the reader saying "this is the query": it goes into the
    // history, and the key goes on to whatever else answers it -- a dialog's
    // default button, or nothing in a viewer that filters live.
    if (event.chord.key == Key::Enter) commit_to_history();
    return false;
}
bool SearchBox::on_mouse(const MouseEvent& event) {
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    if (event.action == MouseAction::Move) { hover_position_ = local; invalidate(); return false; }
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left ||
        !Rect{0, 0, bounds().width, std::min(1, bounds().height)}.contains(local)) return false;
    // The clear control only exists while there is something to clear, and
    // only where it is drawn — the right edge.
    const Layout parts = layout();
    if (parts.clear_width > 0 && local.x >= parts.clear_x && local.x < parts.clear_x + parts.clear_width) {
        clear();
        return true;
    }
    // A press anywhere else on the box -- its prompt, its status -- is a
    // request to type in it. The field is the box's focus stop, so the
    // Application's click-to-focus, which looks for a focusable ancestor of
    // what was pressed, would not find it from here.
    if (field_->focusable() && context().app != nullptr) context().app->set_focus(field_);
    return true;
}
std::optional<PointerShape> SearchBox::pointer_shape_at(Point local) const {
    if (!Rect{0, 0, bounds().width, std::min(1, bounds().height)}.contains(local)) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    const Layout parts = layout();
    return parts.clear_width > 0 && local.x >= parts.clear_x ? PointerShape::Pointer : PointerShape::Text;
}

void SearchBox::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}

void SearchBox::on_attached() {
    accessory_role_ = context().roles->find("ckv.input.accessory");
    accessory_hovered_role_ = context().roles->find("ckv.input.accessory.hovered");
    if (label_role_ == ui::kInvalidRole) label_role_ = context().roles->find("ckv.label.text");
    if (label_disabled_role_ == ui::kInvalidRole)
        label_disabled_role_ = context().roles->find("ckv.label.disabled");
    if (context().app != nullptr && prompt_ == "Search ") {
        prompt_ = context().app->strings().search_box_prompt;
        prompt_columns_ = text::text_width(prompt_);
    }
    place_field();
}

void SearchBox::on_strings_changed() {
    if (context().app == nullptr) return;
    prompt_ = context().app->strings().search_box_prompt;
    prompt_columns_ = text::text_width(prompt_);
    place_field();
    size_hint_changed();
    invalidate();
}


}  // namespace ckv::widgets
