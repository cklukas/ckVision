// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// Read-only, word-wrapped, styled text (plans/widgets.md baseline:
// "Styled, wrapped"). Wraps at grapheme boundaries on whitespace,
// falling back to a hard break mid-word only when a single word is
// wider than the available width. Uses the height-for-width pass
// (ARCHITECTURE.md §5's one sanctioned second layout pass) to report
// how tall it needs to be for a given width.
class StaticText : public View {
public:
    explicit StaticText(std::string text);

    void set_text(std::string text);
    const std::string& text() const noexcept { return raw_text_; }

    void set_role_override(ui::RoleId role) noexcept { role_ = role; }

    void draw(scene::Painter& painter) override;
    int height_for_width(int width) const override;
    void on_attached() override;

private:
    std::vector<std::string> wrap(int width) const;

    std::string raw_text_;
    ui::RoleId role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
