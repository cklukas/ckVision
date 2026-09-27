// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// StatusLine: context-sensitive hints bound to focus, clickable
// command items (the widget catalog M5 baseline). The hint mechanism
// mirrors D-027's F1 routing exactly (the architecture §5: "the status
// line's context-sensitive hints key off the same mechanism"):
// resolves the focused view's nearest help-context key and hands it to
// an injected provider, which returns the hint text to display — the
// library defines no help content format, same as F1's provider.
//
// The item set can follow focus too: a command context may carry its own
// items, which replace the ordinary ones while focus is inside it.
//
// An item referencing a command (M9/WP-11) stops carrying its own
// label text: it renders "{chord} {title}" composed live from
// CommandRegistry (e.g. "Alt+X Quit"), chord omitted if nothing is
// currently bound. A hand-labeled item (command == kInvalidCommand)
// still renders its own `label` verbatim, unparsed — for the item
// list, not the mnemonic-aware navigation menus use, since nothing
// here jumps focus by letter.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/command_presentation.hpp"

namespace ckv::widgets {

using ui::SizeHint;

// One entry of a status line: a hand-labeled item, or a command the line
// labels and runs.
struct StatusLineItem {
    // An empty, command-less item.
    StatusLineItem() = default;
    // A hand-set `item_label`, optionally running `command_id` when clicked.
    // With a valid command the label is replaced by the command's current
    // chord and registered title (e.g. "Alt+X Quit") whenever the command is
    // registered.
    StatusLineItem(std::string item_label, ui::CommandId command_id = ui::kInvalidCommand,
                   int item_priority = 0)
        : label(std::move(item_label)), command(command_id), priority(item_priority) {}
    // A command presented with this surface's own wording or chord spelling
    // (see CommandPresentation); an empty presentation label uses the
    // registered title.
    explicit StatusLineItem(CommandPresentation command_presentation, int item_priority = 0)
        : priority(item_priority), presentation(std::move(command_presentation)) {}

    // Text drawn verbatim (no '&' parsing) for an item with no command, and
    // the fallback for one whose command is not registered.
    std::string label;
    // The command a click runs; ignored when presentation.command is set.
    ui::CommandId command = ui::kInvalidCommand;
    // Which items survive on a line too narrow for all of them. When every
    // item has the same priority, items are kept left to right and the last
    // one that starts on screen is clipped; otherwise the lowest-priority item
    // (the rightmost among equals) is dropped until the rest fit.
    int priority = 0;  // higher priority survives first on narrow status lines
    // The command presentation; its command, when valid, takes precedence
    // over `command`.
    CommandPresentation presentation;
};

// A one-row strip, usually docked to the bottom of a Desktop: the command
// items on the left, then a divider and the hint for where the reader is. A
// click on an item runs its command on release over the same item, and only
// while that command is available; an unavailable command's item is drawn
// in the disabled role.
//
// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.statusline.normal", "ckv.statusline.disabled",
// "ckv.statusline.selected" (a pressed item) with its ".hotkey" and
// ".disabled" variants, and "ckv.hotkey". Also reads context().app for
// the focused view's help-context key (current_hint()), for command titles,
// chords and availability, and to execute an item's command (on_mouse()), so
// it must be attached under an Application before it draws.
class StatusLine : public ui::View {
public:
    // No items and no hint provider; one row tall, any width.
    StatusLine();

    // Replace the role the strip and available items are drawn with, in place
    // of "ckv.statusline.normal".
    void set_role_override(ui::RoleId role) noexcept {
        if (role_ == role) return;
        role_ = role;
        invalidate();
    }
    // Replace the role items with an unavailable command are drawn with.
    void set_disabled_role_override(ui::RoleId role) noexcept {
        if (disabled_role_ == role) return;
        disabled_role_ = role;
        invalidate();
    }
    // Replace the accent role for an item's leading chord, in place of
    // "ckv.hotkey".
    void set_hotkey_role_override(ui::RoleId role) noexcept {
        if (hotkey_role_ == role) return;
        hotkey_role_ = role;
        invalidate();
    }

    // The ordinary items, shown whenever the focused view's nearest command
    // context has no set of its own (see set_context_items).
    void set_items(std::vector<StatusLineItem> items);
    const std::vector<StatusLineItem>& items() const noexcept { return items_; }

    // Items for one command context (View::set_command_context), shown
    // instead of the ordinary items while the focused view's nearest context
    // is `context` — so the legend offers what the reader can do where they
    // are: an editor's keys in the editor, a list's keys in the list. An
    // empty list removes the context's set.
    void set_context_items(std::string context, std::vector<StatusLineItem> items);
    void clear_context_items();
    // The items on the line right now: the focused context's set when it
    // has one, the ordinary items otherwise.
    const std::vector<StatusLineItem>& shown_items() const;

    // Maps a resolved help-context key to the hint text to display.
    // Unset (or a resolved key with no mapping — an empty return is
    // treated the same as "no hint") shows the item list only. The key is
    // the focused view's nearest, or — with nothing focused, an empty
    // desktop — the root's (D-069), so an application whose root carries a
    // key has a hint before its first window opens.
    void set_hint_provider(std::function<std::string(const std::string&)> provider);

    // A hint that outranks the focus-derived one until it is cleared
    // (pass an empty string). While a menu is open the reader is asking
    // about the entry under the highlight, not about whatever holds
    // focus behind the popup — the caller decides when that is true and
    // says so here, so this view keeps one hint-rendering path.
    void set_transient_hint(std::string hint);

    // The hint text that WOULD be shown right now — exposed for
    // testing without needing to scrape rendered cells.
    std::string current_hint() const;

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every item on it is a command that fires when clicked.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_attached() override;

private:
    struct EffectiveLabel {
        std::string text;
        int hotkey_width = 0;  // leading command chord, in terminal cells
    };

    // The rendered text for `item` — either its own hand-set label, or
    // (when it references a command) "{chord} {title}" composed live
    // from the registry, e.g. "Alt+X Quit". The leading chord receives
    // the shared hotkey accent automatically.
    EffectiveLabel effective_label(const StatusLineItem& item) const;
    ui::CommandId item_command(const StatusLineItem& item) const noexcept;
    bool item_available(const StatusLineItem& item) const;
    int item_start_column(std::size_t index) const;
    struct LaidOutItem {
        std::size_t index = 0;
        int x = 0;
        int width = 0;
    };
    std::vector<LaidOutItem> visible_items() const;

    std::vector<StatusLineItem> items_;
    std::vector<std::pair<std::string, std::vector<StatusLineItem>>> context_items_;
    std::function<std::string(const std::string&)> hint_provider_;
    std::string transient_hint_;
    // The item currently held down by the pointer, and whether the pointer
    // is still on it — a press dragged away un-highlights but stays claimed
    // so returning to the item re-arms it.
    std::optional<std::size_t> pressed_item_;
    bool pressed_visible_ = true;
    std::optional<std::size_t> item_at(Point cell) const;
    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId selected_hotkey_role_ = ui::kInvalidRole;
    ui::RoleId selected_disabled_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
