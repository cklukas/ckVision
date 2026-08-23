// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TextView: read-only styled text, scrolling, activatable links, and a
// deterministic OSC 8 export path (the widget catalog M6b).
//
// Text is split into logical lines on '\n'. Those become DISPLAY lines either
// one-for-one, or — with word wrap on — as many as the width needs. Both
// scrollbars follow a ScrollbarPolicy, and the two are sized against each
// other: a vertical bar costs a column, which can be what makes a line no
// longer fit, and a horizontal bar costs a row, which can be what makes the
// text no longer fit. That is resolved before either is drawn, rather than
// leaving the reader with a bar that overlaps content or a row they cannot
// reach.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/text_layout.hpp"

namespace ckv::widgets {

struct TextSpan {
    std::string text;
    Attr attrs = static_cast<Attr>(0);
    std::optional<std::string> link_target;
};

// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.textview.text"; its embedded Scrollbar resolves
// its own roles independently.
class TextView : public ui::View {
public:
    TextView();

    void set_role_override(ui::RoleId text_role) noexcept { text_role_ = text_role; }
    // A containing ScrollViewport can own the visible scrollbars for a
    // document surface. Hiding this view's internal vertical track keeps the
    // text preformatted while avoiding duplicate controls.
    void set_vertical_scrollbar_visible(bool visible) noexcept;
    bool vertical_scrollbar_visible() const noexcept { return vertical_scrollbar_visible_; }

    // When each bar is on screen. Both default to Auto: shown exactly while
    // the content does not fit, which is the only rule a reader can draw a
    // conclusion from.
    void set_vertical_scrollbar_policy(ScrollbarPolicy policy);
    void set_horizontal_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy vertical_scrollbar_policy() const noexcept;
    ScrollbarPolicy horizontal_scrollbar_policy() const noexcept;

    // How a logical line too wide for the view is broken into display lines.
    // WrapMode::None by default — preformatted text means what it means only
    // at its own line breaks — with the horizontal bar reaching the rest.
    // See WrapMode for what each choice is for.
    void set_wrap_mode(WrapMode mode);
    WrapMode wrap_mode() const noexcept { return wrap_mode_; }

    // Display lines — what the reader scrolls through. Equal to the logical
    // line count unless wrapping is on.
    int display_line_count() const noexcept { return static_cast<int>(display_runs_.size()); }
    // The widest display line, in cells. What the horizontal bar scrolls over.
    int content_width() const noexcept { return content_width_; }
    int left_column() const noexcept;

    void set_text(std::string text);
    void set_spans(std::vector<TextSpan> spans);
    const std::string& text() const noexcept { return raw_text_; }

    int line_count() const noexcept { return static_cast<int>(lines_.size()); }
    int top_line() const noexcept;
    std::size_t link_count() const noexcept { return link_targets_.size(); }
    std::optional<std::size_t> current_link() const noexcept { return current_link_; }
    void set_current_link(std::optional<std::size_t> index);
    bool activate_current_link();
    std::string osc8_text() const;

    std::function<void(const std::string&)> on_link_activate;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Read-only, but still text: it is selected and scrolled by pointer,
    // and the shape says what the pointer will do rather than whether the
    // content can be changed.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Text;
    }
    void on_attached() override;

private:
    struct LineRun {
        std::string text;
        Attr attrs = static_cast<Attr>(0);
        std::optional<std::size_t> link_index;
    };

    void split_lines();
    void rebuild_from_spans();
    // Turns the logical lines into the display lines actually drawn, and
    // records how wide the widest of them is.
    void rebuild_display();
    // Resolves both bars' visibility together, since each one's presence
    // changes the room left for the other, then applies bounds and ranges.
    void relayout_scrollbars();
    std::optional<std::size_t> link_at(int line, int column) const;

    std::string raw_text_;
    std::vector<std::string> lines_;
    std::vector<TextSpan> spans_;
    std::vector<std::vector<LineRun>> line_runs_;
    std::vector<std::string> link_targets_;
    std::optional<std::size_t> current_link_;
    std::vector<std::vector<LineRun>> display_runs_;
    int content_width_ = 0;
    // The area left for text once whichever bars are showing have taken
    // their column and row. Both wrapping and drawing measure against this,
    // never against bounds(), so they cannot disagree with what is drawn.
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    WrapMode wrap_mode_ = WrapMode::None;
    Scrollbar* scrollbar_ = nullptr;
    Scrollbar* h_scrollbar_ = nullptr;
    bool vertical_scrollbar_visible_ = true;

    ui::RoleId text_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
