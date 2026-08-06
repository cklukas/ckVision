// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/menu.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {

bool ascii_ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

int menu_bar_offset_for(const std::vector<MenuBarItem>& menus, std::size_t index) {
    int x = 1;  // leading margin
    for (std::size_t i = 0; i < index; ++i)
        x += text::text_width(parse_mnemonic(menus[i].label).display) + 2;  // label + 2-space gap
    return x;
}

}  // namespace

// --- DropdownMenu ------------------------------------------------------

DropdownMenu::DropdownMenu(std::vector<MenuItem> items, const ui::Theme& theme, ui::RoleId normal_role,
                            ui::RoleId highlighted_role, ui::RoleId disabled_role, ui::Application& app)
    : items_(std::move(items)),
      theme_(theme),
      normal_role_(normal_role),
      highlighted_role_(highlighted_role),
      disabled_role_(disabled_role),
      app_(app) {
    highlighted_ = step_selection(-1, +1);
}

bool DropdownMenu::item_enabled(std::size_t index) const {
    const MenuItem& item = items_[index];
    if (item.separator) return false;
    if (item.command != ui::kInvalidCommand) return app_.commands().is_enabled(item.command);
    return true;
}

int DropdownMenu::step_selection(int from, int direction) const {
    if (items_.empty()) return -1;
    const int n = static_cast<int>(items_.size());
    int idx = from;
    for (int i = 0; i < n; ++i) {
        idx = (idx + direction + n) % n;
        if (item_enabled(static_cast<std::size_t>(idx))) return idx;
    }
    return -1;
}

void DropdownMenu::activate(int index) {
    if (index < 0 || !item_enabled(static_cast<std::size_t>(index))) return;
    const MenuItem& item = items_[static_cast<std::size_t>(index)];
    if (item.command != ui::kInvalidCommand)
        app_.execute_command(item.command);
    else if (item.on_activate)
        item.on_activate();
    dismiss();
}

void DropdownMenu::dismiss() {
    if (on_dismiss) on_dismiss();
}

SizeHint DropdownMenu::horizontal_size_hint() const {
    int max_width = 0;
    for (const auto& item : items_) {
        if (item.separator) continue;
        max_width = std::max(max_width, text::text_width(parse_mnemonic(item.label).display));
    }
    const int width = max_width + 2;
    return SizeHint{width, width, width};
}

SizeHint DropdownMenu::vertical_size_hint() const {
    const int height = static_cast<int>(items_.size());
    return SizeHint{height, height, height};
}

bool DropdownMenu::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up: {
            const int next = step_selection(highlighted_, -1);
            if (next != highlighted_) {
                highlighted_ = next;
                invalidate();
            }
            return true;
        }
        case Key::Down: {
            const int next = step_selection(highlighted_, +1);
            if (next != highlighted_) {
                highlighted_ = next;
                invalidate();
            }
            return true;
        }
        case Key::Enter:
            activate(highlighted_);
            return true;
        case Key::Escape:
            dismiss();
            return true;
        case Key::Char:
            for (std::size_t i = 0; i < items_.size(); ++i) {
                if (items_[i].separator) continue;
                const auto parsed = parse_mnemonic(items_[i].label);
                if (!parsed.mnemonic.empty() && ascii_ci_equal(parsed.mnemonic, event.chord.text) &&
                    item_enabled(i)) {
                    activate(static_cast<int>(i));
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool DropdownMenu::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    const bool inside = local.x >= 0 && local.x < abs.width && local.y >= 0 && local.y < abs.height;
    if (!inside) {
        dismiss();  // light dismiss: this event only reaches us via input capture
        return false;
    }
    const auto row = static_cast<std::size_t>(local.y);
    if (row < items_.size() && item_enabled(row)) {
        highlighted_ = static_cast<int>(row);
        activate(static_cast<int>(row));
    }
    return true;
}

void DropdownMenu::draw(scene::Painter& painter) {
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const bool enabled = item_enabled(static_cast<std::size_t>(i));
        const Style style = !enabled ? theme_.resolve(disabled_role_)
                             : (i == highlighted_) ? theme_.resolve(highlighted_role_)
                                                    : theme_.resolve(normal_role_);
        painter.fill(Rect{0, i, bounds().width, 1}, Cell::from_grapheme(" ", style));
        if (items_[static_cast<std::size_t>(i)].separator) {
            painter.hline(Point{0, i}, bounds().width, scene::LineStyle::Single, style);
        } else {
            const auto parsed = parse_mnemonic(items_[static_cast<std::size_t>(i)].label);
            painter.draw_text(Point{1, i}, parsed.display, style);
        }
    }
}

// --- MenuBar ---------------------------------------------------------

MenuBar::MenuBar(std::vector<MenuBarItem> menus, const ui::Theme& theme, ui::RoleId normal_role,
                  ui::RoleId active_role, ui::RoleId dropdown_normal_role, ui::RoleId dropdown_highlighted_role,
                  ui::RoleId dropdown_disabled_role, ui::Application& app, Desktop& desktop)
    : menus_(std::move(menus)),
      theme_(theme),
      normal_role_(normal_role),
      active_role_(active_role),
      dropdown_normal_role_(dropdown_normal_role),
      dropdown_highlighted_role_(dropdown_highlighted_role),
      dropdown_disabled_role_(dropdown_disabled_role),
      app_(app),
      desktop_(desktop) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

MenuBar::~MenuBar() { close_dropdown(); }

void MenuBar::activate() {
    if (active_) return;
    previously_focused_ = app_.focused();
    app_.set_focus(this);  // on_focus(true) below flips active_
}

void MenuBar::deactivate() {
    if (!active_) return;
    close_dropdown();
    ui::View* restore = previously_focused_;
    previously_focused_ = nullptr;
    if (restore != nullptr && restore->focusable())
        app_.set_focus(restore);
    else
        app_.set_focus(nullptr);
}

void MenuBar::on_focus(const FocusEvent& event) {
    active_ = event.gained;
    if (!event.gained) {
        highlighted_ = 0;
        close_dropdown();
    }
    invalidate();
}

void MenuBar::open_dropdown(std::size_t menu_index) {
    CKV_ASSERT(menu_index < menus_.size());
    close_dropdown();
    highlighted_ = menu_index;

    auto dropdown = std::make_unique<DropdownMenu>(menus_[menu_index].items, theme_, dropdown_normal_role_,
                                                     dropdown_highlighted_role_, dropdown_disabled_role_, app_);
    const Rect abs = absolute_bounds();
    const Rect desktop_abs = desktop_.absolute_bounds();
    const int local_x = abs.x + menu_bar_offset_for(menus_, menu_index) - desktop_abs.x;
    const int local_y = abs.y + 1 - desktop_abs.y;

    auto* raw = static_cast<DropdownMenu*>(desktop_.add_popup(std::move(dropdown)));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(Rect{local_x, local_y, w.preferred, h.preferred});
    raw->on_dismiss = [this]() { close_dropdown(); };

    open_dropdown_ = raw;
    app_.set_input_capture(raw);
    invalidate();
}

void MenuBar::close_dropdown() {
    if (open_dropdown_ == nullptr) return;
    if (app_.input_capture() == open_dropdown_) app_.clear_input_capture();
    desktop_.remove_popup(open_dropdown_);
    open_dropdown_ = nullptr;
    invalidate();
}

SizeHint MenuBar::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint MenuBar::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

bool MenuBar::on_key(const KeyEvent& event) {
    if (!active_ || menus_.empty()) return false;
    switch (event.chord.key) {
        case Key::Left:
            highlighted_ = (highlighted_ + menus_.size() - 1) % menus_.size();
            if (open_dropdown_ != nullptr) open_dropdown(highlighted_);
            invalidate();
            return true;
        case Key::Right:
            highlighted_ = (highlighted_ + 1) % menus_.size();
            if (open_dropdown_ != nullptr) open_dropdown(highlighted_);
            invalidate();
            return true;
        case Key::Down:
        case Key::Enter:
            open_dropdown(highlighted_);
            return true;
        case Key::Escape:
            deactivate();
            return true;
        case Key::Char:
            for (std::size_t i = 0; i < menus_.size(); ++i) {
                const auto parsed = parse_mnemonic(menus_[i].label);
                if (!parsed.mnemonic.empty() && ascii_ci_equal(parsed.mnemonic, event.chord.text)) {
                    open_dropdown(i);
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool MenuBar::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    if (event.cell.y != abs.y) return false;
    const int local_x = event.cell.x - abs.x;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        const int w = text::text_width(parse_mnemonic(menus_[i].label).display);
        if (local_x >= x && local_x < x + w) {
            if (!active_) activate();
            open_dropdown(i);
            return true;
        }
    }
    return false;
}

void MenuBar::draw(scene::Painter& painter) {
    const Style base = theme_.resolve(normal_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        const Style style = (active_ && i == highlighted_) ? theme_.resolve(active_role_) : base;
        painter.draw_text(Point{x, 0}, parse_mnemonic(menus_[i].label).display, style);
    }
}

}  // namespace ckv::widgets
