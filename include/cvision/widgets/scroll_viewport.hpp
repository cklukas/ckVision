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

class ScrollViewport : public ui::View {
public:
    ScrollViewport(const ui::Theme& theme, ui::RoleId track_role, ui::RoleId thumb_role);

    // Content's OWN preferred size (via its size hints) is treated as
    // its full scrollable extent; installs vertical and horizontal
    // Scrollbar children alongside it. Replaces and returns ownership
    // of any previous content.
    std::unique_ptr<ui::View> set_content(std::unique_ptr<ui::View> content);
    ui::View* content() const noexcept { return content_; }

    int scroll_x() const noexcept { return scroll_x_; }
    int scroll_y() const noexcept { return scroll_y_; }
    void set_scroll(int x, int y);  // clamped to [0, content extent - viewport extent]

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

    const ui::Theme& theme_;
    ui::RoleId track_role_;
    ui::RoleId thumb_role_;
};

}  // namespace ckv::widgets
