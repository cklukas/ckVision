// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/layout.hpp"
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

    void set_alignment(ui::Alignment alignment) noexcept {
        alignment_ = alignment;
        invalidate();
    }
    ui::Alignment alignment() const noexcept { return alignment_; }

    void set_role_override(ui::RoleId role) noexcept { role_ = role; }

    // Draws the first `count` rendered lines bold. A block whose opening
    // line names what the rest is about — a product title above its
    // version detail, a heading above its paragraph — then reads as one
    // block with a heading, rather than as two views a caller has to
    // keep adjacent and styled apart.
    void set_emphasized_leading_lines(int count) noexcept {
        emphasized_leading_lines_ = count;
        invalidate();
    }

    // Treats the text as already laid out: lines break only where the
    // text says, and runs of spaces survive. Text whose spacing carries
    // meaning — aligned key/value columns, a small table — is destroyed
    // by reflowing it, because wrapping rejoins words with a single
    // space and the columns collapse.
    void set_preformatted(bool preformatted) noexcept {
        preformatted_ = preformatted;
        invalidate();
    }

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    int height_for_width(int width) const override;
    void on_attached() override;

private:
    std::vector<std::string> wrap(int width) const;

    std::string raw_text_;
    ui::Alignment alignment_ = ui::Alignment::Start;
    ui::RoleId role_ = ui::kInvalidRole;
    int emphasized_leading_lines_ = 0;
    bool preformatted_ = false;
};

}  // namespace ckv::widgets
