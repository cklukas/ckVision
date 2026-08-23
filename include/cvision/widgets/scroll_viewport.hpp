// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ScrollViewport: scrolls any content view via keyboard and its own
// Scrollbar children (the widget catalog M6a baseline "Scrolls any
// content view, keyboard and wheel"). Positions `content` at a
// negative offset so the portion at (scroll_x, scroll_y) in content-
// local space lands at the viewport's own origin.
//
// Mouse-wheel scope note (documented, not an oversight): a wheel event
// is delivered to the deepest view under the pointer, and Application
// then walks it up that view's ancestors for as long as nobody
// consumes it — so a wheel over CONTENT does reach this viewport,
// PROVIDED the content widget itself leaves it unhandled. A content
// widget that consumes the wheel for its own scrolling (a TextView, an
// editor) keeps it, which is the right answer: the innermost
// scrollable surface under the pointer is the one that should move.
// What still does not exist is any way for an OUTER viewport to see a
// wheel an inner one has taken. Keyboard scrolling and the Scrollbar
// children (always directly hit-testable, since they are siblings of
// content and never covered by it) are unconditional.
#pragma once

#include <memory>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

// FOCUS IS THE CALLER'S TO DECIDE, and the default (View's own
// FocusPolicy::None) is deliberately not an answer. A scroll region
// needs a tab stop exactly when NOTHING INSIDE IT CAN TAKE ONE:
//
//   * Wrapping focusable content — a form, a list — the region must NOT
//     be focusable, or it lands in the tab order twice: once for itself
//     and again for each control inside. The mechanism there is
//     scroll-follows-focus, which Dialog implements by walking up from
//     whatever took focus to the nearest enclosing viewport and calling
//     ensure_visible().
//   * Wrapping static content — prose, a read-only report — nothing
//     inside can hold focus and there is nothing for scrolling to
//     follow, so the region ITSELF must be focusable
//     (set_focus_policy(FocusPolicy::TabStop)) or it answers the wheel
//     and nothing else. A reader without a mouse then sees a scrollbar
//     promising more text with no way to reach it, which is worse than
//     visibly truncated text because it looks like it works.
//
// Same widget, opposite correct answers, decided by what it wraps —
// which is why no default can be right and the caller has to say. Both
// halves were paid for: the second by the CK Office launcher's
// description pane, the first by a settings dialog that would have been
// broken had the default gone the other way.
//
// Never draws anything itself — its two Scrollbar children resolve
// their own theme roles independently (M9 WP-7, D-028) once attached,
// same as if constructed standalone.
class ScrollViewport : public ui::View {
public:
    ScrollViewport();

    // Content's OWN preferred size (via its size hints) is treated as
    // its full scrollable extent; installs vertical and horizontal
    // Scrollbar children alongside it. Replaces and returns ownership
    // of any previous content.
    std::unique_ptr<ui::View> set_content(std::unique_ptr<ui::View> content);
    ui::View* content() const noexcept { return content_; }

    int scroll_x() const noexcept { return scroll_x_; }
    int scroll_y() const noexcept { return scroll_y_; }
    void set_scroll(int x, int y);  // clamped to [0, content extent - viewport extent]

    // Puts the bottom of the content in view.
    //
    // What a growing document wants: a transcript, a log, a build's
    // output. Such a view that does not follow its own tail has stopped
    // reporting, and every caller computing the offset itself has to
    // know the content's extent — which is the viewport's own business.
    void scroll_to_bottom();

    // Scrolls by the least amount that puts all of `descendant` inside the
    // visible area, and answers whether that moved anything. Already visible
    // is not a special case a caller has to detect: it scrolls by nothing
    // and returns false.
    //
    // The motivating caller is focus. A control that takes focus while
    // scrolled out of sight leaves the terminal cursor blinking where the
    // reader cannot see it, which is worse than not scrolling at all — so
    // whoever moves focus into a scrolled surface says so here. Returns
    // false for a view that is not inside this viewport's content (a caller
    // may ask about any view without first proving where it lives), and for
    // the content view itself, which has no position within itself to
    // reveal.
    bool ensure_visible(const ui::View& descendant);

    // Conventional document views may retain both scrollbar tracks even
    // while their initial content happens to fit. This makes the available
    // navigation affordances stable as content changes.
    void set_scrollbars_always_visible(bool visible) noexcept;
    bool scrollbars_always_visible() const noexcept { return scrollbars_always_visible_; }

    // Per-axis control, for a surface that wants one bar's rule and not the
    // other's — a dialog scrolls its fields vertically and must never scroll
    // them sideways, since a form whose left column has gone missing is not
    // a view of that form. The defaults are unchanged (both Auto, the
    // ordinary "a bar means there is more" rule), and
    // set_scrollbars_always_visible() remains the shorthand that sets both
    // to Always.
    //
    // ScrollbarPolicy::Hidden means more here than "draw no bar": that axis
    // does not scroll at all. The content is given exactly the visible
    // extent rather than its own larger preferred one, so nothing can end up
    // off screen along an axis that offers neither a bar nor a key to bring
    // it back.
    void set_vertical_scrollbar_policy(ScrollbarPolicy policy);
    void set_horizontal_scrollbar_policy(ScrollbarPolicy policy);
    ScrollbarPolicy vertical_scrollbar_policy() const noexcept;
    ScrollbarPolicy horizontal_scrollbar_policy() const noexcept;

    // Whether there is anywhere to scroll to on each axis right now. The
    // question a caller asks before offering scrolling as an affordance —
    // and the reason a viewport whose content fits leaves the arrow keys
    // alone rather than consuming them in order to move by nothing.
    bool can_scroll_vertically() const noexcept;
    bool can_scroll_horizontally() const noexcept;

    void on_resized() override;
    // Content that grows or shrinks re-measures the viewport, exactly as
    // a resize does. Without this a viewport reports the extent its
    // content had when it was last laid out — for content filled in after
    // construction, the extent of nothing at all — so the bar never
    // appears and everything past the first screenful is unreachable.
    // Splitter carries the same override for the same reason.
    void on_child_size_hint_changed(View&) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    void relayout();
    void sync_scrollbars();
    void reposition_content();

    ui::View* content_ = nullptr;
    Scrollbar* v_scrollbar_ = nullptr;
    Scrollbar* h_scrollbar_ = nullptr;
    int scroll_x_ = 0;
    int scroll_y_ = 0;
    // What relayout() last left for the content itself: this view's own
    // extent less whichever bars are on screen. ensure_visible() needs the
    // VISIBLE window rather than the whole viewport, and recomputing it
    // there would be a second copy of the rule relayout() already applied.
    int content_area_width_ = 0;
    int content_area_height_ = 0;
    bool scrollbars_always_visible_ = false;
};

}  // namespace ckv::widgets
