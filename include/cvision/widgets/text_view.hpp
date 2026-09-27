// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TextView: read-only styled text, scrolling, and activatable links that a
// terminal rendering hyperlinks also makes clickable (the widget catalog M6b,
// D-088).
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

// One styled run of a TextView's content (TextView::set_spans). Spans are laid
// end to end; a span does not start a new line unless its text says so.
struct TextSpan {
    // UTF-8 text. A '\n' inside it ends the current logical line, so a span
    // may cover several lines.
    std::string text;
    // Attributes added on top of the view's text role for this run (combined
    // with it, never replacing it).
    Attr attrs = static_cast<Attr>(0);
    // When set, the whole span is one link to this target: drawn underlined,
    // reachable with Tab, and handed to TextView::on_link_activate when
    // activated. Every linked span is its own link, even when two spans share
    // a target. A target that is also a terminal hyperlink — an absolute URI
    // such as "https://example.org/" (is_valid_hyperlink_target) — is painted
    // as one, so a terminal that renders hyperlinks opens it on its own
    // click; there, equal targets are one hyperlink. Any other target
    // ("guide.md", a help topic) is the application's alone.
    std::optional<std::string> link_target;
};

// A read-only, scrollable, focusable text surface with optional styled runs
// and activatable links. The view is a tab stop; Up/Down/PageUp/PageDown/
// Home/End and the wheel scroll it vertically, and the horizontal bar (when
// shown) scrolls it sideways.
//
// Resolves its own theme role from context() once attached (M9
// WP-7, D-028): "ckv.textview.text"; its embedded Scrollbar resolves
// its own roles independently.
class TextView : public ui::View {
public:
    // Empty text, both bars on ScrollbarPolicy::Auto, no wrapping.
    TextView();

    // Replace the role the text is drawn with (spans add their attributes to
    // it). Left as kInvalidRole, "ckv.textview.text" is resolved at attach.
    void set_role_override(ui::RoleId text_role) noexcept {
        if (text_role_ == text_role) return;
        text_role_ = text_role;
        invalidate();
    }
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
    // The first cell column shown at the left edge: the horizontal scroll
    // position, 0 when nothing is scrolled.
    int left_column() const noexcept;

    // Replaces the content with plain text in the view's text role. Clears any
    // spans and links (current_link() becomes empty). The scroll position is
    // kept, clamped to the new content.
    void set_text(std::string text);
    // Replaces the content with styled runs, laid end to end. The links are
    // numbered in span order and the first one becomes current. The scroll
    // position is kept, clamped to the new content.
    void set_spans(std::vector<TextSpan> spans);
    // The content as plain text: what set_text was given, or the spans' text
    // concatenated (links and attributes dropped).
    const std::string& text() const noexcept { return raw_text_; }

    // Logical lines: the content split on '\n', at least 1 even when empty.
    int line_count() const noexcept { return static_cast<int>(lines_.size()); }
    // The display line shown in the top row: the vertical scroll position.
    int top_line() const noexcept;
    // Scrolls so display line `line` is the top row, clamped to the lines
    // there are to scroll to: 0 shows the start.
    void set_top_line(int line);
    // Links, in span order; 0 for content set with set_text.
    std::size_t link_count() const noexcept { return link_targets_.size(); }
    // Which link Enter activates: marked (reverse video) while the view has
    // focus. Empty when there are no links. Tab and Shift+Tab step it forward
    // and back; Tab on the last link and Shift+Tab on the first are left for
    // focus traversal, so the view never traps the keyboard, and losing the
    // focus puts the current link back on the first.
    std::optional<std::size_t> current_link() const noexcept { return current_link_; }
    // An index at or past link_count() clears the current link.
    void set_current_link(std::optional<std::size_t> index);
    // Hands the current link's target to on_link_activate (if set). Returns
    // whether there was a current link, whether or not a handler is set.
    bool activate_current_link();
    // Fired with the link's target when a link is activated: Enter on the
    // current link, a left press on a link, or activate_current_link().
    std::function<void(const std::string&)> on_link_activate;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    // Besides scrolling, consumes Tab and Shift+Tab while they step the
    // current link to another one — focus traversal leaves the view only past
    // the last link, or before the first — and Enter to activate it.
    // Left/Right do not scroll.
    bool on_key(const KeyEvent& event) override;
    // A left press on a link makes it current and activates it; the wheel
    // scrolls ui::kWheelRows display lines per notch.
    bool on_mouse(const MouseEvent& event) override;
    // Read-only, but still text: it is selected and scrolled by pointer,
    // and the shape says what the pointer will do rather than whether the
    // content can be changed.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Text;
    }
    void on_attached() override;
    // The current link is marked only while focused, so focus repaints; losing
    // the focus puts the current link back on the first.
    void on_focus(const FocusEvent& event) override;

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
