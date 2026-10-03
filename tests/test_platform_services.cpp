// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/clipboard.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/input_line.hpp"

#include "cvision/testing/cktest.hpp"

namespace {
class ControlledClipboardWriter final : public ckv::ClipboardWriter {
public:
    ckv::ClipboardWriteResult next{ckv::ClipboardWriteStatus::Unavailable, 17};
    int attempts = 0;
    ckv::ClipboardWriteResult write_text(std::string_view) override { ++attempts; return next; }
};
}

CK_TEST(memory_clipboard_writer_is_a_deterministic_in_memory_platform_service) {
    ckv::MemoryClipboardWriter clipboard;
    ckv::ClipboardWriter& service = clipboard;
    CK_CHECK(service.write_text("first").status == ckv::ClipboardWriteStatus::Ok);
    CK_CHECK(service.write_text("second").accepted());
    CK_CHECK(clipboard.text() == "second");
    clipboard.clear();
    CK_CHECK(clipboard.text().empty());
}

CK_TEST(application_exports_copies_only_through_its_explicit_clipboard_service) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::MemoryClipboardWriter clipboard;
    ckv::ui::Application app(terminal, clock, clipboard);

    app.set_clipboard_text("portable copy");

    CK_CHECK(app.clipboard_text() == "portable copy");
    CK_CHECK(clipboard.text() == "portable copy");
    CK_CHECK(terminal.clipboard().empty());
}

CK_TEST(application_default_clipboard_adapter_is_owned_per_application_instance) {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.clipboard_write = true;
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, caps);
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);

    app.set_clipboard_text("default terminal export");

    CK_CHECK(app.clipboard_text() == "default terminal export");
    CK_CHECK(terminal.clipboard() == "default terminal export");
}

CK_TEST(paste_import_updates_only_the_portable_clipboard_not_the_export_bridge) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::MemoryClipboardWriter clipboard;
    ckv::ui::Application app(terminal, clock, clipboard);

    app.dispatch(ckv::TextEvent{"pasted but not re-exported", true});

    CK_CHECK(app.clipboard_text() == "pasted but not re-exported");
    CK_CHECK(clipboard.text().empty());
}

CK_TEST(terminal_clipboard_writer_is_an_explicit_instance_scoped_terminal_adapter) {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.clipboard_write = true;
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, caps);
    ckv::term::TerminalClipboardWriter clipboard(terminal);

    clipboard.write_text("exported through terminal session");

    CK_CHECK(terminal.clipboard() == "exported through terminal session");
}

CK_TEST(two_applications_keep_explicit_platform_service_instances_isolated) {
    ckv::term::HeadlessTerminal first_terminal(ckv::Size{80, 24});
    ckv::term::HeadlessTerminal second_terminal(ckv::Size{80, 24});
    ckv::ManualClock first_clock;
    ckv::ManualClock second_clock;
    ckv::MemoryClipboardWriter first_clipboard;
    ckv::MemoryClipboardWriter second_clipboard;
    ckv::ui::Application first(first_terminal, first_clock, first_clipboard);
    ckv::ui::Application second(second_terminal, second_clock, second_clipboard);

    first.set_clipboard_text("first only");
    second.set_clipboard_text("second only");

    CK_CHECK(first.clipboard_text() == "first only");
    CK_CHECK(second.clipboard_text() == "second only");
    CK_CHECK(first_clipboard.text() == "first only");
    CK_CHECK(second_clipboard.text() == "second only");
}

CK_TEST(widget_copy_preserves_internal_text_and_reports_the_injected_export_result) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ControlledClipboardWriter writer;
    ckv::ui::Application app(terminal, clock, writer);
    CK_CHECK(!app.last_clipboard_export_result());
    std::vector<ckv::ClipboardWriteResult> reported;
    app.set_clipboard_export_handler([&](auto result) { reported.push_back(result); });
    auto* input = app.root().add(std::make_unique<ckv::widgets::InputLine>());
    input->set_text("selected text");
    app.set_focus(input);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::End, ckv::Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Home, ckv::Modifier::Shift, ""}});
    CK_CHECK(input->has_selection());
    const ckv::KeyEvent copy{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "c"}};
    app.dispatch(copy);
    CK_CHECK(app.clipboard_text() == "selected text");
    CK_CHECK(app.last_clipboard_export_result() == writer.next);
    CK_CHECK(reported.size() == 1 && reported.front() == writer.next);
    CK_CHECK(writer.attempts == 1);
    // The service becomes available under this still-running application.
    writer.next = {ckv::ClipboardWriteStatus::Ok};
    app.dispatch(copy);
    CK_CHECK(app.last_clipboard_export_result() == writer.next);
    CK_CHECK(reported.size() == 2 && reported.back().accepted());
    CK_CHECK(writer.attempts == 2);
    app.dispatch(ckv::TextEvent{"imported", true});
    CK_CHECK(app.clipboard_text() == "imported");
    CK_CHECK(app.last_clipboard_export_result() == writer.next);
    CK_CHECK(reported.size() == 2 && writer.attempts == 2);
}

CK_TEST(default_terminal_adapter_reports_late_clipboard_capability_without_losing_internal_copy) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    CK_CHECK(app.set_clipboard_text("before refinement").status == ckv::ClipboardWriteStatus::Unsupported);
    CK_CHECK(app.clipboard_text() == "before refinement" && terminal.clipboard().empty());
    auto refined = terminal.capabilities();
    refined.clipboard_write = true;
    terminal.inject_capability_change(refined);
    app.step(0);
    CK_CHECK(app.set_clipboard_text("after refinement").status == ckv::ClipboardWriteStatus::Ok);
    CK_CHECK(terminal.clipboard() == "after refinement");
}

CK_TEST(clipboard_export_handler_can_replace_itself_and_reenter_without_changing_the_outer_result) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ControlledClipboardWriter writer;
    ckv::ui::Application app(terminal, clock, writer);
    const auto initial = writer.next;
    int first_calls = 0;
    int replacement_calls = 0;
    app.set_clipboard_export_handler([&](auto result) {
        ++first_calls;
        CK_CHECK(result == initial);
        app.set_clipboard_export_handler([&](auto replacement) {
            ++replacement_calls;
            CK_CHECK(replacement.status == ckv::ClipboardWriteStatus::Ok);
            app.set_clipboard_export_handler({});
        });
        writer.next = {ckv::ClipboardWriteStatus::Ok};
        CK_CHECK(app.set_clipboard_text("nested").accepted());
    });
    CK_CHECK(app.set_clipboard_text("outer") == initial);
    CK_CHECK(app.clipboard_text() == "nested");
    CK_CHECK(app.last_clipboard_export_result() == writer.next);
    CK_CHECK(first_calls == 1 && replacement_calls == 1);
    CK_CHECK(app.set_clipboard_text("later").accepted());
    CK_CHECK(first_calls == 1 && replacement_calls == 1);
}

CK_TEST(clipboard_export_handler_exceptions_are_callback_contract_violations) {
    CK_EXPECT_ABORT({
        ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
        ckv::ManualClock clock;
        ckv::MemoryClipboardWriter writer;
        ckv::ui::Application app(terminal, clock, writer);
        app.set_clipboard_export_handler([](auto) { throw 17; });
        app.set_clipboard_text("copied");
    });
}
