// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ScrollViewport: scrolls any content view via keyboard and its own
// Scrollbar children (plans/widgets.md M6a baseline "Scrolls any
// content view, keyboard and wheel"). Positions `content` at a
// negative offset so the portion at (scroll_x, scroll_y) in content-
// local space lands at the viewport's own origin.
//
// Scope note (documented, not an oversight): mouse-wheel scroll only
// fires when the wheel event lands on the viewport's OWN uncovered
// area — Application's mouse dispatch is deepest-hit-wins with no
// bubbling (only KEY events walk the focus-chain ancestors; see
// Application::dispatch), so a wheel event over CONTENT currently
// reaches content's own on_mouse, not this viewport's. Making wheel
// scroll work anywhere over the content needs either a mouse-bubbling
// mechanism in Application or per-widget wheel forwarding — a real,
// separate increment. The Scrollbar children (always directly
// hit-testable, since they're siblings of content, never covered by
// it) and keyboard scrolling are fully functional today.
#pragma once

#include <memory>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

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

    // Conventional document views may retain both scrollbar tracks even
    // while their initial content happens to fit. This makes the available
    // navigation affordances stable as content changes.
    void set_scrollbars_always_visible(bool visible) noexcept;
    bool scrollbars_always_visible() const noexcept { return scrollbars_always_visible_; }

    void on_resized() override;
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
    bool scrollbars_always_visible_ = false;
};

}  // namespace ckv::widgets
