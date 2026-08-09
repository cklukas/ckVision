// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/widgets/label.hpp"

namespace ckv::widgets {

namespace {
bool in_range(int v, int begin, int end_exclusive) noexcept { return v >= begin && v < end_exclusive; }
}  // namespace

Window::Window(std::string title) : title_(std::move(title)) {}

void Window::on_attached() {
    if (frame_active_role_ == ui::kInvalidRole) frame_active_role_ = context().roles->find("ckv.window.frame.active");
    if (frame_inactive_role_ == ui::kInvalidRole)
        frame_inactive_role_ = context().roles->find("ckv.window.frame.inactive");
    if (title_active_role_ == ui::kInvalidRole) title_active_role_ = context().roles->find("ckv.window.title.active");
    if (title_inactive_role_ == ui::kInvalidRole)
        title_inactive_role_ = context().roles->find("ckv.window.title.inactive");
}

void Window::on_detaching() {
    if (on_detached) on_detached();
}

void Window::set_title(std::string title) {
    title_ = std::move(title);
    invalidate();
}

void Window::set_active(bool active) {
    if (active == active_) return;
    active_ = active;
    invalidate();
}

std::unique_ptr<ui::View> Window::set_content(std::unique_ptr<ui::View> content) {
    std::unique_ptr<ui::View> previous = content_ != nullptr ? remove_child(content_) : nullptr;
    content_ = nullptr;
    if (content != nullptr) {
        content_ = add_child(std::move(content));
        content_->set_bounds(content_rect());
    }
    return previous;
}

ui::AnchorPane& Window::content_pane() {
    if (content_ == nullptr) {
        auto pane = std::make_unique<ui::AnchorPane>();
        ui::AnchorPane* raw = pane.get();
        set_content(std::move(pane));
        return *raw;
    }
    auto* pane = dynamic_cast<ui::AnchorPane*>(content_);
    CKV_ASSERT(pane != nullptr);
    return *pane;
}

ui::View* Window::add_frame_overlay_impl(std::unique_ptr<ui::View> view, FrameSlot slot) {
    CKV_ASSERT(view != nullptr);
    // Top's Center/Fill are the title's own position — see FrameSlot's
    // own doc comment for why this is a hard contract, not an implicit
    // stacking order.
    CKV_ASSERT(slot.edge != Edge::Top || slot.alignment == ui::Alignment::Start ||
               slot.alignment == ui::Alignment::End);
    for (const auto& [existing_view, existing_slot] : frame_overlays_) {
        (void)existing_view;
        CKV_ASSERT(!(existing_slot.edge == slot.edge && existing_slot.alignment == slot.alignment));
    }

    ui::View* observer = add_child(std::move(view));
    frame_overlays_[observer] = slot;
    observer->set_bounds(frame_overlay_rect(*observer, slot));
    return observer;
}

std::unique_ptr<ui::View> Window::remove_frame_overlay(ui::View* view) {
    std::unique_ptr<ui::View> owned = remove_child(view);
    if (owned) frame_overlays_.erase(view);
    return owned;
}

void Window::set_min_size(Size min) noexcept {
    min_size_ = min;
    set_bounds(clamp_size(bounds()));
}

void Window::set_max_size(Size max) noexcept {
    max_size_ = max;
    set_bounds(clamp_size(bounds()));
}

bool Window::close() {
    // Both callbacks are application code. Treat a recursive request for this
    // same Window as already accepted rather than entering either callback
    // again; the outer request remains responsible for its final result.
    const std::shared_ptr<CloseState> close_state = close_state_;
    if (close_state->in_progress) return true;
    close_state->in_progress = true;
    struct ResetCloseGuard {
        std::shared_ptr<CloseState> state;
        ~ResetCloseGuard() { state->in_progress = false; }
    } reset_close{close_state};

    // Both callbacks may detach and destroy this Window. Retain callable
    // values before crossing either user-code boundary, and use the
    // per-instance token rather than this pointer to decide whether a later
    // protocol stage still has an object to act upon.
    const std::weak_ptr<void> window_liveness = lifetime_token();
    const std::function<bool()> held_close_request = close_request;
    const std::function<void()> held_on_closed = on_closed;
    if (held_close_request && !held_close_request()) return false;  // vetoed
    if (window_liveness.expired()) return true;
    if (held_on_closed) held_on_closed();
    return true;
}

void Window::toggle_zoom(Rect available) {
    if (zoomed_) {
        set_bounds(restored_bounds_);
        zoomed_ = false;
    } else {
        restored_bounds_ = bounds();
        fill(available);  // zooming must still honor min/max, like every other resize path
        zoomed_ = true;
    }
}

void Window::fill(Rect available) noexcept { set_bounds(clamp_size(available)); }

void Window::refresh_zoom_area(Rect available) noexcept {
    if (!zoomed_) return;
    fill(available);
}

void Window::reposition_within(Rect available) noexcept {
    const Rect b = bounds();
    // Shrink to fit `available` — but never below min_size_: a window
    // smaller than its own declared minimum isn't usefully reachable
    // either, so min_size_ wins over "fits available" when the two
    // conflict (a desktop shrunk smaller than a window's own minimum).
    const int width = std::min(b.width, std::max(available.width, min_size_.width));
    const int height = std::min(b.height, std::max(available.height, min_size_.height));
    const Rect sized = clamp_size(Rect{b.x, b.y, width, height});

    // Reposition so no edge falls outside `available` when the window
    // does fit; when it doesn't (min_size_ > available), pin to
    // available's own origin so the title bar/close control stay at a
    // predictable, reachable spot rather than wherever a prior larger
    // desktop happened to leave it.
    const int x = std::clamp(sized.x, available.x, available.x + std::max(0, available.width - sized.width));
    const int y = std::clamp(sized.y, available.y, available.y + std::max(0, available.height - sized.height));
    set_bounds(Rect{x, y, sized.width, sized.height});
}

void Window::enter_move_mode() {
    keyboard_mode_ = KeyboardMode::Move;
    keyboard_mode_start_bounds_ = bounds();
}

void Window::enter_resize_mode() {
    keyboard_mode_ = KeyboardMode::Resize;
    keyboard_mode_start_bounds_ = bounds();
}

Rect Window::content_rect() const noexcept {
    const Rect b = bounds();
    if (b.width <= 2 || b.height <= 2) return Rect{1, 1, 0, 0};
    return Rect{1, 1, b.width - 2, b.height - 2};
}

Rect Window::frame_overlay_rect(ui::View& view, FrameSlot slot) const noexcept {
    const Rect b = bounds();
    // Bottom keeps the original single-overlay margin on BOTH sides (2
    // cells in from each corner — leaving the resize grip's own corner
    // cell untouched, and mirroring the close/zoom controls' own inset
    // on the top border for visual symmetry, even though nothing sits
    // in the bottom-left corner). Top's margins instead track whichever
    // controls draw() actually draws (point_in_close_control/
    // point_in_zoom_control's own gates), so a Start/End overlay never
    // sits under a control that happens not to be there at this width.
    const int left_margin = slot.edge == Edge::Top ? (b.width > 6 ? 4 : 1) : 2;
    const int right_margin = slot.edge == Edge::Top ? (b.width > 8 ? 3 : 1) : 2;
    const int y = slot.edge == Edge::Top ? 0 : std::max(0, b.height - 1);

    const int preferred = view.horizontal_size_hint().preferred;
    const auto [aligned_x, width] =
        ui::align_cross_axis(b.width, preferred, slot.alignment, left_margin, right_margin);
    const int max_x = std::max(left_margin, b.width - right_margin - width);
    const int x = std::clamp(aligned_x + slot.offset, left_margin, max_x);
    return Rect{x, y, width, 1};
}

void Window::relayout_frame_overlays() noexcept {
    for (const auto& [view, slot] : frame_overlays_) {
        view->set_bounds(frame_overlay_rect(*view, slot));
    }
}

Rect Window::clamp_size(Rect b) const noexcept {
    int width = std::max(b.width, min_size_.width);
    int height = std::max(b.height, min_size_.height);
    if (max_size_.width > 0) width = std::min(width, max_size_.width);
    if (max_size_.height > 0) height = std::min(height, max_size_.height);
    return Rect{b.x, b.y, width, height};
}

void Window::on_resized() {
    if (backing_surface_ && backing_surface_->size() == Size{bounds().width, bounds().height}) {
        // A pure translation invalidates the window's old and new desktop
        // rectangles through View::set_bounds(), but its local backing cells
        // remain valid. Composition handles that structural movement.
        backing_dirty_ = false;
    } else {
        backing_dirty_ = true;
    }
    if (content_ != nullptr) content_->set_bounds(content_rect());
    relayout_frame_overlays();
}

void Window::on_invalidated(Rect, ui::InvalidationKind kind) {
    if (kind == ui::InvalidationKind::Content) backing_dirty_ = true;
}

void Window::on_descendant_invalidated(const ui::View&, Rect, ui::InvalidationKind) { backing_dirty_ = true; }

bool Window::repaint_backing_if_needed() {
    const Size local_size{bounds().width, bounds().height};
    if (!backing_surface_ || backing_surface_->size() != local_size) {
        backing_surface_.emplace(local_size);
        backing_dirty_ = true;
    }
    if (!backing_dirty_) return false;

    backing_surface_->clear_raster_regions();
    scene::Painter painter(*backing_surface_, Rect{0, 0, local_size.width, local_size.height});
    draw(painter);
    ui::View::paint_children(painter);
    backing_dirty_ = false;
    ++content_repaint_count_;
    return true;
}

scene::Surface& Window::backing_surface() noexcept {
    CKV_ASSERT(backing_surface_.has_value());
    return *backing_surface_;
}

const scene::Surface& Window::backing_surface() const noexcept {
    CKV_ASSERT(backing_surface_.has_value());
    return *backing_surface_;
}

void Window::on_child_size_hint_changed(ui::View& child) {
    auto it = frame_overlays_.find(&child);
    if (it != frame_overlays_.end()) child.set_bounds(frame_overlay_rect(child, it->second));
}

bool Window::point_in_close_control(Point local) const noexcept {
    // Must match draw()'s own "b.width > 6" gate — a control that
    // isn't drawn must not be clickable, and at widths where only the
    // close control is drawn, its region must not extend into where
    // the (undrawn) zoom control's columns would otherwise be.
    if (bounds().width <= 6) return false;
    return local.y == 0 && in_range(local.x, 1, 4);  // "[x]" at columns 1-3
}

bool Window::point_in_zoom_control(Point local) const noexcept {
    if (bounds().width <= 8) return false;  // must match draw()'s own "b.width > 8" gate
    const int w = bounds().width;
    return local.y == 0 && in_range(local.x, w - 3, w - 1);  // "[]" at the last two columns before the corner
}

bool Window::point_in_resize_grip(Point local) const noexcept {
    return resizable_ && local.x == bounds().width - 1 && local.y == bounds().height - 1;
}

void Window::draw(scene::Painter& painter) {
    const Rect b = Rect{0, 0, bounds().width, bounds().height};
    const ui::RoleId frame_role = active_ ? frame_active_role_ : frame_inactive_role_;
    const ui::RoleId title_role = active_ ? title_active_role_ : title_inactive_role_;
    const ui::Theme& theme = *context().theme;
    const Style frame_style = theme.resolve(frame_role);
    const Style title_style = theme.resolve(title_role);

    // The interior fills with the SAME style as the frame — matching
    // the convention's own "the window's palette is uniform across
    // frame and content unless a child widget paints over it," not
    // just the border. Without this, any content cell a child widget
    // doesn't happen to cover shows raw, unstyled Surface fill — a
    // window with sparse content (a couple of labels and buttons, not
    // a full-bleed panel) previously showed a visibly blank patch
    // instead of reading as one solid window.
    painter.fill(content_rect(), Cell::from_grapheme(" ", frame_style));

    // Active windows get a DOUBLE-line frame, inactive ones single —
    // the classic windowed-desktop convention for "this is the one
    // that has focus," and (unlike relying on color alone) still
    // legible on a monochrome terminal.
    painter.draw_box(b, active_ ? scene::LineStyle::Double : scene::LineStyle::Single, frame_style);
    if (b.width > 6) {
        // U+25A0 BLACK SQUARE for close, matching the convention's
        // close-control glyph (not "x" — a plain filled square reads
        // as a "control" the way "x" doesn't, and needs no ASCII
        // fallback since box-drawing-adjacent glyphs are already
        // required for the frame itself).
        painter.draw_text(Point{1, 0}, "[■]", frame_style);
        // Up-arrow "maximize"; changes to an up/down arrow once
        // zoomed, matching the convention's own "toggle" affordance —
        // previously this drew empty brackets with no glyph at all.
        // b.width - 4, not - 3: a 3-glyph "[X]" ending at b.width - 3
        // would land its last character ON the frame's own top-right
        // corner column (b.width - 1) — this must end one column short
        // of the corner, matching the close control's own symmetric
        // margin on the left.
        painter.draw_text(Point{b.width - 4, 0}, zoomed_ ? "[↕]" : "[↑]", frame_style);
    }
    if (b.width > 8) {
        // Centered, with a one-cell padding space on each side. The
        // title budget is bounded by the active frame controls, then
        // centered inside the remaining top border span. Previously this
        // drew the title left-aligned at a fixed column, immediately
        // abutting the close control with no padding at all.
        const int reserved_for_controls = (b.width > 6) ? 10 : 4;  // both controls flank ~2 cells of margin each
        const int available = std::max(0, b.width - reserved_for_controls);
        const std::string shown = text::elide_to_width(title_, available);
        const int shown_width = text::text_width(shown);
        const int start = (b.width - shown_width) / 2;
        if (start > 0) painter.draw_text(Point{start - 1, 0}, " ", frame_style);
        painter.draw_text(Point{start, 0}, shown, title_style);
        if (start + shown_width < b.width) painter.draw_text(Point{start + shown_width, 0}, " ", frame_style);
    }
}

bool Window::on_key(const KeyEvent& event) {
    if (keyboard_mode_ == KeyboardMode::None) {
        if (event.chord.key == Key::Enter && accept_request) {
            accept_request();
            return true;
        }
        if (event.chord.key == Key::Escape && cancel_request) {
            cancel_request();
            return true;
        }
        if (context().app != nullptr && activate_label_mnemonic(*this, event, *context().app))
            return true;
        return false;
    }

    const bool is_resize = keyboard_mode_ == KeyboardMode::Resize;
    switch (event.chord.key) {
        case Key::Left:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width - 1, bounds().height}
                                             : Rect{bounds().x - 1, bounds().y, bounds().width, bounds().height}));
            return true;
        case Key::Right:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width + 1, bounds().height}
                                             : Rect{bounds().x + 1, bounds().y, bounds().width, bounds().height}));
            return true;
        case Key::Up:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width, bounds().height - 1}
                                             : Rect{bounds().x, bounds().y - 1, bounds().width, bounds().height}));
            return true;
        case Key::Down:
            set_bounds(clamp_size(is_resize ? Rect{bounds().x, bounds().y, bounds().width, bounds().height + 1}
                                             : Rect{bounds().x, bounds().y + 1, bounds().width, bounds().height}));
            return true;
        case Key::Enter:
            keyboard_mode_ = KeyboardMode::None;
            return true;
        case Key::Escape:
            set_bounds(keyboard_mode_start_bounds_);
            keyboard_mode_ = KeyboardMode::None;
            return true;
        default:
            return false;
    }
}

bool Window::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};

    if (event.action == MouseAction::Down) {
        if (point_in_close_control(local)) {
            close();
            return true;
        }
        if (point_in_zoom_control(local)) {
            toggle_zoom(zoom_target());
            return true;
        }
        if (point_in_resize_grip(local)) {
            drag_kind_ = DragKind::Resize;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        if (movable_ && local.y == 0) {
            drag_kind_ = DragKind::Move;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        return false;  // not frame chrome — let a child (content) handle it
    }

    if (drag_kind_ == DragKind::None) return false;

    if (event.action == MouseAction::Move) {
        const int dx = event.cell.x - drag_start_mouse_.x;
        const int dy = event.cell.y - drag_start_mouse_.y;
        if (drag_kind_ == DragKind::Move) {
            set_bounds(Rect{drag_start_bounds_.x + dx, drag_start_bounds_.y + dy, drag_start_bounds_.width,
                             drag_start_bounds_.height});
        } else {
            set_bounds(clamp_size(Rect{drag_start_bounds_.x, drag_start_bounds_.y, drag_start_bounds_.width + dx,
                                        drag_start_bounds_.height + dy}));
        }
        return true;
    }
    if (event.action == MouseAction::Up) {
        drag_kind_ = DragKind::None;
        return true;
    }
    return false;
}

Rect Window::zoom_target() const noexcept {
    // Keep a strong capability for the call.  A detached Window has its
    // target cleared before any detach observer can run; the weak check
    // is the second line of defence for Desktop teardown.
    if (const std::shared_ptr<void> lease = desktop_lifetime_.lock(); lease && desktop_zoom_target_)
        return desktop_zoom_target_();
    return bounds();  // standalone Window: a deliberately harmless no-op target
}

void Window::bind_desktop_zoom_target(std::function<Rect()> target, std::weak_ptr<void> desktop_lifetime) {
    desktop_zoom_target_ = std::move(target);
    desktop_lifetime_ = std::move(desktop_lifetime);
}

void Window::clear_desktop_zoom_target() noexcept {
    desktop_zoom_target_ = nullptr;
    desktop_lifetime_.reset();
}

void schedule_self_detach(Window& window, ui::Application& app) {
    // The post crosses an event-loop boundary. The caller may externally
    // detach and destroy the closed Window before that turn runs, so retain
    // only an identity observer alongside the raw address. The weak token is
    // independent of tree attachment and rules out address reuse too.
    Window* const window_ptr = &window;
    const std::weak_ptr<void> window_liveness = window.lifetime_token();
    app.post([window_ptr, window_liveness]() {
        // detach_child(), not remove_child(): if `parent` is a Desktop
        // and `window_ptr` was added via add_window(), plain remove_child
        // would silently leave a dangling pointer in Desktop's own
        // windows_ bookkeeping — detach_child() is the polymorphic
        // entry point that gets this right regardless of what kind of
        // View actually parents this window.
        if (window_liveness.expired()) return;
        if (ui::View* parent = window_ptr->parent()) parent->detach_child(window_ptr);
    });
}

}  // namespace ckv::widgets
