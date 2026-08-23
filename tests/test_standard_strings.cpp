// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/standard_strings.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/terminal_report_dialog.hpp"
#include "cvision/widgets/window_list_dialog.hpp"

using ckv::ManualClock;
using ckv::MemoryFileSystem;
using ckv::Rect;
using ckv::Size;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::StandardRoles;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Desktop;
using ckv::widgets::FileDialogMode;
using ckv::widgets::HelpTopic;
using ckv::widgets::MemoryHelpProvider;
using ckv::widgets::MessageBoxButtons;
using ckv::widgets::MessageBoxDescriptor;
using ckv::widgets::MessageBoxKind;
using ckv::widgets::StandardStrings;
using ckv::widgets::Window;

namespace {

struct Fixture {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop desktop{Rect{0, 0, 80, 24}};

    Fixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    }
};

bool has_button_text(const ckv::ui::View& view, const std::string& text) {
    if (const auto* button = dynamic_cast<const ckv::widgets::Button*>(&view); button != nullptr)
        if (button->text() == text) return true;
    for (const auto& child : view.children())
        if (has_button_text(*child, text)) return true;
    return false;
}

MemoryFileSystem sample_fs() {
    MemoryFileSystem fs;
    fs.add_directory("/work/docs");
    fs.add_file("/work/readme.txt");
    return fs;
}

MemoryHelpProvider sample_help() {
    MemoryHelpProvider provider;
    provider.add_topic("intro", HelpTopic{"Intro", "Body.", {}});
    return provider;
}

}  // namespace

CK_TEST(standard_dialog_factories_use_the_supplied_localized_string_table) {
    Fixture f;
    StandardStrings strings;
    strings.ok = "Weiter";
    strings.cancel = "Abbruch";
    strings.close = "Schliessen";
    strings.back = "Zurueck";
    strings.open = "Oeffnen";
    strings.save = "Speichern";
    strings.select = "Auswaehlen";
    strings.open_file_title = "Datei oeffnen";
    strings.save_file_title = "Datei speichern";
    strings.select_directory_title = "Ordner waehlen";
    strings.window_list_title = "Fensterliste";
    strings.terminal_report_title = "Terminalbericht";
    strings.copy_to_clipboard = "&Kopieren";
    strings.help_title = "Hilfe";

    auto message = ckv::widgets::make_message_box(
        MessageBoxDescriptor{MessageBoxKind::Info, "Info", "Text", MessageBoxButtons::OkCancel},
        f.roles, f.app, nullptr, nullptr, strings);
    CK_CHECK(has_button_text(*message.window, "Weiter"));
    CK_CHECK(has_button_text(*message.window, "Abbruch"));

    auto fs = sample_fs();
    auto open = ckv::widgets::make_file_dialog(FileDialogMode::Open, "/work", fs, f.roles, f.app,
                                               nullptr, nullptr, strings);
    CK_CHECK(open.window->title() == "Datei oeffnen");
    CK_CHECK(has_button_text(*open.window, "Oeffnen"));
    CK_CHECK(has_button_text(*open.window, "Abbruch"));

    auto directory = ckv::widgets::make_directory_picker(fs, "/work", f.roles, f.app, nullptr,
                                                         nullptr, strings);
    CK_CHECK(directory.window->title() == "Ordner waehlen");
    CK_CHECK(has_button_text(*directory.window, "Auswaehlen"));
    CK_CHECK(has_button_text(*directory.window, "Abbruch"));

    f.desktop.add_window(std::make_unique<Window>("Document"));
    auto window_list = ckv::widgets::make_window_list_dialog(f.desktop, f.roles, f.app, nullptr, strings);
    CK_CHECK(window_list.window->title() == "Fensterliste");
    CK_CHECK(has_button_text(*window_list.window, "Schliessen"));

    auto terminal_report =
        ckv::widgets::make_terminal_report_dialog(f.desktop, f.roles, f.app, nullptr, {}, strings);
    CK_CHECK(terminal_report.window->title() == "Terminalbericht");
    CK_CHECK(has_button_text(*terminal_report.window, "&Kopieren"));
    CK_CHECK(has_button_text(*terminal_report.window, "Schliessen"));

    auto provider = sample_help();
    auto help = ckv::widgets::make_help_viewer(provider, "intro", f.roles, f.app, nullptr, strings);
    CK_CHECK(help.window->title() == "Hilfe");
    CK_CHECK(has_button_text(*help.window, "Zurueck"));
    CK_CHECK(has_button_text(*help.window, "Schliessen"));
}
