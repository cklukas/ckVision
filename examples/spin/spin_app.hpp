// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Spin example: one window per rotating solid, every frame drawn on a
// worker thread and shown through the ordinary invalidation path.
//
// What it is here to demonstrate, in the order the code does it:
//
//   * ONE self-clocking timer for the whole application, armed when the
//     first window opens and gone when the last one closes. Each tick
//     arms the next one AFTER it has run, so the loop can never
//     accumulate a backlog of animation ticks the way a repeating timer
//     does when a slow frame makes it fire late — and an application with
//     no windows open waits on input like any other, with no timer at
//     all.
//   * ONE frame in flight per window. A tick that finds the previous
//     frame still rendering asks for nothing, so a slow host renders
//     fewer frames instead of accumulating a queue of stale ones. The
//     angle comes from the injected clock rather than from a frame
//     counter, so a dropped frame costs smoothness and never speed.
//   * A frame interval that FOLLOWS the loop. A tick that arrives much
//     later than it was asked for is the loop reporting that it had
//     something better to do — input to dispatch, a window to repaint, a
//     large raster to encode — and the answer is to ask for frames less
//     often, not to pile on. It eases back towards the target as soon as
//     ticks land on time again, so the cost of a moment's overload is a
//     slower rotation rather than an unresponsive application.
//   * A finished frame is published with ImageView::set_image, which
//     invalidates that one view: the window repaints its own backing
//     store, the compositor touches the cells that changed, and the
//     Presenter re-encodes one raster. Nothing in this example forces a
//     full redraw.
//   * Every frame is painted on its own window's resolved background
//     color, and the picture is sized to whole cells of the terminal's
//     own metric — so the object sits in the window rather than on a
//     rectangle laid over it.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/window.hpp"

#include "mesh.hpp"
#include "render_service.hpp"

namespace ckv::spin {

// How often a window may ask for a new frame when the loop is keeping
// up. Slow enough that a terminal spends most of its time idle and a
// remote session is not flooded with raster payload, fast enough that a
// slow rotation reads as motion.
inline constexpr std::int64_t kFrameIntervalNanos = 60'000'000;

// How many raster pixels a second this application will ask a terminal to
// take in. It is not a property of the machine ckVision runs on: it is what
// the TERMINAL can decode and paint, and terminals differ by an order of
// magnitude. Sixel carries no completion report — a host answers a status
// query from its parser long before its renderer has finished the picture
// that query followed — so a rate is the only defence there is against
// sending the next frame into a picture that is still being drawn, and the
// symptom of getting it wrong is a half-painted image.
//
// The default is deliberately cautious. An application that has measured
// its host raises it; CKVISION_SPIN_PIXEL_RATE does that from the command
// line, and the frame-rate readout shows what was actually achieved.
inline constexpr double kRasterPixelsPerSecond = 8'000'000.0;

// The slowest the animation is allowed to become when the loop is
// overloaded. A floor rather than a cliff: two frames a second still says
// the application is alive and still leaves the loop almost entirely to
// the reader's own input.
inline constexpr std::int64_t kSlowestFrameIntervalNanos = 500'000'000;

// Radians per second. A demonstration turns slowly enough to be looked
// at; the two rates are deliberately unequal so the shape tumbles instead
// of spinning on one axis.
inline constexpr double kYawRateRadiansPerSecond = 0.45;
inline constexpr double kPitchRateRadiansPerSecond = 0.17;

// The rotating picture inside one window.
//
// It is an ImageView, so presenting a frame is set_image() and everything
// downstream — invalidation, retained repaint, raster re-encoding — is the
// framework's ordinary path rather than anything this example invented.
class SpinView final : public widgets::ImageView {
public:
    // `window` owns this view, so the reference cannot outlive it. The
    // view reads the window's active state in order to paint its frames
    // on exactly the surface the frame around them is using.
    SpinView(const widgets::Window& window, ShapeId shape, std::int64_t phase_nanos);

    ShapeId shape() const noexcept { return shape_; }

    // Asks for the frame belonging to `now_nanos` — unless one is already
    // being rendered, in which case this tick is deliberately skipped.
    void request_frame(RenderService& service, std::int64_t now_nanos);

    bool frame_in_flight() const noexcept { return in_flight_; }
    std::size_t frames_shown() const noexcept { return frames_shown_; }
    // Frames actually presented per second, smoothed over the recent ones
    // — zero until two frames have arrived far enough apart to measure.
    // This is the delivered rate, so it reports what the reader is
    // actually seeing rather than what was asked for.
    double frames_per_second() const noexcept;
    // Fired on the owning thread right after a frame has been published.
    // A readout that shows something derived from the animation subscribes
    // here and invalidates itself, so it is repainted with the frame it
    // describes rather than on a clock of its own.
    std::function<void()> on_frame_shown;
    // The pixel size of the most recently shown frame.
    Size frame_pixels() const noexcept { return frame_pixels_; }
    // The pixel size a frame requested right now would have: this view's
    // cell box measured with the terminal's own cell metric.
    Size target_pixels() const;
    // The color a frame is currently being painted on.
    Image::Rgba surface_color() const;

    void on_attached() override;

private:
    void accept_frame(std::shared_ptr<const Image> frame);
    // Keeps the view's own no-graphics/no-image surface on the window's
    // current frame role, so the cells around a picture — and the cells
    // in place of one, on a terminal that cannot show it — stay part of
    // the window.
    void adopt_surface_role() noexcept;
    ui::RoleId surface_role() const noexcept;

    const widgets::Window& window_;
    ShapeId shape_;
    std::int64_t phase_nanos_;
    bool in_flight_ = false;
    std::size_t frames_shown_ = 0;
    // Optional rather than a zero sentinel: an injected clock may
    // legitimately read zero, and a rate measured from a sentinel is a
    // rate measured from the wrong instant.
    std::optional<std::int64_t> last_frame_nanos_;
    double smoothed_interval_seconds_ = 0.0;
    Size frame_pixels_{0, 0};
    ui::RoleId frame_active_role_ = ui::kInvalidRole;
    ui::RoleId frame_inactive_role_ = ui::kInvalidRole;
};

// A one-line readout drawn on a window's own frame border — the pattern
// widgets::EditorWindow uses for its "Ln 1, Col 1" indicator, with two
// properties that matter once the value changes every frame:
//
//   * It PULLS its text when it draws, instead of being pushed a new
//     string whenever the value changes. Pushing would mean a set_text()
//     per frame, and every one of those is a size-hint change the window
//     has to re-lay-out its frame overlays for — layout work to display a
//     number that is about to change again.
//   * Its width is FIXED, so that re-layout never happens at all and a
//     number gaining a digit cannot shuffle the readout along the border.
//     The text is right-aligned inside that box.
//
// It draws in its window's own frame style, active or inactive, so the
// border reads as one line with a number set into it rather than as a
// patch of some other surface pasted onto the frame.
class FrameReadout final : public ui::View {
public:
    // `window` owns this view as a frame overlay, so the reference cannot
    // outlive it. `text` is invoked during draw() and nowhere else.
    FrameReadout(const widgets::Window& window, int width, std::function<std::string()> text);

    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    void on_attached() override;

private:
    const widgets::Window& window_;
    int width_;
    std::function<std::string()> text_;
    ui::RoleId frame_active_role_ = ui::kInvalidRole;
    ui::RoleId frame_inactive_role_ = ui::kInvalidRole;
};

class SpinApp {
public:
    explicit SpinApp(ui::Application& app);
    ~SpinApp();

    SpinApp(const SpinApp&) = delete;
    SpinApp& operator=(const SpinApp&) = delete;

    // Opens a window on `shape`, activates it, and starts the animation
    // clock if it was not already running.
    widgets::Window* open_window(ShapeId shape);

    // The id the registry assigned to one shape's "new window" command.
    // Callers without this object resolve the same command through
    // CommandRegistry::id_for(ShapeEntry::command_key).
    ui::CommandId new_window_command(ShapeId shape) const noexcept;

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    RenderService& frames() noexcept { return frames_; }

    std::size_t open_windows() const noexcept { return panels_.size(); }
    // Whether a tick is currently armed. It is exactly "some window is
    // open": an idle Spin has no timer at all.
    bool animating() const noexcept { return timer_.has_value(); }
    // The interval the next tick is asking for — the target while the loop
    // is keeping up, wider while it is not.
    std::int64_t frame_interval_nanos() const noexcept { return interval_nanos_; }
    // The rate to aim for when the loop is keeping up. The pacer still
    // widens from here under load and eases back to it, so this is the
    // ceiling an application asks for rather than one it imposes.
    void set_target_frame_interval(std::int64_t nanos);
    std::int64_t target_frame_interval_nanos() const noexcept { return target_interval_nanos_; }
    // The raster budget above, as an application may tune it for a host it
    // has measured. Zero or less restores the default.
    void set_raster_pixel_rate(double pixels_per_second) noexcept;
    double raster_pixel_rate() const noexcept { return raster_pixel_rate_; }
    // The interval the pictures currently on screen need at that rate —
    // what the pacer will not go faster than, whatever the target says.
    std::int64_t raster_paced_interval_nanos() const;
    SpinView* view_at(std::size_t index) const noexcept;
    FrameReadout* readout_at(std::size_t index) const noexcept;
    // What this terminal said about pictures, in one sentence — the
    // About box's own second paragraph, and the answer to "is the
    // example broken or is it the host?".
    std::string graphics_summary() const;

private:
    struct Panel {
        widgets::Window* window = nullptr;
        SpinView* view = nullptr;
        FrameReadout* readout = nullptr;
        std::weak_ptr<void> liveness;
    };

    void show_about();
    void tick();
    // Widens or narrows the frame interval from how late this tick was.
    void pace(std::int64_t now_nanos);
    void arm_next_tick();
    void stop_animation_clock();
    void forget(widgets::Window* window);
    Rect next_window_bounds() const;

    ui::Application& app_;
    ui::StandardRoles roles_;
    // Declared before the service that reads it: destruction runs in
    // reverse, so the workers are joined while the meshes they render are
    // still alive.
    MeshLibrary meshes_;
    RenderService frames_;
    widgets::Desktop* desktop_ = nullptr;
    std::vector<Panel> panels_;
    std::array<ui::CommandId, kShapeCount> commands_{};
    std::optional<ui::Application::TimerId> timer_;
    double raster_pixel_rate_ = kRasterPixelsPerSecond;
    std::int64_t target_interval_nanos_ = kFrameIntervalNanos;
    std::int64_t interval_nanos_ = kFrameIntervalNanos;
    std::optional<std::int64_t> last_tick_nanos_;
    int opened_ = 0;
};

}  // namespace ckv::spin
