// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/status_line.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {
constexpr int kSeparatorWidth = 3;  // " | "
}  // namespace

StatusLine::StatusLine() = default;

void StatusLine::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (disabled_role_ == ui::kInvalidRole)
        disabled_role_ = context().roles->find("ckv.statusline.disabled");
    if (hotkey_role_ == ui::kInvalidRole) hotkey_role_ = context().roles->find("ckv.hotkey");
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
    if (const auto chord = context().app->commands().chord_for_command(command)) {
        std::string shortcut = format(*chord);
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

std::string StatusLine::current_hint() const {
    if (!hint_provider_) return {};
    const ui::View* focused = context().app->focused();
    if (focused == nullptr) return {};
    const std::string* key = focused->resolve_help_context_key();
    if (key == nullptr) return {};
    return hint_provider_(*key);
}

std::vector<StatusLine::LaidOutItem> StatusLine::visible_items() const {
    const std::string hint = current_hint();
    const int hint_width = text::text_width(hint);
    const int item_area_start = !hint.empty() && !items_.empty() ? hint_width + 2 : hint_width;
    const int available = std::max(0, bounds().width - item_area_start);

    std::vector<std::size_t> visible;
    visible.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) visible.push_back(i);

    auto total_width = [&]() {
        int width = 0;
        for (std::size_t position = 0; position < visible.size(); ++position) {
            if (position > 0) width += kSeparatorWidth;
            width += text::text_width(effective_label(items_[visible[position]]).text);
        }
        return width;
    };

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

    const std::string hint = text::clip_to_width(current_hint(), bounds().width);
    if (!hint.empty()) painter.draw_text(Point{0, 0}, hint, style);

    const std::vector<LaidOutItem> layout = visible_items();
    for (std::size_t position = 0; position < layout.size(); ++position) {
        const LaidOutItem& item = layout[position];
        const Style item_style =
            item_available(items_[item.index]) ? style : context().theme->resolve(disabled_role_);
        if (position > 0) painter.draw_text(Point{item.x - kSeparatorWidth, 0}, " | ", style);
        const EffectiveLabel label = effective_label(items_[item.index]);
        painter.draw_text(Point{item.x, 0}, text::clip_to_width(label.text, bounds().width - item.x), item_style);
        if (label.hotkey_width > 0 && item_available(items_[item.index]))
            painter.draw_text(Point{item.x, 0}, text::clip_to_width(label.text, label.hotkey_width),
                              accent_style(item_style, context().theme->resolve(hotkey_role_)));
    }
}

bool StatusLine::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const int local_x = event.cell.x - abs.x;
    if (event.cell.y != abs.y) return false;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int x = item_start_column(i);
        if (x < 0) continue;
        const int w = text::text_width(effective_label(items_[i]).text);
        if (local_x >= x && local_x < x + w) {
            if (item_command(items_[i]) != ui::kInvalidCommand && item_available(items_[i]))
                context().app->execute_command(item_command(items_[i]));
            return true;
        }
    }
    return false;
}

}  // namespace ckv::widgets
