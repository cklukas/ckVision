// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The composed surfaces: dialogs a client presents rather than builds,
// the wizard, and the two raster views (captured on a terminal that
// claims Sixel, since that is what they are for).
#include "widget_shots.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/core/image.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/date_time_dialog.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/terminal_report_dialog.hpp"
#include "cvision/widgets/theme_editor.hpp"
#include "cvision/widgets/window_list_dialog.hpp"
#include "widget_stage.hpp"

namespace ckv::docgen {
namespace {

// A small tree the file and directory dialogs can browse. Deterministic
// and injected, so the figures never depend on the machine that built
// them -- the same reason ckVision takes a FileSystem rather than
// calling the platform itself.
MemoryFileSystem demo_filesystem() {
    MemoryFileSystem fs;
    fs.add_directory("/project");
    fs.add_directory("/project/docs");
    fs.add_directory("/project/src");
    fs.add_file("/project/README.md", "# ckVision\n");
    fs.add_file("/project/release-notes.md", "0.4.0\n");
    fs.add_file("/project/docs/widget-gallery.md", "");
    fs.add_file("/project/src/main.cpp", "int main() {}\n");
    return fs;
}

std::shared_ptr<const Image> gradient_image(int width, int height) {
    auto image = std::make_shared<Image>(PixelSize{width, height});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto r = static_cast<std::uint8_t>(255 * x / (width - 1));
            const auto g = static_cast<std::uint8_t>(255 * y / (height - 1));
            image->set_pixel(x, y, Image::Rgba{r, g, static_cast<std::uint8_t>(255 - r), 255});
        }
    }
    return image;
}

void shot_message_box(const std::filesystem::path& dir) {
    WidgetStage stage;
    stage.window("Document", Rect{2, 2, 46, 12});

    // ckvision-doc: messagebox
    widgets::MessageBoxDescriptor descriptor{
        widgets::MessageBoxKind::Confirm, "Unsaved changes",
        "release-notes.md has been edited since it was last saved.\n"
        "Close it anyway?",
        widgets::MessageBoxButtons::YesNoCancel};

    widgets::MessageBoxPresentation box =
        widgets::present_modal_message_box(stage.app(), stage.desktop(), stage.roles(), descriptor);
    box.set_completion_handler([](widgets::MessageBoxResult result) {
        (void)result;  // Yes, No, or Cancel -- arrives after the box detaches
    });
    // ckvision-doc-end: messagebox

    stage.step();
    stage.save_active_window(dir, "widget-messagebox");
}

void shot_descriptor_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;
    stage.window("Document", Rect{2, 2, 40, 10});

    // ckvision-doc: dialogdescriptor
    widgets::DialogDescriptor descriptor;
    descriptor.title = "Export report";
    widgets::FieldDescriptor name;
    name.label = "&Name";
    name.initial_text = "release-notes";
    descriptor.fields.push_back(std::move(name));

    widgets::FieldDescriptor passphrase;
    passphrase.label = "&Passphrase";
    passphrase.password_echo = true;
    descriptor.fields.push_back(std::move(passphrase));

    widgets::FieldDescriptor format;
    format.label = "&Format";
    format.kind = widgets::FieldKind::Combo;
    format.options = {"PDF", "HTML", "Plain text"};
    format.initial_selection = 0;
    descriptor.fields.push_back(std::move(format));

    widgets::FieldDescriptor overwrite;
    overwrite.label = "&Overwrite an existing file";
    overwrite.kind = widgets::FieldKind::Check;
    overwrite.initial_checked = true;
    descriptor.fields.push_back(std::move(overwrite));

    descriptor.buttons = {
        widgets::ButtonDescriptor{"E&xport", widgets::ButtonRole::Accept, [] {}},
        widgets::ButtonDescriptor{"Cancel", widgets::ButtonRole::Dismiss, [] {}},
    };

    widgets::DescriptorDialogPresentation dialog =
        widgets::present_modal_dialog(std::move(descriptor), stage.app(), stage.desktop(), stage.roles());
    dialog.set_completion_handler([](widgets::DialogResult result) {
        (void)result;  // .accepted, plus one value per field
    });
    // ckvision-doc-end: dialogdescriptor

    stage.step();
    stage.save_active_window(dir, "widget-dialogdescriptor");
}

void shot_file_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;
    MemoryFileSystem fs = demo_filesystem();

    // ckvision-doc: filedialog
    widgets::FileDialogOptions options;
    options.filters = {widgets::FileDialogFilter{"Markdown", {".md"}},
                       widgets::FileDialogFilter{"All files", {}}};
    options.active_filter = 0;

    widgets::FileDialogPresentation picker = widgets::present_modal_file_dialog(
        widgets::FileDialogMode::Open, "/project", fs, options, stage.app(), stage.desktop(),
        stage.roles());
    picker.set_completion_handler([](widgets::FileDialogResult result) {
        (void)result;  // {accepted, path}
    });
    // ckvision-doc-end: filedialog

    stage.step();
    stage.save_active_window(dir, "widget-filedialog");
}

void shot_directory_picker(const std::filesystem::path& dir) {
    WidgetStage stage;
    MemoryFileSystem fs = demo_filesystem();

    // ckvision-doc: directorypicker
    widgets::DirectoryPickerPresentation picker = widgets::present_modal_directory_picker(
        fs, "/project", stage.app(), stage.desktop(), stage.roles());
    picker.set_completion_handler([](widgets::DirectoryPickerResult result) {
        (void)result;  // {accepted, path}
    });
    // ckvision-doc-end: directorypicker

    stage.step();
    stage.save_active_window(dir, "widget-directorypicker");
}

void shot_help_viewer(const std::filesystem::path& dir) {
    WidgetStage stage;
    static widgets::MemoryHelpProvider provider;

    // ckvision-doc: helpviewer
    provider.add_topic("gallery",
                       widgets::HelpTopic{"Widget gallery",
                                          {{"Every public widget, with a picture and the code that "
                                            "drew it. How they are arranged is the "},
                                           {"layout guide", "layout"},
                                           {"'s subject."}},
                                          {{"layout", "Layout guide"}, {"themes", "Themes"}}});
    provider.add_topic("layout", widgets::HelpTopic{"Layout guide", {{"Row, Column, Grid, Dock."}}, {}});

    widgets::HelpViewerPresentation help = widgets::present_modeless_help_viewer(
        provider, "gallery", stage.app(), stage.desktop(), stage.roles());
    help.set_completion_handler([](widgets::HelpViewerResult result) { (void)result; });
    // ckvision-doc-end: helpviewer

    stage.step();
    stage.save_active_window(dir, "widget-helpviewer");
}

void shot_window_list_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;
    for (const char* title : {"release-notes.md", "widget-gallery.md", "Terminal"}) {
        auto frame = std::make_unique<widgets::Window>(title);
        frame->set_bounds(Rect{2, 2, 30, 8});
        frame->set_content(std::make_unique<ui::View>());
        stage.desktop().add_window(std::move(frame));
    }

    // ckvision-doc: windowlistdialog
    widgets::WindowListDialogPresentation list =
        widgets::present_modal_window_list_dialog(stage.desktop(), stage.app(), stage.roles());
    list.set_completion_handler([](widgets::WindowListDialogResult result) { (void)result; });
    // ckvision-doc-end: windowlistdialog

    stage.step();
    stage.save_active_window(dir, "widget-windowlistdialog");
}

void shot_theme_editor(const std::filesystem::path& dir) {
    // A terminal two cells larger than the dialog each way, so the figure
    // holds the whole window, its shadow and a cell of desktop around it.
    WidgetStage stage(Size{82, 26});

    // ckvision-doc: themeeditor
    widgets::ThemeEditorPresentation editor =
        widgets::present_modal_theme_editor(stage.app().theme(), stage.app(), stage.desktop(), stage.roles());
    editor.set_completion_handler([&app = stage.app()](widgets::ThemeEditorResult result) {
        if (result.theme) app.set_theme(*result.theme);
    });
    // ckvision-doc-end: themeeditor

    stage.step();
    stage.save_active_window(dir, "widget-themeeditor");
}

void shot_terminal_report_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: terminalreportdialog
    widgets::TerminalReportDialogOptions options;
    options.mouse_reports_decoded = [] { return std::size_t{0}; };

    widgets::TerminalReportDialogPresentation report = widgets::present_modal_terminal_report_dialog(
        stage.desktop(), stage.app(), stage.roles(), options);
    report.set_completion_handler([](widgets::TerminalReportDialogResult result) { (void)result; });
    // ckvision-doc-end: terminalreportdialog

    stage.step();
    stage.save_active_window(dir, "widget-terminalreportdialog");
}

void shot_wizard(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: wizard
    auto wizard = std::make_unique<widgets::Wizard>();
    auto name = std::make_unique<widgets::InputLine>();
    name->set_text("Ledger");
    const widgets::InputLine* const name_field = name.get();
    wizard->set_pages({
        widgets::WizardPage{"Choose a name", [name_field] { return !name_field->text().empty(); }},
        widgets::WizardPage{"Pick a template", {}},
        widgets::WizardPage{"Confirm", {}},
    });
    wizard->set_page_content(0, std::move(name));
    widgets::WizardPresentation setup =
        widgets::present_modal_wizard(std::move(wizard), "Set up", stage.app(), stage.desktop(), stage.roles());
    setup.set_completion_handler([](widgets::WizardOutcome outcome) {
        (void)outcome;  // Finished: read the pages' fields; Cancelled: nothing happened
    });
    // ckvision-doc-end: wizard

    stage.step();
    stage.save_active_window(dir, "widget-wizard");
}

void shot_date_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: datedialog
    widgets::DateDialogOptions options;
    options.initial = widgets::DateValue{2026, 8, 19};
    options.today = widgets::DateValue{2026, 8, 9};  // the host's today, never a clock's
    options.maximum = widgets::DateValue{2026, 8, 28};
    options.labels.weekday_names = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};  // any language's
    widgets::DateDialogPresentation due =
        widgets::present_modal_date_dialog(stage.app(), stage.desktop(), stage.roles(), std::move(options));
    due.set_completion_handler([](widgets::DateDialogResult result) {
        if (result.accepted) (void)result.date;
    });
    // ckvision-doc-end: datedialog

    stage.step();
    stage.save_active_window(dir, "widget-datedialog");
}

void shot_time_dialog(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: timedialog
    widgets::TimeDialogOptions options;
    options.initial = widgets::TimeValue{21, 30, 0};
    options.show_seconds = false;
    options.hour_format = widgets::HourFormat::TwelveHour;
    options.labels.am = "AM";  // the host's words for the two halves of the day
    options.labels.pm = "PM";
    widgets::TimeDialogPresentation alarm =
        widgets::present_modal_time_dialog(stage.app(), stage.desktop(), stage.roles(), std::move(options));
    alarm.set_completion_handler([](widgets::TimeDialogResult result) {
        if (result.accepted) (void)result.time;
    });
    // ckvision-doc-end: timedialog

    stage.step();
    stage.save_active_window(dir, "widget-timedialog");
}

void shot_image_view(const std::filesystem::path& dir) {
    WidgetStage stage(Size{80, 24}, StageGraphics::On);
    ui::View& content = stage.window("Preview", Rect{22, 5, 34, 11});

    // ckvision-doc: imageview
    auto* preview = content.make<widgets::ImageView>();
    preview->set_bounds(Rect{1, 1, 30, 7});
    preview->set_image(gradient_image(240, 120));
    preview->set_stretch_to_fill(false);  // keep the picture's own aspect
    preview->on_click = [](const MouseEvent& event) {
        (void)event;  // carries both the cell and the pixel it was in
    };
    // ckvision-doc-end: imageview

    stage.step();
    stage.save_window(dir, "widget-imageview");
}

void shot_canvas(const std::filesystem::path& dir) {
    WidgetStage stage(Size{80, 24}, StageGraphics::On);
    ui::View& content = stage.window("Signal", Rect{22, 5, 34, 11});

    // ckvision-doc: canvas
    auto* canvas = content.make<widgets::Canvas>();
    canvas->set_bounds(Rect{1, 1, 30, 7});
    canvas->set_cell_metrics(stage.app().terminal_cell_pixels());
    canvas->set_pixel_size(term::cells_to_pixels(Size{30, 7}, stage.app().terminal_cell_pixels()));
    canvas->set_draw_callback([](Image& image) {
        for (int x = 0; x < image.width(); ++x) {
            const double phase = 6.283 * x / image.width();
            const int y = static_cast<int>((0.5 + 0.42 * std::sin(phase * 2)) * image.height());
            for (int thickness = 0; thickness < 2; ++thickness)
                image.set_pixel(x, std::min(image.height() - 1, y + thickness),
                                Image::Rgba{80, 220, 160, 255});
        }
    });
    canvas->set_fallback_painter([](scene::Painter& painter, Rect area) {
        painter.draw_text(Point{0, area.height / 2}, "[no graphics: 2 Hz sine]", Style{});
    });
    // ckvision-doc-end: canvas

    stage.step();
    stage.save_window(dir, "widget-canvas");

    // The same scene on a terminal that cannot draw pixels: the
    // fallback painter is the whole difference.
    WidgetStage plain(Size{80, 24}, StageGraphics::Off);
    ui::View& plain_content = plain.window("Signal", Rect{22, 5, 34, 11});
    auto* fallback = plain_content.make<widgets::Canvas>();
    fallback->set_bounds(Rect{1, 1, 30, 7});
    fallback->set_fallback_painter([](scene::Painter& painter, Rect area) {
        painter.draw_text(Point{0, area.height / 2}, "[no graphics: 2 Hz sine]", Style{});
    });
    plain.step();
    plain.save_window(dir, "widget-canvas-no-graphics");
}

}  // namespace

void capture_composite_shots(const std::filesystem::path& dir) {
    shot_message_box(dir);
    shot_descriptor_dialog(dir);
    shot_file_dialog(dir);
    shot_directory_picker(dir);
    shot_help_viewer(dir);
    shot_window_list_dialog(dir);
    shot_theme_editor(dir);
    shot_terminal_report_dialog(dir);
    shot_wizard(dir);
    shot_date_dialog(dir);
    shot_time_dialog(dir);
    shot_image_view(dir);
    shot_canvas(dir);
}

}  // namespace ckv::docgen
