// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/status_line.hpp"

#include <algorithm>
#include <string_view>

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {
// Items are set apart by space alone: a rule between every command turns a
// legend into a table and competes with the one divider that carries real
// meaning — the boundary between what you can press and what is being
// explained. That one uses a box-drawing vertical, matching the frames
// around it rather than an ASCII stand-in.
constexpr int kSeparatorWidth = 2;      // blank gap between items
constexpr std::string_view kHintSeparator = "│ ";
constexpr int kHintSeparatorWidth = 2;  // cells occupied by kHintSeparator
// One blank cell either side of an item's text. It belongs to the item —
// a pressed item highlights its padding too, which is what makes the
// highlight read as a button rather than as coloured words.
constexpr int kItemPadding = 1;
}  // namespace

StatusLine::StatusLine() = default;

void StatusLine::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (disabled_role_ == ui::kInvalidRole)
        disabled_role_ = context().roles->find("ckv.statusline.disabled");
    if (hotkey_role_ == ui::kInvalidRole) hotkey_role_ = context().roles->find("ckv.hotkey");
    if (selected_role_ == ui::kInvalidRole)
        selected_role_ = context().roles->find("ckv.statusline.selected");
    if (selected_hotkey_role_ == ui::kInvalidRole)
        selected_hotkey_role_ = context().roles->find("ckv.statusline.selected.hotkey");
    if (selected_disabled_role_ == ui::kInvalidRole)
        selected_disabled_role_ = context().roles->find("ckv.statusline.selected.disabled");
}

StatusLine::EffectiveLabel StatusLine::effective_label(const StatusLineItem& item) const {
    const ui::CommandId command = item_command(item);
    if (command == ui::kInvalidCommand) return EffectiveLabel{item.label, 0};
    const ui::CommandInfo* info = context().app->commands().find(command);
    if (info == nullptr) return EffectiveLabel{item.presentation.label.empty() ? item.label : item.presentation.label, 0};
    // Menu titles may carry a '&' mnemonic marker for menus to parse;
    // the status line never navigates by letter, so it always strips
    // one rather than leaking the raw character into the rendered text.
    std::string text = parse_mnemonic(item.presentation.label.empty() ? info->title : item.presentation.label).display;
    // A surface-stated chord wins over the registry's: it is the application
    // saying how the command is reached from where the reader is now.
    if (!item.presentation.chord.empty())
        return EffectiveLabel{item.presentation.chord + " " + text,
                              text::text_width(item.presentation.chord)};
    if (const auto chord = context().app->commands().chord_for_command(command)) {
        std::string shortcut = context().app->commands().format_chord(*chord);
        return EffectiveLabel{shortcut + " " + text, text::text_width(shortcut)};
    }
    return EffectiveLabel{std::move(text), 0};
}

ui::CommandId StatusLine::item_command(const StatusLineItem& item) const noexcept {
    return item.presentation.command != ui::kInvalidCommand ? item.presentation.command : item.command;
}

bool StatusLine::item_available(const StatusLineItem& item) const {
    const ui::CommandId command = item_command(item);
    return command == ui::kInvalidCommand || context().app->command_available(command);
}

void StatusLine::set_items(std::vector<StatusLineItem> items) {
    items_ = std::move(items);
    invalidate();
}

void StatusLine::set_hint_provider(std::function<std::string(const std::string&)> provider) {
    hint_provider_ = std::move(provider);
    invalidate();
}

void StatusLine::set_transient_hint(std::string hint) {
    if (transient_hint_ == hint) return;
    transient_hint_ = std::move(hint);
    invalidate();
}

std::string StatusLine::current_hint() const {
    if (!transient_hint_.empty()) return transient_hint_;
    if (!hint_provider_) return {};
    const ui::View* focused = context().app->focused();
    if (focused == nullptr) return {};
    const std::string* key = focused->resolve_help_context_key();
    if (key == nullptr) return {};
    return hint_provider_(*key);
}

std::vector<StatusLine::LaidOutItem> StatusLine::visible_items() const {
    // A leading margin so the first item does not begin against the screen
    // edge, matching the one-cell padding each item carries on both sides.
    const int item_area_start = kItemPadding;
    const int available = std::max(0, bounds().width - kItemPadding);

    std::vector<std::size_t> visible;
    visible.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) visible.push_back(i);
    if (visible.empty()) return {};

    auto total_width = [&]() {
        int width = 0;
        for (std::size_t position = 0; position < visible.size(); ++position) {
            if (position > 0) width += kSeparatorWidth;
            width += text::text_width(effective_label(items_[visible[position]]).text);
        }
        return width;
    };

    const bool equal_priorities = std::all_of(items_.begin(), items_.end(),
                                              [priority = items_.front().priority](const StatusLineItem& item) {
                                                  return item.priority == priority;
                                              });
    if (equal_priorities) {
        // A conventional status legend is read left-to-right.  Retain every
        // item that begins on screen and let Painter clip only the final
        // label, so a narrow terminal still conveys the start of its next
        // instruction instead of silently discarding it.
        int occupied = 0;
        std::vector<std::size_t> clipped;
        clipped.reserve(visible.size());
        for (const std::size_t index : visible) {
            const int start = occupied + (clipped.empty() ? 0 : kSeparatorWidth);
            if (start >= available) break;
            clipped.push_back(index);
            occupied = start + text::text_width(effective_label(items_[index]).text);
        }
        visible = std::move(clipped);
    } else {
        while (!visible.empty() && total_width() > available) {
            auto remove = visible.begin();
            for (auto it = visible.begin(); it != visible.end(); ++it) {
                const StatusLineItem& candidate = items_[*it];
                const StatusLineItem& current = items_[*remove];
                if (candidate.priority < current.priority ||
                    (candidate.priority == current.priority && it > remove))
                    remove = it;
            }
            visible.erase(remove);
        }
    }

    std::vector<LaidOutItem> layout;
    layout.reserve(visible.size());
    int x = item_area_start;
    for (std::size_t position = 0; position < visible.size(); ++position) {
        if (position > 0) x += kSeparatorWidth;
        const std::size_t index = visible[position];
        const int width = text::text_width(effective_label(items_[index]).text);
        layout.push_back(LaidOutItem{index, x, width});
        x += width;
    }
    return layout;
}

int StatusLine::item_start_column(std::size_t index) const {
    for (const LaidOutItem& item : visible_items())
        if (item.index == index) return item.x;
    return -1;
}

SizeHint StatusLine::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint StatusLine::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

void StatusLine::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));

    const std::vector<LaidOutItem> layout = visible_items();
    for (std::size_t position = 0; position < layout.size(); ++position) {
        const LaidOutItem& item = layout[position];
        const bool is_pressed =
            pressed_item_.has_value() && *pressed_item_ == item.index && pressed_visible_;
        const bool available = item_available(items_[item.index]);
        // A pressed item wears the theme's selected colours, padding
        // included, so the highlight reads as one pressed button rather
        // than as recoloured words. Whether that is an inversion or a
        // colour of its own is the theme's decision, not this widget's.
        const Style pressed_style = context().theme->resolve(
            available ? selected_role_ : selected_disabled_role_);
        Style item_style = available ? style : context().theme->resolve(disabled_role_);
        if (is_pressed) item_style = pressed_style;
        // Nothing drawn between items: the gap is the separation.
        if (is_pressed)
            painter.fill(Rect{item.x - kItemPadding, 0, item.width + 2 * kItemPadding, 1},
                         Cell::from_grapheme(" ", pressed_style));
        const EffectiveLabel label = effective_label(items_[item.index]);
        painter.draw_text(Point{item.x, 0}, text::clip_to_width(label.text, bounds().width - item.x), item_style);
        // The chord keeps its accent while pressed — it is the item's
        // identity, and losing it mid-press makes the item look like a
        // different one for as long as the button is held.
        if (label.hotkey_width > 0 && available)
            painter.draw_text(Point{item.x, 0}, text::clip_to_width(label.text, label.hotkey_width),
                              accent_style(item_style, context().theme->resolve(
                                                           is_pressed ? selected_hotkey_role_ : hotkey_role_)));
    }

    // The hint follows the items. They are what the reader can act on and
    // so keep the cells they need; the explanation takes whatever is left,
    // which on a narrow terminal is nothing rather than the items' space.
    const std::string hint = current_hint();
    if (hint.empty() || layout.empty()) {
        if (!hint.empty())
            painter.draw_text(Point{0, 0}, text::clip_to_width(hint, bounds().width), style);
        return;
    }
    // The divider sits clear of the last item's own trailing padding, so a
    // pressed item keeps that cell highlighted underneath it.
    const LaidOutItem& last = layout.back();
    const int separator_x = last.x + last.width + kItemPadding;
    const int hint_x = separator_x + kHintSeparatorWidth;
    if (hint_x >= bounds().width) return;
    painter.draw_text(Point{separator_x, 0}, std::string(kHintSeparator), style);
    painter.draw_text(Point{hint_x, 0}, text::clip_to_width(hint, bounds().width - hint_x), style);
}

std::optional<std::size_t> StatusLine::item_at(Point cell) const {
    const Rect abs = absolute_bounds();
    if (cell.y != abs.y) return std::nullopt;
    const int local_x = cell.x - abs.x;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int x = item_start_column(i);
        if (x < 0) continue;
        const int w = text::text_width(effective_label(items_[i]).text);
        // The padding is part of the item: it highlights with the text and
        // it accepts the click, exactly as its appearance promises.
        if (local_x >= x - kItemPadding && local_x < x + w + kItemPadding) return i;
    }
    return std::nullopt;
}

bool StatusLine::on_mouse(const MouseEvent& event) {
    const std::optional<std::size_t> hit = item_at(event.cell);
    if (event.action == MouseAction::Down) {
        if (!hit) return false;
        // Show the press before acting on it: a command that runs with no
        // visible acknowledgement leaves the reader unsure it was hit.
        pressed_item_ = hit;
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Move) {
        if (!pressed_item_) return false;
        const std::optional<std::size_t> now = hit == pressed_item_ ? hit : std::nullopt;
        if (now.has_value() != pressed_visible_) {
            pressed_visible_ = now.has_value();
            invalidate();
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        const std::optional<std::size_t> pressed = pressed_item_;
        pressed_item_.reset();
        pressed_visible_ = true;
        invalidate();
        // Releasing away from the item it started on takes the press back.
        if (!pressed || hit != pressed) return pressed.has_value();
        const StatusLineItem& item = items_[*pressed];
        if (item_command(item) != ui::kInvalidCommand && item_available(item))
            context().app->execute_command(item_command(item));
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
