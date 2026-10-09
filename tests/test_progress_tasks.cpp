// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress_tasks.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include <cmath>
#include <limits>
#include <thread>

using namespace ckv;
using namespace ckv::widgets;
namespace {
struct ProgressFixture {
    ManualClock clock;
    term::HeadlessTerminal terminal{Size{80, 24}};
    ui::Application app{terminal, clock};
    ProgressFixture() { const auto roles = ui::intern_standard_roles(app.roles()); app.set_theme(ui::make_classic_theme(app.roles(), roles)); }
};
std::string progress_row(const scene::Surface& surface, int row = 0) {
    std::string result;
    for (int x = 0; x < surface.size().width; ++x) result += surface.at(Point{x, row}).grapheme();
    return result;
}
}

CK_TEST(progress_tasks_unknown_zero_and_terminal_states_are_distinct) {
    ProgressModel model;
    const auto unknown = model.add_task({.title = "scan"});
    const auto empty = model.add_task({.title = "empty", .total = 0});
    CK_CHECK(!model.metrics(unknown).fraction);
    CK_CHECK(model.metrics(empty).fraction == 0);
    CK_CHECK(model.start(empty)); CK_CHECK(model.metrics(empty).fraction == 0);
    CK_CHECK(model.complete(empty)); CK_CHECK(model.metrics(empty).fraction == 1);
    CK_CHECK(!model.start(empty)); CK_CHECK(model.reset(empty)); CK_CHECK(model.start(empty));
    CK_CHECK(!model.add_task({.total = -1}));
    CK_CHECK(!model.set_completed(empty, std::numeric_limits<double>::infinity()));
}

CK_TEST(progress_weighted_children_preserve_unknown_and_exceptional_work) {
    ProgressModel model;
    const auto parent = model.add_task({.title = "import", .aggregation = ProgressAggregation::WeightedChildren});
    const auto a = model.add_task({.parent = parent, .total = 100, .weight = 3});
    const auto b = model.add_task({.parent = parent, .weight = 1});
    model.set_completed(a, 50);
    auto result = model.metrics(parent);
    CK_CHECK(!result.fraction); CK_CHECK(std::abs(result.known_lower_bound - 0.375) < 1e-10);
    model.set_total(b, 20); model.set_completed(b, 10); model.fail(b);
    result = model.metrics(parent);
    CK_CHECK(result.fraction == 0.5); CK_CHECK(result.child_exception);
    CK_CHECK(model.find(parent)->state == ProgressTaskState::Queued);
    model.complete(a);
    CK_CHECK(std::abs(*model.metrics(parent).fraction - 0.875) < 1e-10);
}

CK_TEST(progress_hierarchy_rejects_cycles_and_frozen_membership_atomically) {
    ProgressModel model;
    const auto parent = model.add_task({.total = 5});
    const auto child = model.add_task({.parent = parent, .total = 100});
    CK_CHECK(!model.set_parent(parent, child));
    CK_CHECK(model.find(parent)->options.parent == 0);
    model.finalize_children(parent);
    CK_CHECK(!model.add_task({.parent = parent}));
    CK_CHECK(!model.set_parent(child, 0)); CK_CHECK(!model.remove(child)); CK_CHECK(!model.set_weight(child, 2));
    CK_CHECK(model.remove(parent)); CK_CHECK(!model.find(child));
    const auto next = model.add_task({}); CK_CHECK(next > child);
}

CK_TEST(progress_rate_pause_stall_and_regression_have_explicit_time_contracts) {
    ProgressModel model;
    const auto task = model.add_task({.total = 100});
    model.start(task); model.set_time(1'000'000'000); model.advance(task, 10);
    CK_CHECK(model.metrics(task).rate == 10); CK_CHECK(model.metrics(task).remaining_seconds == 9);
    model.pause(task); model.set_time(8'000'000'000);
    CK_CHECK(model.metrics(task).elapsed_nanos == 1'000'000'000); CK_CHECK(!model.metrics(task).rate);
    model.start(task); model.set_time(9'000'000'000); model.advance(task, 10);
    CK_CHECK(model.metrics(task).rate == 10); CK_CHECK(model.metrics(task).elapsed_nanos == 2'000'000'000);
    model.set_time(12'000'000'000); CK_CHECK(!model.metrics(task).rate); CK_CHECK(!model.metrics(task).remaining_seconds);
    model.set_completed(task, 2); CK_CHECK(!model.metrics(task).rate); CK_CHECK(model.metrics(task).elapsed_nanos == 0);
    model.set_time(-1); CK_CHECK(model.time() == 12'000'000'000);
}

CK_TEST(progress_model_batches_and_scoped_observers_are_safe) {
    ProgressSubscription detached;
    int notifications = 0;
    {
        ProgressModel model;
        detached = model.subscribe([&](bool) { ++notifications; });
        model.begin_update(); const auto task = model.add_task({.total = 5}); model.start(task); model.advance(task);
        CK_CHECK(notifications == 0); model.end_update(); CK_CHECK(notifications == 1);
        detached.reset(); model.advance(task); CK_CHECK(notifications == 1);
        detached = model.subscribe([&](bool) { ++notifications; });
    }
    CK_CHECK(notifications == 2); detached.reset();
}

CK_TEST(progress_smooth_fill_has_every_eighth_cell_and_exact_endpoints) {
    ProgressFixture fixture;
    Progress bar; bar.set_context(fixture.app.root().context()); bar.set_bounds(Rect{0, 0, 2, 1});
    bar.set_presentation(ProgressPresentation::Smooth);
    const std::array<std::string_view, 9> glyphs{"░", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};
    for (int eighth = 0; eighth <= 8; ++eighth) {
        bar.set_fraction(static_cast<double>(eighth) / 16);
        scene::Surface surface(Size{2, 1}); scene::Painter painter(surface, Rect{0, 0, 2, 1}); bar.draw(painter);
        CK_CHECK(surface.at(Point{0, 0}).grapheme() == glyphs[static_cast<std::size_t>(eighth)]);
    }
    bar.set_fraction(1); scene::Surface surface(Size{2, 1}); scene::Painter painter(surface, Rect{0, 0, 2, 1}); bar.draw(painter);
    CK_CHECK(progress_row(surface) == "██"); bar.set_glyphs(ProgressGlyphs::Ascii); bar.draw(painter); CK_CHECK(progress_row(surface) == "##");
}

CK_TEST(progress_animation_is_one_timer_and_stops_for_hidden_static_or_completed_views) {
    ProgressFixture fixture; ProgressModel model;
    const auto task = model.add_task({.title = "scan"}); model.start(task);
    auto* host = fixture.app.root().make<ui::View>();
    auto* view = host->make<ProgressView>(model); view->set_bounds(Rect{0, 0, 40, 3});
    ProgressController controller(fixture.app, model); controller.bind(*view);
    CK_CHECK(fixture.app.next_timer_deadline_nanos().has_value());
    host->set_visible(false); CK_CHECK(!fixture.app.next_timer_deadline_nanos());
    host->set_visible(true); CK_CHECK(fixture.app.next_timer_deadline_nanos().has_value());
    view->set_motion(ProgressMotion::Static); CK_CHECK(!fixture.app.next_timer_deadline_nanos());
    view->set_motion(ProgressMotion::Animated);
    fixture.clock.advance(1'050'000'000); fixture.app.step(fixture.clock.now_nanos());
    CK_CHECK(model.time() == 1'050'000'000);
    CK_CHECK(fixture.app.next_timer_deadline_nanos() == 1'150'000'000);
    model.complete(task); CK_CHECK(!fixture.app.next_timer_deadline_nanos());
}

CK_TEST(progress_controller_and_view_survive_model_destruction) {
    ProgressFixture fixture; auto model = std::make_unique<ProgressModel>();
    const auto task = model->add_task({}); model->start(task);
    auto* view = fixture.app.root().make<ProgressView>(*model); view->set_bounds(Rect{0, 0, 20, 1});
    ProgressController controller(fixture.app, *model); controller.bind(*view);
    model.reset(); CK_CHECK(!fixture.app.next_timer_deadline_nanos());
    scene::Surface surface(Size{20, 1}); scene::Painter painter(surface, Rect{0, 0, 20, 1}); view->draw(painter);
    CK_CHECK(progress_row(surface) == std::string(20, ' '));
}

CK_TEST(progress_mailbox_is_bounded_coalesced_and_shutdown_safe) {
    ProgressFixture fixture; ProgressModel model; const auto task = model.add_task({.total = 100});
    ProgressMailbox mailbox(fixture.app, model, 1);
    bool accepted = false;
    std::thread worker([&] { accepted = mailbox.submit(task, 10, 100) && mailbox.submit(task, 40, 100) && !mailbox.submit(task + 1, 1, 10); }); worker.join();
    CK_CHECK(accepted); CK_CHECK(model.find(task)->completed == 0);
    fixture.app.step(0); CK_CHECK(model.find(task)->completed == 40);
    CK_CHECK(mailbox.submit(task, 80, 100)); mailbox.shutdown(); fixture.app.step(0);
    CK_CHECK(model.find(task)->completed == 40); CK_CHECK(!mailbox.submit(task, 90, 100));
}

CK_TEST(activity_custom_frames_clip_whole_graphemes_and_clear_previous_frame) {
    ProgressFixture fixture; ActivityIndicator indicator; indicator.set_context(fixture.app.root().context());
    indicator.set_bounds(Rect{0, 0, 4, 1}); CK_CHECK(indicator.set_frames({"界", "é"}));
    CK_CHECK(!indicator.set_frames({"\x1b[31m"})); CK_CHECK(!indicator.set_frames({"\xff"}));
    scene::Surface surface(Size{4, 1}); scene::Painter painter(surface, Rect{0, 0, 4, 1});
    indicator.draw(painter); indicator.set_phase(1); indicator.draw(painter);
    CK_CHECK(surface.at(Point{0, 0}).grapheme() == "é"); CK_CHECK(surface.at(Point{1, 0}).grapheme() == " ");
    indicator.set_glyphs(ProgressGlyphs::Ascii); indicator.draw(painter); CK_CHECK(surface.at(Point{0, 0}).grapheme() == "/");
}

CK_TEST(progress_manual_and_completed_parents_keep_child_exceptions) {
    ProgressModel model;
    const auto root = model.add_task({.total = 4});
    const auto child = model.add_task({.parent = root});
    model.cancel(child);
    CK_CHECK(model.metrics(root).child_exception);
    model.complete(root);
    CK_CHECK(model.metrics(root).fraction == 1);
    CK_CHECK(model.metrics(root).child_exception);
}
CK_TEST(progress_hierarchy_depth_limit_preserves_existing_tree) {
    ProgressModel model;
    ProgressTaskId parent = 0;
    for (int i = 0; i < 64; ++i) { parent = model.add_task({.parent = parent}); CK_CHECK(parent != 0); }
    CK_CHECK(model.add_task({.parent = parent}) == 0);
    const auto root = model.add_task({});
    CK_CHECK(!model.set_parent(root, parent));
    CK_CHECK(model.find(root)->options.parent == 0);
}

CK_TEST(progress_collapsed_children_keep_summary_and_clear_old_rows) {
    ProgressFixture fixture; ProgressModel model;
    const auto parent = model.add_task({.title = "Import", .total = 2});
    const auto child = model.add_task({.title = "Pictures", .parent = parent, .total = 10});
    model.start(parent); model.complete(child);
    auto* view = fixture.app.root().make<ProgressView>(model);
    view->set_bounds({0, 0, 80, 4}); fixture.app.step(0);
    CK_CHECK(progress_row(fixture.app.composed_surface(), 1).find("Pictures") != std::string::npos);
    view->set_retain_completed(false); fixture.app.step(0);
    CK_CHECK(progress_row(fixture.app.composed_surface()).find("1 done") != std::string::npos);
    CK_CHECK(progress_row(fixture.app.composed_surface(), 1).find("Pictures") == std::string::npos);
    view->set_max_rows(0); fixture.app.step(0);
    CK_CHECK(progress_row(fixture.app.composed_surface()).find("Import") == std::string::npos);
}

CK_TEST(progress_rate_expiry_between_seconds_refreshes_prepared_metadata) {
    ProgressFixture fixture; ProgressModel model;
    const auto task = model.add_task({.title = "Files", .total = 100, .unit = "files"});
    model.set_time(1'000'000'000); model.start(task);
    model.set_time(1'400'000'000); model.advance(task, 10);
    auto* view = fixture.app.root().make<ProgressView>(model);
    view->set_bounds({0, 0, 80, 1}); view->set_columns({true, false, false, true, false, false});
    model.set_time(4'300'000'000); fixture.app.step(0);
    CK_CHECK(progress_row(fixture.app.composed_surface()).find("25 files/s") != std::string::npos);
    bool phase_only = true;
    auto subscription = model.subscribe([&](bool phase) { phase_only = phase; });
    model.set_time(4'400'000'000); fixture.app.step(0);
    CK_CHECK(!phase_only); CK_CHECK(!model.metrics(task).rate);
    CK_CHECK(progress_row(fixture.app.composed_surface()).find("25 files/s") == std::string::npos);
}
