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

bool is_descendant_of(const ui::View& view, const ui::View& ancestor) noexcept {
    for (const ui::View* p = &view; p != nullptr; p = p->parent())
        if (p == &ancestor) return true;
    return false;
}

}  // namespace

// --- DropdownMenu ------------------------------------------------------

DropdownMenu::DropdownMenu(std::vector<MenuItem> items, DropdownMenu* parent_menu)
    : items_(std::move(items)), parent_menu_(parent_menu) {}

DropdownMenu::~DropdownMenu() { dismiss(); }

void DropdownMenu::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.menu.dropdown.normal");
    if (highlighted_role_ == ui::kInvalidRole)
        highlighted_role_ = context().roles->find("ckv.menu.dropdown.highlighted");
    if (disabled_role_ == ui::kInvalidRole)
        disabled_role_ = context().roles->find("ckv.menu.dropdown.disabled");
    app_ = context().app;
    for (ui::View* p = parent(); p != nullptr; p = p->parent()) {
        if (auto* d = dynamic_cast<Desktop*>(p)) {
            desktop_ = d;
            break;
        }
    }
    // Deferred from construction: item_enabled() below needs
    // context().app, which is only valid from here on.
    highlighted_ = step_selection(-1, +1);
}

bool DropdownMenu::item_enabled(std::size_t index) const {
    const MenuItem& item = items_[index];
    if (item.separator) return false;
    if (item.command != ui::kInvalidCommand)
        return context().app->commands().is_enabled(item.command);
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
    if (!items_[static_cast<std::size_t>(index)].submenu.empty()) {
        open_submenu(index);
        return;
    }
    // Copy what we need out of items_/context() BEFORE dismiss(): on_dismiss
    // may destroy `this` (e.g. MenuBar::close_dropdown() removes this
    // popup from its owning Desktop) — nothing below may read a member
    // (or call context()) on `this` after that point.
    const ui::CommandId command = items_[static_cast<std::size_t>(index)].command;
    const std::function<void()> on_activate = items_[static_cast<std::size_t>(index)].on_activate;
    ui::Application& app = *context().app;
    dismiss_chain();
    if (command != ui::kInvalidCommand)
        app.execute_command(command);
    else if (on_activate)
        on_activate();
}

void DropdownMenu::dismiss() {
    if (dismissing_) return;
    dismissing_ = true;
    close_submenu(false);
    // Copy before invoking: the callback may destroy `this`, so
    // `this->on_dismiss` must not be touched after it starts running.
    const std::function<void()> callback = on_dismiss;
    if (callback) callback();
}

void DropdownMenu::dismiss_chain() {
    DropdownMenu* root = this;
    while (root->parent_menu_ != nullptr) root = root->parent_menu_;
    root->dismiss();
}

void DropdownMenu::open_submenu(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) return;
    const MenuItem& item = items_[static_cast<std::size_t>(index)];
    if (item.submenu.empty() || desktop_ == nullptr || app_ == nullptr) return;
    close_submenu(false);

    auto submenu = std::make_unique<DropdownMenu>(item.submenu, this);
    auto* raw = desktop_->add_popup(std::move(submenu));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    const Rect abs = absolute_bounds();
    const Rect desktop_abs = desktop_->absolute_bounds();
    int local_x = abs.x + bounds().width - desktop_abs.x - 1;
    if (local_x + w.preferred > desktop_->bounds().width)
        local_x = abs.x - desktop_abs.x - w.preferred + 1;
    const int local_y = abs.y + index - desktop_abs.y;
    raw->set_bounds(Rect{local_x, local_y, w.preferred, h.preferred});
    raw->on_dismiss = [this, raw]() {
        if (child_menu_ == raw) close_submenu(true);
    };
    child_menu_ = raw;
    app_->set_input_capture(raw);
    invalidate();
}

void DropdownMenu::close_submenu(bool restore_capture) {
    DropdownMenu* child = child_menu_;
    if (child == nullptr) return;
    child_menu_ = nullptr;
    if (app_ != nullptr && app_->input_capture() == child) app_->clear_input_capture();
    if (desktop_ != nullptr) desktop_->remove_popup(child);
    if (restore_capture && app_ != nullptr) app_->set_input_capture(this);
    invalidate();
}

bool DropdownMenu::has_check_column() const noexcept {
    return std::any_of(items_.begin(), items_.end(), [](const MenuItem& item) {
        return item.check != MenuItemCheck::None;
    });
}

bool DropdownMenu::has_submenu_column() const noexcept {
    return std::any_of(items_.begin(), items_.end(), [](const MenuItem& item) {
        return !item.submenu.empty();
    });
}

std::string DropdownMenu::item_source_text(const MenuItem& item) const {
    if (item.command != ui::kInvalidCommand) {
        if (const ui::CommandInfo* info = context().app->commands().find(item.command))
            return info->title;
    }
    return item.label;
}

std::optional<std::string> DropdownMenu::item_chord_hint(const MenuItem& item) const {
    if (item.command == ui::kInvalidCommand) return std::nullopt;
    const auto chord = context().app->commands().chord_for_command(item.command);
    if (!chord) return std::nullopt;
    return format(*chord);
}

SizeHint DropdownMenu::horizontal_size_hint() const {
    int max_width = 0;
    const int check_columns = has_check_column() ? 2 : 0;
    const int submenu_columns = has_submenu_column() ? 2 : 0;
    for (const auto& item : items_) {
        if (item.separator) continue;
        int width = check_columns + text::text_width(parse_mnemonic(item_source_text(item)).display) +
                    submenu_columns;
        if (const auto hint = item_chord_hint(item)) width += 2 + text::text_width(*hint);
        max_width = std::max(max_width, width);
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
            close_submenu(true);
            const int next = step_selection(highlighted_, -1);
            if (next != highlighted_) {
                highlighted_ = next;
                invalidate();
            }
            return true;
        }
        case Key::Down: {
            close_submenu(true);
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
        case Key::Right:
            open_submenu(highlighted_);
            return true;
        case Key::Left:
            if (parent_menu_ != nullptr) {
                dismiss();
                return true;
            }
            return false;
        case Key::Escape:
            dismiss();
            return true;
        case Key::Char:
            for (std::size_t i = 0; i < items_.size(); ++i) {
                if (items_[i].separator) continue;
                const auto parsed = parse_mnemonic(item_source_text(items_[i]));
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
    const ui::Theme& theme = *context().theme;
    const bool check_column = has_check_column();
    const bool submenu_column = has_submenu_column();
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const MenuItem& item = items_[static_cast<std::size_t>(i)];
        const bool enabled = item_enabled(static_cast<std::size_t>(i));
        const Style style = !enabled ? theme.resolve(disabled_role_)
                             : (i == highlighted_) ? theme.resolve(highlighted_role_)
                                                    : theme.resolve(normal_role_);
        painter.fill(Rect{0, i, bounds().width, 1}, Cell::from_grapheme(" ", style));
        if (item.separator) {
            painter.hline(Point{0, i}, bounds().width, scene::LineStyle::Single, style);
        } else {
            const auto parsed = parse_mnemonic(item_source_text(item));
            const int label_x = check_column ? 3 : 1;
            const int submenu_columns = submenu_column ? 2 : 0;
            int label_columns = std::max(0, bounds().width - label_x - 1 - submenu_columns);
            if (check_column && item.check != MenuItemCheck::None)
                painter.draw_text(Point{1, i}, item.check == MenuItemCheck::Checked ? "x" : " ", style);
            if (const auto hint = item_chord_hint(item)) {
                const int hint_x = bounds().width - 1 - submenu_columns - text::text_width(*hint);
                label_columns = std::max(0, hint_x - label_x - 1);
                const int label_end_x =
                    label_x + text::text_width(text::clip_to_width(parsed.display, label_columns));
                painter.draw_text(Point{label_x, i}, text::clip_to_width(parsed.display, label_columns), style);
                if (hint_x > label_end_x) painter.draw_text(Point{hint_x, i}, *hint, style);
            } else {
                painter.draw_text(Point{label_x, i}, text::clip_to_width(parsed.display, label_columns), style);
            }
            if (submenu_column && !item.submenu.empty() && bounds().width >= 2)
                painter.draw_text(Point{bounds().width - 2, i}, ">", style);
        }
    }
}

// --- MenuBar ---------------------------------------------------------

MenuBar::MenuBar(std::vector<MenuBarItem> menus) : menus_(std::move(menus)) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

MenuBar::~MenuBar() {
    close_dropdown();
    if (installed_default_menu_handler_ && app_ != nullptr)
        app_->commands().set_handler(ui::std_commands::kMenu, nullptr);
}

void MenuBar::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.menu.bar.normal");
    if (active_role_ == ui::kInvalidRole)
        active_role_ = context().roles->find("ckv.menu.bar.active");
    app_ = context().app;
    for (ui::View* p = parent(); p != nullptr; p = p->parent()) {
        if (auto* d = dynamic_cast<Desktop*>(p)) {
            desktop_ = d;
            break;
        }
    }
    // F10 activation, default and overridable (M9/WP-13, D-029) — see
    // the class comment for why this only installs itself when kMenu
    // is still unclaimed.
    if (app_ != nullptr && !app_->commands().has_handler(ui::std_commands::kMenu)) {
        app_->commands().set_handler(ui::std_commands::kMenu, [this] { activate(); });
        installed_default_menu_handler_ = true;
    }
}

void MenuBar::activate() {
    if (active_) return;
    previously_focused_ = app_->focused();
    app_->set_focus(this);  // on_focus(true) below flips active_
}

void MenuBar::deactivate() {
    if (!active_) return;
    close_dropdown();
    ui::View* restore = previously_focused_;
    previously_focused_ = nullptr;
    if (restore != nullptr && restore->focusable())
        app_->set_focus(restore);
    else
        app_->set_focus(nullptr);
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

    auto dropdown = std::make_unique<DropdownMenu>(menus_[menu_index].items);
    const Rect abs = absolute_bounds();
    const Rect desktop_abs = desktop_->absolute_bounds();
    const int local_x = abs.x + menu_bar_offset_for(menus_, menu_index) - desktop_abs.x;
    const int local_y = abs.y + 1 - desktop_abs.y;

    auto* raw = desktop_->add_popup(std::move(dropdown));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(Rect{local_x, local_y, w.preferred, h.preferred});
    raw->on_dismiss = [this]() { close_dropdown(); };

    open_dropdown_ = raw;
    app_->set_input_capture(raw);
    invalidate();
}

void MenuBar::close_dropdown() {
    if (open_dropdown_ == nullptr) return;
    if (app_->input_capture() == open_dropdown_) app_->clear_input_capture();
    desktop_->remove_popup(open_dropdown_);
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
    const ui::Theme& theme = *context().theme;
    const Style base = theme.resolve(normal_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        if (x >= bounds().width) break;
        const Style style = (active_ && i == highlighted_) ? theme.resolve(active_role_) : base;
        painter.draw_text(Point{x, 0},
                          text::clip_to_width(parse_mnemonic(menus_[i].label).display, bounds().width - x), style);
    }
}

// --- show_context_menu ---------------------------------------------------

DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                 ui::Application& app, Desktop& desktop) {
    auto menu = std::make_unique<DropdownMenu>(std::move(items));
    const Rect desktop_abs = desktop.absolute_bounds();
    auto* raw = desktop.add_popup(std::move(menu));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(Rect{screen_position.x - desktop_abs.x, screen_position.y - desktop_abs.y, w.preferred,
                          h.preferred});

    raw->on_dismiss = [&app, &desktop, raw]() {
        if (app.input_capture() == raw) app.clear_input_capture();
        desktop.remove_popup(raw);  // discards ownership -> destroys the DropdownMenu
    };
    app.set_input_capture(raw);
    return raw;
}

bool is_keyboard_context_menu_request(const KeyEvent& event) noexcept {
    return event.action == KeyAction::Press && event.chord.key == Key::F10 &&
           event.chord.modifiers == Modifier::Shift;
}

DropdownMenu* show_context_menu_for_focus(std::vector<MenuItem> items, ui::Application& app,
                                          Desktop& desktop) {
    const Rect desktop_abs = desktop.absolute_bounds();
    Point position{desktop_abs.x, desktop_abs.y};
    if (ui::View* focused = app.focused(); focused != nullptr && is_descendant_of(*focused, desktop)) {
        const Rect focused_abs = focused->absolute_bounds();
        position = Point{std::clamp(focused_abs.x, desktop_abs.x, std::max(desktop_abs.x, desktop_abs.right() - 1)),
                         std::clamp(focused_abs.y, desktop_abs.y, std::max(desktop_abs.y, desktop_abs.bottom() - 1))};
    }
    return show_context_menu(std::move(items), position, app, desktop);
}

}  // namespace ckv::widgets
