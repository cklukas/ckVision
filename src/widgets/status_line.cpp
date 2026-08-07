// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/status_line.hpp"

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {
constexpr int kSeparatorWidth = 3;  // " | "
}  // namespace

StatusLine::StatusLine() = default;

void StatusLine::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
}

std::string StatusLine::effective_label(const StatusLineItem& item) const {
    if (item.command == ui::kInvalidCommand) return item.label;
    const ui::CommandInfo* info = context().app->commands().find(item.command);
    if (info == nullptr) return item.label;
    // Menu titles may carry a '&' mnemonic marker for menus to parse;
    // the status line never navigates by letter, so it always strips
    // one rather than leaking the raw character into the rendered text.
    std::string text = parse_mnemonic(info->title).display;
    if (const auto chord = context().app->commands().chord_for_command(item.command))
        text = format(*chord) + " " + text;
    return text;
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

int StatusLine::item_start_column(std::size_t index) const {
    const std::string hint = current_hint();
    int x = text::text_width(hint);
    if (!hint.empty() && !items_.empty()) x += 2;
    for (std::size_t i = 0; i < index; ++i)
        x += text::text_width(effective_label(items_[i])) + kSeparatorWidth;
    return x;
}

SizeHint StatusLine::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint StatusLine::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

void StatusLine::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", style));

    const std::string hint = current_hint();
    if (!hint.empty()) painter.draw_text(Point{0, 0}, hint, style);

    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int x = item_start_column(i);
        if (x >= bounds().width) break;
        if (i > 0) painter.draw_text(Point{x - kSeparatorWidth, 0}, " | ", style);
        painter.draw_text(Point{x, 0}, effective_label(items_[i]), style);
    }
}

bool StatusLine::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const int local_x = event.cell.x - abs.x;
    if (event.cell.y != abs.y) return false;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int x = item_start_column(i);
        const int w = text::text_width(effective_label(items_[i]));
        if (local_x >= x && local_x < x + w) {
            if (items_[i].command != ui::kInvalidCommand)
                context().app->execute_command(items_[i].command);
            return true;
        }
    }
    return false;
}

}  // namespace ckv::widgets
