// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A documentation-only visual for navigation components that do not fit the
// application examples naturally.  It uses only public ckVision APIs and still
// paints through Application, Presenter, HeadlessTerminal, and VirtualDisplay.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"
#include "frame_svg.hpp"

namespace {
void write_svg(const std::filesystem::path& dir, const ckv::term::VirtualDisplay& display) {
    const std::filesystem::path path = dir / "widget-navigation.svg";
    std::ofstream out(path);
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", path.string().c_str(), display.size().width, display.size().height);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    auto desktop = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* desktop_ptr = desktop.get();
    app.root().add_child(std::move(desktop));

    auto window = std::make_unique<ckv::widgets::Window>("Navigation components");
    window->set_bounds(ckv::Rect{2, 2, 76, 20});
    auto content = std::make_unique<ckv::ui::View>();

    auto calendar = std::make_unique<ckv::widgets::CalendarView>();
    calendar->set_bounds(ckv::Rect{1, 1, 31, 10});
    calendar->set_month(ckv::widgets::DateValue{2026, 8, 1});
    calendar->set_selected(ckv::widgets::DateValue{2026, 8, 9});
    calendar->set_today(ckv::widgets::DateValue{2026, 8, 9});
    content->add_child(std::move(calendar));

    auto scrollbar = std::make_unique<ckv::widgets::Scrollbar>(ckv::widgets::Orientation::Vertical);
    scrollbar->set_bounds(ckv::Rect{34, 1, 1, 12});
    scrollbar->set_range(100, 20);
    scrollbar->set_position(35);
    content->add_child(std::move(scrollbar));

    auto viewport = std::make_unique<ckv::widgets::ScrollViewport>();
    viewport->set_bounds(ckv::Rect{38, 1, 34, 12});
    auto scroll_content = std::make_unique<ckv::ui::View>();
    scroll_content->set_preferred_size(ckv::Size{50, 20});
    auto text = std::make_unique<ckv::widgets::StaticText>(
        "ScrollViewport clips a larger content view. Its owned scrollbars track the visible range.");
    text->set_bounds(ckv::Rect{0, 0, 48, 20});
    scroll_content->add_child(std::move(text));
    viewport->set_content(std::move(scroll_content));
    // Keep the explanatory first line visible in the documentation frame;
    // the viewport's owned bars still show the larger scrollable extent.
    viewport->set_scroll(0, 0);
    content->add_child(std::move(viewport));

    window->set_content(std::move(content));
    desktop_ptr->add_window(std::move(window));
    app.step(0);
    write_svg(out_dir, term.display());
    return 0;
}
