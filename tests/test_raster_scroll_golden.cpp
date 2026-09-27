// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A scrolled picture is a re-anchored raster region (the decision log D-081;
// The roadmap M2 "raster-region goldens for ... scrolling"). One event script
// (tools/docgen/raster_scroll_script.hpp) scrolls a picture in a
// ScrollViewport by a key and then a wheel notch, and a picture inline in a
// FlowView by the arrow keys, each by one row and then by three more. It is played on a
// fixed-metric Sixel profile and on NoGraphics in lockstep, and every beat
// is compared with its pinned symbolic scene (which both profiles compose
// byte for byte), its decoded cells and pixel plane under Sixel, and its
// decoded cells under NoGraphics -- the files generate_raster_scroll_goldens
// writes from the same script. The cases below then ask the decoded planes
// what a reader would see: each picture moved by exactly the rows scrolled,
// cut off at its view's edge, and no pixel left where no picture is.
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "plane_capture.hpp"
#include "raster_checks.hpp"
#include "raster_scroll_script.hpp"

using ckv::Image;
using ckv::Rect;
using ckv::RasterSlice;
using ckv::docgen::RasterScrollStage;
using ckv::docgen::ScriptBeat;

namespace {

constexpr ckv::PixelSize kCell = ckv::docgen::kScriptCellPixels;

// One script, played on both profiles at once.
struct Pair {
    RasterScrollStage sixel{ckv::docgen::script_sixel_profile()};
    RasterScrollStage fallback{ckv::docgen::script_no_graphics_profile()};

    // Plays both runs through the beat called `name` and returns it, or
    // nullptr when either run has no such beat ahead.
    const ScriptBeat* play_to(std::string_view name) {
        const ScriptBeat* const beat = sixel.player.play_to(name);
        const ScriptBeat* const fallback_beat = fallback.player.play_to(name);
        return beat != nullptr && fallback_beat != nullptr ? beat : nullptr;
    }

    const std::vector<RasterSlice>& slices() const { return sixel.app.compositor().visible_rasters(); }
    const Image& plane() const { return sixel.terminal.display().raster_plane(); }
};

// Whether the beat both runs just played shows exactly its four pinned
// files, and the NoGraphics run never wrote a Sixel.
bool matches_pinned(const Pair& pair, const ScriptBeat& beat) {
    return ckv::docgen::matches_pinned(
               ckv::docgen::capture_paired_raster_beat(beat.golden, pair.sixel.app, pair.sixel.terminal,
                                                       pair.fallback.app, pair.fallback.terminal),
               "golden") &&
           pair.fallback.terminal.written_bytes().find("\x1BP") == std::string_view::npos;
}

// Where a picture sits, its slices, and the plane they were decoded into,
// after one beat.
struct Shot {
    Rect anchor;
    std::vector<RasterSlice> slices;
    Image plane;
};

Shot shoot(const Pair& pair, const std::shared_ptr<const Image>& picture) {
    Shot shot;
    shot.anchor = ckv::docgen::full_anchor_of(pair.slices(), picture.get()).value_or(Rect{});
    shot.slices = ckv::docgen::slices_of(pair.slices(), picture.get());
    shot.plane = pair.plane();
    return shot;
}

Rect moved_up(Rect rect, int rows) { return Rect{rect.x, rect.y - rows, rect.width, rect.height}; }

// The single slice of an unoccluded picture is its anchor cut to `clip`.
bool clipped_to(const Shot& shot, Rect clip) {
    return shot.slices.size() == 1U && shot.slices.front().visible_rect == shot.anchor.intersected(clip);
}

bool moved_exactly(const Shot& before, const Shot& after) {
    return ckv::docgen::moved_pixels_compared(before.plane, before.anchor, before.slices, after.plane,
                                              after.anchor, after.slices, kCell) > 0;
}

}  // namespace

CK_TEST(every_beat_of_the_scrolled_raster_script_matches_its_pinned_scene_cells_and_pixels) {
    Pair pair;
    for (const char* name : {"initial", "viewport_1", "viewport_4", "flow_1", "flow_4"}) {
        const ScriptBeat* const beat = pair.play_to(name);
        CK_CHECK(beat != nullptr);
        if (beat == nullptr) return;
        CK_CHECK(matches_pinned(pair, *beat));
        // Both pictures are on screen at every beat, and the plane holds them
        // and nothing else.
        CK_CHECK(pair.slices().size() == 2U);
        CK_CHECK(ckv::docgen::stray_pixels(pair.plane(), pair.slices(), kCell) == 0);
    }
}

CK_TEST(a_picture_in_a_scroll_viewport_moves_by_exactly_the_scrolled_rows_and_is_clipped_at_its_edge) {
    Pair pair;
    const std::shared_ptr<const Image>& picture = pair.sixel.viewport_picture();
    // What the viewport shows: its area less the scrollbar's column, which
    // is the frame the content is clipped to.
    const ckv::ui::View* const frame = pair.sixel.viewport().content()->parent();
    CK_CHECK(frame != nullptr);
    if (frame == nullptr) return;

    CK_CHECK(pair.play_to("initial") != nullptr);
    const Rect clip = frame->absolute_bounds();
    const Shot initial = shoot(pair, picture);
    // The picture is twice as tall as the viewport: its lower half is cut
    // off at the bottom edge before anything scrolls.
    CK_CHECK(initial.anchor == (Rect{clip.x, clip.y, clip.width, 2 * clip.height}));
    CK_CHECK(clipped_to(initial, clip));

    CK_CHECK(pair.play_to("viewport_1") != nullptr);
    CK_CHECK(pair.sixel.viewport().scroll_y() == 1);
    const Shot one = shoot(pair, picture);
    CK_CHECK(one.anchor == moved_up(initial.anchor, 1));
    CK_CHECK(clipped_to(one, clip));
    CK_CHECK(one.slices.front().visible_rect.y == clip.y);  // cut off at the top edge now
    CK_CHECK(moved_exactly(initial, one));

    CK_CHECK(pair.play_to("viewport_4") != nullptr);
    CK_CHECK(pair.sixel.viewport().scroll_y() == 1 + RasterScrollStage::kSecondScrollRows);
    const Shot four = shoot(pair, picture);
    CK_CHECK(four.anchor == moved_up(initial.anchor, 1 + RasterScrollStage::kSecondScrollRows));
    CK_CHECK(clipped_to(four, clip));
    CK_CHECK(moved_exactly(one, four));
    CK_CHECK(ckv::docgen::stray_pixels(four.plane, pair.slices(), kCell) == 0);
}

CK_TEST(a_picture_inline_in_a_flow_view_moves_by_exactly_the_scrolled_rows_and_is_clipped_at_its_edge) {
    Pair pair;
    const std::shared_ptr<const Image>& picture = pair.sixel.flow_picture();
    const ckv::widgets::FlowView& flow = pair.sixel.flow();
    CK_CHECK(pair.play_to("viewport_4") != nullptr);
    // The flow's text and its picture wrap to its width less the scrollbar.
    const Rect view = flow.absolute_bounds();
    const Rect clip{view.x, view.y, view.width - 1, view.height};
    const Shot before = shoot(pair, picture);
    CK_CHECK(flow.top_line() == 0);
    CK_CHECK(clipped_to(before, clip));
    CK_CHECK(before.slices.front().visible_rect == before.anchor);  // wholly in view

    CK_CHECK(pair.play_to("flow_1") != nullptr);
    CK_CHECK(flow.top_line() == 1);
    const Shot one = shoot(pair, picture);
    CK_CHECK(one.anchor == moved_up(before.anchor, 1));
    CK_CHECK(clipped_to(one, clip));
    CK_CHECK(moved_exactly(before, one));

    CK_CHECK(pair.play_to("flow_4") != nullptr);
    CK_CHECK(flow.top_line() == 1 + RasterScrollStage::kSecondScrollRows);
    const Shot four = shoot(pair, picture);
    CK_CHECK(four.anchor == moved_up(before.anchor, 1 + RasterScrollStage::kSecondScrollRows));
    // Its top rows have gone above the view: the anchor starts above the
    // clip, and what is shown starts at the view's first row.
    CK_CHECK(four.anchor.y < clip.y);
    CK_CHECK(clipped_to(four, clip));
    CK_CHECK(four.slices.front().visible_rect.y == clip.y);
    CK_CHECK(moved_exactly(one, four));
    CK_CHECK(ckv::docgen::stray_pixels(four.plane, pair.slices(), kCell) == 0);
}

CK_TEST(scrolling_one_picture_leaves_the_other_where_it_was) {
    Pair pair;
    CK_CHECK(pair.play_to("viewport_4") != nullptr);
    const Shot viewport_before = shoot(pair, pair.sixel.viewport_picture());
    CK_CHECK(pair.play_to("flow_4") != nullptr);
    const Shot viewport_after = shoot(pair, pair.sixel.viewport_picture());
    CK_CHECK(viewport_after.anchor == viewport_before.anchor);
    CK_CHECK(moved_exactly(viewport_before, viewport_after));
}
