// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
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
