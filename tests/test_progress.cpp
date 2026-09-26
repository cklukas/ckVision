// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Rect;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Progress;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

std::string row_text(const ckv::scene::Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}
}  // namespace

CK_TEST(progress_fraction_is_clamped_to_the_valid_range) {
    Progress progress;
    progress.set_fraction(1.5);
    CK_CHECK(progress.fraction() == 1.0);
    progress.set_fraction(-2.0);
    CK_CHECK(progress.fraction() == 0.0);
}

CK_TEST(progress_supports_indeterminate_pulse_state) {
    Progress progress;
    progress.set_indeterminate(true);
    progress.set_pulse(7);
    CK_CHECK(progress.indeterminate());
    CK_CHECK(progress.pulse() == 7);
}

CK_TEST(progress_draws_the_label_slot) {
    Fixture f;
    Progress progress;
    progress.set_context(f.ctx());
    progress.set_bounds(Rect{0, 0, 12, 1});
    progress.set_fraction(0.5);
    progress.set_label("50%");

    ckv::scene::Surface surface(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 12, 1});
    progress.draw(painter);

    CK_CHECK(row_text(surface, 0).find("50%") != std::string::npos);
}

CK_TEST(each_label_cluster_lies_on_whichever_surface_is_under_it) {
    // Drawn in the track's style throughout, the label erased the fill
    // beneath it, and a half-done bar read as barely begun.
    Fixture f;
    Progress progress;
    progress.set_context(f.ctx());
    progress.set_bounds(Rect{0, 0, 12, 1});
    progress.set_fraction(0.5);  // cells 0..5 lit
    progress.set_label("ABCD");  // centred: cells 4..7

    ckv::scene::Surface surface(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 12, 1});
    progress.draw(painter);

    const ckv::Style fill = f.theme.resolve(f.roles.menu_bar_active);
    const ckv::Style track = f.theme.resolve(f.roles.list_normal);
    CK_CHECK(row_text(surface, 0) == "    ABCD    ");
    CK_CHECK(surface.at(ckv::Point{4, 0}).style() == fill);
    CK_CHECK(surface.at(ckv::Point{5, 0}).style() == fill);
    CK_CHECK(surface.at(ckv::Point{6, 0}).style() == track);
    CK_CHECK(surface.at(ckv::Point{7, 0}).style() == track);
    CK_CHECK(surface.at(ckv::Point{3, 0}).style() == fill);
    CK_CHECK(surface.at(ckv::Point{8, 0}).style() == track);
}

CK_TEST(a_scripted_progress_bar_shows_each_state_its_host_sets_between_steps) {
    // Application-level script: Progress takes no input, so the script is
    // the host's: set a state, step() the loop, read the presented frame.
    ckv::term::HeadlessTerminal term(ckv::Size{20, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* progress = app.root().add(std::make_unique<Progress>());
    progress->set_bounds(Rect{0, 1, 12, 1});
    app.step(0);
    const ckv::Style fill = app.theme().resolve(app.roles().find("ckv.menu.bar.active"));
    // '#' for each cell of the bar in the lit style, '.' for the track.
    const auto lit = [&] {
        std::string out;
        for (int x = 0; x < 12; ++x) out += app.composed_surface().at(ckv::Point{x, 1}).style() == fill ? '#' : '.';
        return out;
    };
    CK_CHECK(lit() == "............");

    progress->set_fraction(0.5);
    progress->set_label("50%");
    app.step(0);
    CK_CHECK(lit() == "######......");
    CK_CHECK(row_text(app.composed_surface(), 1).find("50%") != std::string::npos);

    progress->set_fraction(1.0);
    progress->set_label("done");
    app.step(0);
    CK_CHECK(lit() == "############");
    CK_CHECK(row_text(app.composed_surface(), 1).find("done") != std::string::npos);

    // Indeterminate: the pulse the host advances moves a quarter-width block.
    progress->set_label("");
    progress->set_indeterminate(true);
    progress->set_pulse(5);
    app.step(0);
    CK_CHECK(lit() == "..###.......");
    progress->set_pulse(9);
    app.step(0);
    CK_CHECK(lit() == "......###...");
}
