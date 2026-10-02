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
    const std::string shortcut = context().app->commands().chord_text(command);
    if (shortcut.empty()) return EffectiveLabel{std::move(text), 0};
    return EffectiveLabel{shortcut + " " + text, text::text_width(shortcut)};
}

ui::CommandId StatusLine::item_command(const StatusLineItem& item) const noexcept {
    return item.presentation.command != ui::kInvalidCommand ? item.presentation.command : item.command;
}

bool StatusLine::item_available(const StatusLineItem& item) const {
    const ui::CommandId command = item_command(item);
    return command == ui::kInvalidCommand || context().app->command_available(command);
}

bool StatusLine::press_context_current() const {
    const ui::Application* app = context().app;
    return app && pressed_source_ == &shown_items() && pressed_focus_ == app->focused() &&
           (!pressed_focus_ || !pressed_focus_lifetime_.expired()) &&
           pressed_revision_ == app->commands().revision();
}

void StatusLine::set_presentation(StatusLinePresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    preparation_dirty_ = true;
    pressed_item_.reset();
    invalidate();
}

void StatusLine::on_resized() { pressed_item_.reset(); }

int StatusLine::separator_width(const std::vector<StatusLineItem>& items,
                                std::size_t before, std::size_t after) const {
    if (presentation_ == StatusLinePresentation::Grouped) {
        for (std::size_t i = before + 1; i <= after; ++i)
            if (items[i].group_break_before) return 3; // blank, divider, blank
    }
    return kSeparatorWidth;
}

void StatusLine::set_items(std::vector<StatusLineItem> items) {
    items_ = std::move(items);
    preparation_dirty_ = true;
    pressed_item_.reset();
    invalidate();
}

void StatusLine::set_context_items(std::string context, std::vector<StatusLineItem> items) {
    const auto existing = std::find_if(context_items_.begin(), context_items_.end(),
                                       [&context](const auto& entry) { return entry.first == context; });
    if (items.empty()) {
        if (existing != context_items_.end()) context_items_.erase(existing);
    } else if (existing != context_items_.end()) {
        existing->second = std::move(items);
    } else {
        context_items_.emplace_back(std::move(context), std::move(items));
    }
    preparation_dirty_ = true;
    pressed_item_.reset();
    invalidate();
}

void StatusLine::clear_context_items() {
    context_items_.clear();
    preparation_dirty_ = true;
    pressed_item_.reset();
    invalidate();
}

const std::vector<StatusLineItem>& StatusLine::shown_items() const {
    if (context_items_.empty() || context().app == nullptr) return items_;
    for (const ui::View* view = context().app->focused(); view != nullptr; view = view->parent()) {
        if (!view->command_context()) continue;
        for (const auto& [context_name, items] : context_items_)
            if (context_name == *view->command_context()) return items;
        // The nearest context decides: an outer context's set does not show
        // through an inner context that has none of its own.
        return items_;
    }
    return items_;
}

void StatusLine::set_hint_provider(std::function<std::string(const std::string&)> provider) {
    hint_provider_ = std::move(provider);
    refresh_hint();
}

void StatusLine::set_transient_hint(std::string hint) {
    if (transient_hint_ == hint) return;
    transient_hint_ = std::move(hint);
    invalidate();
}

void StatusLine::refresh_hint() {
    hint_dirty_ = true;
    invalidate();
}

std::string_view StatusLine::hint_view() const {
    if (!transient_hint_.empty()) return transient_hint_;
    const ui::Application* app = context().app;
    if (!hint_provider_ || !app) return {};
    const ui::View* origin = app->focused();
    if (!origin) origin = &app->root();
    const std::string* key = origin->resolve_help_context_key();
    const std::uint64_t revision = app->commands().revision();
    if (hint_dirty_ || hint_app_ != app || hint_revision_ != revision ||
        prepared_hint_has_key_ != (key != nullptr) || (key && prepared_hint_key_ != *key)) {
        prepared_hint_has_key_ = key != nullptr;
        prepared_hint_key_ = key ? *key : std::string{};
        prepared_hint_ = key ? hint_provider_(*key) : std::string{};
        hint_revision_ = revision;
        hint_app_ = app;
        hint_dirty_ = false;
    }
    return prepared_hint_;
}

std::string StatusLine::current_hint() const { return std::string(hint_view()); }

void StatusLine::prepare() const {
    const auto& items = shown_items();
    const auto* app = context().app;
    const std::uint64_t revision = app ? app->commands().revision() : 0;
    const bool labels_changed = preparation_dirty_ || prepared_source_ != &items ||
                                prepared_app_ != app || prepared_revision_ != revision;
    if (!labels_changed && prepared_width_ == bounds().width) return;
    if (labels_changed) {
        prepared_.resize(items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            prepared_[i].label = effective_label(items[i]);
            prepared_[i].width = text::text_width(prepared_[i].label.text);
        }
        visible_indices_.reserve(items.size());
        layout_.reserve(items.size());
    }
    prepared_source_ = &items;
    prepared_app_ = app;
    prepared_revision_ = revision;
    prepared_width_ = bounds().width;
    preparation_dirty_ = false;

    const int available = std::max(0, bounds().width - kItemPadding);
    visible_indices_.clear();
    layout_.clear();
    for (std::size_t i = 0; i < items.size(); ++i) visible_indices_.push_back(i);
    if (items.empty()) return;
    const auto total_width = [&] {
        int width = 0;
        for (std::size_t p = 0; p < visible_indices_.size(); ++p) {
            if (p > 0) width += separator_width(items, visible_indices_[p - 1], visible_indices_[p]);
            width += prepared_[visible_indices_[p]].width;
        }
        return width;
    };
    const bool equal_priorities = std::all_of(items.begin(), items.end(),
        [priority = items.front().priority](const StatusLineItem& item) { return item.priority == priority; });
    if (equal_priorities) {
        int occupied = 0;
        std::size_t count = 0;
        for (const std::size_t index : visible_indices_) {
            const int start = occupied + (count == 0 ? 0 : separator_width(items, visible_indices_[count - 1], index));
            if (start >= available) break;
            ++count;
            occupied = start + prepared_[index].width;
        }
        visible_indices_.resize(count);
    } else {
        while (!visible_indices_.empty() && total_width() > available) {
            auto remove = visible_indices_.begin();
            for (auto it = visible_indices_.begin(); it != visible_indices_.end(); ++it) {
                if (items[*it].priority < items[*remove].priority ||
                    (items[*it].priority == items[*remove].priority && it > remove)) remove = it;
            }
            visible_indices_.erase(remove);
        }
    }
    int x = kItemPadding;
    for (std::size_t p = 0; p < visible_indices_.size(); ++p) {
        if (p > 0) x += separator_width(items, visible_indices_[p - 1], visible_indices_[p]);
        const std::size_t index = visible_indices_[p];
        layout_.push_back(LaidOutItem{index, x, prepared_[index].width});
        x += prepared_[index].width;
    }
}

const std::vector<StatusLine::LaidOutItem>& StatusLine::visible_items() const {
    prepare();
    return layout_;
}

SizeHint StatusLine::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint StatusLine::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

void StatusLine::draw(scene::Painter& painter) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, bounds().width, 1});
    const Style style = context().theme->resolve(role_);
    clipped.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));

    const std::vector<StatusLineItem>& items = shown_items();
    const std::vector<LaidOutItem>& layout = visible_items();
    for (std::size_t position = 0; position < layout.size(); ++position) {
        const LaidOutItem& item = layout[position];
        const bool is_pressed =
            pressed_item_.has_value() && press_context_current() && *pressed_item_ == item.index && pressed_visible_;
        const bool available = enabled_in_tree() && item_available(items[item.index]);
        // A pressed item wears the theme's selected colours, padding
        // included, so the highlight reads as one pressed button rather
        // than as recoloured words. Whether that is an inversion or a
        // colour of its own is the theme's decision, not this widget's.
        const Style pressed_style = context().theme->resolve(
            available ? selected_role_ : selected_disabled_role_);
        Style item_style = available ? style : context().theme->resolve(disabled_role_);
        if (is_pressed) item_style = pressed_style;
        // One separator marks an actual group boundary, never each command.
        if (position > 0 && separator_width(items, layout[position - 1].index, item.index) == 3)
            clipped.draw_text(Point{item.x - 2, 0}, "│", style);
        if (is_pressed)
            clipped.fill(Rect{item.x - kItemPadding, 0, item.width + 2 * kItemPadding, 1},
                         Cell::from_grapheme(" ", pressed_style));
        const EffectiveLabel& label = prepared_[item.index].label;
        clipped.draw_text(Point{item.x, 0}, text::clip_to_width_view(label.text, bounds().width - item.x), item_style);
        // The chord keeps its accent while pressed — it is the item's
        // identity, and losing it mid-press makes the item look like a
        // different one for as long as the button is held.
        if (label.hotkey_width > 0 && available)
            clipped.draw_text(Point{item.x, 0}, text::clip_to_width_view(label.text, label.hotkey_width),
                              accent_style(item_style, context().theme->resolve(
                                                           is_pressed ? selected_hotkey_role_ : hotkey_role_)));
    }

    // The hint follows the items. They are what the reader can act on and
    // so keep the cells they need; the explanation takes whatever is left,
    // which on a narrow terminal is nothing rather than the items' space.
    const std::string_view hint = hint_view();
    if (hint.empty() || layout.empty()) {
        if (!hint.empty())
            clipped.draw_text(Point{0, 0}, text::clip_to_width_view(hint, bounds().width), style);
        return;
    }
    // The divider sits clear of the last item's own trailing padding, so a
    // pressed item keeps that cell highlighted underneath it.
    const LaidOutItem& last = layout.back();
    const int separator_x = last.x + last.width + kItemPadding;
    const int hint_x = separator_x + kHintSeparatorWidth;
    if (hint_x >= bounds().width) return;
    clipped.draw_text(Point{separator_x, 0}, kHintSeparator, style);
    clipped.draw_text(Point{hint_x, 0}, text::clip_to_width_view(hint, bounds().width - hint_x), style);
}

std::optional<std::size_t> StatusLine::item_at(Point cell) const {
    const Rect abs = absolute_bounds();
    if (!abs.contains(cell) || cell.y != abs.y) return std::nullopt;
    const int local_x = cell.x - abs.x;
    for (const LaidOutItem& item : visible_items()) {
        if (local_x >= item.x - kItemPadding && local_x < item.x + item.width + kItemPadding)
            return item.index;
    }
    return std::nullopt;
}

std::optional<PointerShape> StatusLine::pointer_shape_at(Point local) const {
    const Rect abs = absolute_bounds();
    const auto hit = item_at(Point{abs.x + local.x, abs.y + local.y});
    if (!hit) return std::nullopt;
    const auto& items = shown_items();
    if (item_command(items[*hit]) == ui::kInvalidCommand) return std::nullopt;
    return enabled_in_tree() && item_available(items[*hit]) ? PointerShape::Pointer : PointerShape::NotAllowed;
}

bool StatusLine::on_mouse(const MouseEvent& event) {
    if (pressed_item_ && (!press_context_current() || !enabled_in_tree())) {
        pressed_item_.reset();
        invalidate();
        return true; // Context/enablement changed: consume cancellation, never activate.
    }
    if (!enabled_in_tree()) return false;
    const std::optional<std::size_t> hit = item_at(event.cell);
    if (event.action == MouseAction::Down) {
        if (!hit || event.button != MouseButton::Left) return false;
        // Show the press before acting on it: a command that runs with no
        // visible acknowledgement leaves the reader unsure it was hit.
        pressed_item_ = hit;
        pressed_source_ = &shown_items();
        pressed_focus_ = context().app->focused();
        pressed_focus_lifetime_ = pressed_focus_ ? pressed_focus_->lifetime_token() : std::weak_ptr<void>{};
        pressed_revision_ = context().app->commands().revision();
        pressed_visible_ = true;
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
        if (!pressed || hit != pressed || event.button != MouseButton::Left) return pressed.has_value();
        const std::vector<StatusLineItem>& items = shown_items();
        if (*pressed >= items.size()) return true;
        const StatusLineItem& item = items[*pressed];
        if (item_command(item) != ui::kInvalidCommand && item_available(item))
            context().app->execute_command(item_command(item));
        return true;
    }
    return false;
}

}  // namespace ckv::widgets
