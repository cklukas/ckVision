// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/menu.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

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
    // Two cells of leading margin, so a dropped popup can sit one cell to
    // the left of its title and still start on screen — see open_dropdown
    // for why the popup hangs left rather than under the title's first
    // letter.
    int x = 2;
    for (std::size_t i = 0; i < index; ++i)
        x += text::text_width(parse_mnemonic(menus[i].label).display) + 2;  // label + 2-space gap
    return x;
}

Rect clamp_popup_to_desktop(Rect popup, const Rect& desktop) noexcept {
    const int max_x = std::max(0, desktop.width - popup.width);
    const int max_y = std::max(0, desktop.height - popup.height);
    popup.x = std::clamp(popup.x, 0, max_x);
    popup.y = std::clamp(popup.y, 0, max_y);
    return popup;
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
    if (hotkey_role_ == ui::kInvalidRole) hotkey_role_ = context().roles->find("ckv.hotkey");
    app_ = context().app;
    for (ui::View* p = parent(); p != nullptr; p = p->parent()) {
        if (auto* d = dynamic_cast<Desktop*>(p)) {
            desktop_ = d;
            break;
        }
    }
    // Deferred from construction: item_enabled() below needs
    // context().app, which is only valid from here on. A pointer-opened
    // menu deliberately starts with nothing selected — see MenuOpenReason.
    if (open_reason_ == MenuOpenReason::Keyboard) set_highlighted(step_selection(-1, +1));
}

void DropdownMenu::end_pointer_press() {
    pointer_pressed_ = false;
    // Releasing without having pointed at an item is how a menu is opened
    // for browsing: it now needs a selection for the keyboard to move from.
    if (highlighted_ < 0) {
        set_highlighted(step_selection(-1, +1));
        invalidate();
    }
}

void DropdownMenu::set_highlighted(int index) {
    if (highlighted_ == index) return;
    highlighted_ = index;
    if (on_highlight_changed) on_highlight_changed(highlighted_command());
}

ui::CommandId DropdownMenu::highlighted_command() const noexcept {
    if (highlighted_ < 0 || static_cast<std::size_t>(highlighted_) >= items_.size())
        return ui::kInvalidCommand;
    const MenuItem& item = items_[static_cast<std::size_t>(highlighted_)];
    if (item.separator || !item.submenu.empty()) return ui::kInvalidCommand;
    return item_command(item);
}

bool DropdownMenu::item_enabled(std::size_t index) const {
    const MenuItem& item = items_[index];
    if (item.separator) return false;
    if (item_command(item) != ui::kInvalidCommand)
        return context().app->command_available(item_command(item));
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
    const ui::CommandId command = item_command(items_[static_cast<std::size_t>(index)]);
    const std::function<void()> on_activate = items_[static_cast<std::size_t>(index)].on_activate;
    ui::Application& app = *context().app;
    // Chosen, not merely closed: the bar unwinds and returns focus where it
    // found it BEFORE the command runs, so anything the command opens sees
    // the reader's real focus rather than the menu that launched it.
    dismiss_chain(MenuDismissReason::ItemChosen);
    if (command != ui::kInvalidCommand)
        app.execute_command(command);
    else if (on_activate)
        on_activate();
}

void DropdownMenu::dismiss(MenuDismissReason reason) {
    if (dismissing_) return;
    dismissing_ = true;
    close_submenu(false);
    // Copy before invoking: the callback may destroy `this`, so
    // `this->on_dismiss` must not be touched after it starts running.
    const std::function<void(MenuDismissReason)> callback = on_dismiss;
    if (callback) callback(reason);
}

void DropdownMenu::dismiss_chain(MenuDismissReason reason) {
    DropdownMenu* root = this;
    while (root->parent_menu_ != nullptr) root = root->parent_menu_;
    root->dismiss(reason);
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
    // A submenu aligns its frame with the selected item's padded row, rather
    // than with the parent frame above it.
    const int local_y = abs.y + index + 1 - desktop_abs.y;
    raw->set_bounds(clamp_popup_to_desktop(Rect{local_x, local_y, w.preferred, h.preferred}, desktop_->bounds()));
    raw->on_dismiss = [this, raw](MenuDismissReason) {
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
    return item_presentation_label(item);
}

std::optional<std::string> DropdownMenu::item_chord_hint(const MenuItem& item) const {
    const ui::CommandId command = item_command(item);
    if (command == ui::kInvalidCommand) return std::nullopt;
    // A surface-stated chord wins over the registry's (see
    // CommandPresentation::chord): the menu advertises the way this
    // application actually reaches the command.
    if (!item.presentation.chord.empty()) return item.presentation.chord;
    const auto chord = context().app->commands().chord_for_command(command);
    if (!chord) return std::nullopt;
    return context().app->commands().format_chord(*chord);
}

ui::CommandId DropdownMenu::item_command(const MenuItem& item) const noexcept {
    return item.presentation.command != ui::kInvalidCommand ? item.presentation.command : item.command;
}

std::string DropdownMenu::item_presentation_label(const MenuItem& item) const {
    if (!item.presentation.label.empty()) return item.presentation.label;
    const ui::CommandId command = item_command(item);
    if (command != ui::kInvalidCommand) {
        if (const ui::CommandInfo* info = context().app->commands().find(command))
            return info->title;
    }
    return item.label;
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
    // The popup owns an opaque interior plus a one-cell frame. The frame is
    // part of its geometry rather than an overlay, so retained composition
    // cannot reveal the desktop along a menu edge.
    const int width = max_width + 4;
    return SizeHint{width, width, width};
}

SizeHint DropdownMenu::vertical_size_hint() const {
    const int height = static_cast<int>(items_.size()) + 2;
    return SizeHint{height, height, height};
}

bool DropdownMenu::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up: {
            close_submenu(true);
            const int next = step_selection(highlighted_, -1);
            if (next != highlighted_) {
                set_highlighted(next);
                invalidate();
            }
            return true;
        }
        case Key::Down: {
            close_submenu(true);
            const int next = step_selection(highlighted_, +1);
            if (next != highlighted_) {
                set_highlighted(next);
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
    // A menu bar owns the full press-drag-release gesture while its dropdown
    // has input capture. In particular, the captured popup must still hand a
    // button release over the top bar back to MenuBar: treating that release
    // as "outside" would immediately dismiss a just-opened menu.
    if (pointer_navigation_ && pointer_navigation_(event)) return true;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    const bool inside = local.x >= 0 && local.x < abs.width && local.y >= 0 && local.y < abs.height;
    const bool framed = bounds().width >= 4 && bounds().height >= static_cast<int>(items_.size()) + 2;
    int candidate = -1;
    if (inside && !(framed && (local.x == 0 || local.x == bounds().width - 1 || local.y == 0 ||
                                local.y == bounds().height - 1))) {
        const int row = local.y - (framed ? 1 : 0);
        if (row >= 0 && row < static_cast<int>(items_.size()) && item_enabled(static_cast<std::size_t>(row)))
            candidate = row;
    }
    if (event.action == MouseAction::Down) {
        if (!inside) {
            dismiss();  // light dismiss: this event only reaches us via input capture
            return false;
        }
        pointer_pressed_ = true;
        if (highlighted_ != candidate) {
            set_highlighted(candidate);
            invalidate();
        }
        return true;
    }
    if (event.action == MouseAction::Move) {
        if (highlighted_ != candidate) {
            set_highlighted(candidate);
            invalidate();
        }
        return pointer_pressed_ || inside;
    }
    if (event.action == MouseAction::Up) {
        const bool was_pressed = pointer_pressed_;
        pointer_pressed_ = false;
        if (was_pressed && candidate >= 0) activate(candidate);
        else if (!inside) dismiss();
        return was_pressed || inside;
    }
    return false;
}

void DropdownMenu::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const bool check_column = has_check_column();
    const bool submenu_column = has_submenu_column();
    const Style normal = theme.resolve(normal_role_);
    // Direct unit renderings may deliberately assign a one-row rectangle to
    // inspect a single item. Real popup geometry always comes from the size
    // hints above and is therefore framed.
    const bool framed = bounds().width >= 4 && bounds().height >= static_cast<int>(items_.size()) + 2;
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", normal));
    if (framed)
        painter.draw_box(Rect{0, 0, bounds().width, bounds().height}, scene::LineStyle::Single, normal);
    const int item_left = framed ? 1 : 0;
    const int item_top = framed ? 1 : 0;
    const int item_width = bounds().width - (framed ? 2 : 0);
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const MenuItem& item = items_[static_cast<std::size_t>(i)];
        const bool enabled = item_enabled(static_cast<std::size_t>(i));
        const Style style = !enabled ? theme.resolve(disabled_role_)
                             : (i == highlighted_) ? theme.resolve(highlighted_role_)
                                                    : normal;
        painter.fill(Rect{item_left, item_top + i, item_width, 1}, Cell::from_grapheme(" ", style));
        if (item.separator) {
            // Run the rule into the frame itself rather than stopping one
            // cell short: meeting the side borders is what lets junction
            // merging turn them into tees, so the separator reads as part
            // of the frame instead of a detached dash sitting inside it.
            if (framed)
                painter.hline(Point{0, item_top + i}, bounds().width, scene::LineStyle::Single, normal);
            else
                painter.hline(Point{item_left, item_top + i}, item_width, scene::LineStyle::Single, style);
        } else {
            const auto parsed = parse_mnemonic(item_source_text(item));
            const int label_x = item_left + (check_column ? 3 : 1);
            const int submenu_columns = submenu_column ? 2 : 0;
            int label_columns = std::max(0, item_left + item_width - label_x - 1 - submenu_columns);
            if (check_column && item.check != MenuItemCheck::None)
                painter.draw_text(Point{item_left + 1, item_top + i},
                                  item.check == MenuItemCheck::Checked ? "x" : " ", style);
            if (const auto hint = item_chord_hint(item)) {
                const int hint_x = item_left + item_width - 1 - submenu_columns - text::text_width(*hint);
                label_columns = std::max(0, hint_x - label_x - 1);
                const int label_end_x =
                    label_x + text::text_width(text::clip_to_width(parsed.display, label_columns));
                draw_mnemonic(painter, Point{label_x, item_top + i}, parsed, label_columns, style,
                              accent_style(style, theme.resolve(hotkey_role_)));
                if (hint_x > label_end_x) painter.draw_text(Point{hint_x, item_top + i}, *hint, style);
            } else {
                draw_mnemonic(painter, Point{label_x, item_top + i}, parsed, label_columns, style,
                              accent_style(style, theme.resolve(hotkey_role_)));
            }
            if (submenu_column && !item.submenu.empty() && bounds().width >= 2)
                painter.draw_text(Point{item_left + item_width - 2, item_top + i}, ">", style);
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

void MenuBar::set_menus(std::vector<MenuBarItem> menus) {
    close_dropdown();
    remove_menu_accelerators();
    menus_ = std::move(menus);
    install_menu_accelerators();
    if (menus_.empty()) {
        highlighted_ = 0;
    } else if (highlighted_ >= menus_.size()) {
        highlighted_ = menus_.size() - 1;
    }
    invalidate();
    size_hint_changed();
}

void MenuBar::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.menu.bar.normal");
    if (active_role_ == ui::kInvalidRole)
        active_role_ = context().roles->find("ckv.menu.bar.active");
    if (hotkey_role_ == ui::kInvalidRole) hotkey_role_ = context().roles->find("ckv.hotkey");
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
    // Menus set before the bar was attached have no accelerators yet: the
    // registry only becomes reachable here.
    install_menu_accelerators();
}

void MenuBar::install_menu_accelerators() {
    if (app_ == nullptr || !menu_accelerators_.empty()) return;
    for (std::size_t index = 0; index < menus_.size(); ++index) {
        const ui::CommandId id =
            ui::std_commands::kFirstMenuAccelerator + static_cast<ui::CommandId>(index);
        if (id >= ui::std_commands::kMenuAcceleratorLimit) break;
        const auto parsed = parse_mnemonic(menus_[index].label);
        if (parsed.mnemonic.empty()) continue;
        KeyChord chord;
        chord.key = Key::Char;
        chord.modifiers = Modifier::Alt;
        // Mnemonics are matched case-insensitively; the chord carries the
        // lowercase spelling the decoder produces.
        chord.text = parsed.mnemonic;
        if (chord.text.size() == 1)
            chord.text[0] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(chord.text[0])));
        app_->commands().register_command(id, parsed.display, "Menu", chord);
        app_->commands().set_handler(id, [this, index] {
            activate();
            open_dropdown(static_cast<int>(index));
        });
        app_->commands().bind_key(chord, id);
        menu_accelerators_.push_back(id);
    }
}

void MenuBar::remove_menu_accelerators() {
    if (app_ == nullptr) return;
    for (const ui::CommandId id : menu_accelerators_) {
        if (const auto chord = app_->commands().chord_for_command(id))
            app_->commands().unbind_key(*chord);
        app_->commands().retract_command(id);
    }
    menu_accelerators_.clear();
}

void MenuBar::activate() {
    // Opening the bar starts at its first menu, the way a reader who just
    // pressed F10 expects — a selection carried over from the last visit
    // would make the following `A`, `C` land somewhere they never looked.
    highlighted_ = 0;
    if (active_) {
        close_dropdown();
        invalidate();
        return;
    }
    previously_focused_ = app_->focused();
    app_->set_focus(this);  // on_focus(true) below flips active_
}

void MenuBar::deactivate() {
    if (!active_) return;
    if (MenuBarAccessory* const accessory = trailing_accessory()) accessory->set_menu_highlighted(false);
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

void MenuBar::open_dropdown(std::size_t menu_index, MenuOpenReason reason) {
    CKV_ASSERT(menu_index < menus_.size());
    close_dropdown();
    highlighted_ = menu_index;

    auto dropdown = std::make_unique<DropdownMenu>(menus_[menu_index].items);
    // Before add_popup(): on_attached() decides the initial selection from it.
    dropdown->set_open_reason(reason);
    const Rect abs = absolute_bounds();
    const Rect desktop_abs = desktop_->absolute_bounds();
    // The popup's FRAME hangs one cell left of its title, which puts the
    // item text one cell right of the title's first letter. Aligning the
    // frame with the title instead pushes every item two cells right and
    // the popup visibly fails to hang under the menu it belongs to.
    const int local_x = abs.x + menu_bar_offset_for(menus_, menu_index) - 1 - desktop_abs.x;
    const int local_y = abs.y + 1 - desktop_abs.y;

    auto* raw = desktop_->add_popup(std::move(dropdown));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(clamp_popup_to_desktop(Rect{local_x, local_y, w.preferred, h.preferred}, desktop_->bounds()));
    raw->on_dismiss = [this](MenuDismissReason reason) {
        // A chosen item ends the menu interaction; anything else only
        // closes the popup and leaves the bar where the reader left it.
        if (reason == MenuDismissReason::ItemChosen)
            deactivate();
        else
            close_dropdown();
    };
    raw->set_pointer_navigation([this](const MouseEvent& event) { return navigate_pointer(event); });
    // A menu opens and closes many times over a session; the application
    // wires the bar once and hears about every highlight through it,
    // rather than re-attaching to each popup as it appears.
    raw->on_highlight_changed = [this](ui::CommandId command) {
        if (on_highlight_changed) on_highlight_changed(command);
    };
    // The popup settles on its first selectable entry while attaching,
    // which is before the line above could hear it. Report that opening
    // position explicitly so a listener starts out describing the entry
    // the reader is actually looking at.
    if (on_highlight_changed) on_highlight_changed(raw->highlighted_command());

    open_dropdown_ = raw;
    app_->set_input_capture(raw);
    invalidate();
}

void MenuBar::close_dropdown() {
    if (open_dropdown_ == nullptr) return;
    if (app_->input_capture() == open_dropdown_) app_->clear_input_capture();
    desktop_->remove_popup(open_dropdown_);
    open_dropdown_ = nullptr;
    // Nothing is highlighted once the popup is gone: a surface showing
    // the highlighted command's explanation has to stop showing it, or
    // it would outlive the menu that produced it.
    if (on_highlight_changed) on_highlight_changed(ui::kInvalidCommand);
    invalidate();
}

SizeHint MenuBar::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint MenuBar::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

bool MenuBar::on_key(const KeyEvent& event) {
    if (!active_ || menus_.empty()) return false;
    // Keyboard focus remains on the bar while its popup owns mouse capture.
    // Route popup-local navigation and mnemonics explicitly; otherwise a
    // character intended for a menu item is examined only as a top-level
    // mnemonic and cannot activate the item. Left/right deliberately remain
    // bar-owned so they switch top-level menus in the established manner.
    if (open_dropdown_ != nullptr) {
        switch (event.chord.key) {
            case Key::Up:
            case Key::Down:
            case Key::Enter:
            case Key::Char:
                return open_dropdown_->on_key(event);
            default:
                break;
        }
    }
    switch (event.chord.key) {
        case Key::Left:
            // The walk runs over one slot per menu plus the trailing title,
            // so a clock at the right end is reached by walking to it rather
            // than by knowing it is there.
            set_bar_highlight((highlighted_ + navigable_slots() - 1) % navigable_slots());
            if (open_dropdown_ != nullptr && !trailing_slot_highlighted()) open_dropdown(highlighted_);
            invalidate();
            return true;
        case Key::Right:
            set_bar_highlight((highlighted_ + 1) % navigable_slots());
            if (open_dropdown_ != nullptr && !trailing_slot_highlighted()) open_dropdown(highlighted_);
            invalidate();
            return true;
        case Key::Down:
        case Key::Enter:
            if (trailing_slot_highlighted()) {
                // What drops out of a trailing title is the caller's; the bar
                // only says when.
                trailing_accessory()->activate_from_menu_bar();
                return true;
            }
            open_dropdown(highlighted_);
            return true;

        case Key::Escape:
            deactivate();
            return true;
        case Key::Char:
            // Space acts on the trailing title, as Enter does. It arrives as
            // an ordinary character, so it is handled with the mnemonics
            // rather than as a key of its own.
            if (event.chord.text == " " && trailing_slot_highlighted()) {
                trailing_accessory()->activate_from_menu_bar();
                return true;
            }
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
    if (event.action != MouseAction::Down) return navigate_pointer(event);
    const Rect abs = absolute_bounds();
    if (event.cell.y != abs.y) return false;
    const int local_x = event.cell.x - abs.x;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        const int w = text::text_width(parse_mnemonic(menus_[i].label).display);
        if (local_x >= x - 1 && local_x < x + w + 1) {
            if (!active_) activate();
            open_dropdown(i, MenuOpenReason::PointerPress);
            if (open_dropdown_ != nullptr) open_dropdown_->begin_pointer_press();
            return true;
        }
    }
    return false;
}

void MenuBar::on_resized() { layout_trailing_view(); }

void MenuBar::set_bar_highlight(std::size_t slot) {
    highlighted_ = slot;
    // The trailing title is a view, not a label the bar paints, so it has to
    // be told when the walk is standing on it.
    if (MenuBarAccessory* const accessory = trailing_accessory())
        accessory->set_menu_highlighted(trailing_slot_highlighted());
    invalidate();
}

MenuBarAccessory* MenuBar::trailing_accessory() const noexcept {
    return dynamic_cast<MenuBarAccessory*>(trailing_view_);
}

std::size_t MenuBar::navigable_slots() const noexcept {
    return menus_.size() + (trailing_accessory() != nullptr ? 1 : 0);
}

bool MenuBar::trailing_slot_highlighted() const noexcept {
    return active_ && trailing_accessory() != nullptr && highlighted_ == menus_.size();
}

ui::View* MenuBar::set_trailing_view_impl(std::unique_ptr<ui::View> view) {
    if (trailing_view_ != nullptr) {
        (void)remove_child(trailing_view_);
        trailing_view_ = nullptr;
    }
    if (view == nullptr) return nullptr;
    trailing_view_ = add_child(std::move(view));
    layout_trailing_view();
    return trailing_view_;
}

void MenuBar::layout_trailing_view() {
    if (trailing_view_ == nullptr) return;
    // Its own preferred width, pinned to the right edge. Recomputed rather
    // than remembered: a clock that gains seconds is a cell or two wider,
    // and the right edge is wherever the bar ends now.
    const int width = std::clamp(trailing_view_->horizontal_size_hint().preferred, 0, bounds().width);
    trailing_view_->set_bounds(Rect{bounds().width - width, 0, width, 1});
}

void MenuBar::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const Style base = theme.resolve(normal_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        if (x >= bounds().width) break;
        const Style style = (active_ && i == highlighted_) ? theme.resolve(active_role_) : base;
        if (active_ && i == highlighted_ && x > 0)
            painter.fill(Rect{x - 1, 0, std::min(bounds().width - (x - 1),
                                                  text::text_width(parse_mnemonic(menus_[i].label).display) + 2),
                              1},
                         Cell::from_grapheme(" ", style));
        draw_mnemonic(painter, Point{x, 0}, parse_mnemonic(menus_[i].label), bounds().width - x, style,
                      accent_style(style, theme.resolve(hotkey_role_)));
    }
}

bool MenuBar::navigate_pointer(const MouseEvent& event) {
    if (!active_ || menus_.empty() || event.cell.y != absolute_bounds().y) return false;
    const int local_x = event.cell.x - absolute_bounds().x;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        const int width = text::text_width(parse_mnemonic(menus_[i].label).display);
        if (local_x < x - 1 || local_x >= x + width + 1) continue;
        if (open_dropdown_ == nullptr || highlighted_ != i)
            open_dropdown(i, MenuOpenReason::PointerPress);
        if (open_dropdown_ != nullptr) {
            if (event.action == MouseAction::Up)
                open_dropdown_->end_pointer_press();
            else
                open_dropdown_->begin_pointer_press();
        }
        return true;
    }
    return false;
}

// --- show_context_menu ---------------------------------------------------

DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                 ui::Application& app, Desktop& desktop) {
    auto menu = std::make_unique<DropdownMenu>(std::move(items));
    const Rect desktop_abs = desktop.absolute_bounds();
    auto* raw = desktop.add_popup(std::move(menu));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(clamp_popup_to_desktop(
        Rect{screen_position.x - desktop_abs.x, screen_position.y - desktop_abs.y, w.preferred, h.preferred},
        desktop.bounds()));

    raw->on_dismiss = [&app, &desktop, raw](MenuDismissReason) {
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
