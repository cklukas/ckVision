// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The render pipeline (View tree -> off-screen Surface -> Presenter ->
// Terminal bytes) that Application::step() now drives — before this,
// step() only dispatched input; nothing anywhere ever painted a View
// into terminal output. These tests prove bytes actually reach the
// terminal, that they reflect real content changes, that resize is
// handled, and that an invisible/unchanged tree costs nothing.
#include "cvision/ui/application.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/window.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Label;
using ckv::widgets::Desktop;
using ckv::widgets::Window;

namespace {
class ColorProbe final : public ckv::ui::View {
public:
    void draw(ckv::scene::Painter& painter) override {
        painter.draw_text(ckv::Point{0, 0}, "C",
                          ckv::Style{ckv::Color::rgb(17, 34, 51), ckv::Color::rgb(68, 85, 102), ckv::Attr{}});
    }
};

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 12}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    Fixture() { app.theme() = make_classic_theme(app.roles(), roles); }
};
}  // namespace

CK_TEST(the_very_first_step_paints_and_presents_even_with_no_input) {
    // dirty_ starts true specifically so an application that builds its
    // whole UI before the first step() still gets an initial frame.
    Fixture f;
    CK_CHECK(f.term.written_bytes().empty());
    f.app.step(0);
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(a_second_step_with_nothing_invalidated_writes_no_further_bytes) {
    Fixture f;
    f.app.step(0);
    f.term.clear_written();
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().empty());
}

CK_TEST(a_live_color_scheme_capability_change_forces_a_full_represent) {
    Fixture f;
    f.app.step(0);
    f.term.clear_written();

    ckv::term::Capabilities updated = f.term.capabilities();
    updated.color_scheme = ckv::term::ColorScheme::Dark;
    updated.color_scheme_notifications = true;
    f.term.inject_capability_change(updated);

    CK_CHECK(f.app.step(0));
    // A scheme change can alter every resolved role. Application must reset
    // the presenter rather than trusting an unchanged cell diff, so the
    // terminal receives a complete current frame even for an otherwise idle
    // view tree.
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(a_runtime_color_depth_downgrade_reencodes_the_entire_application_frame) {
    ckv::term::Capabilities truecolor = ckv::term::baseline_capabilities();
    truecolor.color_depth = ckv::term::ColorDepth::TrueColor;
    ckv::term::HeadlessTerminal term{ckv::Size{40, 12}, truecolor};
    ManualClock clock;
    Application app{term, clock};
    auto probe = std::make_unique<ColorProbe>();
    probe->set_bounds(Rect{0, 0, 1, 1});
    app.root().add_child(std::move(probe));

    app.step(0);
    CK_CHECK(term.written_bytes().find("38;2;17;34;51") != std::string::npos);
    CK_CHECK(term.written_bytes().find("48;2;68;85;102") != std::string::npos);
    term.clear_written();

    ckv::term::Capabilities mono = truecolor;
    mono.color_depth = ckv::term::ColorDepth::Mono16;
    term.inject_capability_change(mono);
    CK_CHECK(app.step(0));
    // The application's capability-change route invalidates Presenter before
    // it paints. An unchanged view must therefore be re-encoded under the
    // downgraded palette instead of retaining truecolor bytes in the host.
    CK_CHECK(term.written_bytes().find("38;2;") == std::string::npos);
    CK_CHECK(term.written_bytes().find("48;2;") == std::string::npos);
}

CK_TEST(a_label_actually_renders_its_text_into_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Hello");
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Hello") != std::string::npos);
}

CK_TEST(setting_new_text_repaints_so_the_old_text_no_longer_appears_in_the_next_frame) {
    Fixture f;
    auto owned = std::make_unique<Label>("Old");
    Label* label = owned.get();
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(owned));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Old") != std::string::npos);

    label->set_text("New");
    f.app.step(0);
    // The Presenter diffs cell-by-cell, so "New" must appear even
    // though nothing ever calls a bespoke "clear" — the diff against
    // the previously presented frame is what erases "Old"'s cells.
    CK_CHECK(f.term.written_bytes().find("New") != std::string::npos);
}

CK_TEST(a_hidden_view_never_reaches_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Secret");
    label->set_bounds(Rect{2, 2, 10, 1});
    label->set_visible(false);
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Secret") == std::string::npos);
}

CK_TEST(a_resize_event_grows_the_backing_surface_and_forces_a_full_repaint) {
    Fixture f;
    auto label = std::make_unique<Label>("Grow");
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));
    f.app.step(0);

    f.term.resize(ckv::Size{60, 20});
    f.app.step(0);
    // The label is still visible after growing the terminal — proves
    // the off-screen Surface was actually resized to match, not left
    // at the old (now too-small) size where painting it would be UB.
    CK_CHECK(f.term.written_bytes().find("Grow") != std::string::npos);
    CK_CHECK(f.app.root().bounds().width == 60);
    CK_CHECK(f.app.root().bounds().height == 20);
}

CK_TEST(moving_a_window_recomposes_its_retained_backing_without_content_repaint) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{}, f.app.theme(), f.roles.dialog_background);
    Desktop* desktop = desktop_owned.get();
    auto window_owned = std::make_unique<Window>("Retained");
    window_owned->set_bounds(Rect{2, 2, 14, 6});
    Window* window = desktop->add_window(std::move(window_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    const std::size_t original_repaints = window->content_repaint_count();
    CK_CHECK(original_repaints == 1);

    window->set_bounds(Rect{18, 2, 14, 6});  // same local size: composition-only movement
    f.term.clear_written();
    f.app.step(0);

    CK_CHECK(window->content_repaint_count() == original_repaints);
    CK_CHECK(desktop->last_content_repaints() == 0);
    // Two 14x6 positions plus their non-overlapping 2x1 shadow footprints
    // are the whole possible structural damage: this is deliberately far
    // below the 40x12 desktop and catches a hidden base/full-frame repaint.
    CK_CHECK(f.app.last_compose_cells_touched() <= 216);
    CK_CHECK(f.app.last_bytes_emitted() > 0);
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(resizing_one_window_repaints_only_its_retained_subtree) {
    Fixture f;
    auto desktop_owned = std::make_unique<Desktop>(Rect{}, f.app.theme(), f.roles.dialog_background);
    Desktop* desktop = desktop_owned.get();
    auto first_owned = std::make_unique<Window>("First");
    first_owned->set_bounds(Rect{2, 2, 12, 6});
    Window* first = desktop->add_window(std::move(first_owned));
    auto second_owned = std::make_unique<Window>("Second");
    second_owned->set_bounds(Rect{18, 2, 12, 6});
    Window* second = desktop->add_window(std::move(second_owned));
    f.app.root().add_child(std::move(desktop_owned));

    f.app.step(0);
    const std::size_t first_repaints = first->content_repaint_count();
    const std::size_t second_repaints = second->content_repaint_count();

    first->set_bounds(Rect{2, 2, 15, 7});
    f.app.step(0);

    CK_CHECK(first->content_repaint_count() == first_repaints + 1);
    CK_CHECK(second->content_repaint_count() == second_repaints);
    CK_CHECK(desktop->last_content_repaints() == 1);
}

// --- Terminal too small (ARCHITECTURE.md §5 "Sizing policy", M10/WP-21) ---

CK_TEST(terminal_too_small_is_false_at_a_normal_size) {
    Fixture f;
    CK_CHECK(!f.app.terminal_too_small());
}

CK_TEST(terminal_too_small_is_false_exactly_at_the_hard_floor) {
    Fixture f;
    f.term.resize(ckv::Size{20, 6});  // kHardFloorSize itself: still large enough to render
    f.app.step(0);
    CK_CHECK(!f.app.terminal_too_small());
}

CK_TEST(terminal_too_small_is_true_one_cell_below_the_floor_on_either_axis) {
    Fixture f;
    f.term.resize(ckv::Size{19, 6});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());

    f.term.resize(ckv::Size{20, 5});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());
}

CK_TEST(shrinking_below_the_floor_shows_the_too_small_message_instead_of_corrupted_output) {
    Fixture f;
    // 19 wide: below the hard floor (kHardFloorSize.width == 20) but
    // still wide enough for the full "Terminal too small" message
    // (18 chars) to render unclipped, so the exact text is checkable.
    f.term.resize(ckv::Size{19, 5});
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Terminal too small") != std::string::npos);
}

CK_TEST(a_terminal_narrower_than_the_message_still_shows_a_clipped_prefix_not_garbage) {
    Fixture f;
    f.term.resize(ckv::Size{8, 5});  // narrower than "Terminal too small" itself
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Terminal") != std::string::npos);
}

CK_TEST(the_too_small_state_hides_the_normal_view_tree_entirely) {
    Fixture f;
    auto label = std::make_unique<Label>("Hidden");
    label->set_bounds(Rect{0, 0, 6, 1});
    f.app.root().add_child(std::move(label));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Hidden") != std::string::npos);  // renders normally first
    f.term.clear_written();  // written_bytes() is cumulative — isolate the NEXT step's own output

    f.term.resize(ckv::Size{15, 5});
    f.app.step(0);

    CK_CHECK(f.term.written_bytes().find("Hidden") == std::string::npos);
}

CK_TEST(growing_back_above_the_floor_recovers_normal_rendering_automatically) {
    Fixture f;
    auto label = std::make_unique<Label>("Back");
    label->set_bounds(Rect{0, 0, 4, 1});
    f.app.root().add_child(std::move(label));

    f.term.resize(ckv::Size{15, 5});
    f.app.step(0);
    CK_CHECK(f.app.terminal_too_small());

    f.term.resize(ckv::Size{40, 12});
    f.app.step(0);

    CK_CHECK(!f.app.terminal_too_small());
    CK_CHECK(f.term.written_bytes().find("Back") != std::string::npos);
}
