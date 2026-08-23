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

struct StatusLineItem {
    StatusLineItem() = default;
    StatusLineItem(std::string item_label, ui::CommandId command_id = ui::kInvalidCommand,
                   int item_priority = 0)
        : label(std::move(item_label)), command(command_id), priority(item_priority) {}
    explicit StatusLineItem(CommandPresentation command_presentation, int item_priority = 0)
        : priority(item_priority), presentation(std::move(command_presentation)) {}

    std::string label;
    ui::CommandId command = ui::kInvalidCommand;
    int priority = 0;  // higher priority survives first on narrow status lines
    CommandPresentation presentation;
};

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.statusline.normal" and "ckv.hotkey". Also reads context().app for
// the focused view's help-context key (current_hint()) and to
// execute an item's command (on_mouse()) — see the file comment on
// why the status line is one of the few widgets that needs it.
class StatusLine : public ui::View {
public:
    StatusLine();

    void set_role_override(ui::RoleId role) noexcept { role_ = role; }
    void set_disabled_role_override(ui::RoleId role) noexcept { disabled_role_ = role; }
    void set_hotkey_role_override(ui::RoleId role) noexcept { hotkey_role_ = role; }

    void set_items(std::vector<StatusLineItem> items);
    const std::vector<StatusLineItem>& items() const noexcept { return items_; }

    // Maps a resolved help-context key to the hint text to display.
    // Unset (or a resolved key with no mapping — an empty return is
    // treated the same as "no hint") shows the item list only.
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
