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
// Scope note: command-chord text (e.g. "F1 Help") is NOT rendered next
// to each item — formatting an arbitrary KeyChord into a short display
// string needs a real key-name table (this framework's Key enum
// currently has no such table anywhere), which is its own increment.
// Items show their label only; documented gap, not an oversight.
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

class StatusLine : public ui::View {
public:
    StatusLine(const ui::Theme& theme, ui::RoleId role, ui::Application& app);

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

private:
    int item_start_column(std::size_t index) const;

    std::vector<StatusLineItem> items_;
    std::function<std::string(const std::string&)> hint_provider_;
    const ui::Theme& theme_;
    ui::RoleId role_;
    ui::Application& app_;
};

}  // namespace ckv::widgets
