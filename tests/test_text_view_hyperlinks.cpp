// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// TextView links as terminal hyperlinks (D-088), end to end: an Application
// on a host that renders OSC 8 plays the event script in
// tools/docgen/hyperlink_script.hpp. After each beat the composed frame and
// the display decoded from the Presenter's bytes are compared with each
// other and with their pinned goldens, and the decoded display is asked
// which cells a terminal would make clickable. Keyboard and mouse activation
// inside ckVision is unchanged by any of it.
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/term/osc_sequences.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/text_view.hpp"
#include "event_script.hpp"
#include "hyperlink_script.hpp"
#include "presented_frame.hpp"

using ckv::Point;
using ckv::docgen::HyperlinkStage;
using ckv::docgen::ScriptBeat;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// Plays to `name` and compares both of that beat's pinned files.
bool plays_to_pinned_frames(HyperlinkStage& stage, const char* name) {
    const ScriptBeat* beat = stage.player.play_to(name);
    if (beat == nullptr || beat->golden.empty()) return false;
    const std::string composed = read_file("golden/" + beat->golden);
    const std::string presented =
        read_file("golden/" + ckv::docgen::presented_golden_name(beat->golden));
    return !composed.empty() && !presented.empty() &&
           ckv::docgen::capture_frame(stage.app) == composed &&
           ckv::docgen::capture_presented(stage.terminal) == presented;
}

// The decoded display's link targets along a row, from `x` for `width` cells.
std::vector<std::string_view> presented_links(const HyperlinkStage& stage, int y, int x, int width) {
    std::vector<std::string_view> targets;
    for (int column = x; column < x + width; ++column)
        targets.push_back(stage.terminal.display().frame().link_target(Point{column, y}));
    return targets;
}

bool all_link_to(const std::vector<std::string_view>& targets, std::string_view target) {
    for (const std::string_view t : targets)
        if (t != target) return false;
    return !targets.empty();
}

}  // namespace

CK_TEST(a_scripted_text_view_presents_its_links_as_terminal_hyperlinks) {
    HyperlinkStage stage(ckv::docgen::text_view_hyperlink_script());
    CK_CHECK(plays_to_pinned_frames(stage, "initial"));
    CK_CHECK(cktest_support::presented_equals_composed(stage.terminal, stage.app));

    // "ckVision guide" wraps: "ckVision" ends the first row (view column 14,
    // screen column 15) and "guide" begins the second. Both halves are one
    // hyperlink to the terminal: the same target under the same id.
    CK_CHECK(all_link_to(presented_links(stage, 1, 15, 8), HyperlinkStage::kGuide));
    CK_CHECK(presented_links(stage, 1, 14, 1)[0].empty());
    CK_CHECK(all_link_to(presented_links(stage, 2, 1, 5), HyperlinkStage::kGuide));
    CK_CHECK(presented_links(stage, 2, 6, 1)[0].empty());
    // "café" is re-addressed around its non-ASCII letter and stays linked.
    CK_CHECK(all_link_to(presented_links(stage, 2, 12, 4), HyperlinkStage::kCafe));
    // The internal target and the hostile one never reach the terminal.
    CK_CHECK(presented_links(stage, 3, 5, 5) == std::vector<std::string_view>(5));
    CK_CHECK(presented_links(stage, 3, 15, 4) == std::vector<std::string_view>(4));
    // "the guide" is a second span to the same target: the same hyperlink.
    CK_CHECK(all_link_to(presented_links(stage, 4, 8, 9), HyperlinkStage::kGuide));

    const std::string_view bytes = stage.terminal.written_bytes();
    const std::string guide_open = ckv::term::osc_hyperlink_open(HyperlinkStage::kGuide);
    std::size_t guide_opens = 0;
    for (std::size_t at = bytes.find(guide_open); at != std::string_view::npos;
         at = bytes.find(guide_open, at + 1))
        ++guide_opens;
    CK_CHECK(guide_opens == 3);  // two rows of one span, and the second span
    CK_CHECK(bytes.find("owned") == std::string_view::npos);
    CK_CHECK(bytes.find("notes.md") == std::string_view::npos);

    // Following links is the view's business, exactly as before.
    CK_CHECK(stage.player.play_to("followed") != nullptr);
    CK_CHECK(stage.player.play_to("clicked") != nullptr);
    CK_CHECK((stage.activated() == std::vector<std::string>{std::string(HyperlinkStage::kCafe),
                                                            std::string(HyperlinkStage::kInternal)}));

    // Two display lines down, "the guide" has moved up two rows with its
    // link, and the rows that held the wrapped link now hold text without
    // one: nothing is left linked where no link is drawn.
    CK_CHECK(plays_to_pinned_frames(stage, "scrolled"));
    CK_CHECK(cktest_support::presented_equals_composed(stage.terminal, stage.app));
    CK_CHECK(stage.view().top_line() == 2);
    CK_CHECK(presented_links(stage, 1, 1, 23) == std::vector<std::string_view>(23));
    CK_CHECK(all_link_to(presented_links(stage, 2, 8, 9), HyperlinkStage::kGuide));
    CK_CHECK(presented_links(stage, 3, 1, 23) == std::vector<std::string_view>(23));
    CK_CHECK(presented_links(stage, 4, 1, 23) == std::vector<std::string_view>(23));
}

CK_TEST(without_the_capability_the_same_script_writes_no_hyperlink_at_all) {
    HyperlinkStage stage(ckv::docgen::text_view_hyperlink_script());
    stage.terminal.set_capability_overrides(ckv::term::CapabilityOverrides{.hyperlinks = false});
    stage.player.play_to("scrolled");
    CK_CHECK(stage.terminal.written_bytes().find("\x1B]8;") == std::string_view::npos);
    CK_CHECK(stage.terminal.display().frame().links()->empty());
    // The composed frame still carries them: the scene does not depend on
    // the host, only the presentation does.
    CK_CHECK(!stage.app.composed_surface().links().empty());
    CK_CHECK(cktest_support::presented_equals_composed(stage.terminal, stage.app));
}
