// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Appearance specimens: media — the views that draw pictures: an image, an
// application's canvas, a document with inline pictures, and a contained
// terminal whose child draws one.
//
// A raster element is seen three ways: `normal` is the view on the default
// (no-graphics) profile with no picture of the application's in it, `sixel`
// is its picture decoded, and `no-graphics` is the same scene where the
// terminal cannot draw it. Every picture is generated here, in integers, so
// the decoded plane is the same on every host.
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "appearance_matrix.hpp"
#include "cvision/core/image.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/terminal_view.hpp"

namespace ckv::docgen::appearance {

namespace {

// Raster screens stay small: a decoded plane is 9 x 18 pixels a cell.
constexpr Size kScreen{24, 8};
constexpr Rect kWindow{1, 1, 21, 6};
// The same window, too narrow for the specimen's text.
constexpr Rect kNarrowWindow{1, 1, 10, 6};

// Shows `view` as the content of a document window and returns it.
template <class T>
T& shown(Stage& stage, std::string title, std::unique_ptr<T> view, Rect bounds = kWindow) {
    T& placed = *view;
    stage.window(bounds, std::move(title), std::move(view), false);
    return placed;
}

std::uint8_t ramp(int at, int extent) {
    return static_cast<std::uint8_t>(extent > 1 ? 255 * at / (extent - 1) : 0);
}

// A colour ramp in a dark frame: red grows to the right, green downwards, so
// scaling, cropping and letterboxing all show in the decoded plane.
std::shared_ptr<const Image> picture(int width, int height) {
    auto image = std::make_shared<Image>(PixelSize{width, height});
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const bool frame = x < 2 || y < 2 || x >= width - 2 || y >= height - 2;
            image->set_pixel(x, y, frame ? Image::Rgba{24, 24, 48, 255}
                                         : Image::Rgba{ramp(x, width), ramp(y, height), 160, 255});
        }
    return image;
}

// --- ImageView ------------------------------------------------------------

widgets::ImageView& image_view(Stage& stage, std::shared_ptr<const Image> image) {
    auto view = std::make_unique<widgets::ImageView>();
    view->set_image(std::move(image));
    return shown(stage, "Preview", std::move(view));
}

void add_image_view(Catalog& catalog) {
    Element& e = catalog.element("ImageView", "include/cvision/widgets/image_view.hpp", Traits{.raster = true});
    state(e, "normal", kScreen, [](Stage& s) { image_view(s, nullptr); });
    state(e, "sixel", kScreen, [](Stage& s) { image_view(s, picture(64, 32)); }, Graphics::Sixel);
    state(e, "no-graphics", kScreen, [](Stage& s) { image_view(s, picture(64, 32)); });
    state(e, "stretched", kScreen, [](Stage& s) { image_view(s, picture(64, 32)).set_stretch_to_fill(true); },
          Graphics::Sixel);
}

// --- Canvas ---------------------------------------------------------------

// A bar chart drawn straight into the canvas's backing image.
void draw_chart(Image& image) {
    constexpr int kBars[] = {3, 5, 4, 7, 6, 8};
    constexpr int kBarCount = static_cast<int>(sizeof kBars / sizeof kBars[0]);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int bar = x * kBarCount / image.width();
            const int left = bar * image.width() / kBarCount;
            const bool gap = x - left < 3;
            const bool filled = !gap && (image.height() - y) * 8 <= kBars[bar] * image.height();
            image.set_pixel(x, y, filled ? Image::Rgba{80, 200, 150, 255} : Image::Rgba{16, 24, 40, 255});
        }
}

// Alternating pixels: a backing image a few pixels across, shown in a box
// of many cells, is scaled up by the terminal layer and reads as blocks.
void draw_checker(Image& image) {
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            image.set_pixel(x, y, (x + y) % 2 == 0 ? Image::Rgba{230, 160, 40, 255} : Image::Rgba{40, 40, 90, 255});
}

widgets::Canvas& canvas(Stage& stage) {
    auto view = std::make_unique<widgets::Canvas>();
    view->set_cell_metrics(stage.app().terminal_cell_pixels());
    view->set_draw_callback(draw_chart);
    return shown(stage, "Signal", std::move(view));
}

// The fallback an application gives its canvas: what the chart says.
void chart_fallback(widgets::Canvas& view) {
    view.set_fallback_painter([](scene::Painter& painter, Rect area) {
        painter.draw_text(Point{0, area.height / 2}, "Peak 8 at bar 6", Style{});
    });
}

void add_canvas(Catalog& catalog) {
    Element& e = catalog.element("Canvas", "include/cvision/widgets/canvas.hpp", Traits{.raster = true});
    state(e, "normal", kScreen, [](Stage& s) { canvas(s); });
    state(e, "sixel", kScreen, [](Stage& s) { chart_fallback(canvas(s)); }, Graphics::Sixel);
    state(e, "no-graphics", kScreen, [](Stage& s) { chart_fallback(canvas(s)); });
    state(e, "pixel-size", kScreen, [](Stage& s) {
        widgets::Canvas& view = canvas(s);
        view.set_pixel_size(PixelSize{19, 8});
        view.set_draw_callback(draw_checker);
    }, Graphics::Sixel);
}

// --- FlowView -------------------------------------------------------------

widgets::FlowBlock heading(std::string text) {
    widgets::FlowBlock block;
    block.content.push_back(widgets::FlowText{std::move(text), Attr::Bold, std::nullopt});
    return block;
}

widgets::FlowBlock paragraph() {
    widgets::FlowBlock block;
    block.content.push_back(widgets::FlowText{"Read the ", Attr{}, std::nullopt});
    block.content.push_back(widgets::FlowText{"guide", Attr{}, std::string("guide.md")});
    block.content.push_back(widgets::FlowText{" or the ", Attr{}, std::nullopt});
    block.content.push_back(widgets::FlowText{"notes", Attr{}, std::string("notes.md")});
    block.content.push_back(widgets::FlowText{".", Attr{}, std::nullopt});
    return block;
}

// A heading, a picture as its own run of rows, and a paragraph.
widgets::FlowDocument illustrated() {
    widgets::FlowBlock figure;
    figure.content.push_back(widgets::FlowImage{picture(64, 32), Size{8, 2}, "[chart]"});
    widgets::FlowDocument document;
    document.blocks = {heading("Release 0.4"), std::move(figure), paragraph()};
    return document;
}

widgets::FlowDocument text_only(std::string title = "Release 0.4") {
    widgets::FlowDocument document;
    document.blocks = {heading(std::move(title)), paragraph()};
    return document;
}

widgets::FlowView& flow(Stage& stage, widgets::FlowDocument document, Rect bounds = kWindow) {
    auto view = std::make_unique<widgets::FlowView>();
    view->set_document(std::move(document));
    return shown(stage, "Help", std::move(view), bounds);
}

void add_flow_view(Catalog& catalog) {
    Element& e = catalog.element("FlowView", "include/cvision/widgets/flow_view.hpp",
                                 Traits{.focusable = true, .text = true, .raster = true});
    state(e, "normal", kScreen, [](Stage& s) { flow(s, text_only()); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(flow(s, text_only())); });
    state(e, "current-link", kScreen, [](Stage& s) {
        widgets::FlowView& view = flow(s, text_only());
        view.set_current_link(1);
        s.focus(view);
    });
    state(e, "sixel", kScreen, [](Stage& s) { flow(s, illustrated()); }, Graphics::Sixel);
    state(e, "no-graphics", kScreen, [](Stage& s) { flow(s, illustrated()); });
    state(e, "wide", kScreen, [](Stage& s) { s.focus(flow(s, text_only(std::string(kWideText)))); });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(flow(s, text_only(), kNarrowWindow)); });
}

// --- TerminalView ---------------------------------------------------------

// The child a Console hosts: a terminal emulator fed bytes in process, as a
// program's output would reach it. A base declared ahead of the view is
// built before the view borrows it and destroyed after the view has gone,
// which is the lifetime TerminalView asks of its session.
struct ConsoleChild {
    term::TerminalEmulator child;
};

// A window's content: one TerminalView over a child of its own.
class Console final : private ConsoleChild, public ui::View {
public:
    // The identity the child's pictures are placed under; a raster needs one
    // that is not zero to be drawn.
    static constexpr int kPictures = 1;

    Console() : terminal_(make<widgets::TerminalView>(child)) { child.set_raster_identity(kPictures); }

    widgets::TerminalView& terminal() noexcept { return *terminal_; }

    // Output from the child's program, delivered as a real session delivers it.
    void write(std::string_view bytes) {
        child.feed_output(bytes);
        notify_terminal_subsession_changed(child);
    }

    void on_resized() override { terminal_->set_bounds(Rect{0, 0, bounds().width, bounds().height}); }

private:
    widgets::TerminalView* terminal_;
};

constexpr std::string_view kSession = "ckvision$ ls\r\ndocs  include  src\r\nckvision$ ";

// Three bands, each two Sixel rows (twelve pixels) tall and 72 pixels wide:
// a picture eight cells across and two cells high, wide enough for the
// "[sixel]" the view shows in its place without graphics.
constexpr std::string_view kChildPicture =
    "picture:\r\n"
    "\x1bPq#0;2;100;20;20#1;2;20;80;20#2;2;20;40;100"
    "#0!72~-#0!72~-#1!72~-#1!72~-#2!72~-#2!72~\x1b\\";

Console& console(Stage& stage, std::string_view output = kSession, Rect bounds = kWindow) {
    Console& placed = shown(stage, "Terminal", std::make_unique<Console>(), bounds);
    placed.write(output);
    return placed;
}

void add_terminal_view(Catalog& catalog) {
    Element& e = catalog.element("TerminalView", "include/cvision/widgets/terminal_view.hpp",
                                 Traits{.focusable = true, .text = true, .raster = true});
    state(e, "normal", kScreen, [](Stage& s) { console(s); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(console(s).terminal()); });
    state(e, "colours", kScreen, [](Stage& s) {
        s.focus(console(s, "\x1b[32mpass\x1b[0m 148 \x1b[1;31mfail\x1b[0m 2\r\n"
                           "\x1b[7m reverse \x1b[0m \x1b[4munder\x1b[0m\r\n"
                           "\x1b[44;97m status \x1b[0m\r\n$ ")
                    .terminal());
    });
    state(e, "scrolled", kScreen, [](Stage& s) {
        std::string output;
        for (int line = 1; line <= 9; ++line) output += "line " + std::to_string(line) + "\r\n";
        Console& view = console(s, output + "$ ");
        view.terminal().set_scrollback_offset(3);
    });
    state(e, "sixel", kScreen, [](Stage& s) { console(s, kChildPicture); }, Graphics::Sixel);
    state(e, "no-graphics", kScreen, [](Stage& s) { console(s, kChildPicture); });
    state(e, "wide", kScreen, [](Stage& s) {
        s.focus(console(s, "$ cat title\r\n" + std::string(kWideText) + "\r\n$ ").terminal());
    });
    state(e, "narrow", kScreen, [](Stage& s) { s.focus(console(s, kSession, kNarrowWindow).terminal()); });
}

}  // namespace

void add_media_specimens(Catalog& catalog) {
    add_image_view(catalog);
    add_canvas(catalog);
    add_flow_view(catalog);
    add_terminal_view(catalog);
}

}  // namespace ckv::docgen::appearance
