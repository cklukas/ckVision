// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "spin_app.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <cmath>

#include "cvision/core/cell.hpp"
#include "cvision/core/palette.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::spin {

namespace {

// Cells the frame readout reserves on the border. Wide enough for the
// longest thing it can say with its spaces around it (" 120 fps "), and
// constant so the border never has to be laid out again.
constexpr int kReadoutWidth = 9;

// The picture `pixels` becomes when the host will not accept one that
// large. A limit of {0, 0} — nothing reported — is no limit.
//
// A host that states a maximum Sixel geometry means it: a picture past it
// is not clipped, it is refused, and the window shows the cell fallback
// where a picture should be. Scaling uniformly keeps the proportions the
// cell box was measured for, so the terminal layer still spreads the
// smaller picture across that whole box; the only thing given up is
// sharpness, and only on a host that asked for it.
Size scaled_within(Size pixels, Size limit) noexcept {
    double scale = 1.0;
    if (limit.width > 0 && pixels.width > limit.width)
        scale = std::min(scale, static_cast<double>(limit.width) / pixels.width);
    if (limit.height > 0 && pixels.height > limit.height)
        scale = std::min(scale, static_cast<double>(limit.height) / pixels.height);
    if (scale >= 1.0) return pixels;
    return Size{std::max(1, static_cast<int>(pixels.width * scale)),
                std::max(1, static_cast<int>(pixels.height * scale))};
}

const ShapeEntry& catalog_entry(ShapeId shape) noexcept {
    const std::span<const ShapeEntry> catalog = shape_catalog();
    const auto index = static_cast<std::size_t>(shape);
    return catalog[index < catalog.size() ? index : 0];
}

}  // namespace

SpinView::SpinView(const widgets::Window& window, ShapeId shape, std::int64_t phase_nanos)
    : window_(window), shape_(shape), phase_nanos_(phase_nanos) {}

void SpinView::on_attached() {
    // The base class resolves its own fallback role first; overriding it
    // afterwards is what keeps the surface under (and instead of) a
    // picture part of the window.
    ImageView::on_attached();
    frame_active_role_ = context().roles->find("ckv.window.frame.active");
    frame_inactive_role_ = context().roles->find("ckv.window.frame.inactive");
    adopt_surface_role();
}

ui::RoleId SpinView::surface_role() const noexcept {
    return window_.active() ? frame_active_role_ : frame_inactive_role_;
}

void SpinView::adopt_surface_role() noexcept {
    const ui::RoleId role = surface_role();
    if (role != ui::kInvalidRole) set_role_override(role);
}

Image::Rgba SpinView::surface_color() const {
    const ui::RoleId role = surface_role();
    if (context().theme == nullptr || role == ui::kInvalidRole) return Image::Rgba{0, 0, 0, 255};
    // A style's background may name a palette entry rather than carry
    // channels, and a picture needs channels. resolved_color is the one
    // place that answers that question, so the raster and the cells
    // around it cannot disagree about what "blue" was.
    const Color background = resolved_color(context().theme->resolve(role).bg, Color::rgb(0, 0, 0));
    return Image::Rgba{background.r(), background.g(), background.b(), 255};
}

Size SpinView::target_pixels() const {
    const ui::Application* const app = context().app;
    if (app == nullptr) return Size{};
    Size cell = app->terminal_cell_pixels();
    // A terminal that draws pictures but never measured its cell is
    // Canvas's case exactly, and it answers it with the same assumption
    // rather than with an empty picture.
    if (cell.width <= 0 || cell.height <= 0) cell = widgets::kAssumedCellPixels;
    return scaled_within(Size{bounds().width * cell.width, bounds().height * cell.height},
                         app->terminal_capabilities().sixel_max_geometry);
}

void SpinView::request_frame(RenderService& service, std::int64_t now_nanos) {
    // The back-pressure, and the whole of it: while a frame is being
    // rendered this view asks for nothing. Ticks are opportunities, not
    // obligations.
    if (in_flight_) return;
    const ui::Application* const app = context().app;
    if (app == nullptr) return;
    const Size pixels = target_pixels();
    if (pixels.width <= 0 || pixels.height <= 0) return;

    adopt_surface_role();

    FrameSpec spec;
    spec.pixels = pixels;
    spec.background = surface_color();
    // Angles come from the clock, so a frame that took too long leaves a
    // gap in the animation rather than slowing the rotation down.
    const double seconds = static_cast<double>(now_nanos - phase_nanos_) * 1e-9;
    spec.yaw = seconds * kYawRateRadiansPerSecond;
    spec.pitch = seconds * kPitchRateRadiansPerSecond;
    const int registers = app->terminal_capabilities().sixel_color_registers;
    spec.color_budget = registers > 0 ? registers : 256;

    in_flight_ = true;
    service.submit(shape_, spec, lifetime_token(),
                   [this](std::shared_ptr<const Image> frame) { accept_frame(std::move(frame)); });
}

double SpinView::frames_per_second() const noexcept {
    return smoothed_interval_seconds_ > 0.0 ? 1.0 / smoothed_interval_seconds_ : 0.0;
}

void SpinView::accept_frame(std::shared_ptr<const Image> frame) {
    in_flight_ = false;
    if (frame == nullptr) return;
    frame_pixels_ = Size{frame->width(), frame->height()};
    ++frames_shown_;

    // Measuring the rate is what an unattached view cannot do; showing the
    // frame is not. Publishing comes first for that reason — a frame is
    // never withheld because something about the readout was unavailable.
    if (context().app != nullptr) {
        const std::int64_t now = context().app->clock().now_nanos();
        if (last_frame_nanos_ && now > *last_frame_nanos_) {
            const double interval = static_cast<double>(now - *last_frame_nanos_) * 1e-9;
            // Smoothed rather than instantaneous: the rate is there to be
            // read, and a readout flickering between 9 and 21 says less
            // than a steady 15 does.
            smoothed_interval_seconds_ = smoothed_interval_seconds_ > 0.0
                                             ? smoothed_interval_seconds_ * 0.75 + interval * 0.25
                                             : interval;
        }
        last_frame_nanos_ = now;
    }

    // One call, and the framework does the rest: this view is invalidated,
    // its window repaints its own backing store, and the Presenter
    // re-encodes exactly one raster region.
    set_image(std::move(frame));
    // Anything showing a number about this animation is repainted with the
    // frame it describes, in the same batch and therefore in the same
    // presented frame — never on a clock of its own.
    if (on_frame_shown) on_frame_shown();
}

FrameReadout::FrameReadout(const widgets::Window& window, int width, std::function<std::string()> text)
    : window_(window), width_(std::max(1, width)), text_(std::move(text)) {}

void FrameReadout::on_attached() {
    frame_active_role_ = context().roles->find("ckv.window.frame.active");
    frame_inactive_role_ = context().roles->find("ckv.window.frame.inactive");
}

ui::SizeHint FrameReadout::horizontal_size_hint() const {
    // The same width whatever the text says. A frame overlay is placed
    // from this hint, so a hint that tracked the text would move the
    // readout every time the number changed width.
    return ui::SizeHint{width_, width_, width_};
}

ui::SizeHint FrameReadout::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void FrameReadout::draw(scene::Painter& painter) {
    const ui::RoleId role = window_.active() ? frame_active_role_ : frame_inactive_role_;
    if (context().theme == nullptr || role == ui::kInvalidRole) return;
    // The window's own frame style, so this reads as part of the border
    // whichever theme is loaded and whether or not the window is active.
    const Style style = context().theme->resolve(role);
    // Pulled here, in the paint that will show it — see this class's own
    // comment for why it is not pushed.
    const std::string text = text_ ? text_() : std::string();
    const int width = text::text_width(text);
    if (width <= 0) return;
    // One space either side and no more, the way a window sets its own
    // title into the top border. Only those cells are painted: whatever
    // the reserved box does not need stays border line, so the readout
    // reads as a label on the frame rather than as a gap cut out of it.
    const int run = width + 2;
    const int x = std::max(0, bounds().width - run);
    painter.fill(Rect{x, 0, run, 1}, Cell::from_grapheme(" ", style));
    painter.draw_text(Point{x + 1, 0}, text, style);
}

SpinApp::SpinApp(ui::Application& app)
    : app_(app),
      roles_(ui::intern_standard_roles(app.roles())),
      frames_(app, meshes_, RenderService::default_worker_count()) {
    std::vector<widgets::MenuItem> new_window_items;
    for (const ShapeEntry& entry : shape_catalog()) {
        const ui::CommandId command = app_.commands().declare(ui::CommandDescriptor{
            .key = std::string(entry.command_key),
            .title = std::string(entry.menu_label),
            .category = "Object",
            .handler = [this, shape = entry.id] { (void)open_window(shape); },
        });
        commands_[static_cast<std::size_t>(entry.id)] = command;
        new_window_items.push_back(widgets::MenuItem{widgets::CommandPresentation{command}});
    }

    widgets::MenuBarItem file_menu{
        "&File",
        {
            widgets::MenuItem{"&New", ui::kInvalidCommand, {}, false, widgets::MenuItemCheck::None,
                              std::move(new_window_items)},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().close}},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().quit}},
        },
    };
    widgets::MenuBarItem window_menu{
        "&Window",
        {
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().next_window}},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().previous_window}},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().zoom}},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().tile}},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().cascade}},
            widgets::MenuItem{"", ui::kInvalidCommand, {}, true},
            widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().window_list}},
        },
    };
    widgets::MenuBarItem help_menu{
        "&Help",
        {widgets::MenuItem{widgets::CommandPresentation{app_.commands().standard().help,
                                                        "&About..."}}},
    };

    widgets::ApplicationShell shell(
        app_, {.theme = ui::make_classic_theme(app_.roles(), roles_),
               .menus = {std::move(file_menu), std::move(window_menu), std::move(help_menu)},
               .status_items = {
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().next_window}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().tile}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().quit}},
               }});
    desktop_ = &shell.desktop();

    // Not install_about_help: the box this application wants to show also
    // states what the terminal said about pictures, and that is only true
    // as of the moment F1 is pressed — capability probes answer after the
    // first frames are already on screen.
    app_.commands().set_handler(app_.commands().standard().help, [this] { show_about(); });

    (void)open_window(ShapeId::SolidCube);
}

SpinApp::~SpinApp() {
    // Everything this object installed elsewhere outlives it otherwise:
    // Application owns the timer and the command registry, and its View
    // tree owns the windows. Each of the three holds a callback that
    // captures `this`, so each of the three is taken back here rather
    // than left pointing at freed storage.
    if (timer_) app_.cancel_timer(*timer_);
    for (const ui::CommandId command : commands_)
        if (command != ui::kInvalidCommand) app_.commands().set_handler(command, {});
    // Including the standard help command, whose handler this application
    // replaced with one of its own (see the constructor).
    app_.commands().set_handler(app_.commands().standard().help, {});
    for (const Panel& panel : panels_)
        if (!panel.liveness.expired()) (void)desktop_->remove_window(panel.window);
}

std::string SpinApp::graphics_summary() const {
    const term::Capabilities caps = app_.terminal_capabilities();
    // The one question a reader of a graphics application actually asks
    // when a window shows the fallback: is that the terminal or is it me?
    // The answer is the evidence, not a guess — the same facts
    // term::capability_report() names, in the words of this application.
    if (!caps.sixel_graphics)
        return "This terminal reports no Sixel graphics, so every window shows the documented cell "
               "fallback instead of a picture. Frames are still rendered.";

    const auto size_text = [](Size size) {
        return std::to_string(size.width) + "x" + std::to_string(size.height);
    };
    std::string summary = "This terminal draws Sixel graphics";
    if (caps.cell_pixels.width > 0 && caps.cell_pixels.height > 0)
        summary += ", cell " + size_text(caps.cell_pixels) + " px";
    if (caps.sixel_color_registers > 0)
        summary += ", " + std::to_string(caps.sixel_color_registers) + " colour registers";
    if (caps.sixel_max_geometry.width > 0 || caps.sixel_max_geometry.height > 0)
        summary += ", largest picture " + size_text(caps.sixel_max_geometry) +
                   " px — frames are rendered within that limit rather than refused by it";
    summary += ".";
    if (const SpinView* const view = view_at(0); view != nullptr && view->frames_shown() > 0)
        summary += " This window's frames are " + size_text(view->frame_pixels()) + " px.";
    return summary;
}

void SpinApp::show_about() {
    widgets::MessageBoxDescriptor descriptor{
        widgets::MessageBoxKind::Info, "About",
        "ckVision Spin example\n\nA window per rotating solid. Every frame is drawn on a worker "
        "thread, handed back through Application::post, and shown by invalidating one view — no "
        "polling, and no forced redraw. Drag a window's corner to resize it: the next frame is "
        "rendered at the new pixel size, on the window's own background.\n\n" +
            graphics_summary(),
        widgets::MessageBoxButtons::Ok};
    descriptor.emphasized_leading_lines = 1;
    auto presentation = widgets::present_message_box(app_, *desktop_, roles_, descriptor);
    presentation.set_completion_handler([](widgets::MessageBoxResult) {});
}

ui::CommandId SpinApp::new_window_command(ShapeId shape) const noexcept {
    const auto index = static_cast<std::size_t>(shape);
    return index < commands_.size() ? commands_[index] : ui::kInvalidCommand;
}

SpinView* SpinApp::view_at(std::size_t index) const noexcept {
    return index < panels_.size() ? panels_[index].view : nullptr;
}

FrameReadout* SpinApp::readout_at(std::size_t index) const noexcept {
    return index < panels_.size() ? panels_[index].readout : nullptr;
}

Rect SpinApp::next_window_bounds() const {
    const Rect area = desktop_->content_area();
    const int width = std::clamp(area.width * 2 / 5, 20, 54);
    const int height = std::clamp(area.height * 2 / 3, 7, 18);
    // A short cascade so a second window is visibly a second window,
    // wrapping before it can walk off the desktop.
    const int step = opened_ % 6;
    const int x = area.x + std::clamp(2 + step * 4, 0, std::max(0, area.width - width));
    const int y = area.y + std::clamp(1 + step * 2, 0, std::max(0, area.height - height));
    return Rect{x, y, std::min(width, area.width), std::min(height, area.height)};
}

widgets::Window* SpinApp::open_window(ShapeId shape) {
    const ShapeEntry& entry = catalog_entry(shape);
    auto window = std::make_unique<widgets::Window>(std::string(entry.title));
    window->set_bounds(next_window_bounds());
    window->set_min_size(Size{18, 6});
    ++opened_;

    // Each window starts its own shape facing forward, so opening a second
    // one is not a copy of the first at a different angle by accident.
    auto view = std::make_unique<SpinView>(*window, shape, app_.clock().now_nanos());
    SpinView* const spin = view.get();
    window->set_content(std::move(view));

    // The delivered frame rate, on the bottom border where a window's own
    // indicators belong. It reads the view when it paints; the view tells
    // it when to paint.
    FrameReadout* const readout = window->add_frame_overlay(
        std::make_unique<FrameReadout>(*window, kReadoutWidth,
                                       [spin] {
                                           const double rate = spin->frames_per_second();
                                           if (rate <= 0.0) return std::string("-- fps");
                                           return std::to_string(static_cast<int>(std::lround(rate))) + " fps";
                                       }),
        // Two cells further in than the border's own margin: the corner
        // already carries a resize grip, and a readout crowding it reads as
        // part of the control rather than as a label on the frame.
        widgets::FrameSlot{widgets::Edge::Bottom, ui::Alignment::End, /*offset=*/-2});
    spin->on_frame_shown = [readout] { readout->invalidate(); };

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        forget(opened);
        widgets::schedule_self_detach(*opened, app_);
    };
    desktop_->add_window(std::move(window));

    panels_.push_back(Panel{opened, spin, readout, opened->lifetime_token()});
    arm_next_tick();
    // The first frame does not wait for the first tick: a window that
    // opened empty and filled a frame later would read as a slow program.
    spin->request_frame(frames_, app_.clock().now_nanos());
    return opened;
}

void SpinApp::forget(widgets::Window* window) {
    std::erase_if(panels_, [window](const Panel& panel) { return panel.window == window; });
    if (panels_.empty()) stop_animation_clock();
}

void SpinApp::stop_animation_clock() {
    if (timer_) {
        app_.cancel_timer(*timer_);
        timer_.reset();
    }
    // The next window starts from the target rate rather than inheriting
    // whatever the last overloaded moment settled on.
    interval_nanos_ = kFrameIntervalNanos;
    last_tick_nanos_.reset();
}

void SpinApp::arm_next_tick() {
    // Self-clocking, and the reason this is a one-shot rather than a
    // repeating timer: the next tick is scheduled once the previous one
    // has run, so the loop cannot accumulate a backlog of ticks it owes
    // while it was busy. A repeating timer reschedules from its own fire
    // time and would deliver that backlog all at once afterwards.
    if (panels_.empty() || timer_) return;
    timer_ = app_.start_timer(interval_nanos_, /*repeating=*/false, [this] { tick(); });
}

void SpinApp::pace(std::int64_t now_nanos) {
    const std::int64_t observed = now_nanos - *last_tick_nanos_;
    last_tick_nanos_ = now_nanos;
    if (observed <= 0) return;

    if (observed > interval_nanos_ * 3 / 2) {
        // The loop is running late, and the only honest response is to ask
        // for less. Asking at the rate it is actually managing leaves
        // everything it was busy with — keystrokes, a window drag, a large
        // raster to encode — the room it was already taking.
        interval_nanos_ = std::min(kSlowestFrameIntervalNanos, observed);
    } else if (observed >= interval_nanos_ * 3 / 4) {
        // On time: ease back towards the target, geometrically rather than
        // at once, so one quiet tick after a busy stretch does not put the
        // load straight back.
        interval_nanos_ = std::max(kFrameIntervalNanos, interval_nanos_ * 7 / 8);
    }
    // Anything much EARLIER than asked for is not a measurement of the
    // loop's health at all — it is a clock that jumped, or a host that
    // stepped this application several times in a row — so it neither
    // widens nor narrows the interval.
}

void SpinApp::tick() {
    timer_.reset();  // a one-shot: it has fired, and is no longer armed
    // A window may have been destroyed by something other than its own
    // close protocol (a quit sweep, a host tearing the tree down). The
    // liveness token is what makes that safe to discover here rather than
    // something every removal path has to remember to report.
    std::erase_if(panels_, [](const Panel& panel) { return panel.liveness.expired(); });
    if (panels_.empty()) {
        stop_animation_clock();
        return;
    }
    const std::int64_t now = app_.clock().now_nanos();
    if (last_tick_nanos_) pace(now);
    else last_tick_nanos_ = now;
    for (const Panel& panel : panels_) panel.view->request_frame(frames_, now);
    arm_next_tick();
}

}  // namespace ckv::spin
