// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// TextView: read-only styled text, scrolling (plans/widgets.md M6b
// baseline — "the building block for help browsers and about boxes").
//
// Scope note (documented, not an oversight): text is split into rows
// on '\n' only — no word-wrap. The baseline wording ("styled text,
// scrolling") doesn't call for wrapping the way StaticText's does;
// long lines are horizontally clipped by the Painter rather than
// wrapped, and there is no horizontal scrollbar in v1. Links
// (activatable, OSC 8) and inline styles are explicitly beyond-
// baseline.
#pragma once

#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.textview.text"; its embedded Scrollbar resolves
// its own roles independently.
class TextView : public ui::View {
public:
    TextView();

    void set_role_override(ui::RoleId text_role) noexcept { text_role_ = text_role; }

    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    int line_count() const noexcept { return static_cast<int>(lines_.size()); }
    int top_line() const noexcept;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    void split_lines();

    std::string raw_text_;
    std::vector<std::string> lines_;
    Scrollbar* scrollbar_ = nullptr;

    ui::RoleId text_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
