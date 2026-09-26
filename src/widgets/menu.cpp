// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/menu.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "cvision/core/ascii.hpp"
#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

// --- MenuItem ---------------------------------------------------------

MenuItem MenuItem::command(ui::CommandId id) {
    return command(CommandPresentation{id});
}

MenuItem MenuItem::command(CommandPresentation presentation) {
    MenuItem item;
    item.kind_ = MenuItemKind::Command;
    item.presentation_ = std::move(presentation);
    return item;
}

MenuItem MenuItem::action(std::string label, std::function<void()> run) {
    MenuItem item;
    item.kind_ = MenuItemKind::Action;
    item.label_ = std::move(label);
    item.action_ = std::move(run);
    return item;
}

MenuItem MenuItem::submenu(std::string label, std::vector<MenuItem> children) {
    MenuItem item;
    item.kind_ = MenuItemKind::Submenu;
    item.label_ = std::move(label);
    item.children_ = std::move(children);
    return item;
}

MenuItem MenuItem::separator() {
    MenuItem item;
    item.kind_ = MenuItemKind::Separator;
    return item;
}

MenuItem MenuItem::with_mark(MenuMark mark) const {
    MenuItem copy = *this;
    copy.mark_ = mark;
    copy.mark_provider_ = {};
    return copy;
}

MenuItem MenuItem::with_mark_provider(std::function<MenuMark()> provider) const {
    MenuItem copy = *this;
    copy.mark_provider_ = std::move(provider);
    return copy;
}

MenuItem MenuItem::with_help(std::string help_context) const {
    MenuItem copy = *this;
    copy.help_context_ = std::move(help_context);
    return copy;
}

MenuItem MenuItem::with_disabled_reason(std::string reason) const {
    MenuItem copy = *this;
    copy.disabled_reason_ = std::move(reason);
    return copy;
}

MenuItem MenuItem::with_enabled(bool enabled) const {
    // A Command row's availability belongs to its command: the registry's
    // predicate and context decide it, and every surface showing that
    // command has to agree. Letting a menu override it here is how a menu
    // comes to offer a verb the palette greys.
    CKV_ASSERT(kind_ != MenuItemKind::Command);
    MenuItem copy = *this;
    copy.enabled_ = enabled;
    return copy;
}

namespace {

int menu_title_width(const MenuBarItem& menu) {
    return text::text_width(parse_mnemonic(menu.label).display);
}

// What a bar too narrow for its titles ends the drawn ones with. The glyph
// reads as "more, further along", which is what the titles behind it are;
// an ellipsis would read as one title cut short.
constexpr std::string_view kOverflowTitle = "\u00BB";

int menu_bar_offset_for(const std::vector<MenuBarItem>& menus, std::size_t index) {
    // Two cells of leading margin, so a dropped popup can sit one cell to
    // the left of its title and still start on screen — see open_dropdown
    // for why the popup hangs left rather than under the title's first
    // letter.
    int x = 2;
    for (std::size_t i = 0; i < index; ++i)
        x += menu_title_width(menus[i]) + 2;  // label + 2-space gap
    return x;
}

Rect clamp_popup_to_desktop(Rect popup, const Rect& desktop) noexcept {
    // Never larger than the desktop either: a popup cut off by the screen's
    // edge loses its frame on that side, and whatever it held there with it.
    // A narrower popup elides its labels instead (DropdownMenu::draw).
    popup.width = std::min(popup.width, desktop.width);
    popup.height = std::min(popup.height, desktop.height);
    const int max_x = std::max(0, desktop.width - popup.width);
    const int max_y = std::max(0, desktop.height - popup.height);
    popup.x = std::clamp(popup.x, 0, max_x);
    popup.y = std::clamp(popup.y, 0, max_y);
    return popup;
}

// What a menu item says when choosing it opens another menu rather than
// running a command. The glyph the convention has always used — the solid
// right-pointing pointer, which Unicode carries at U+25BA and the widget
// set already draws on a horizontal scrollbar's end button — rather than
// an ASCII '>', which reads as text somebody typed into the label.
constexpr std::string_view kSubmenuMarker = "►";

// The column a menu item's right-hand content occupies: a chord hint, or
// the submenu marker. One item never has both — a submenu parent runs no
// command — so they share a column, and a menu with some of each reads as
// one right-hand column rather than two ragged ones.
// A box for an independent switch, a round mark for one-of-a-set. The
// shapes differ because the promises do: turning a radio row on turns its
// neighbour off, and a reader scanning the column should be able to see
// that without being told.
std::string_view mark_glyph(MenuMark mark) {
    switch (mark) {
        case MenuMark::Checked: return "x";
        case MenuMark::Unchecked: return " ";
        case MenuMark::RadioOn: return "\u2022";
        case MenuMark::RadioOff: return " ";
        case MenuMark::None: break;
    }
    return " ";
}

int right_column_width(const std::optional<std::string>& chord, bool marks_submenu) {
    if (chord) return text::text_width(*chord);
    return marks_submenu ? text::text_width(kSubmenuMarker) : 0;
}

bool is_descendant_of(const ui::View& view, const ui::View& ancestor) noexcept {
    for (const ui::View* p = &view; p != nullptr; p = p->parent())
        if (p == &ancestor) return true;
    return false;
}

}  // namespace

// --- DropdownMenu ------------------------------------------------------

DropdownMenu::DropdownMenu(std::vector<MenuItem> items, DropdownMenu* parent_menu)
    : ui::View({}, ui::FocusPolicy::TabStop), items_(std::move(items)), parent_menu_(parent_menu) {}

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
    if (on_highlight_changed) on_highlight_changed(highlight());
}

MenuHighlight DropdownMenu::highlight() const {
    // Down the open chain first: a reader standing in a submenu is not
    // standing on the parent row that opened it.
    if (child_menu_ != nullptr) return child_menu_->highlight();
    MenuHighlight out;
    if (highlighted_ < 0 || static_cast<std::size_t>(highlighted_) >= items_.size()) {
        out.none = true;
        return out;
    }
    const MenuItem& item = items_[static_cast<std::size_t>(highlighted_)];
    out.command = highlighted_command();
    out.help_context = item.help_context();
    out.disabled_reason = item.disabled_reason();
    out.enabled = item_enabled(static_cast<std::size_t>(highlighted_));
    return out;
}

ui::CommandId DropdownMenu::highlighted_command() const noexcept {
    if (highlighted_ < 0 || static_cast<std::size_t>(highlighted_) >= items_.size())
        return ui::kInvalidCommand;
    const MenuItem& item = items_[static_cast<std::size_t>(highlighted_)];
    if (item.is_separator() || item.has_children()) return ui::kInvalidCommand;
    return item_command(item);
}

bool DropdownMenu::item_enabled(std::size_t index) const {
    const MenuItem& item = items_[index];
    if (item.is_separator()) return false;
    // One source per kind: a command row is available exactly when its
    // command is, so the menu cannot disagree with the palette or the
    // status line; every other kind carries its own flag, because there
    // is nothing else that could know.
    //
    // Through context(), which follows attachment, and never through the
    // cached app_: a menu is asked what it shows while it is being torn
    // down too, and app_ outlives the application it points at. A row
    // nobody is there to adjudicate reads as available, which is what it
    // looked like before anyone asked.
    if (item.kind() == MenuItemKind::Command) {
        if (context().app == nullptr) return true;
        if (has_invocation_contexts_)
            return context().app->commands().is_available(item.command(), invocation_contexts_);
        return context().app->command_available(item.command());
    }
    return item.enabled_flag();
}

void DropdownMenu::follow_highlight_with_submenu() {
    const bool wanted = highlighted_ >= 0 &&
                        static_cast<std::size_t>(highlighted_) < items_.size() &&
                        items_[static_cast<std::size_t>(highlighted_)].has_children();
    if (!wanted) {
        close_submenu(true);
        return;
    }
    // Not "open one if none is open" — open THIS one. Moving from one row
    // with children straight onto another must change which submenu is
    // showing, and open_submenu already costs nothing when it is the one
    // already up.
    open_submenu(highlighted_);
}

bool DropdownMenu::item_reachable(std::size_t index) const {
    // A separator is scenery. Everything else can be stood on, including
    // a row that cannot be used: a reader who cannot reach a grey verb
    // cannot be told why it is grey, and "why not" is the question a grey
    // verb provokes. Enter simply does nothing there — see activate().
    return !items_[index].is_separator();
}

int DropdownMenu::step_selection(int from, int direction) const {
    if (items_.empty()) return -1;
    const int n = static_cast<int>(items_.size());
    int idx = from;
    for (int i = 0; i < n; ++i) {
        idx = (idx + direction + n) % n;
        if (item_reachable(static_cast<std::size_t>(idx))) return idx;
    }
    return -1;
}

void DropdownMenu::activate(int index) {
    if (index < 0 || !item_enabled(static_cast<std::size_t>(index))) return;
    if (items_[static_cast<std::size_t>(index)].has_children()) {
        open_submenu(index);
        return;
    }
    // Copy what we need out of items_/context() BEFORE dismiss(): on_dismiss
    // may destroy `this` (e.g. MenuBar::close_dropdown() removes this
    // popup from its owning Desktop) — nothing below may read a member
    // (or call context()) on `this` after that point.
    const ui::CommandId command = item_command(items_[static_cast<std::size_t>(index)]);
    const std::function<void()> on_activate = items_[static_cast<std::size_t>(index)].action();
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

DropdownMenu* DropdownMenu::innermost_menu() noexcept {
    DropdownMenu* menu = this;
    while (menu->child_menu_ != nullptr) menu = menu->child_menu_;
    return menu;
}

DropdownMenu* DropdownMenu::root_menu() noexcept {
    DropdownMenu* menu = this;
    while (menu->parent_menu_ != nullptr) menu = menu->parent_menu_;
    return menu;
}

DropdownMenu* DropdownMenu::menu_under_pointer(Point cell) noexcept {
    for (DropdownMenu* menu = innermost_menu(); menu != nullptr; menu = menu->parent_menu_)
        if (menu->absolute_bounds().contains(cell)) return menu;
    return nullptr;
}

bool& DropdownMenu::chain_pointer_pressed() noexcept { return root_menu()->pointer_pressed_; }

void DropdownMenu::open_submenu(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) return;
    const MenuItem& item = items_[static_cast<std::size_t>(index)];
    if (!item.has_children() || desktop_ == nullptr || app_ == nullptr) return;
    // Asking for the submenu that is already showing is not a request to
    // show it again: tearing it down and rebuilding it would take the input
    // capture and the reader's place inside it with it, for no visible
    // change. It is what a release over the row that just opened one asks
    // for, and what the pointer asks for on every move across that row.
    if (child_menu_ != nullptr && child_index_ == index) return;
    close_submenu(false);

    auto submenu = std::make_unique<DropdownMenu>(item.children(), this);
    if (has_invocation_contexts_) submenu->set_invocation_contexts(invocation_contexts_);
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
    // One highlight is current in a chain of menus, and it belongs to the
    // innermost one: browsing into a submenu is still browsing, so whoever
    // listens hears about it through the same route as the parent's own moves
    // rather than going dark for as long as the submenu is up.
    raw->on_highlight_changed = on_highlight_changed;
    child_menu_ = raw;
    child_index_ = index;
    // The submenu settled on its first entry while attaching, before the line
    // above could hear it — report that opening position explicitly.
    if (on_highlight_changed) on_highlight_changed(raw->highlight());
    app_->set_input_capture(raw);
    invalidate();
}

void DropdownMenu::close_submenu(bool restore_capture) {
    DropdownMenu* child = child_menu_;
    if (child == nullptr) return;
    child_menu_ = nullptr;
    child_index_ = -1;
    if (app_ != nullptr && app_->input_capture() == child) app_->clear_input_capture();
    if (desktop_ != nullptr) desktop_->remove_popup(child);
    if (restore_capture && app_ != nullptr) app_->set_input_capture(this);
    // The innermost highlight is this menu's again, now that the submenu that
    // held it is gone.
    if (on_highlight_changed) on_highlight_changed(highlight());
    invalidate();
}

bool DropdownMenu::has_check_column() const {
    return std::any_of(items_.begin(), items_.end(),
                       [this](const MenuItem& item) { return item_mark(item) != MenuMark::None; });
}

MenuMark DropdownMenu::item_mark(const MenuItem& item) const {
    const MenuMark own = item.mark();
    if (own != MenuMark::None) return own;
    // A toggle command carries its state in the registry, where every surface
    // that presents it reads the same answer.
    const ui::CommandId command = item_command(item);
    if (command == ui::kInvalidCommand || context().app == nullptr) return MenuMark::None;
    const std::optional<bool> checked = context().app->commands().checked(command);
    if (!checked) return MenuMark::None;
    return *checked ? MenuMark::Checked : MenuMark::Unchecked;
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
    if (!item.presentation().chord.empty()) return item.presentation().chord;
    std::string chord = context().app->commands().chord_text(command);
    if (chord.empty()) return std::nullopt;
    return chord;
}

ui::CommandId DropdownMenu::item_command(const MenuItem& item) const noexcept {
    return item.command();
}

std::string DropdownMenu::item_presentation_label(const MenuItem& item) const {
    if (!item.presentation().label.empty()) return item.presentation().label;
    const ui::CommandId command = item_command(item);
    if (command != ui::kInvalidCommand) {
        if (const ui::CommandInfo* info = context().app->commands().find(command))
            return info->title;
    }
    return item.label();
}

SizeHint DropdownMenu::horizontal_size_hint() const {
    int max_width = 0;
    const int check_columns = has_check_column() ? 2 : 0;
    for (const auto& item : items_) {
        if (item.is_separator()) continue;
        int width = check_columns + text::text_width(parse_mnemonic(item_source_text(item)).display);
        // Only what this item itself puts in the right-hand column. Charging
        // every item for a submenu marker one of them happens to need is
        // what left a chord hint stranded two columns short of the marker
        // it was supposed to line up with.
        if (const int right = right_column_width(item_chord_hint(item), item.has_children()); right > 0)
            width += 2 + right;
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
    // A context menu keeps the keyboard focus on its root while a submenu
    // takes the pointer capture, exactly as a menu bar keeps it on the bar.
    // The key belongs to the innermost open menu, where the reader's
    // highlight is: spent here it would move a row they are no longer
    // looking at, close their submenu on the way past, and make Esc close
    // the whole chain instead of one level. A bar routes to the innermost
    // menu itself, so it never reaches this with a submenu open.
    if (child_menu_ != nullptr) return innermost_menu()->on_key(event);
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
        case Key::Home:
        case Key::End: {
            // A menu long enough to need them is exactly the menu where
            // walking to an end one row at a time is tedious. step_selection
            // from outside the range lands on the first (or last) row that
            // can actually be chosen, so a leading separator or a greyed
            // first entry does not swallow the key.
            close_submenu(true);
            const bool to_end = event.chord.key == Key::End;
            const int from = to_end ? static_cast<int>(items_.size()) : -1;
            const int next = step_selection(from, to_end ? -1 : +1);
            if (next >= 0 && next != highlighted_) {
                set_highlighted(next);
                invalidate();
            }
            return true;
        }
        case Key::Enter:
            activate(highlighted_);
            return true;
        case Key::Right: {
            // A submenu opens where the reader is standing. An item that has
            // none leaves the key unclaimed rather than swallowing it: a menu
            // bar above this popup is then free to carry the walk on to the
            // next top-level menu, which is what Right means everywhere else
            // on the bar.
            if (highlighted_ < 0 || static_cast<std::size_t>(highlighted_) >= items_.size())
                return false;
            if (!items_[static_cast<std::size_t>(highlighted_)].has_children()) return false;
            open_submenu(highlighted_);
            return child_menu_ != nullptr;
        }
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
                if (items_[i].is_separator()) continue;
                const auto parsed = parse_mnemonic(item_source_text(items_[i]));
                if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, event.chord.text) &&
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
    // The same is true one level down, and for the same reason. Input capture
    // follows the innermost menu, but a pointer gesture belongs to the whole
    // chain: the press that opens a submenu lands on the PARENT row, and the
    // release that ends it arrives after the submenu has taken the capture —
    // over a point the submenu does not contain. Read by the submenu alone
    // that release is a click outside it, and the menu the reader just opened
    // vanishes before they can aim at anything in it. So an event goes to the
    // menu of this chain the pointer is actually over, and to the root when it
    // is over none of them — the root being the one that can hand a bar-row
    // event back to its MenuBar, or close the chain when the reader has
    // pressed somewhere else entirely.
    DropdownMenu* const over = menu_under_pointer(event.cell);
    DropdownMenu* const owner = over != nullptr ? over : root_menu();
    if (owner != this) return owner->on_mouse(event);
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    const bool inside = local.x >= 0 && local.x < abs.width && local.y >= 0 && local.y < abs.height;
    const bool framed = bounds().width >= 4 && bounds().height >= static_cast<int>(items_.size()) + 2;
    int candidate = -1;
    if (inside && !(framed && (local.x == 0 || local.x == bounds().width - 1 || local.y == 0 ||
                                local.y == bounds().height - 1))) {
        const int row = local.y - (framed ? 1 : 0);
        if (row >= 0 && row < static_cast<int>(items_.size()) &&
            item_reachable(static_cast<std::size_t>(row)))
            candidate = row;
    }
    if (event.action == MouseAction::Down) {
        if (!inside) {
            // Light dismiss: this press only reaches us via input capture,
            // and it is outside every menu of the chain. It ends the whole
            // menu interaction and is consumed by doing so: the popup
            // convention never lets the press that dismisses a popup act on
            // what lies beneath it as well.
            dismiss(MenuDismissReason::Outside);
            return true;
        }
        chain_pointer_pressed() = true;
        if (highlighted_ != candidate) {
            set_highlighted(candidate);
            invalidate();
        }
        // A press on a parent row opens its children, so a reader can go
        // straight down into a submenu with the pointer the same way the
        // keyboard's Right does.
        follow_highlight_with_submenu();
        return true;
    }
    if (event.action == MouseAction::Move) {
        if (highlighted_ != candidate) {
            set_highlighted(candidate);
            invalidate();
        }
        // The open submenu follows the pointer, whether or not the
        // highlight just moved: a submenu standing over a row the reader
        // has already left is a menu that no longer describes where the
        // pointer is, and one that has not opened yet under a row the
        // pointer is resting on is a menu that has not caught up.
        if (inside) follow_highlight_with_submenu();
        return chain_pointer_pressed() || inside;
    }
    if (event.action == MouseAction::Up) {
        // The press this release ends may have landed on another menu of the
        // chain — on the parent row whose children are now under the pointer,
        // most of all. A release that completed a real gesture chooses what it
        // is over, wherever that gesture began.
        const bool was_pressed = chain_pointer_pressed();
        chain_pointer_pressed() = false;
        if (was_pressed && candidate >= 0) activate(candidate);
        else if (!inside) dismiss(MenuDismissReason::Outside);  // a gesture let go of outside every menu
        return was_pressed || inside;
    }
    return false;
}

void DropdownMenu::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const bool check_column = has_check_column();
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
        // An unavailable row under the highlight keeps the highlight bar, in
        // the disabled foreground: the reader walking the menu has to see
        // where they are even on a row that will not act.
        const Style style = !enabled && i == highlighted_
                                ? accent_style(theme.resolve(highlighted_role_), theme.resolve(disabled_role_))
                            : !enabled             ? theme.resolve(disabled_role_)
                            : (i == highlighted_) ? theme.resolve(highlighted_role_)
                                                   : normal;
        painter.fill(Rect{item_left, item_top + i, item_width, 1}, Cell::from_grapheme(" ", style));
        if (item.is_separator()) {
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
            // The last column any content may use. One cell of padding
            // stands between it and the frame, matching the one on the left.
            const int content_right = item_left + item_width - 2;
            const std::optional<std::string> hint = item_chord_hint(item);
            const bool marks_submenu = item.has_children();
            const int right_width = right_column_width(hint, marks_submenu);
            const int right_x = content_right - right_width + 1;
            // Whatever the right-hand column holds ends at the same column
            // for every item, so a chord and a submenu marker sit under one
            // another instead of at two different margins.
            int label_columns = std::max(0, content_right - label_x + 1);
            if (right_width > 0) label_columns = std::max(0, right_x - label_x - 1);
            if (const MenuMark mark = item_mark(item); check_column && mark != MenuMark::None)
                painter.draw_text(Point{item_left + 1, item_top + i}, mark_glyph(mark), style);
            // A label that does not fit is elided, not cut: the reader must
            // be able to tell a shortened name from a short one. Its
            // mnemonic keeps its accent only if it survives the elision.
            MnemonicText shown = parsed;
            if (text::text_width(parsed.display) > label_columns) {
                shown.display = text::elide_to_width(parsed.display, label_columns);
                constexpr std::size_t kEllipsisBytes = std::string_view("\u2026").size();
                const std::size_t kept =
                    shown.display.size() >= kEllipsisBytes ? shown.display.size() - kEllipsisBytes : 0;
                if (shown.mnemonic_byte_offset != std::string::npos &&
                    shown.mnemonic_byte_offset + shown.mnemonic.size() > kept)
                    shown.mnemonic_byte_offset = std::string::npos;
            }
            const int label_end_x = label_x + text::text_width(shown.display);
            // A disabled row shows no mnemonic accent (D-076): its letter is
            // drawn like the rest of the label, in the disabled style, as the
            // status line draws an unavailable item. An accent there would
            // advertise a key the row does not answer.
            draw_mnemonic(painter, Point{label_x, item_top + i}, shown, label_columns, style,
                          enabled ? accent_style(style, theme.resolve(hotkey_role_)) : style);
            // A clipped label has taken the room the right-hand column
            // wanted; drawing into it anyway would overprint the label's
            // last cells rather than say anything.
            if (right_width > 0 && right_x > label_end_x)
                painter.draw_text(Point{right_x, item_top + i}, hint ? std::string_view{*hint} : kSubmenuMarker,
                                  style);
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
        app_->commands().set_handler(app_->commands().standard().menu, nullptr);
}

void MenuBar::set_menus(std::vector<MenuBarItem> menus) {
    close_dropdown();
    // The accelerators are a function of the titles alone -- each one's
    // mnemonic and its position -- so a bar rebuilt around the same titles
    // (an application refreshing item marks or enablement) keeps the ones it
    // has. Withdrawing and re-declaring identical commands would change the
    // registry, and every surface showing commands would repaint for nothing.
    const bool same_titles = std::equal(menus_.begin(), menus_.end(), menus.begin(), menus.end(),
                                        [](const MenuBarItem& a, const MenuBarItem& b) { return a.label == b.label; });
    if (!same_titles) remove_menu_accelerators();
    menus_ = std::move(menus);
    if (!same_titles) install_menu_accelerators();
    layout_trailing_view();
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
    // the class comment for why this only installs itself when the
    // standard menu command is still unclaimed.
    if (app_ != nullptr && !app_->commands().has_handler(app_->commands().standard().menu)) {
        app_->commands().set_handler(app_->commands().standard().menu, [this] { activate(); });
        installed_default_menu_handler_ = true;
    }
    // Menus set before the bar was attached have no accelerators yet: the
    // registry only becomes reachable here.
    install_menu_accelerators();
}

// Each top-level menu's Alt+<mnemonic> accelerator is an ordinary
// command, so it works from anywhere the command keymap reaches rather
// than only while the bar already holds focus — which is the whole point
// of a menu accelerator.
void MenuBar::install_menu_accelerators() {
    if (app_ == nullptr || !menu_accelerators_.empty()) return;
    for (std::size_t index = 0; index < menus_.size(); ++index) {
        const auto parsed = parse_mnemonic(menus_[index].label);
        if (parsed.mnemonic.empty()) continue;
        KeyChord chord;
        chord.key = Key::Char;
        chord.modifiers = Modifier::Alt;
        // Mnemonics are matched case-insensitively; the chord carries the
        // lowercase spelling the decoder produces.
        chord.text = parsed.mnemonic;
        if (chord.text.size() == 1)
            chord.text[0] = ascii_lower(chord.text[0]);
        // The accelerator's identity is the chord it exists to carry, so
        // that is what it declares itself under: a bar rebuilding its
        // menus re-declares the same key for the same Alt+<mnemonic> and
        // keeps the id every surface already holds, while a menu that
        // disappears takes its own accelerator with it. Hidden, because
        // it duplicates a menu the reader can already see.
        const std::string key = "ckv.menu-bar.accelerator." + chord.text;
        const ui::CommandId id = app_->commands().declare(
            ui::CommandDescriptor{.key = key,
                                  .title = parsed.display,
                                  .category = "Menu",
                                  .visibility = ui::CommandVisibility::Hidden});
        app_->commands().set_handler(id, [this, index] {
            activate();
            open_menu(index);
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
        app_->commands().withdraw(id);
    }
    menu_accelerators_.clear();
}

void MenuBar::activate() {
    // Opening the bar starts at its first menu, the way a reader who just
    // pressed F10 expects — a selection carried over from the last visit
    // would make the following `A`, `C` land somewhere they never looked.
    highlighted_ = 0;
    if (has_focus()) {
        close_dropdown();
        invalidate();
        return;
    }
    invocation_contexts_ = ui::command_context_path(app_->focused());
    previously_focused_ = app_->save_focus();
    app_->set_focus(this);  // on_focus(true) below paints the walk
}

void MenuBar::deactivate() {
    if (!has_focus()) return;
    if (MenuBarAccessory* const accessory = trailing_accessory()) accessory->set_menu_highlighted(false);
    close_dropdown();
    const std::optional<ui::Application::FocusBookmark> restore = previously_focused_;
    previously_focused_.reset();
    if (restore) app_->restore_focus(*restore);
    else app_->set_focus(nullptr);
    invocation_contexts_.clear();
}

void MenuBar::on_focus(const FocusEvent& event) {
    if (!event.gained) {
        // The walk is over however focus left -- Escape, a chosen item, or
        // another view taking it, such as a calendar dropped from the
        // trailing title. Nothing of it may stay behind: not the bar's
        // highlight, not the trailing title's, and not the bookmark, which
        // would otherwise send a later deactivate() back to a view the
        // reader left long ago.
        highlighted_ = 0;
        close_dropdown();
        previously_focused_.reset();
        invocation_contexts_.clear();
    }
    sync_trailing_highlight();
    invalidate();
}

void MenuBar::open_dropdown(std::size_t menu_index, MenuOpenReason reason) {
    CKV_ASSERT(menu_index < menus_.size());
    close_dropdown();
    highlighted_ = menu_index;
    menus_follow_walk_ = true;
    sync_trailing_highlight();

    // A hidden menu has no title of its own to hang from; the overflow title
    // stands for it, and what drops is the list of every hidden menu.
    const std::size_t visible = visible_menu_count();
    const bool overflow = menu_index >= visible;
    auto dropdown = std::make_unique<DropdownMenu>(overflow ? overflow_items() : menus_[menu_index].items);
    dropdown->set_invocation_contexts(invocation_contexts_);
    // Before add_popup(): on_attached() decides the initial selection from it.
    dropdown->set_open_reason(reason);
    const Rect abs = absolute_bounds();
    const Rect desktop_abs = desktop_->absolute_bounds();
    // The popup's FRAME hangs one cell left of its title, which puts the
    // item text one cell right of the title's first letter. Aligning the
    // frame with the title instead pushes every item two cells right and
    // the popup visibly fails to hang under the menu it belongs to.
    const int title_x = overflow ? overflow_title_x() : menu_bar_offset_for(menus_, menu_index);
    const int local_x = abs.x + title_x - 1 - desktop_abs.x;
    const int local_y = abs.y + 1 - desktop_abs.y;

    auto* raw = desktop_->add_popup(std::move(dropdown));
    const SizeHint w = raw->horizontal_size_hint();
    const SizeHint h = raw->vertical_size_hint();
    raw->set_bounds(clamp_popup_to_desktop(Rect{local_x, local_y, w.preferred, h.preferred}, desktop_->bounds()));
    // The keyboard reached the overflow title for one particular hidden menu
    // (a mnemonic, a resize that hid the open one), so the list opens on it.
    if (overflow && reason == MenuOpenReason::Keyboard) raw->set_highlighted(static_cast<int>(menu_index - visible));
    raw->on_dismiss = [this](MenuDismissReason dismiss_reason) {
        // A chosen item and a press outside every menu both end the menu
        // interaction: the bar deactivates and the focus goes back to where
        // it was. Esc only closes the popup and leaves the bar where the
        // reader left it, one level at a time.
        if (dismiss_reason == MenuDismissReason::ItemChosen || dismiss_reason == MenuDismissReason::Outside)
            deactivate();
        else
            close_dropdown();
    };
    raw->set_pointer_navigation([this](const MouseEvent& event) { return navigate_pointer(event); });
    // A menu opens and closes many times over a session; the application
    // wires the bar once and hears about every highlight through it,
    // rather than re-attaching to each popup as it appears.
    raw->on_highlight_changed = [this](const MenuHighlight& highlight) {
        if (on_highlight_changed) on_highlight_changed(highlight);
    };
    // The popup settles on its first selectable entry while attaching,
    // which is before the line above could hear it. Report that opening
    // position explicitly so a listener starts out describing the entry
    // the reader is actually looking at.
    if (on_highlight_changed) on_highlight_changed(raw->highlight());

    open_dropdown_ = raw;
    open_dropdown_is_overflow_ = overflow;
    app_->set_input_capture(raw);
    invalidate();
}

void MenuBar::open_menu(std::size_t menu_index) {
    open_dropdown(menu_index);
    // A hidden menu is a submenu of the overflow list, and asking for the
    // menu is asking to be inside it, as its title's mnemonic does for a
    // drawn one.
    if (open_dropdown_ != nullptr && open_dropdown_is_overflow_)
        open_dropdown_->open_submenu(static_cast<int>(menu_index - visible_menu_count()));
}

std::vector<MenuItem> MenuBar::overflow_items() const {
    std::vector<MenuItem> rows;
    for (std::size_t i = visible_menu_count(); i < menus_.size(); ++i)
        rows.push_back(MenuItem::submenu(menus_[i].label, menus_[i].items));
    return rows;
}

std::size_t MenuBar::visible_menu_count() const {
    const std::size_t count = menus_.size();
    if (count == 0) return 0;
    const int width = bounds().width;
    // Every title fits in full: nothing is hidden and no overflow title is
    // drawn.
    if (menu_bar_offset_for(menus_, count - 1) + menu_title_width(menus_[count - 1]) <= width) return count;
    // Otherwise as many as fit in full with the overflow title after them. A
    // title is drawn whole or not at all: a clipped one would hide the rest
    // of its name and, with it, perhaps the mnemonic that reaches it.
    const int overflow_width = text::text_width(kOverflowTitle);
    std::size_t visible = 0;
    while (visible + 1 < count && menu_bar_offset_for(menus_, visible + 1) + overflow_width <= width) ++visible;
    return visible;
}

int MenuBar::overflow_title_x() const { return menu_bar_offset_for(menus_, visible_menu_count()); }

int MenuBar::titles_end() const {
    if (menus_.empty()) return 0;
    if (overflowing()) return overflow_title_x() + text::text_width(kOverflowTitle);
    return menu_bar_offset_for(menus_, menus_.size() - 1) + menu_title_width(menus_.back());
}

std::optional<std::size_t> MenuBar::menu_at_column(int local_x) const {
    // A title answers for one cell either side of its text, the padding its
    // highlight covers.
    const auto covers = [local_x](int x, int width) { return local_x >= x - 1 && local_x < x + width + 1; };
    const std::size_t visible = visible_menu_count();
    for (std::size_t i = 0; i < visible; ++i)
        if (covers(menu_bar_offset_for(menus_, i), menu_title_width(menus_[i]))) return i;
    if (visible < menus_.size() && covers(overflow_title_x(), text::text_width(kOverflowTitle))) return visible;
    return std::nullopt;
}

std::size_t MenuBar::walk_position(std::size_t index) const {
    const std::size_t visible = visible_menu_count();
    if (index < visible) return index;
    if (index < menus_.size()) return visible;  // the overflow title
    return visible + (overflowing() ? 1 : 0);  // the trailing title
}

std::size_t MenuBar::index_at_walk_position(std::size_t position) const {
    const std::size_t visible = visible_menu_count();
    if (position < visible) return position;
    if (position == visible && visible < menus_.size()) return visible;
    return menus_.size();
}

std::string MenuBar::highlighted_help_context() const {
    if (open_dropdown_ == nullptr) return {};
    return open_dropdown_->highlight().help_context;
}

void MenuBar::close_dropdown() {
    // Closing is the reader saying they are done with menus, unless the one
    // caller that means "hold this thought" says otherwise straight after.
    menus_follow_walk_ = false;
    open_dropdown_is_overflow_ = false;
    if (open_dropdown_ == nullptr) return;
    if (app_->input_capture() == open_dropdown_) app_->clear_input_capture();
    desktop_->remove_popup(open_dropdown_);
    open_dropdown_ = nullptr;
    // Nothing is highlighted once the popup is gone: a surface showing
    // the highlighted command's explanation has to stop showing it, or
    // it would outlive the menu that produced it.
    if (on_highlight_changed) {
        MenuHighlight cleared;
        cleared.none = true;
        on_highlight_changed(cleared);
    }
    invalidate();
}

SizeHint MenuBar::horizontal_size_hint() const { return SizeHint{0, 0, ui::kUnboundedExtent}; }
SizeHint MenuBar::vertical_size_hint() const { return SizeHint{1, 1, 1}; }

bool MenuBar::on_key(const KeyEvent& event) {
    if (!has_focus() || menus_.empty()) return false;
    // Keyboard focus remains on the bar while its popup owns mouse capture, so
    // the bar is the only route by which an open menu — or a submenu opened out
    // of one — can be reached from the keyboard at all. Deliver to the
    // INNERMOST open menu: that is where the reader's highlight is, and a key
    // spent on a shallower one would move a selection they are no longer
    // looking at. Without this a submenu could be opened and then not
    // operated: the next arrow went to its parent, which closed it.
    if (open_dropdown_ != nullptr) {
        DropdownMenu* const target = open_dropdown_->innermost_menu();
        const bool in_submenu = target != open_dropdown_;
        switch (event.chord.key) {
            case Key::Up:
            case Key::Down:
            case Key::Home:
            case Key::End:
            case Key::Enter:
            case Key::Char:
                return target->on_key(event);
            case Key::Right:
                // Right opens the submenu under the highlight. Only where the
                // highlighted item has none does the key fall through to the
                // bar and become "on to the next menu".
                if (target->on_key(event)) return true;
                break;
            case Key::Left:
                // Out of a submenu one level at a time, back to the item that
                // opened it — where the reader came from. At the top level it
                // stays bar-owned and steps to the previous menu.
                if (in_submenu && target->on_key(event)) return true;
                break;
            case Key::Escape:
                // Esc closes one level, whichever level the reader is on: the
                // innermost submenu, back to the row that opened it; else the
                // dropdown, back to its title with the walk still on the bar.
                // Leaving the bar altogether is the next Esc's business, so a
                // reader backing out never overshoots the menu they meant to
                // leave.
                if (in_submenu) return target->on_key(event);
                close_dropdown();
                return true;
            default:
                break;
        }
    }
    switch (event.chord.key) {
        case Key::Home:
        case Key::End: {
            // With no menu open the walk is over the bar itself, so the
            // ends are its first and last slot — the same promise the keys
            // make inside a dropdown, one level up.
            const std::size_t slots = navigable_slots();
            if (slots == 0) return true;
            set_bar_highlight(index_at_walk_position(event.chord.key == Key::Home ? 0 : slots - 1));
            return true;
        }
        case Key::Left:
        case Key::Right: {
            // The walk runs over the drawn titles, the overflow title and the
            // trailing title, so a clock at the right end and the menus that
            // did not fit are reached by walking to them rather than by
            // knowing they are there.
            const std::size_t slots = navigable_slots();
            const std::size_t step = event.chord.key == Key::Right ? 1 : slots - 1;
            set_bar_highlight(index_at_walk_position((walk_position(highlighted_) + step) % slots));
            return true;
        }
        case Key::Down:
        case Key::Enter:
            if (trailing_slot_highlighted()) {
                activate_trailing_accessory();
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
                activate_trailing_accessory();
                return true;
            }
            for (std::size_t i = 0; i < menus_.size(); ++i) {
                const auto parsed = parse_mnemonic(menus_[i].label);
                if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, event.chord.text)) {
                    open_menu(i);
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
    const std::optional<std::size_t> index = menu_at_column(event.cell.x - abs.x);
    if (!index) return false;
    if (!has_focus()) activate();
    open_dropdown(*index, MenuOpenReason::PointerPress);
    if (open_dropdown_ != nullptr) open_dropdown_->begin_pointer_press();
    return true;
}

void MenuBar::on_resized() {
    layout_trailing_view();
    // An open dropdown hangs from the title that holds its menu. A resize
    // that hides that title, or shows it again, moves the menu to the other
    // kind of title, and the reader stays in the same menu: a drawn menu that
    // becomes hidden reopens entered inside the overflow list, a hidden one
    // that comes into view reopens under its own title.
    if (open_dropdown_ == nullptr || open_dropdown_is_overflow_ == is_hidden_menu(highlighted_)) return;
    if (is_hidden_menu(highlighted_))
        open_menu(highlighted_);
    else
        open_dropdown(highlighted_);
}

void MenuBar::on_child_size_hint_changed(ui::View& child) {
    // The bar owns where its trailing view sits, so it is the bar that has to
    // answer when that view's own width changes -- a clock gaining seconds, an
    // indicator gaining a word. Anything else is a dropdown popup, which is
    // placed when it opens and is nobody's business here.
    if (&child == trailing_view_) layout_trailing_view();
}

void MenuBar::set_bar_highlight(std::size_t index) {
    highlighted_ = index;
    // One slot is highlighted, and what is open belongs to it. The bar owns
    // both facts, so it reconciles them here rather than leaving every caller
    // that moves the highlight to remember: walking from one menu to the next
    // carries the open menu along, and walking onto a title that has no menu
    // of its own -- the trailing view -- closes what was open. Left to the
    // callers, a dropdown stayed open beside a highlighted clock and kept the
    // keys, so Enter chose that menu's item instead of the one the reader
    // could see was selected.
    if (trailing_slot_highlighted()) {
        // Suspended, not cancelled: the trailing title has no menu of its
        // own, but the walk is still the one the reader started with a menu
        // open, so stepping back onto a menu shows it again.
        if (open_dropdown_ != nullptr) {
            close_dropdown();
            menus_follow_walk_ = true;
        }
    } else if (menus_follow_walk_) {
        open_dropdown(index);
    }
    sync_trailing_highlight();
    invalidate();
}

void MenuBar::activate_trailing_accessory() {
    // What drops out of a trailing title is the caller's; the bar only says
    // when -- and it says so AFTER ending the walk, the way choosing an item
    // ends it. Focus goes back to where the reader was before F10 first, so
    // whatever the title opens -- a calendar -- saves and later restores that
    // view, not a bar that would wake up highlighted when the calendar closed.
    MenuBarAccessory* const accessory = trailing_accessory();
    deactivate();
    accessory->activate_from_menu_bar();
}

void MenuBar::on_descendant_mouse_down(ui::View& target) {
    // A pointer on the trailing title while the bar is being walked ends the
    // walk for the same reason: the click is a hand-off, and the title's own
    // reaction to it must not find the bar still standing.
    if (!has_focus() || trailing_view_ == nullptr) return;
    for (const ui::View* view = &target; view != nullptr; view = view->parent())
        if (view == trailing_view_) {
            deactivate();
            return;
        }
}

void MenuBar::sync_trailing_highlight() {
    // The trailing title is a view, not a label the bar paints, so it has to
    // be told when the walk is standing on it -- and told just as promptly
    // when it is not, or it goes on wearing the active colours beside a menu
    // that is now the highlighted one.
    if (MenuBarAccessory* const accessory = trailing_accessory())
        accessory->set_menu_highlighted(has_focus() && trailing_slot_highlighted());
}

MenuBarAccessory* MenuBar::trailing_accessory() const noexcept {
    return dynamic_cast<MenuBarAccessory*>(trailing_view_);
}

std::size_t MenuBar::navigable_slots() const {
    return visible_menu_count() + (overflowing() ? 1 : 0) + (trailing_accessory() != nullptr ? 1 : 0);
}

bool MenuBar::trailing_slot_highlighted() const noexcept {
    return has_focus() && trailing_accessory() != nullptr && highlighted_ == menus_.size();
}

ui::View* MenuBar::set_trailing_view_impl(std::unique_ptr<ui::View> view) {
    // Whether the keyboard walk is standing on the title that is about to be
    // replaced or removed. Its slot exists only while it does.
    const bool walk_was_on_it = trailing_view_ != nullptr && highlighted_ == menus_.size();
    if (trailing_view_ != nullptr) {
        (void)remove_child(trailing_view_);
        trailing_view_ = nullptr;
    }
    if (view == nullptr) {
        // Nothing to walk onto now, so the highlight comes back to the last
        // menu rather than pointing one past the end -- from where Enter asked
        // for a dropdown that does not exist, which is an abort rather than a
        // misdrawn bar.
        if (walk_was_on_it && !menus_.empty()) highlighted_ = menus_.size() - 1;
        invalidate();
        return nullptr;
    }
    trailing_view_ = add_child(std::move(view));
    layout_trailing_view();
    // A replacement inherits the walk from the title it replaces: to the
    // reader the same thing is still at the right end of the bar.
    if (walk_was_on_it) sync_trailing_highlight();
    return trailing_view_;
}

void MenuBar::layout_trailing_view() {
    if (trailing_view_ == nullptr) return;
    // Its own preferred width, pinned to the right edge. Recomputed rather
    // than remembered: a clock that gains seconds is a cell or two wider,
    // and the right edge is wherever the bar ends now.
    //
    // The menus come first. The trailing view takes what is left after the
    // last title and one cell of gap, and no room at all when that is less
    // than it can be drawn in: a clock laid over a menu title hides the only
    // way into that menu.
    const ui::SizeHint hint = trailing_view_->horizontal_size_hint();
    const int start = menus_.empty() ? 0 : titles_end() + 1;
    const int available = std::max(0, bounds().width - start);
    int width = std::clamp(hint.preferred, 0, available);
    if (width < hint.min) width = 0;
    trailing_view_->set_bounds(Rect{bounds().width - width, 0, width, 1});
}

void MenuBar::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const Style base = theme.resolve(normal_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));
    // The highlight covers a title and one cell of padding either side.
    const auto lit_title = [&](int x, int width) {
        const Style active = theme.resolve(active_role_);
        if (x > 0)
            painter.fill(Rect{x - 1, 0, std::min(bounds().width - (x - 1), width + 2), 1},
                         Cell::from_grapheme(" ", active));
        return active;
    };
    const std::size_t visible = visible_menu_count();
    for (std::size_t i = 0; i < visible; ++i) {
        const int x = menu_bar_offset_for(menus_, i);
        const Style style =
            (has_focus() && i == highlighted_) ? lit_title(x, menu_title_width(menus_[i])) : base;
        draw_mnemonic(painter, Point{x, 0}, parse_mnemonic(menus_[i].label), bounds().width - x, style,
                      accent_style(style, theme.resolve(hotkey_role_)));
    }
    if (visible < menus_.size()) {
        const int x = overflow_title_x();
        const Style style = (has_focus() && is_hidden_menu(highlighted_))
                                ? lit_title(x, text::text_width(kOverflowTitle))
                                : base;
        painter.draw_text(Point{x, 0}, kOverflowTitle, style);
    }
}

bool MenuBar::navigate_pointer(const MouseEvent& event) {
    if (!has_focus() || menus_.empty() || event.cell.y != absolute_bounds().y) return false;
    const std::optional<std::size_t> index = menu_at_column(event.cell.x - absolute_bounds().x);
    if (!index) return false;
    // Every hidden menu is the overflow title, so moving between two of them
    // is not moving at all.
    if (open_dropdown_ == nullptr || walk_position(highlighted_) != walk_position(*index))
        open_dropdown(*index, MenuOpenReason::PointerPress);
    if (open_dropdown_ != nullptr) {
        if (event.action == MouseAction::Up)
            open_dropdown_->end_pointer_press();
        else
            open_dropdown_->begin_pointer_press();
    }
    return true;
}

// --- show_context_menu ---------------------------------------------------

namespace {

// Puts `menu` up as a self-removing popup on `desktop`, at the desktop-local
// rect `place` computes from the menu's own size, with the keyboard and input
// capture on it until it dismisses and `previous_focus` handed back then.
DropdownMenu* present_popup_menu(std::unique_ptr<DropdownMenu> menu,
                                 const ui::Application::FocusBookmark& previous_focus, ui::Application& app,
                                 Desktop& desktop, const std::function<Rect(Size)>& place) {
    auto* raw = desktop.add_popup(std::move(menu));
    const Size size{raw->horizontal_size_hint().preferred, raw->vertical_size_hint().preferred};
    raw->set_bounds(clamp_popup_to_desktop(place(size), desktop.bounds()));

    auto dismissed = std::make_shared<bool>(false);
    raw->on_dismiss = [&app, &desktop, raw, previous_focus, dismissed](MenuDismissReason) {
        const std::shared_ptr<bool> state = dismissed;
        if (std::exchange(*state, true)) return;
        const ui::Application::FocusBookmark restore = previous_focus;
        ui::Application* const application = &app;
        Desktop* const host = &desktop;
        if (application->input_capture() == raw) application->clear_input_capture();
        if (application->focused() == raw) application->set_focus(nullptr);
        // Nothing is highlighted once the menu is gone, and a listener showing
        // the row's explanation has to hear so, as a bar's listener does.
        if (const std::function<void(const MenuHighlight&)> listener = raw->on_highlight_changed) {
            MenuHighlight closed;
            closed.none = true;
            listener(closed);
        }
        host->remove_popup(raw);  // discards ownership -> destroys the DropdownMenu
        application->restore_focus(restore);
    };
    app.set_input_capture(raw);
    app.set_focus(raw);
    return raw;
}

}  // namespace

DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                 ui::Application& app, Desktop& desktop) {
    const ui::Application::FocusBookmark previous_focus = app.save_focus();
    auto menu = std::make_unique<DropdownMenu>(std::move(items));
    menu->set_invocation_contexts(ui::command_context_path(app.focused()));
    const Rect desktop_abs = desktop.absolute_bounds();
    return present_popup_menu(std::move(menu), previous_focus, app, desktop, [&](Size size) {
        return Rect{screen_position.x - desktop_abs.x, screen_position.y - desktop_abs.y, size.width, size.height};
    });
}

DropdownMenu* show_anchored_menu(std::vector<MenuItem> items, Rect anchor, ui::Application& app,
                                 Desktop& desktop) {
    const ui::Application::FocusBookmark previous_focus = app.save_focus();
    auto menu = std::make_unique<DropdownMenu>(std::move(items));
    menu->set_invocation_contexts(ui::command_context_path(app.focused()));
    const Rect desktop_abs = desktop.absolute_bounds();
    return present_popup_menu(std::move(menu), previous_focus, app, desktop, [&](Size size) {
        const int left = anchor.x - desktop_abs.x;
        const int below = anchor.y - desktop_abs.y + anchor.height;
        const int above = anchor.y - desktop_abs.y - size.height;
        // Below the anchor while the menu fits beneath it; otherwise above it
        // when it fits there -- a bar docked at the bottom drops its menus
        // upward -- and failing both, below, where the clamp keeps it on the
        // desktop.
        const bool fits_below = below + size.height <= desktop.bounds().height;
        const int top = fits_below || above < 0 ? below : above;
        return Rect{left, top, size.width, size.height};
    });
}

bool is_keyboard_context_menu_request(const KeyEvent& event) noexcept {
    if (event.action != KeyAction::Press) return false;
    return (event.chord.key == Key::Menu && event.chord.modifiers == Modifier::None) ||
           (event.chord.key == Key::F10 && event.chord.modifiers == Modifier::Shift);
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
