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
    if (control_role_ == ui::kInvalidRole) control_role_ = context().roles->find("ckv.window.control");
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

ui::SizeHint Window::vertical_size_hint() const {
    if (content_ == nullptr) return ui::SizeHint{min_size_.height, min_size_.height, ui::kUnboundedExtent};
    const ui::SizeHint content = content_->vertical_size_hint();
    const int chrome = 2 + 2 * vertical_content_margin_;
    const int minimum = std::max(min_size_.height, content.min + chrome);
    const int preferred = std::max(minimum, content.preferred + chrome);
    const int maximum = content.max == ui::kUnboundedExtent ? ui::kUnboundedExtent : content.max + chrome;
    if (!resizable_) return ui::SizeHint{minimum, preferred, preferred};
    return ui::SizeHint{minimum, preferred, std::max(preferred, maximum)};
}

int Window::height_for_width(int width) const {
    const int content_width = std::max(0, width - 2 - 2 * horizontal_content_margin_);
    const int content_height = content_ != nullptr ? content_->height_for_width(content_width) : 0;
    return std::max(vertical_size_hint().min, content_height + 2 + 2 * vertical_content_margin_);
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
    // A margin is a request, not a guarantee: a window narrow enough that
    // honouring it would leave nothing for the content keeps the content
    // and gives up the margin, since blank cells are worth less than the
    // text they would displace.
    const int h = std::min(horizontal_content_margin_, std::max(0, (b.width - 3) / 2));
    const int v = std::min(vertical_content_margin_, std::max(0, (b.height - 3) / 2));
    return Rect{1 + h, 1 + v, b.width - 2 - 2 * h, b.height - 2 - 2 * v};
}

void Window::set_content_margin(int horizontal, int vertical) {
    horizontal_content_margin_ = std::max(0, horizontal);
    vertical_content_margin_ = std::max(0, vertical);
    if (content_ != nullptr) content_->set_bounds(content_rect());
    invalidate();
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
    const int left_margin = slot.edge == Edge::Top ? (b.width > 6 ? 5 : 1) : 2;
    const int right_margin =
        slot.edge == Edge::Top ? (resizable_ && b.width > 8 ? 4 : 1) : 2;
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

int Window::resize_grip_width() const noexcept {
    if (!resizable_) return 0;
    // Two cells where there is room for two of them plus a cell of plain
    // border between, so the two grips stay visibly separate things rather
    // than merging into one continuous bottom edge.
    constexpr int kWide = 2;
    return bounds().width >= 2 * kWide + 1 ? kWide : 1;
}

bool Window::point_in_resize_right_grip(Point local) const noexcept {
    const int grip = resize_grip_width();
    return grip > 0 && local.y == bounds().height - 1 && local.x >= bounds().width - grip &&
           local.x < bounds().width;
}

bool Window::point_in_resize_left_grip(Point local) const noexcept {
    const int grip = resize_grip_width();
    return grip > 0 && local.y == bounds().height - 1 && local.x >= 0 && local.x < grip;
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
        // Drawn in the control colour, not the frame's: a grip is something
        // to take hold of, the same kind of thing as the close and zoom
        // controls, and the convention colours all three alike. Rendering it
        // as frame makes the one part of the border you can grab look like
        // the parts you cannot.
        const int grip = resize_grip_width();
        const int y = b.height - 1;
        painter.draw_text(Point{0, y}, "└", control_style);
        painter.draw_text(Point{b.width - 1, y}, "┘", control_style);
        for (int i = 1; i < grip; ++i) {
            painter.draw_text(Point{i, y}, "─", control_style);
            painter.draw_text(Point{b.width - 1 - i, y}, "─", control_style);
        }
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
        painter.draw_text(Point{2, 0}, "[", frame_style);
        painter.draw_text(Point{3, 0}, "■", control_style);
        painter.draw_text(Point{4, 0}, "]", frame_style);
    }
    if (resizable_ && b.width > 8) {
        // Up-arrow "maximize"; changes to an up/down arrow once zoomed.
        // A fixed-size dialog does not draw or expose this control.
        const int control_x = b.width - 5;
        painter.draw_text(Point{control_x, 0}, "[", frame_style);
        painter.draw_text(Point{control_x + 1, 0}, maximized() ? "↕" : "↑", control_style);
        painter.draw_text(Point{control_x + 2, 0}, "]", frame_style);
    }
    if (b.width > 8) {
        // Centered, with a one-cell padding space on each side. The
        // title budget is bounded by the active frame controls, then
        // centered inside the remaining top border span. Previously this
        // drew the title left-aligned at a fixed column, immediately
        // abutting the close control with no padding at all.
        if (resizable_) {
            const int available = std::max(0, b.width - (b.width > 6 ? 10 : 4));
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
        if (point_in_resize_right_grip(local)) {
            drag_kind_ = DragKind::ResizeRight;
            drag_start_mouse_ = event.cell;
            drag_start_bounds_ = bounds();
            return true;
        }
        if (point_in_resize_left_grip(local)) {
            drag_kind_ = DragKind::ResizeLeft;
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
        } else if (drag_kind_ == DragKind::ResizeLeft) {
            const Rect resized = clamp_size(Rect{drag_start_bounds_.x + dx, drag_start_bounds_.y,
                                                  drag_start_bounds_.width - dx,
                                                  drag_start_bounds_.height + dy});
            // Keep the opposite edge anchored if min/max size clamping limits
            // how far the left edge can travel.
            set_bounds(Rect{drag_start_bounds_.x + drag_start_bounds_.width - resized.width,
                             resized.y, resized.width, resized.height});
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
