// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/clipboard.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(memory_clipboard_writer_is_a_deterministic_in_memory_platform_service) {
    ckv::MemoryClipboardWriter clipboard;
    ckv::ClipboardWriter& service = clipboard;
    service.write_text("first");
    service.write_text("second");
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
