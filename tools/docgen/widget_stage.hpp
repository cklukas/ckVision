// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation tooling, NOT part of the cvision library.
//
// A small stage on which one widget can be posed for its portrait. It
// builds the same Application -> Desktop -> Window graph a client
// writes, dresses it in the classic theme, and then writes a CUT-OUT of
// the composed screen rather than the whole terminal: a figure the size
// of the control, taken out of a screen that really had a desktop, a
// window frame and neighbouring chrome in it. That is the difference
// between a widget photographed in its habitat and a widget rendered
// alone in a terminal the size of itself -- only the first tells a
// reader what the thing will look like in their application.
//
// Every scene here goes through public API only, and through the real
// Presenter/HeadlessTerminal path, exactly as the example-app captures
// do: nothing reads widget internals, and nothing draws a mock-up.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::docgen {

// Whether the staged terminal claims Sixel support. Most controls are
// cells only and are captured on the no-graphics profile, which is also
// what every documentation capture before this one used; the two
// raster-bearing widgets need the other one.
enum class StageGraphics { Off, On };

class WidgetStage {
public:
    explicit WidgetStage(Size screen = Size{80, 24}, StageGraphics graphics = StageGraphics::Off);

    ui::Application& app() noexcept { return app_; }
    widgets::Desktop& desktop() noexcept { return *desktop_; }
    term::HeadlessTerminal& terminal() noexcept { return terminal_; }

    // Adds a window with an empty content view and returns that view,
    // which is where a client puts its controls. The window becomes the
    // one `save_window()` frames.
    ui::View& window(std::string title, Rect bounds);

    // The same, dressed in the DIALOG roles: a light body with dark
    // text, rather than the deep blue a plain window paints. That is
    // where form controls belong -- the classic input, label and option
    // roles are all chosen against a dialog body, and an input field on
    // a plain window background is white text on the same blue the
    // window itself is. The Forms example does exactly this
    // (`examples/forms/forms_app.cpp`), and the gallery says so.
    ui::View& dialog_window(std::string title, Rect bounds);

    const ui::StandardRoles& roles() const noexcept { return roles_; }

    // Adds a window whose content is the given view (a widget that fills
    // its window, such as an editor or a table).
    widgets::Window& window_with_content(std::string title, Rect bounds,
                                         std::unique_ptr<ui::View> content);

    // Focuses `view` when it can take focus, and does nothing when it
    // cannot. A scene says "show this one focused" without also having
    // to know which views are tab stops -- a decorative view (a
    // breadcrumb trail, a progress bar) simply keeps the frame it would
    // have had, instead of tripping the Application's focusable
    // precondition and killing the whole capture run.
    void focus(ui::View* view);

    // Composes one frame. Call after building, and again after any input.
    void step();

    // Writes `<name>.svg` under `dir` -- the whole composed screen.
    void save(const std::filesystem::path& dir, std::string_view name);

    // Writes a cut-out: `crop` is in cells, in screen coordinates.
    void save(const std::filesystem::path& dir, std::string_view name, Rect crop);

    // Writes the cut-out around the last window, one cell of desktop on
    // each side so the frame is not flush with the figure's edge and the
    // cast shadow is inside it.
    void save_window(const std::filesystem::path& dir, std::string_view name);

    // Writes the cut-out around whichever window the desktop has made
    // active -- for a dialog the library presented, which the scene
    // never holds a pointer to.
    void save_active_window(const std::filesystem::path& dir, std::string_view name);

    // Writes the cut-out around a rect given in the last window's
    // CONTENT coordinates -- how a scene frames one control inside a
    // window that holds several, without the caller doing the arithmetic
    // that turns content coordinates into screen ones.
    void save_content(const std::filesystem::path& dir, std::string_view name, Rect content_rect,
                      int margin = 1);

    // The screen rect of a rect given in the last window's content
    // coordinates.
    Rect content_to_screen(Rect content_rect) const;

private:
    term::HeadlessTerminal terminal_;
    ManualClock clock_;
    ui::Application app_;
    ui::StandardRoles roles_{};
    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    ui::View* content_ = nullptr;
};

// The number of screenshots written so far, for the capture tool's own
// summary line -- a capture run that silently wrote nothing is the one
// failure mode a per-file "wrote ..." line does not make obvious.
int screenshots_written() noexcept;

}  // namespace ckv::docgen
