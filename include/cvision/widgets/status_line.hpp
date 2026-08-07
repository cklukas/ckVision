// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// StatusLine: context-sensitive hints bound to focus, clickable
// command items (plans/widgets.md M5 baseline). The hint mechanism
// mirrors D-027's F1 routing exactly (ARCHITECTURE.md §5: "the status
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
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;

struct StatusLineItem {
    std::string label;
    ui::CommandId command = ui::kInvalidCommand;
};

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.statusline.normal". Also reads context().app for
// the focused view's help-context key (current_hint()) and to
// execute an item's command (on_mouse()) — see the file comment on
// why the status line is one of the few widgets that needs it.
class StatusLine : public ui::View {
public:
    StatusLine();

    void set_role_override(ui::RoleId role) noexcept { role_ = role; }

    void set_items(std::vector<StatusLineItem> items);
    const std::vector<StatusLineItem>& items() const noexcept { return items_; }

    // Maps a resolved help-context key to the hint text to display.
    // Unset (or a resolved key with no mapping — an empty return is
    // treated the same as "no hint") shows the item list only.
    void set_hint_provider(std::function<std::string(const std::string&)> provider);

    // The hint text that WOULD be shown right now — exposed for
    // testing without needing to scrape rendered cells.
    std::string current_hint() const;

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    // The rendered text for `item` — either its own hand-set label, or
    // (when it references a command) "{chord} {title}" composed live
    // from the registry, e.g. "Alt+X Quit".
    std::string effective_label(const StatusLineItem& item) const;
    int item_start_column(std::size_t index) const;

    std::vector<StatusLineItem> items_;
    std::function<std::string(const std::string&)> hint_provider_;
    ui::RoleId role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
