// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "progress_lab_app.hpp"
CK_TEST(progress_lab_variants_run_pause_reset_and_resize) {
    ckv::ManualClock clock;
    ckv::term::HeadlessTerminal terminal({100, 32});
    ckv::ui::Application app(terminal, clock);
    ckv::progress_lab::ProgressLabApp lab(app);
    app.step(0);
    CK_CHECK(terminal.written_bytes().find("Progress Lab") != std::string::npos);
    for (const auto name : {"Meter", "Activity", "Spinner", "Glyphs", "Motion", "Rows", "Unit scale", "Aggregation"}) {
        auto* control = lab.selector(name); CK_CHECK(control != nullptr);
        for (std::size_t i = 0; i < control->items().size(); ++i) {
            control->set_selected_index(i); control->on_select(i); app.set_focus(control); app.step(0);
        }
    }
    const auto id = lab.selected_task();
    lab.set_running(true); clock.advance(250'000'000); app.step(0);
    CK_CHECK(lab.model().find(id)->completed == 75);
    lab.set_running(false); CK_CHECK(lab.model().find(id)->state == ckv::widgets::ProgressTaskState::Paused);
    clock.advance(1'000'000'000); app.step(0); CK_CHECK(lab.model().find(id)->completed == 75);
    lab.set_running(true); clock.advance(250'000'000); app.step(0); CK_CHECK(lab.model().find(id)->completed == 76);
    lab.reset_tasks(); CK_CHECK(lab.model().find(lab.selected_task())->completed == 74);
    CK_CHECK(lab.selected_task() != id);
    auto* motion = lab.selector("Motion");
    motion->set_selected_index(0); motion->on_select(0);
    terminal.resize({40, 20}); app.step(0);
    CK_CHECK(!app.next_timer_deadline_nanos());
    terminal.resize({100, 32}); app.step(0);
    CK_CHECK(app.next_timer_deadline_nanos().has_value());
}

CK_TEST(progress_lab_keyboard_selects_meter_and_task_state_stays_truthful) {
    ckv::ManualClock clock; ckv::term::HeadlessTerminal terminal({100, 32});
    ckv::ui::Application app(terminal, clock); ckv::progress_lab::ProgressLabApp lab(app);
    auto* meter = lab.selector("Meter"); meter->set_selected_index(0); meter->on_select(0);
    app.set_focus(meter); app.step(0);
    CK_CHECK(app.dispatch(ckv::KeyEvent{{ckv::Key::Down, ckv::Modifier::None, ""}}));
    CK_CHECK(app.dispatch(ckv::KeyEvent{{ckv::Key::Down, ckv::Modifier::None, ""}}));
    CK_CHECK(app.dispatch(ckv::KeyEvent{{ckv::Key::Enter, ckv::Modifier::None, ""}})); app.step(0);
    CK_CHECK(lab.meter().presentation() == ckv::widgets::ProgressPresentation::Block);
    auto* state = lab.selector("State"); state->on_select(5); app.step(0);
    CK_CHECK(lab.model().find(lab.selected_task())->state == ckv::widgets::ProgressTaskState::Cancelled);
    state->set_selected_index(1); state->on_select(1); app.step(0);
    CK_CHECK(state->selected_index() == 5);
    auto* task = lab.selector("Task"); task->on_select(3); app.step(0);
    CK_CHECK(state->selected_index() == 1);
}
