// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/terminal_report_dialog.hpp"

#include <cstddef>
#include <optional>
#include <string>

#include "cvision/core/clipboard.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/text_view.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::Size;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::StandardRoles;
using ckv::widgets::Desktop;
using ckv::widgets::make_terminal_report_dialog;
using ckv::widgets::present_terminal_report_dialog;
using ckv::widgets::ScrollbarPolicy;
using ckv::widgets::TerminalReportDialogOptions;
using ckv::widgets::TerminalReportDialogResult;

namespace {

struct Fixture {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    StandardRoles roles = intern_standard_roles(app.roles());

    Fixture() { app.theme() = make_classic_theme(app.roles(), roles); }
};

ckv::KeyEvent key(Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

const ckv::widgets::TextView* report_text(const ckv::widgets::WindowHandle& handle) {
    const auto* viewport = dynamic_cast<const ckv::widgets::ScrollViewport*>(handle.initial_focus);
    if (viewport == nullptr) return nullptr;
    return dynamic_cast<const ckv::widgets::TextView*>(viewport->content());
}

ckv::widgets::Button* find_button(ckv::ui::View& view, const std::string& text) {
    if (auto* button = dynamic_cast<ckv::widgets::Button*>(&view);
        button != nullptr && button->text() == text)
        return button;
    for (const auto& child : view.children())
        if (auto* found = find_button(*child, text)) return found;
    return nullptr;
}

}  // namespace

CK_TEST(the_report_leads_with_mouse_diagnostics_and_carries_the_capability_table) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    auto handle = make_terminal_report_dialog(desktop, f.roles, f.app, nullptr);
    const auto* text = report_text(handle);
    CK_CHECK(text != nullptr);
    if (text == nullptr) return;
    CK_CHECK(text->text().find("Mouse events seen") != std::string::npos);
    CK_CHECK(text->text().find("SIXEL graphics") != std::string::npos);
    // No probe was given, so the byte-stream line must not pretend to count.
    CK_CHECK(text->text().find("Mouse reports decoded") == std::string::npos);
}

CK_TEST(the_document_shows_no_scrollbars_of_its_own_the_viewport_already_owns_both) {
    // Field report: the dialog's bottom scrollbar rendered twice. The
    // viewport owns both visible tracks (set_scrollbars_always_visible), and
    // the document's vertical track is hidden through the override flag —
    // but horizontal has no such override, only a policy that defaults to
    // Auto, and every report row is wider than the document's 86-column
    // preferred size. So it showed too: two horizontal bars, one under the
    // other, where a reader should see one.
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    auto handle = make_terminal_report_dialog(desktop, f.roles, f.app, nullptr);
    const auto* text = report_text(handle);
    CK_CHECK(text != nullptr);
    if (text == nullptr) return;
    CK_CHECK(!text->vertical_scrollbar_visible());
    CK_CHECK(text->horizontal_scrollbar_policy() == ScrollbarPolicy::Hidden);
}

CK_TEST(a_decoder_probe_adds_the_decoded_reports_line_with_its_count) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    TerminalReportDialogOptions options;
    options.mouse_reports_decoded = [] { return std::size_t{412}; };
    auto handle = make_terminal_report_dialog(desktop, f.roles, f.app, nullptr, options);
    const auto* text = report_text(handle);
    CK_CHECK(text != nullptr);
    if (text == nullptr) return;
    CK_CHECK(text->text().find("Mouse reports decoded") != std::string::npos);
    CK_CHECK(text->text().find("412") != std::string::npos);
}

CK_TEST(the_title_and_copy_label_come_from_the_string_table) {
    Fixture f;
    Desktop desktop(Rect{0, 0, 80, 24});
    ckv::widgets::StandardStrings strings;
    strings.terminal_report_title = "Terminalbericht";
    strings.copy_to_clipboard = "&Kopieren";
    strings.close = "Schliessen";
    auto handle = make_terminal_report_dialog(desktop, f.roles, f.app, nullptr, {}, strings);
    CK_CHECK(handle.window->title() == "Terminalbericht");
    CK_CHECK(find_button(*handle.window, "&Kopieren") != nullptr);
    CK_CHECK(find_button(*handle.window, "Schliessen") != nullptr);
}

CK_TEST(copy_exports_the_plain_report_with_the_application_diagnostics) {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ckv::MemoryClipboardWriter clipboard;
    Application app(terminal, clock, clipboard);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    Desktop desktop(Rect{0, 0, 80, 24});

    TerminalReportDialogOptions options;
    options.mouse_reports_decoded = [] { return std::size_t{7}; };
    auto handle = make_terminal_report_dialog(desktop, roles, app, nullptr, options);
    auto* copy = find_button(*handle.window,
                             ckv::widgets::english_standard_strings().copy_to_clipboard);
    CK_CHECK(copy != nullptr);
    if (copy == nullptr) return;
    copy->on_press();

    CK_CHECK(clipboard.text().find("SIXEL graphics") != std::string::npos);
    CK_CHECK(clipboard.text().find("Application diagnostics") != std::string::npos);
    CK_CHECK(clipboard.text().find("Mouse events seen") != std::string::npos);
    // The capability half comes from capability_report_text once; the
    // diagnostics section repeats only the application-side lines.
    CK_CHECK(clipboard.text().find("Mouse reports decoded") != std::string::npos);
}

CK_TEST(escape_closes_and_restores_focus_to_the_view_that_invoked_it) {
    Fixture f;
    auto* invoker = f.app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    Desktop desktop(Rect{0, 0, 80, 24});

    auto handle = make_terminal_report_dialog(desktop, f.roles, f.app, invoker);
    bool closed = false;
    handle.window->on_closed = [&closed, previous = handle.window->on_closed]() {
        closed = true;
        if (previous) previous();
    };
    f.app.root().add_child(std::move(handle.window));
    f.app.set_focus(handle.initial_focus);

    f.app.dispatch(key(Key::Escape));
    CK_CHECK(closed);
    CK_CHECK(f.app.focused() == invoker);
}

CK_TEST(present_terminal_report_dialog_completes_after_modal_detachment) {
    Fixture f;
    auto* desktop = f.app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    auto presentation = present_terminal_report_dialog(*desktop, f.app, f.roles);
    std::optional<TerminalReportDialogResult> completion;
    presentation.set_completion_handler(
        [&](TerminalReportDialogResult result) { completion = result; });

    CK_CHECK(f.app.is_modal());
    f.app.dispatch(key(Key::Escape));
    CK_CHECK(!presentation.completed());
    f.app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == TerminalReportDialogResult::Closed);
    CK_CHECK(completion == TerminalReportDialogResult::Closed);
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(the_standard_command_presents_one_report_at_a_time_over_the_desktop) {
    Fixture f;
    f.app.root().add(std::make_unique<Desktop>(Rect{0, 0, 80, 24}));

    const ckv::ui::CommandId command = f.app.commands().standard().terminal_report;
    CK_CHECK(f.app.commands().has_handler(command));
    CK_CHECK(f.app.commands().execute(command));
    CK_CHECK(f.app.is_modal());
    // A second execution while the report is up presents no second dialog;
    // the command still runs (harmlessly), the way show_window_list holds.
    CK_CHECK(f.app.commands().execute(command));

    f.app.dispatch(key(Key::Escape));
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
}
