// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the event-script goldens: the M4 form demo
// (rootdialog_*.dump), the M5 close-veto paths (close_veto_*.dump), the
// M5 keyboard-only and mouse-only menu scripts (menu_*.dump), and TextView
// links as terminal hyperlinks (text_view_hyperlinks_*.dump, each with the
// decoded display beside it as *_presented.dump). It plays the same scripts
// their tests play (rootdialog_script.hpp, close_veto_script.hpp,
// menu_operability_script.hpp, hyperlink_script.hpp), so regenerating is
// running this program:
//   build/tools/docgen/generate_event_script_goldens tests/golden
// and reviewing the diff like any other source change. The
// generated_golden_bytes test runs it on every host and compares the bytes.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "close_veto_script.hpp"
#include "event_script.hpp"
#include "hyperlink_script.hpp"
#include "menu_operability_script.hpp"
#include "rootdialog_script.hpp"

namespace {

template <class Stage>
void write_goldens(Stage& stage, const std::filesystem::path& directory) {
    (void)ckv::docgen::write_script_goldens(stage.player, stage.app, directory);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    {
        ckv::docgen::RootDialogSession accept(ckv::docgen::rootdialog_accept_script());
        write_goldens(accept, directory);
        ckv::docgen::RootDialogSession cancel(ckv::docgen::rootdialog_cancel_script());
        write_goldens(cancel, directory);
    }
    {
        ckv::docgen::CloseVetoStage control(ckv::docgen::close_control_script());
        write_goldens(control, directory);
        ckv::docgen::CloseVetoStage chord(ckv::docgen::close_chord_script());
        write_goldens(chord, directory);
        ckv::docgen::CloseVetoStage dialog(ckv::docgen::dialog_escape_script());
        write_goldens(dialog, directory);
        ckv::docgen::CloseVetoStage sweep(ckv::docgen::quit_sweep_script());
        write_goldens(sweep, directory);
    }
    {
        ckv::docgen::MenuStage keyboard(ckv::docgen::menu_keyboard_script());
        write_goldens(keyboard, directory);
        ckv::docgen::MenuStage mouse(ckv::docgen::menu_mouse_script());
        write_goldens(mouse, directory);
    }
    {
        // Each pinned beat twice: the composed frame, and what the terminal
        // shows after the Presenter's bytes — which is where a link becomes
        // a hyperlink or does not.
        ckv::docgen::HyperlinkStage stage(ckv::docgen::text_view_hyperlink_script());
        while (const ckv::docgen::ScriptBeat* beat = stage.player.play_next()) {
            if (beat->golden.empty()) continue;
            ckv::docgen::write_frame(directory, beat->golden, stage.app);
            std::ofstream presented(directory / ckv::docgen::presented_golden_name(beat->golden),
                                    std::ios::binary);
            presented << ckv::docgen::capture_presented(stage.terminal);
        }
    }
    return 0;
}
