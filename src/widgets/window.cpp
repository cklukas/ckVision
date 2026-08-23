// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window.hpp"

#include <algorithm>
#include <string_view>

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
    if (control_role_ == ui::kInvalidRole) control_role_ = context().roles->find("ckv.window.control");
    if (control_pressed_role_ == ui::kInvalidRole)
        control_pressed_role_ = context().roles->find("ckv.window.control.pressed");
}

void Window::on_detaching() {
    // A window removed mid-gesture still closes its gesture bracket, so a
    // Desktop that rested every picture for the move is told the move is
    // over rather than left waiting for an end that cannot come.
    end_gesture();
    if (on_detached) on_detached();
}

void Window::set_title(std::string title) {
    title_ = std::move(title);
    invalidate();
    // Told, not discovered: nothing else in the tree carries a rename to a
    // view that lists windows rather than containing them — see
    // bind_title_observer.
    if (title_observer_ && !title_observer_lifetime_.expired()) title_observer_();
}

void Window::set_footer(std::string footer) {
    if (footer_ == footer) return;
    footer_ = std::move(footer);
    invalidate();
}

Rect Window::clamp_move(Rect moved) const noexcept {
    if (move_bounds_.empty()) return moved;
    // The title bar is the only way to move a window, so it is the part that
    // has to stay reachable. Its row must remain inside vertically -- one row
    // under the menu bar or below the footer and there is nothing to grab --
    // and enough of it must remain inside horizontally to aim at.
    constexpr int kReachable = 8;
    const int min_x = move_bounds_.x - std::max(0, moved.width - kReachable);
    const int max_x = move_bounds_.right() - std::min(kReachable, moved.width);
    moved.x = std::clamp(moved.x, min_x, max_x);
    moved.y = std::clamp(moved.y, move_bounds_.y, move_bounds_.bottom() - 1);
    return moved;
}

void Window::end_drag() {
    held_control_ = Control::None;
    if (drag_kind_ == DragKind::None) return;
    const bool was_resizing = drag_kind_ == DragKind::Resize;
    drag_kind_ = DragKind::None;
    end_gesture();
    if (was_resizing) invalidate();  // the three temporary grips go away
}

void Window::begin_gesture() {
    if (gesture_active_) return;
    gesture_active_ = true;
    suspend_rasters();
    // Told even when this window itself had nothing to suspend: the window
    // being moved is often a plain one, and it is everything underneath it
    // whose pictures the move is about to churn.
    if (gesture_observer_ && !gesture_observer_lifetime_.expired()) gesture_observer_(true);
}

void Window::end_gesture() {
    if (!gesture_active_) return;
    gesture_active_ = false;
    resume_rasters();
    if (gesture_observer_ && !gesture_observer_lifetime_.expired()) gesture_observer_(false);
}

void Window::bind_gesture_observer(std::function<void(bool)> observer, std::weak_ptr<void> lifetime) {
    gesture_observer_ = std::move(observer);
    gesture_observer_lifetime_ = std::move(lifetime);
}

void Window::clear_gesture_observer() noexcept {
    gesture_observer_ = nullptr;
    gesture_observer_lifetime_.reset();
}

void Window::bind_title_observer(std::function<void()> observer, std::weak_ptr<void> lifetime) {
    title_observer_ = std::move(observer);
    title_observer_lifetime_ = std::move(lifetime);
}

void Window::clear_title_observer() noexcept {
    title_observer_ = nullptr;
    title_observer_lifetime_.reset();
}

void Window::suspend_rasters() {
    // Nothing to suspend, nothing to pay: a window of text is already as
    // cheap to move as composition can make it, and the repaint below
    // would be spent on a saving that does not exist here.
    if (!backing_surface_ || backing_surface_->raster_regions().empty()) return;
    // Repainting once here is what actually drops the pictures: a window
    // being moved keeps its backing store untouched by design, so a flag
    // alone would leave the last painted raster in place for the whole
    // gesture.
    if (rasters_suppressed_) return;
    rasters_suppressed_ = true;
    backing_dirty_ = true;
    invalidate();
}

void Window::resume_rasters() {
    // And once more here, which is where the picture comes back — at the
    // position the gesture finished on, sent exactly once rather than once
    // per position it passed through.
    if (!rasters_suppressed_) return;
    rasters_suppressed_ = false;
    backing_dirty_ = true;
    invalidate();
}

void Window::set_active(bool active) {
    if (active == active_) return;
    active_ = active;
    // Attention moved elsewhere, so whatever was being dragged here is over.
    end_drag();
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

ui::SizeHint Window::horizontal_size_hint() const {
    // draw() reserves twelve columns for the close/zoom controls, their
    // padding before it places a title.  Include that exact budget here so a
    // freshly presented standard dialog never immediately elides its title.
    const int title_width = text::text_width(title_) + 12;
    if (content_ == nullptr) return ui::SizeHint{min_size_.width, std::max(min_size_.width, title_width),
                                                  ui::kUnboundedExtent};
    const ui::SizeHint content = content_->horizontal_size_hint();
    // The frame plus any content margin: a margin the window asks for but
    // does not budget would be taken out of the content instead, silently
    // re-wrapping text that was measured to fit.
    const int chrome = 2 + 2 * horizontal_content_margin_;
    const int minimum = std::max(min_size_.width, content.min + chrome);
    const int preferred = std::max({minimum, content.preferred + chrome, title_width});
    const int maximum = content.max == ui::kUnboundedExtent ? ui::kUnboundedExtent : content.max + chrome;
    // A window the user cannot resize must not be stretched by its host
    // either: an expanding child inside it means "share what this window
    // has", not "make this window as large as the screen allows".
    if (!resizable_) return ui::SizeHint{minimum, preferred, preferred};
    return ui::SizeHint{minimum, preferred, std::max(preferred, maximum)};
}

int Window::effective_top_margin() const noexcept { return top_content_margin_; }

// The bottom margin works itself out. Content whose last row is a cast shadow
// already stands clear of the border, so a margin under it is a second gap
// where one was asked for -- the blank line beneath a dialog's buttons.
// Asking the content beats making every caller remember which of its dialogs
// happens to end in a button row.
int Window::effective_bottom_margin() const noexcept {
    return content_ != nullptr && content_->trailing_row_is_shadow() ? 0 : bottom_content_margin_;
}

ui::SizeHint Window::vertical_size_hint() const {
    if (content_ == nullptr) return ui::SizeHint{min_size_.height, min_size_.height, ui::kUnboundedExtent};
    const ui::SizeHint content = content_->vertical_size_hint();
    const int chrome = 2 + effective_top_margin() + effective_bottom_margin();
    const int minimum = std::max(min_size_.height, content.min + chrome);
    const int preferred = std::max(minimum, content.preferred + chrome);
    const int maximum = content.max == ui::kUnboundedExtent ? ui::kUnboundedExtent : content.max + chrome;
    if (!resizable_) return ui::SizeHint{minimum, preferred, preferred};
    return ui::SizeHint{minimum, preferred, std::max(preferred, maximum)};
}

int Window::height_for_width(int width) const {
    const int content_width = std::max(0, width - 2 - 2 * horizontal_content_margin_);
    const int content_height = content_ != nullptr ? content_->height_for_width(content_width) : 0;
    return std::max(vertical_size_hint().min, content_height + 2 + effective_top_margin() +
                                                   effective_bottom_margin());
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
    // An all-zero rectangle is the Desktop's deliberate "not positioned yet"
    // sentinel. Applying a size policy must not materialize that sentinel at
    // the origin: Desktop will use the new hints to choose both size and
    // centered position when this Window is presented.
    if (bounds().width > 0 || bounds().height > 0) set_bounds(clamp_size(bounds()));
}

void Window::set_max_size(Size max) noexcept {
    max_size_ = max;
    // Preserve the same unpositioned sentinel as set_min_size().
    if (bounds().width > 0 || bounds().height > 0) set_bounds(clamp_size(bounds()));
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
    if (held_on_closed) {
        held_on_closed();
        return true;
    }
    // No on_closed installed: closing still has to mean something. A close
    // box that changes nothing is indistinguishable from a broken one, so
    // the default is the request's plain meaning — leave the tree. An
    // application that wants bookkeeping installs on_closed and owns the
    // detach itself, exactly as every dialog presentation does.
    if (parent() != nullptr && context().app != nullptr) schedule_self_detach(*this, *context().app);
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

void Window::set_minimized(bool minimized) {
    if (minimized == minimized_) return;
    minimized_ = minimized;
    // A window that has left the desktop cannot still be being dragged across
    // it — the same reason set_active() ends the drag when attention moves
    // away, and the release would land wherever this window no longer is.
    end_drag();
    // Deliberately after the state: an observer runs with a window whose
    // hiddenness and visibility already agree, never between the two.
    set_visible(!minimized_);
    if (minimize_observer_ && !minimize_observer_lifetime_.expired()) minimize_observer_();
}

void Window::set_minimizable(bool minimizable) noexcept {
    if (minimizable_ == minimizable) return;
    minimizable_ = minimizable;
    invalidate();  // one control fewer, or one more, on the top border
}

void Window::bind_minimize_observer(std::function<void()> observer, std::weak_ptr<void> lifetime) {
    minimize_observer_ = std::move(observer);
    minimize_observer_lifetime_ = std::move(lifetime);
}

void Window::clear_minimize_observer() noexcept {
    minimize_observer_ = nullptr;
    minimize_observer_lifetime_.reset();
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
    begin_gesture();
    keyboard_mode_start_bounds_ = bounds();
}

void Window::enter_resize_mode() {
    keyboard_mode_ = KeyboardMode::Resize;
    begin_gesture();
    keyboard_mode_start_bounds_ = bounds();
}

Rect Window::content_rect() const noexcept {
    const Rect b = bounds();
    if (b.width <= 2 || b.height <= 2) return Rect{1, 1, 0, 0};
    // A margin is a request, not a guarantee: a window narrow enough that
    // honouring it would leave nothing for the content keeps the content
    // and gives up the margin, since blank cells are worth less than the
    // text they would displace.
    const int horizontal_room = std::max(0, (b.width - 3) / 2);
    const int vertical_room = std::max(0, (b.height - 3) / 2);
    const int left = std::min(left_content_margin_, horizontal_room);
    const int right = std::min(right_content_margin_, horizontal_room);
    const int top = std::min(effective_top_margin(), vertical_room);
    const int bottom = std::min(effective_bottom_margin(), vertical_room);
    return Rect{1 + left, 1 + top, b.width - 2 - left - right, b.height - 2 - top - bottom};
}

void Window::set_content_margin(int horizontal, int vertical) {
    horizontal_content_margin_ = std::max(0, horizontal);
    vertical_content_margin_ = std::max(0, vertical);
    set_content_margins(horizontal_content_margin_, vertical_content_margin_,
                        horizontal_content_margin_, vertical_content_margin_);
}

void Window::set_content_margins(int left, int top, int right, int bottom) {
    left_content_margin_ = std::max(0, left);
    top_content_margin_ = std::max(0, top);
    right_content_margin_ = std::max(0, right);
    bottom_content_margin_ = std::max(0, bottom);
    if (content_ != nullptr) content_->set_bounds(content_rect());
    invalidate();
}

Rect Window::frame_overlay_rect(ui::View& view, FrameSlot slot) const noexcept {
    const Rect b = bounds();
    if (slot.edge == Edge::Left || slot.edge == Edge::Right) {
        // A side border's corner cells belong to the frame — the title row
        // above, the resize grip's row below — so an overlay gets the run
        // between them, one cell wide, extended per its own vertical hint
        // and alignment the way a top overlay extends per its horizontal one.
        const int x = slot.edge == Edge::Left ? 0 : std::max(0, b.width - 1);
        const auto [aligned_y, height] = ui::align_cross_axis(
            b.height, view.vertical_size_hint().preferred, slot.alignment, 1, 1);
        const int max_y = std::max(1, b.height - 1 - height);
        const int y = std::clamp(aligned_y + slot.offset, 1, max_y);
        return Rect{x, y, 1, height};
    }
    // Bottom keeps the original single-overlay margin on BOTH sides (2
    // cells in from each corner — leaving the resize grip's own corner
    // cell untouched, and mirroring the close/zoom controls' own inset
    // on the top border for visual symmetry, even though nothing sits
    // in the bottom-left corner). Top's margins instead track whichever
    // controls draw() actually draws (point_in_close_control/
    // point_in_zoom_control/draws_minimize_control's own gates), so a
    // Start/End overlay never sits under a control that happens not to be
    // there at this width.
    const int left_margin = slot.edge == Edge::Top ? (b.width > 6 ? 5 : 1) : 2;
    // The controls at the right end are one group: a drawn minimize control
    // extends it leftwards by exactly its own three cells.
    const int top_right_margin =
        draws_minimize_control() ? 7 : (resizable_ && b.width > 8 ? 4 : 1);
    const int right_margin = slot.edge == Edge::Top ? top_right_margin : 2;
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
    // A pure translation invalidates the window's old and new desktop
    // rectangles through View::set_bounds(), but its local backing cells
    // remain valid: composition handles that structural movement, and
    // nothing here marks them dirty. Note what is NOT done — clearing the
    // flag. A move does not make a repaint somebody else asked for
    // unnecessary, and clearing it here swallowed exactly that: the repaint
    // that drops a window's pictures for the duration of a drag never ran,
    // because the first movement of the drag cancelled it.
    if (!backing_surface_ || backing_surface_->size() != Size{bounds().width, bounds().height})
        backing_dirty_ = true;
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
    backing_surface_->set_rasters_suppressed(rasters_suppressed_);
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
    return local.y == 0 && in_range(local.x, 2, 5);  // "[x]" at columns 2-4
}

bool Window::point_in_zoom_control(Point local) const noexcept {
    // A fixed-size dialog has no meaningful zoom operation.  Its title bar
    // deliberately keeps the close control only, matching the absence of
    // resize grips and avoiding a visual affordance that cannot succeed.
    if (!resizable_ || bounds().width <= 8) return false;
    const int w = bounds().width;
    return local.y == 0 && in_range(local.x, w - 5, w - 2);  // "[]" leaves one frame cell before the corner
}

bool Window::draws_minimize_control() const noexcept {
    // Three cells, immediately left of the zoom control's own three: the
    // right-hand group runs from width-8 to width-3, and the title is
    // budgeted and centred against nine cells a side (see draw()).
    //
    // This gate IS that budget, not a second number beside it: 22 is the
    // smallest width for which `width - 2 * 9 >= 4`, so wherever the
    // control is drawn the title still has four columns, and there is no
    // band where the frame claims room for a third control while the
    // caption has none at all. Below 22 the centred title's own padding
    // lands on the control's cells — a fourteen-column window centres it
    // exactly on the brackets — and a control that arrives by eating the
    // window's name is not worth its cells. Narrow windows therefore keep
    // precisely the frame they had before this control existed.
    //
    // A fixed-size window keeps the close control alone, exactly as it does
    // for zoom: an alert has nothing to minimize to.
    return minimizable_ && resizable_ && bounds().width >= 22;
}

bool Window::point_in_minimize_control(Point local) const noexcept {
    if (!draws_minimize_control()) return false;
    const int w = bounds().width;
    return local.y == 0 && in_range(local.x, w - 8, w - 5);  // "[_]" abutting the zoom control
}

int Window::resize_grip_width() const noexcept {
    if (!resizable_) return 0;
    // Two cells where there is room for two of them plus a cell of plain
    // border between, so the two grips stay visibly separate things rather
    // than merging into one continuous bottom edge.
    constexpr int kWide = 2;
    return bounds().width >= 2 * kWide + 1 ? kWide : 1;
}

std::optional<Window::Corner> Window::resize_corner_at(Point local) const noexcept {
    const int grip = resize_grip_width();
    if (grip <= 0) return std::nullopt;
    const Rect b = bounds();
    const bool left = local.x >= 0 && local.x < grip;
    const bool right = local.x >= b.width - grip && local.x < b.width;
    const bool top = local.y == 0;
    const bool bottom = local.y == b.height - 1;
    if (top && left) return Corner::TopLeft;
    if (top && right) return Corner::TopRight;
    if (bottom && left) return Corner::BottomLeft;
    if (bottom && right) return Corner::BottomRight;
    return std::nullopt;
}

PointerShape Window::corner_pointer_shape(Corner corner) noexcept {
    switch (corner) {
        case Corner::TopLeft:
        case Corner::BottomRight: return PointerShape::ResizeNorthWestSouthEast;
        case Corner::TopRight:
        case Corner::BottomLeft: return PointerShape::ResizeNorthEastSouthWest;
    }
    return PointerShape::Grab;  // exhaustive enum fallback for defensive builds
}

std::optional<PointerShape> Window::pointer_shape_at(Point local) const {
    // A gesture owns the pointer wherever it has got to. A resize drag
    // spends nearly all of its life away from the corner it started on —
    // that is what resizing IS — and a shape that reverted the moment the
    // pointer left the grip would be showing the corner's affordance only
    // while it was not being used.
    if (drag_kind_ == DragKind::Resize) return corner_pointer_shape(resize_corner_);
    // The hand closes while the window is actually in it, and opens again
    // when the drag ends. On a host with only the open hand nothing visibly
    // changes, which understates the moment rather than misstating it.
    if (drag_kind_ == DragKind::Move) return PointerShape::Grabbing;
    // Controls before corners before the title row, matching the press
    // order in on_mouse(): the shape has to promise whatever the press
    // would actually do, and the top corners sit on the title row.
    if (point_in_close_control(local) || point_in_minimize_control(local) ||
        point_in_zoom_control(local))
        return PointerShape::Pointer;
    if (const std::optional<Corner> corner = resize_corner_at(local))
        return corner_pointer_shape(*corner);
    if (movable_ && local.y == 0) return PointerShape::Grab;
    return std::nullopt;
}

bool Window::corner_shows_grip(Corner corner) const noexcept {
    if (drag_kind_ == DragKind::Resize) return true;
    return corner == Corner::BottomRight;
}

void Window::draw(scene::Painter& painter) {
    const Rect b = Rect{0, 0, bounds().width, bounds().height};
    const ui::RoleId frame_role = active_ ? frame_active_role_ : frame_inactive_role_;
    const ui::RoleId title_role = active_ ? title_active_role_ : title_inactive_role_;
    const ui::Theme& theme = *context().theme;
    const Style frame_style = theme.resolve(frame_role);
    const Style title_style = theme.resolve(title_role);
    // A control contributes its foreground/attributes only. Its background
    // must remain the active or inactive frame beneath it, including when a
    // dialog overrides the window's chrome roles.
    //
    // Only the active window's controls carry their own colour. On an
    // inactive one they fall back to the frame, so a desktop of windows
    // shows one set of live controls rather than several competing for the
    // eye — the colour is what marks the window you are working in.
    const Style control_role_style = theme.resolve(control_role_);
    const Style pressed_role_style = theme.resolve(control_pressed_role_);
    const Style pressed_style{pressed_role_style.fg, pressed_role_style.bg,
                              frame_style.attrs | pressed_role_style.attrs};
    const Style control_style =
        active_ ? Style{control_role_style.fg, frame_style.bg,
                        frame_style.attrs | control_role_style.attrs}
                : frame_style;

    // The interior fills with the SAME style as the frame — matching
    // the convention's own "the window's palette is uniform across
    // frame and content unless a child widget paints over it," not
    // just the border. Without this, any content cell a child widget
    // doesn't happen to cover shows raw, unstyled Surface fill — a
    // window with sparse content (a couple of labels and buttons, not
    // a full-bleed panel) previously showed a visibly blank patch
    // instead of reading as one solid window.
    // The whole interior, not just the content: a content margin belongs
    // to the window, so those cells must carry the window's own surface
    // rather than whatever the frame buffer last held there.
    if (b.width > 2 && b.height > 2)
        painter.fill(Rect{1, 1, b.width - 2, b.height - 2}, Cell::from_grapheme(" ", frame_style));

    // Active windows get a DOUBLE-line frame, inactive ones single —
    // the classic windowed-desktop convention for "this is the one
    // that has focus," and (unlike relying on color alone) still
    // legible on a monochrome terminal.
    painter.draw_box(b, active_ ? scene::LineStyle::Double : scene::LineStyle::Single, frame_style);
    if (active_ && resizable_ && b.width > 1 && b.height > 1) {
        // A focused, resizable window keeps single-line corner grips against
        // the double-line frame. They run a short way along the bottom border
        // rather than marking the corner cell alone: the corner is where the
        // eye looks for the handle, but one cell is a hard thing to hit, and
        // the neighbouring border cells cost nothing to give away — they are
        // frame, not content.
        // Grips are drawn in the control colour, not the frame's: one is
        // something to take hold of, the same kind of thing as the close and
        // zoom controls, and the convention colours all three alike.
        //
        // Idle, only the bottom right is marked -- that is the mark the
        // classic desktop uses, and four of them on a quiet window is three
        // more than it needs. All four resize regardless; the other three
        // appear for as long as a resize is under way, which is exactly when
        // knowing they are there is of any use.
        // Only the bottom right wears the control colour. The three that
        // appear during a resize are drawn in the frame's own, so the frame
        // changes shape without changing colour: the reader is in the middle
        // of a gesture, and three corners lighting up would be a louder
        // announcement than the information is worth.
        const int grip = resize_grip_width();
        const int bottom = b.height - 1;
        const auto mark = [&](Corner corner, Point at, std::string_view elbow, int inward) {
            if (!corner_shows_grip(corner)) return;
            const Style style = corner == Corner::BottomRight ? control_style : frame_style;
            painter.draw_text(at, std::string(elbow), style);
            for (int i = 1; i < grip; ++i)
                painter.draw_text(Point{at.x + inward * i, at.y}, "─", style);
        };
        mark(Corner::TopLeft, Point{0, 0}, "┌", 1);
        mark(Corner::TopRight, Point{b.width - 1, 0}, "┐", -1);
        mark(Corner::BottomLeft, Point{0, bottom}, "└", 1);
        mark(Corner::BottomRight, Point{b.width - 1, bottom}, "┘", -1);
    }
    if (b.width > 6) {
        // U+25A0 BLACK SQUARE for close, matching the convention's
        // close-control glyph (not "x" — a plain filled square reads
        // as a "control" the way "x" doesn't, and needs no ASCII
        // fallback since box-drawing-adjacent glyphs are already
        // required for the frame itself).
        // Keep one frame-rule cell between the corner and the control.  This
        // makes the chrome read as ╔═[■] rather than a control fused into the
        // corner, and matches the corresponding gap before the zoom control.
        // Held down, the whole control highlights -- brackets included, so
        // it reads as one pressed thing rather than a recoloured glyph. It
        // un-highlights the moment the pointer leaves, which is the window
        // saying what would happen if the button came up now.
        const bool armed = held_control_ == Control::Close && held_inside_;
        const Style face = armed ? pressed_style : frame_style;
        painter.draw_text(Point{2, 0}, "[", face);
        painter.draw_text(Point{3, 0}, "■", armed ? pressed_style : control_style);
        painter.draw_text(Point{4, 0}, "]", face);
    }
    if (draws_minimize_control()) {
        // U+005F LOW LINE for minimize — the mark the reader asked for, and
        // the one the convention puts on this control: a line along the
        // bottom of the cell, where the window is about to go. It sits
        // immediately left of the maximize/restore control, so the two read
        // as one group of things done TO the window, opposite the one thing
        // done to close it.
        const int control_x = b.width - 8;
        const bool armed = held_control_ == Control::Minimize && held_inside_;
        const Style face = armed ? pressed_style : frame_style;
        painter.draw_text(Point{control_x, 0}, "[", face);
        painter.draw_text(Point{control_x + 1, 0}, "_", armed ? pressed_style : control_style);
        painter.draw_text(Point{control_x + 2, 0}, "]", face);
    }
    if (resizable_ && b.width > 8) {
        // Up-arrow "maximize"; changes to an up/down arrow once zoomed.
        // A fixed-size dialog does not draw or expose this control.
        const int control_x = b.width - 5;
        const bool armed = held_control_ == Control::Zoom && held_inside_;
        const Style face = armed ? pressed_style : frame_style;
        painter.draw_text(Point{control_x, 0}, "[", face);
        painter.draw_text(Point{control_x + 1, 0}, maximized() ? "↕" : "↑",
                          armed ? pressed_style : control_style);
        painter.draw_text(Point{control_x + 2, 0}, "]", face);
    }
    if (!footer_.empty() && b.height > 1 && b.width > 6) {
        // Left-aligned and kept clear of the corner grips, which is the
        // side a reader's eye lands on and the side that stays put as the
        // window is resized.
        const int reserved = resizable_ ? 8 : 4;
        const std::string shown = text::elide_to_width(footer_, std::max(0, b.width - reserved));
        if (!shown.empty()) {
            painter.draw_text(Point{2, b.height - 1}, " ", frame_style);
            painter.draw_text(Point{3, b.height - 1}, shown, title_style);
            painter.draw_text(Point{3 + text::text_width(shown), b.height - 1}, " ", frame_style);
        }
    }
    if (b.width > 8) {
        // Centered, with a one-cell padding space on each side. The
        // title budget is bounded by the active frame controls, then
        // centered inside the remaining top border span. Previously this
        // drew the title left-aligned at a fixed column, immediately
        // abutting the close control with no padding at all.
        if (resizable_) {
            // The title is CENTRED, so what it may occupy is bounded by the
            // WIDER of the two control groups counted twice: five cells for
            // the close control and the frame cell after it, nine once the
            // minimize control extends the right-hand group to eight.
            // Budgeting each side separately would centre a full-width title
            // straight into whichever group is the wider one.
            //
            // Nine, not eight, because the title's own padding cell sits
            // outside its measured width: at eight, a title using its whole
            // budget would put that space on the minimize control's opening
            // bracket. The existing five has that same off-by-one against
            // the zoom control — visible in the goldens as a maximal title
            // abutting `↑]` — and correcting it there would re-elide titles
            // on windows this control never appears on.
            //
            // draws_minimize_control()'s width gate is derived from this
            // number: it is exactly the widths at which `available` is still
            // 4 or more, so the control never costs the window its name.
            const int reserved = draws_minimize_control() ? 9 : 5;
            const int available = std::max(0, b.width - (b.width > 6 ? 2 * reserved : 4));
            const std::string shown = text::elide_to_width(title_, available);
            const int shown_width = text::text_width(shown);
            const int start = (b.width - shown_width) / 2;
            if (start > 0) painter.draw_text(Point{start - 1, 0}, " ", frame_style);
            painter.draw_text(Point{start, 0}, shown, title_style);
            if (start + shown_width < b.width)
                painter.draw_text(Point{start + shown_width, 0}, " ", frame_style);
        } else {
            // A fixed dialog has only the close control, but its caption is
            // still centered on the complete frame rather than the remaining
            // span to the right of that control.  The title budget preserves
            // the control's readable space; the title's position preserves
            // the visual centre of the dialog.
            const int available = std::max(0, b.width - (b.width > 6 ? 10 : 2));
            const std::string shown = text::elide_to_width(title_, available);
            const int shown_width = text::text_width(shown);
            const int start = std::clamp((b.width - shown_width) / 2, b.width > 6 ? 5 : 1,
                                         std::max(0, b.width - 1 - shown_width));
            if (start > 0) painter.draw_text(Point{start - 1, 0}, " ", frame_style);
            painter.draw_text(Point{start, 0}, shown, title_style);
            if (start + shown_width < b.width - 1)
                painter.draw_text(Point{start + shown_width, 0}, " ", frame_style);
        }
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
        if (context().app != nullptr && activate_control_mnemonic(*this, event, *context().app))
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
            end_gesture();
            return true;
        case Key::Escape:
            set_bounds(keyboard_mode_start_bounds_);
            keyboard_mode_ = KeyboardMode::None;
            end_gesture();
            return true;
        default:
            return false;
    }
}

bool Window::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};

    if (event.action == MouseAction::Down) {
        // A new press supersedes anything still believed to be in progress.
        // Releasing outside the terminal delivers the release elsewhere, so
        // pressing again is the first news this window gets that the last
        // gesture ended.
        end_drag();
        if (point_in_close_control(local)) {
            held_control_ = Control::Close;
            held_inside_ = true;
            invalidate();
            return true;
        }
        if (point_in_minimize_control(local)) {
            held_control_ = Control::Minimize;
            held_inside_ = true;
            invalidate();
            return true;
        }
        if (point_in_zoom_control(local)) {
            held_control_ = Control::Zoom;
            held_inside_ = true;
            invalidate();
            return true;
        }
        // Before the title-bar move: the top corners sit on that row, and a
        // reader who aimed at a corner meant the corner.
        if (const std::optional<Corner> corner = resize_corner_at(local)) {
            drag_kind_ = DragKind::Resize;
            begin_gesture();
            resize_corner_ = *corner;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            invalidate();  // the other three corners now show their grips
            return true;
        }
        if (movable_ && local.y == 0) {
            drag_kind_ = DragKind::Move;
            begin_gesture();
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        return false;  // not frame chrome — let a child (content) handle it
    }

    if (held_control_ != Control::None) {
        // Which region the held control answers in. One answer serves both
        // questions a held press asks — whether it is still armed, and
        // whether the release lands on what it went down on — so the two can
        // never disagree about where the control is.
        const auto over_control = [&](Control control) {
            switch (control) {
                case Control::Close: return point_in_close_control(local);
                case Control::Minimize: return point_in_minimize_control(local);
                case Control::Zoom: return point_in_zoom_control(local);
                case Control::None: break;
            }
            return false;
        };
        const bool over = over_control(held_control_);
        if (event.action != MouseAction::Up) {
            // Held: nothing is decided, but say whether it still would be.
            if (over != held_inside_) {
                held_inside_ = over;
                invalidate();
            }
            return true;
        }
        const Control held = held_control_;
        held_control_ = Control::None;
        held_inside_ = true;
        invalidate();
        // Released off the control it started on: the reader moved away, and
        // moving away is how a press is taken back.
        if (!over) return true;
        switch (held) {
            case Control::Close: close(); break;
            case Control::Minimize: set_minimized(true); break;
            case Control::Zoom: toggle_zoom(zoom_target()); break;
            case Control::None: break;
        }
        return true;
    }

    if (drag_kind_ == DragKind::None) return false;

    // Motion with no button held: the release happened where this window
    // could not see it, and the pointer coming back is the proof. Without
    // this the window keeps following a pointer nobody is pressing.
    if (event.action == MouseAction::Move && event.button == MouseButton::None) {
        end_drag();
        return false;
    }

    if (event.action == MouseAction::Move) {
        const int dx = event.cell.x - drag_start_mouse_.x;
        const int dy = event.cell.y - drag_start_mouse_.y;
        if (drag_kind_ == DragKind::Move) {
            set_bounds(clamp_move(Rect{drag_start_bounds_.x + dx, drag_start_bounds_.y + dy,
                                        drag_start_bounds_.width, drag_start_bounds_.height}));
            return true;
        }
        // One rule for all four corners: the grabbed corner follows the
        // pointer and the opposite one does not move. Which edges travel is
        // the only thing that differs between them.
        const bool moves_left =
            resize_corner_ == Corner::TopLeft || resize_corner_ == Corner::BottomLeft;
        const bool moves_top = resize_corner_ == Corner::TopLeft || resize_corner_ == Corner::TopRight;
        const Rect resized = clamp_size(Rect{drag_start_bounds_.x, drag_start_bounds_.y,
                                              drag_start_bounds_.width + (moves_left ? -dx : dx),
                                              drag_start_bounds_.height + (moves_top ? -dy : dy)});
        // Re-anchor after clamping, or a window that has hit its minimum
        // keeps walking across the desktop while the pointer pushes into it.
        set_bounds(Rect{moves_left ? drag_start_bounds_.x + drag_start_bounds_.width - resized.width
                                   : drag_start_bounds_.x,
                        moves_top ? drag_start_bounds_.y + drag_start_bounds_.height - resized.height
                                  : drag_start_bounds_.y,
                        resized.width, resized.height});
        return true;
    }
    if (event.action == MouseAction::Up) {
        end_drag();
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
