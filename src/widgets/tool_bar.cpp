// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tool_bar.hpp"
#include "cvision/widgets/command_palette.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/core/ascii.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/dialog_presentation.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace ckv::widgets {
using detail::is_press;
using detail::stepped;
using detail::enclosing_desktop;
using detail::tail_to_width;
using detail::without_last_grapheme;

namespace {
// A ToolBar entry naming a command no registry has declared — or one
// drawn before the bar is attached to an Application at all — is an
// application defect. Draw a placeholder the reader can report rather
// than an empty button that looks deliberate. There is nothing more
// informative to show: a CommandId is a handle its own registry
// assigned, so an id that registry does not know has no title, no key,
// and no meaning anywhere else.
std::string command_title(ui::Application* app, ui::CommandId id) {
    if (app == nullptr) return "(unknown)";
    if (const ui::CommandInfo* info = app->commands().find(id); info != nullptr) return info->title;
    return "(unknown)";
}
}  // namespace

namespace {
// The control whose menu holds the buttons that do not fit.
// A toggle's mark column, as a menu row draws it: the mark and a blank.
constexpr std::string_view kCheckedMark = "x ";
constexpr std::string_view kUncheckedMark = "  ";
}  // namespace

ToolBar::ToolBar() { set_focus_policy(ui::FocusPolicy::TabStop); }

void ToolBar::set_items(std::vector<CommandPresentation> items) {
    cancel_press();
    group_starts_.clear();
    hover_position_.reset();
    items_ = std::move(items);
    preparation_dirty_ = true;
    focused_ = 0;
    overflow_focused_ = false;
    pressed_slot_.reset();
    invalidate();
    size_hint_changed();
}

void ToolBar::set_groups(std::vector<std::vector<CommandPresentation>> groups) {
    std::vector<CommandPresentation> items;
    std::vector<std::size_t> starts;
    for (auto& group : groups) {
        if (group.empty()) continue;
        if (!items.empty()) starts.push_back(items.size());
        for (auto& item : group) items.push_back(std::move(item));
    }
    set_items(std::move(items));
    group_starts_ = std::move(starts);
    size_hint_changed();
    invalidate();
}

void ToolBar::set_presentation(ToolBarPresentation presentation) {
    if (presentation_ == presentation) return;
    cancel_press();
    hover_position_.reset();
    presentation_ = presentation;
    preparation_dirty_ = true;
    size_hint_changed();
    invalidate();
}

int ToolBar::presentation_height() const { return presentation_ == ToolBarPresentation::Framed ? 3 : 1; }
int ToolBar::caption_inset() const { return presentation_ == ToolBarPresentation::Compact ? 1 : 2; }
int ToolBar::control_width() const { return presentation_ == ToolBarPresentation::Compact ? 3 : 5; }

void ToolBar::cancel_press() {
    pressed_slot_.reset();
    pressed_item_.reset();
    pressed_visible_ = false;
    armed_key_.reset();
    const bool owned = std::exchange(press_owns_walk_, false);
    if (owned) end_walk();
    invalidate();
}

void ToolBar::on_resized() { cancel_press(); hover_position_.reset(); }
void ToolBar::on_detaching() { cancel_press(); walk_.reset(); hover_position_.reset(); }
void ToolBar::on_hover_changed(bool now_hovered) {
    if (!now_hovered) hover_position_.reset();
    invalidate();
}

void ToolBar::set_show_chords(bool show) {
    if (show_chords_ == show) return;
    cancel_press();
    hover_position_.reset();
    show_chords_ = show;
    preparation_dirty_ = true;
    invalidate();
    size_hint_changed();
}

void ToolBar::prepare() const {
    const auto* app = context().app;
    const std::uint64_t revision = app ? app->commands().revision() : 0;
    bool rebuild = preparation_dirty_ || prepared_revision_ != revision || prepared_app_ != app;
    if (!rebuild) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const bool toggle = app && app->commands().checked(items_[i].command).has_value();
            if (toggle != prepared_[i].toggle) { rebuild = true; break; }
        }
    }
    if (!rebuild) return;
    prepared_.resize(items_.size());
    layout_.slots.reserve(items_.size() + 1);
    layout_.overflow.reserve(items_.size());
    layout_.separators.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        auto& prepared = prepared_[i];
        prepared.label = parse_mnemonic(item.label.empty() ? command_title(context().app, item.command) : item.label);
        prepared.toggle = app && app->commands().checked(item.command).has_value();
        std::string shortcut = item.chord;
        if (show_chords_ && shortcut.empty() && app) shortcut = app->commands().chord_text(item.command);
        const auto make_face = [&](bool checked) {
            std::string text(static_cast<std::size_t>(caption_inset()), ' ');
            if (prepared.toggle) text += checked ? kCheckedMark : kUncheckedMark;
            text += prepared.label.display;
            if (show_chords_ && !shortcut.empty()) { text += " "; text += shortcut; }
            text.append(static_cast<std::size_t>(caption_inset()), ' ');
            return text;
        };
        prepared.unchecked_face = make_face(false);
        prepared.checked_face = make_face(true);
        prepared.width = text::text_width(prepared.unchecked_face);
    }
    prepared_revision_ = revision;
    prepared_app_ = app;
    preparation_dirty_ = false;
}

const MnemonicText& ToolBar::label(std::size_t item) const {
    return prepared_[item].label;
}

std::string_view ToolBar::face(std::size_t item) const {
    const bool checked = context().app && context().app->commands().checked(items_[item].command).value_or(false);
    return checked ? prepared_[item].checked_face : prepared_[item].unchecked_face;
}

bool ToolBar::available(std::size_t item) const {
    ui::Application* const app = context().app;
    if (app == nullptr || app->commands().find(items_[item].command) == nullptr) return false;
    // Judged for the place the reader is working in: the focus a walk came
    // from while there is one, and wherever the focus is otherwise.
    if (walk_) return app->commands().is_available(items_[item].command, walk_->contexts);
    return app->command_available(items_[item].command);
}

const ToolBar::Layout& ToolBar::layout() const {
    prepare();
    Layout& bar = layout_;
    bar.slots.clear();
    bar.overflow.clear();
    bar.separators.clear();
    const int width = std::max(0, bounds().width);
    if (width == 0 || bounds().height <= 0) return bar;
    int whole = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i > 0) whole += std::binary_search(group_starts_.begin(), group_starts_.end(), i) ? 3 : 1;
        whole += prepared_[i].width;
    }
    const bool overflows = whole > width;
    const int control = control_width();
    const int limit = overflows ? width - control - 1 : width;
    int x = 0;
    std::size_t index = 0;
    for (; index < items_.size(); ++index) {
        const bool group = std::binary_search(group_starts_.begin(), group_starts_.end(), index);
        const int gap = index == 0 ? 0 : (group ? 3 : 1);
        if (x + gap + prepared_[index].width > limit) break;
        if (group) bar.separators.push_back(x + 1);
        x += gap;
        bar.slots.push_back(Slot{index, x, prepared_[index].width});
        x += prepared_[index].width;
    }
    for (std::size_t i = index; i < items_.size(); ++i) bar.overflow.push_back(i);
    if (overflows) bar.slots.push_back(Slot{std::nullopt, std::max(0, width - control), control});
    return bar;
}

std::vector<std::size_t> ToolBar::shown_items() const {
    std::vector<std::size_t> shown;
    for (const Slot& slot : layout().slots)
        if (slot.item) shown.push_back(*slot.item);
    return shown;
}

std::vector<std::size_t> ToolBar::overflow_items() const { return layout().overflow; }

std::size_t ToolBar::focused_slot(const Layout& bar) const {
    std::optional<std::size_t> control;
    for (std::size_t i = 0; i < bar.slots.size(); ++i) {
        if (!bar.slots[i].item) control = i;
        else if (!overflow_focused_ && *bar.slots[i].item == focused_) return i;
    }
    // On the overflow control, or on a button that has overflowed into it.
    if (control) return *control;
    // On a control a wider bar no longer draws: the first button it held.
    for (std::size_t i = 0; i < bar.slots.size(); ++i)
        if (bar.slots[i].item && *bar.slots[i].item >= focused_) return i;
    return 0;
}

std::optional<std::size_t> ToolBar::focused_item() const {
    const Layout& bar = layout();
    if (bar.slots.empty()) return std::nullopt;
    return bar.slots[focused_slot(bar)].item;
}

void ToolBar::focus_slot(const Layout& bar, std::size_t slot) {
    if (const std::optional<std::size_t> item = bar.slots[slot].item) {
        focused_ = *item;
        overflow_focused_ = false;
    } else {
        overflow_focused_ = true;
    }
    invalidate();
}

int ToolBar::slot_at(const Layout& bar, Point local) const {
    if (!Rect{0, 0, bounds().width, std::min(bounds().height, presentation_height())}.contains(local)) return -1;
    for (std::size_t i = 0; i < bar.slots.size(); ++i)
        if (local.x >= bar.slots[i].x && local.x < bar.slots[i].x + bar.slots[i].width) return static_cast<int>(i);
    return -1;
}

bool ToolBar::slot_available(const Slot& slot) const {
    if (!enabled_in_tree()) return false;
    if (slot.item) return available(*slot.item);
    for (const ui::View* view = parent(); view != nullptr; view = view->parent())
        if (dynamic_cast<const Desktop*>(view) != nullptr) return true;
    return false;
}

std::optional<PointerShape> ToolBar::pointer_shape_at(Point local) const {
    const Layout& bar = layout();
    const int hit = slot_at(bar, local);
    if (hit < 0 || !slot_available(bar.slots[static_cast<std::size_t>(hit)])) return std::nullopt;
    return PointerShape::Pointer;
}

void ToolBar::begin_walk() {
    ui::Application* const app = context().app;
    walk_ = Walk{app->save_focus(), ui::command_context_path(app->focused())};
    app->set_focus(this);
}

std::optional<std::vector<std::string>> ToolBar::end_walk() {
    if (!walk_) return std::nullopt;
    Walk walk = std::move(*walk_);
    walk_.reset();
    invalidate();
    // After forgetting it: handing the focus back is a focus change this bar
    // hears, and a walk still held would be ended twice.
    context().app->restore_focus(walk.focus);
    return std::move(walk.contexts);
}

void ToolBar::activate() {
    cancel_press();
    ui::Application* const app = context().app;
    CKV_ASSERT(app != nullptr);
    focused_ = 0;
    overflow_focused_ = false;
    invalidate();
    if (has_focus()) return;
    begin_walk();
}

void ToolBar::deactivate() { end_walk(); }

bool ToolBar::run_item(std::size_t item) {
    if (!available(item)) return false;
    const ui::CommandId command = items_[item].command;
    ui::Application* const app = context().app;
    // The reader has chosen, and is done with the bar: the focus goes back
    // first, so whatever the command does sees where the reader works.
    if (std::optional<std::vector<std::string>> contexts = end_walk())
        return app->commands().execute(command, *contexts);
    return app->execute_command(command);
}

void ToolBar::open_overflow(const Layout& bar) {
    if (bar.overflow.empty() || context().app == nullptr) return;
    Desktop* const desktop = enclosing_desktop(*this);
    if (desktop == nullptr) return;
    ui::Application& app = *context().app;
    std::vector<MenuItem> items;
    for (const std::size_t item : bar.overflow) {
        if (!items.empty() && std::binary_search(group_starts_.begin(), group_starts_.end(), item))
            items.push_back(MenuItem::separator());
        items.push_back(MenuItem::command(items_[item]));
    }
    // The walk ends before the menu opens, so the menu takes the focus from
    // where the reader works and judges and runs its commands for it.
    end_walk();
    const Rect abs = absolute_bounds();
    const Slot& control = bar.slots.back();
    show_anchored_menu(std::move(items), Rect{abs.x + control.x, abs.y, std::min(control.width, abs.width), std::min(abs.height, presentation_height())}, app, *desktop);
}

bool ToolBar::activate_slot(const Layout& bar, std::size_t slot) {
    if (slot >= bar.slots.size() || !slot_available(bar.slots[slot])) return false;
    if (const std::optional<std::size_t> item = bar.slots[slot].item) return run_item(*item);
    open_overflow(bar);
    return true;
}

void ToolBar::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const int height = std::min(bounds().height, presentation_height());
    if (bounds().width <= 0 || height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, bounds().width, height});
    const Style normal = theme.resolve(role_);
    clipped.fill(Rect{0, 0, bounds().width, height}, Cell::from_grapheme(" ", normal));
    const Layout& bar = layout();
    const int hovered_slot = hover_position_ ? slot_at(bar, *hover_position_) : -1;
    for (const int x : bar.separators)
        clipped.draw_text(Point{x, presentation_height() / 2}, "│", theme.resolve(separator_role_));
    const std::size_t focused_index = enabled_in_tree() && has_focus() && !bar.slots.empty()
        ? focused_slot(bar) : bar.slots.size();
    for (std::size_t i = 0; i < bar.slots.size(); ++i) {
        const Slot& slot = bar.slots[i];
        const bool acts = slot_available(slot);
        const bool pressed = acts && pressed_slot_ == i && pressed_visible_ && pressed_item_ == slot.item;
        const bool focused = acts && focused_index == i;
        const bool checked = slot.item && context().app != nullptr &&
            context().app->commands().checked(items_[*slot.item].command).value_or(false);
        const ui::RoleId role = !acts ? disabled_role_ : pressed ? pressed_role_ : focused ? focused_role_ :
            hovered_slot == static_cast<int>(i) ? hovered_role_ : checked ? checked_role_ : role_;
        const Style style = theme.resolve(role);
        const int y = presentation_height() / 2;
        std::string_view whole = slot.item ? face(*slot.item) : (caption_inset() == 1 ? " » " : "  »  ");
        if (presentation_ == ToolBarPresentation::Framed) {
            clipped.fill(Rect{slot.x, 0, slot.width, 3}, Cell::from_grapheme(" ", style));
            clipped.draw_box(Rect{slot.x, 0, slot.width, 3}, scene::LineStyle::Single, style);
            clipped.draw_text(Point{slot.x + 1, y}, whole.substr(1, whole.size() - 2), style);
        } else {
            clipped.draw_text(Point{slot.x, y}, whole, style);
        }
        if (!slot.item) continue;
        const MnemonicText& parsed = label(*slot.item);
        const bool toggle = context().app != nullptr &&
            context().app->commands().checked(items_[*slot.item].command).has_value();
        const int label_x = slot.x + caption_inset() + (toggle ? text::text_width(kCheckedMark) : 0);
        Style caption = style;
        if (focused) caption.attrs |= Attr::Underline;
        const Style accent = acts ? accent_style(caption, theme.resolve(hotkey_role_)) : caption;
        draw_mnemonic(clipped, Point{label_x, y}, parsed, std::max(0, slot.x + slot.width - caption_inset() - label_x), caption, accent);
    }
}

bool ToolBar::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || (!is_press(event) && event.action != KeyAction::Repeat)) return false;
    const KeyChord& chord = event.chord;
    if (armed_key_) {
        if (chord.key == Key::Escape) {
            cancel_press();
            end_walk();
            return true;
        }
        if (chord == *armed_key_) return true;
        cancel_press();
    }
    if (chord.key == Key::Escape && is_press(event) && walk_) {
        end_walk();
        return true;
    }
    const Layout& bar = layout();
    if (bar.slots.empty()) return false;
    const std::size_t focus = focused_slot(bar);
    const std::size_t count = bar.slots.size();
    if (chord.key == Key::Left) { focus_slot(bar, (focus + count - 1) % count); return true; }
    if (chord.key == Key::Right) { focus_slot(bar, (focus + 1) % count); return true; }
    if (chord.key == Key::Home) { focus_slot(bar, 0); return true; }
    if (chord.key == Key::End) { focus_slot(bar, count - 1); return true; }
    if (!is_press(event)) return false;
    if (chord.key == Key::Enter || (chord.key == Key::Char && chord.text == " " && chord.modifiers == Modifier::None)) {
        if (event.reports_release && slot_available(bar.slots[focus])) {
            armed_key_ = chord;
            pressed_slot_ = focus;
            pressed_item_ = bar.slots[focus].item;
            pressed_visible_ = true;
            invalidate();
        } else activate_slot(bar, focus);
        return true;
    }
    // A mnemonic letter runs its button, on the bar or in the overflow menu,
    // typed alone or with Alt as a menu's is.
    if (chord.key == Key::Char && !chord.text.empty() && !has_modifier(chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(chord.modifiers, Modifier::Super)) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            const MnemonicText& parsed = label(i);
            if (!parsed.mnemonic.empty() && ascii_iequals(parsed.mnemonic, chord.text)) {
                focused_ = i;
                overflow_focused_ = false;
                invalidate();
                run_item(i);
                return true;
            }
        }
    }
    return false;
}

bool ToolBar::on_key_release(const KeyEvent& event) {
    if (!armed_key_ || event.chord != *armed_key_) return false;
    const Layout& bar = layout();
    const auto slot = pressed_slot_;
    const auto item = pressed_item_;
    cancel_press();
    if (slot && *slot < bar.slots.size() && bar.slots[*slot].item == item) activate_slot(bar, *slot);
    return true;
}

bool ToolBar::on_mouse(const MouseEvent& event) {
    const Layout& bar = layout();
    const Rect abs = absolute_bounds();
    const int hit = slot_at(bar, Point{event.cell.x - abs.x, event.cell.y - abs.y});
    if (event.action == MouseAction::Move) {
        const int previous = hover_position_ ? slot_at(bar, *hover_position_) : -1;
        hover_position_ = Point{event.cell.x - abs.x, event.cell.y - abs.y};
        if (previous != hit) invalidate();
    }
    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || hit < 0 || context().app == nullptr ||
            !slot_available(bar.slots[static_cast<std::size_t>(hit)])) return false;
        cancel_press();
        // A press on a bar that did not have the keyboard borrows it for as
        // long as the button is down, so it can hand it back.
        press_owns_walk_ = !has_focus();
        if (press_owns_walk_) begin_walk();
        const auto slot = static_cast<std::size_t>(hit);
        focus_slot(bar, slot);
        pressed_slot_ = slot;
        pressed_item_ = bar.slots[slot].item;
        pressed_visible_ = true;
        return true;
    }
    if (!pressed_slot_) return false;
    if (armed_key_) return false;
    const bool over = hit >= 0 && static_cast<std::size_t>(hit) == *pressed_slot_ &&
        bar.slots[static_cast<std::size_t>(hit)].item == pressed_item_;
    if (event.action == MouseAction::Move) {
        if (over != pressed_visible_) {
            pressed_visible_ = over;
            invalidate();
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        const std::size_t slot = *pressed_slot_;
        const bool own_release = event.button == MouseButton::Left || event.button == MouseButton::None;
        pressed_slot_.reset();
        pressed_item_.reset();
        pressed_visible_ = false;
        invalidate();
        // Taken back, or refused: a borrowed keyboard goes home with nothing
        // done, and a walk the reader began stays where it was.
        const bool owned = std::exchange(press_owns_walk_, false);
        if ((!over || !own_release || !activate_slot(bar, slot)) && owned) end_walk();
        return true;
    }
    return false;
}

void ToolBar::on_focus(const FocusEvent& event) {
    // However the keyboard left, the walk is over, and the focus it would
    // have handed back is not this bar's to hand back any more.
    if (!event.gained) {
        pressed_slot_.reset(); pressed_item_.reset(); pressed_visible_ = false; armed_key_.reset();
        press_owns_walk_ = false; walk_.reset();
    }
    invalidate();
}

ui::SizeHint ToolBar::horizontal_size_hint() const {
    prepare();
    int whole = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i > 0) whole += std::binary_search(group_starts_.begin(), group_starts_.end(), i) ? 3 : 1;
        whole += prepared_[i].width;
    }
    const int control = control_width();
    return ui::SizeHint{std::min(control, std::max(0, whole)), whole, ui::kUnboundedExtent};
}

ui::SizeHint ToolBar::vertical_size_hint() const {
    const int height = presentation_height();
    return ui::SizeHint{height, height, height};
}

void ToolBar::on_attached() {
    preparation_dirty_ = true;
    role_ = context().roles->find("ckv.toolbar.normal");
    focused_role_ = context().roles->find("ckv.toolbar.focused");
    hovered_role_ = context().roles->find("ckv.toolbar.hovered");
    pressed_role_ = context().roles->find("ckv.toolbar.pressed");
    checked_role_ = context().roles->find("ckv.toolbar.checked");
    disabled_role_ = context().roles->find("ckv.toolbar.disabled");
    separator_role_ = context().roles->find("ckv.toolbar.separator");
    hotkey_role_ = context().roles->find("ckv.toolbar.mnemonic");
}


}  // namespace ckv::widgets
