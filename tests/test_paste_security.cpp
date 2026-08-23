// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Semantic hostile-paste assertions: observe the command dispatcher and the
// focused widget, not merely the decoder's variant types.
#include "cvision/core/clock.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/input_line.hpp"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"

namespace {

CK_TEST(embedded_paste_terminator_and_ctrl_chord_cannot_execute_an_application_command) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto input = std::make_unique<ckv::widgets::InputLine>();
    auto* input_ptr = input.get();
    app.root().add_child(std::move(input));
    app.set_focus(input_ptr);

    int command_calls = 0;
    app.commands().declare(ckv::ui::CommandDescriptor{
        .key = "test.dangerous-action",
        .title = "Dangerous action",
        .chord = "Ctrl+Q",
        .handler = [&] { ++command_calls; },
    });

    ckv::term::InputDecoder decoder;
    std::vector<ckv::term::TerminalEvent> events =
        decoder.feed("\x1B[200~safe\x1B[201~\x11\x1B[201~", 0);
    auto completed = decoder.poll_timeout(ckv::term::kPasteTerminationQuietNanos);
    events.insert(events.end(), std::make_move_iterator(completed.begin()),
                  std::make_move_iterator(completed.end()));
    for (const ckv::term::TerminalEvent& event : events)
        app.dispatch(event);

    CK_CHECK(command_calls == 0);
    CK_CHECK(input_ptr->text() == "safe\xEF\xBF\xBD[201~\xEF\xBF\xBD");

    // The recovery is user-visible in the real frame, not merely an internal
    // TextEvent flag. Capture through the symbolic golden representation so
    // control bytes cannot hide in the presenter's byte stream.
    app.step(0);
    const std::string golden =
        ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    CK_CHECK(golden.find("safe\xEF\xBF\xBD[201~\xEF\xBF\xBD") != std::string::npos);
}

}  // namespace
