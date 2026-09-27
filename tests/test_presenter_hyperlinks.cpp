// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hyperlinks in the term layer (D-088): the OSC 8 brackets osc_sequences
// builds, the exact bytes the Presenter writes around linked runs with and
// without the hyperlinks capability, and the VirtualDisplay's decoding of
// OSC 8 into the cells a terminal would make clickable, including its refusal
// of every form ckVision never emits.
#include "cvision/core/clock.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/osc_sequences.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/virtual_display.hpp"

#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::term;
using ckv::scene::Painter;
using ckv::scene::Surface;

namespace {

constexpr std::string_view kDocs = "https://example.test/docs";
constexpr std::string_view kHome = "https://example.test/";
// FNV-1a 64 of each target, as hyperlink_id spells it.
constexpr std::string_view kOpenDocs = "\x1B]8;id=34ad6241ce53a328;https://example.test/docs\x1B\\";
constexpr std::string_view kOpenHome = "\x1B]8;id=814049ff5282b59f;https://example.test/\x1B\\";
constexpr std::string_view kClose = "\x1B]8;;\x1B\\";

Capabilities hyperlink_host() {
    Capabilities caps = baseline_capabilities();
    caps.hyperlinks = true;
    return caps;
}

std::size_t count(std::string_view haystack, std::string_view needle) {
    std::size_t found = 0;
    for (std::size_t at = haystack.find(needle); at != std::string_view::npos;
         at = haystack.find(needle, at + needle.size()))
        ++found;
    return found;
}

// "ab" plain, "cd" linked to kDocs, "ef" plain.
Surface linked_row() {
    Surface surface(Size{6, 1});
    Painter painter(surface, Rect{0, 0, 6, 1});
    painter.draw_text(Point{0, 0}, "ab", Style{});
    painter.draw_text(Point{2, 0}, "cd", Style{}, kDocs);
    painter.draw_text(Point{4, 0}, "ef", Style{});
    return surface;
}

std::string presented_bytes(const Surface& surface, Capabilities caps) {
    HeadlessTerminal term(surface.size(), caps);
    Presenter presenter(term);
    presenter.present(surface.view(), CursorState{}, 0);
    return std::string(term.written_bytes());
}

}  // namespace

// --- The sequences --------------------------------------------------------------

CK_TEST(osc_hyperlink_open_carries_the_target_and_an_id_derived_from_it) {
    CK_CHECK(osc_hyperlink_open(kDocs) == kOpenDocs);
    CK_CHECK(osc_hyperlink_close() == kClose);
    CK_CHECK(hyperlink_id(kDocs) == "34ad6241ce53a328");
    CK_CHECK(hyperlink_id(kDocs) == hyperlink_id(std::string(kDocs)));
    CK_CHECK(hyperlink_id(kDocs) != hyperlink_id(kHome));
    // The introducer and ST are the only ESC bytes; there is nothing else a
    // host could read as a terminator.
    const std::string open = osc_hyperlink_open(kDocs);
    CK_CHECK(count(open, "\x1B") == 2);
    CK_CHECK(open.find('\x07') == std::string::npos);
}

CK_TEST(osc_hyperlink_open_drops_a_target_it_cannot_send_safely) {
    CK_CHECK(osc_hyperlink_open("https://x.test/\x1B\\\x1B]0;evil\x07").empty());
    CK_CHECK(osc_hyperlink_open("https://x.test/\xC2\x9C").empty());
    CK_CHECK(osc_hyperlink_open("guide.md").empty());
    CK_CHECK(osc_hyperlink_open("").empty());
}

// --- Presenter: raw bytes ---------------------------------------------------------

CK_TEST(a_linked_run_is_opened_once_and_closed_where_it_ends) {
    const std::string bytes = presented_bytes(linked_row(), hyperlink_host());
    CK_CHECK(bytes == std::string("\x1B[1;1H\x1B[0mab") + std::string(kOpenDocs) + "cd" +
                          std::string(kClose) + "ef\x1B[?25l");
}

CK_TEST(without_the_capability_a_linked_frame_is_byte_identical_to_an_unlinked_one) {
    Surface plain(Size{6, 1});
    Painter(plain, Rect{0, 0, 6, 1}).draw_text(Point{0, 0}, "abcdef", Style{});
    const Capabilities host = baseline_capabilities();
    CK_CHECK(presented_bytes(linked_row(), host) == presented_bytes(plain, host));
    CK_CHECK(presented_bytes(linked_row(), host).find("\x1B]8") == std::string::npos);
}

CK_TEST(adjacent_runs_of_different_targets_close_one_before_opening_the_next) {
    Surface surface(Size{4, 1});
    Painter painter(surface, Rect{0, 0, 4, 1});
    painter.draw_text(Point{0, 0}, "ab", Style{}, kDocs);
    painter.draw_text(Point{2, 0}, "cd", Style{}, kHome);
    const std::string bytes = presented_bytes(surface, hyperlink_host());
    CK_CHECK(bytes == std::string("\x1B[1;1H") + std::string(kOpenDocs) + "\x1B[0mab" +
                          std::string(kClose) + std::string(kOpenHome) + "cd" + std::string(kClose) +
                          "\x1B[?25l");
}

CK_TEST(a_link_split_by_a_cursor_re_address_is_reopened_with_the_same_id) {
    // D-019: a non-ASCII grapheme ends its run and the next cell is
    // addressed absolutely. The hyperlink closes before that address and
    // opens again after it, under the same id, so it is still one link.
    Surface surface(Size{5, 1});
    Painter(surface, Rect{0, 0, 5, 1}).draw_text(Point{0, 0}, "caf\xC3\xA9!", Style{}, kDocs);
    const std::string bytes = presented_bytes(surface, hyperlink_host());
    CK_CHECK(bytes == std::string("\x1B[1;1H") + std::string(kOpenDocs) + "\x1B[0mcaf\xC3\xA9" +
                          std::string(kClose) + "\x1B[1;5H" + std::string(kOpenDocs) + "!" +
                          std::string(kClose) + "\x1B[?25l");
}

CK_TEST(runs_on_two_rows_each_close_before_the_next_row_is_addressed) {
    Surface surface(Size{3, 2});
    Painter painter(surface, Rect{0, 0, 3, 2});
    painter.draw_text(Point{1, 0}, "ab", Style{}, kDocs);
    painter.draw_text(Point{0, 1}, "cd", Style{}, kDocs);
    const std::string bytes = presented_bytes(surface, hyperlink_host());
    CK_CHECK(count(bytes, kOpenDocs) == 2);
    CK_CHECK(count(bytes, kClose) == 2);
    CK_CHECK(bytes.find(std::string(kClose) + "\x1B[2;1H") != std::string::npos);
}

CK_TEST(a_change_of_link_alone_repaints_only_where_the_host_renders_links) {
    for (const bool hyperlinks : {true, false}) {
        Capabilities caps = baseline_capabilities();
        caps.hyperlinks = hyperlinks;
        HeadlessTerminal term(Size{4, 1}, caps);
        Presenter presenter(term);
        Surface surface(Size{4, 1});
        Painter painter(surface, Rect{0, 0, 4, 1});
        painter.draw_text(Point{0, 0}, "link", Style{}, kDocs);
        presenter.present(surface.view(), CursorState{}, 0);
        term.clear_written();

        painter.draw_text(Point{0, 0}, "link", Style{}, kHome);
        presenter.present(surface.view(), CursorState{}, 0);
        if (hyperlinks) {
            CK_CHECK(term.written_bytes() == std::string("\x1B[1;1H") + std::string(kOpenHome) +
                                                 "\x1B[0mlink" + std::string(kClose));
            CK_CHECK(term.display().frame().link_target(Point{3, 0}) == kHome);
        } else {
            CK_CHECK(term.written_bytes().empty());
        }
    }
}

CK_TEST(an_unchanged_linked_frame_writes_nothing_even_after_its_table_renumbers) {
    // The frame's table reuses a freed id for a new target. The presenter
    // compares targets, so an unchanged cell stays unchanged and a cell whose
    // id was reused for another target is repainted.
    HeadlessTerminal term(Size{4, 1}, hyperlink_host());
    Presenter presenter(term);
    Surface surface(Size{4, 1});
    Painter painter(surface, Rect{0, 0, 4, 1});
    painter.draw_text(Point{0, 0}, "ab", Style{}, kDocs);
    painter.draw_text(Point{2, 0}, "cd", Style{}, kHome);
    presenter.present(surface.view(), CursorState{}, 0);
    term.clear_written();
    presenter.present(surface.view(), CursorState{}, 0);
    CK_CHECK(term.written_bytes().empty());

    const LinkId docs_id = surface.at(Point{0, 0}).link();
    painter.draw_text(Point{0, 0}, "ab", Style{});  // frees kDocs's id
    painter.draw_text(Point{2, 0}, "cd", Style{}, "https://example.test/new");
    CK_CHECK(surface.at(Point{2, 0}).link() == docs_id);  // reused
    presenter.present(surface.view(), CursorState{}, 0);
    CK_CHECK(term.display().frame().link_target(Point{0, 0}).empty());
    CK_CHECK(term.display().frame().link_target(Point{2, 0}) == "https://example.test/new");
}

CK_TEST(turning_the_capability_on_repaints_the_links_the_host_now_renders) {
    HeadlessTerminal term(Size{6, 1}, baseline_capabilities());
    Presenter presenter(term);
    const Surface surface = linked_row();
    presenter.present(surface.view(), CursorState{}, 0);
    CK_CHECK(term.display().frame().link_target(Point{2, 0}).empty());
    term.set_capability_overrides(CapabilityOverrides{.hyperlinks = true});
    CK_CHECK(term.capabilities().hyperlinks);
    term.clear_written();
    presenter.present(surface.view(), CursorState{}, 0);
    CK_CHECK(term.written_bytes() == std::string("\x1B[1;3H") + std::string(kOpenDocs) + "\x1B[0mcd" +
                                         std::string(kClose));
    CK_CHECK(term.display().frame().link_target(Point{2, 0}) == kDocs);
    CK_CHECK(term.display().frame().link_target(Point{3, 0}) == kDocs);
    CK_CHECK(term.display().frame().link_target(Point{4, 0}).empty());
}

CK_TEST(a_synchronized_frame_closes_its_links_inside_the_bracket) {
    Capabilities caps = hyperlink_host();
    caps.synchronized_output = true;
    const std::string bytes = presented_bytes(linked_row(), caps);
    CK_CHECK(bytes.starts_with("\x1B[?2026h"));
    CK_CHECK(bytes.ends_with("\x1B[?2026l"));
    CK_CHECK(bytes.rfind(kClose) < bytes.rfind("\x1B[?2026l"));
}

// --- VirtualDisplay: decoding ------------------------------------------------------

CK_TEST(the_display_links_the_text_printed_while_a_hyperlink_is_open) {
    VirtualDisplay display(Size{6, 1});
    CK_CHECK(display.write(std::string("ab") + std::string(kOpenDocs) + "c\x1B[1md" +
                           std::string(kClose) + "e"));
    const FrameView frame = display.frame();
    CK_CHECK(frame.link_target(Point{1, 0}).empty());
    CK_CHECK(frame.link_target(Point{2, 0}) == kDocs);
    CK_CHECK(frame.link_target(Point{3, 0}) == kDocs);
    CK_CHECK(frame.link_target(Point{4, 0}).empty());
    CK_CHECK(display.open_hyperlink().empty());
}

CK_TEST(the_display_links_both_columns_of_a_wide_glyph) {
    VirtualDisplay display(Size{4, 1});
    CK_CHECK(display.write(std::string(kOpenDocs) + "\xE6\x97\xA5" + std::string(kClose)));
    CK_CHECK(display.frame().link_target(Point{0, 0}) == kDocs);
    CK_CHECK(display.frame().link_target(Point{1, 0}) == kDocs);
    CK_CHECK(display.frame().link_target(Point{2, 0}).empty());
}

CK_TEST(erasing_or_overprinting_a_linked_cell_unlinks_it_and_clear_forgets_every_link) {
    VirtualDisplay display(Size{4, 2});
    CK_CHECK(display.write(std::string(kOpenDocs) + "abcd" + std::string(kClose)));
    CK_CHECK(display.write("\x1B[1;1Hx\x1B[1;4H\x1B[K"));
    CK_CHECK(display.frame().link_target(Point{0, 0}).empty());
    CK_CHECK(display.frame().link_target(Point{1, 0}) == kDocs);
    CK_CHECK(display.frame().link_target(Point{3, 0}).empty());
    CK_CHECK(display.write("\x1B[S"));  // scrolls the linked row away
    CK_CHECK(display.frame().link_target(Point{1, 1}).empty());
    CK_CHECK(display.frame().links()->empty());
    CK_CHECK(display.write(std::string("\x1B[2;1H") + std::string(kOpenHome) + "z" + std::string(kClose)));
    display.clear();
    CK_CHECK(display.frame().links()->empty());
}

CK_TEST(a_synchronized_update_shows_its_links_only_when_it_ends) {
    VirtualDisplay display(Size{2, 1});
    CK_CHECK(display.feed(std::string("\x1B[?2026h") + std::string(kOpenDocs) + "ab" + std::string(kClose)));
    CK_CHECK(display.frame().link_target(Point{0, 0}).empty());
    CK_CHECK(display.feed("\x1B[?2026l"));
    CK_CHECK(display.frame().link_target(Point{0, 0}) == kDocs);
}

CK_TEST(the_display_refuses_every_hyperlink_form_ckvision_never_emits) {
    const auto refused = [](std::string_view bytes) {
        VirtualDisplay display(Size{8, 2});
        return !display.write(bytes);
    };
    const std::string open(kOpenDocs);
    const std::string close(kClose);
    // Not the id ckVision derives from the target, or not an id at all.
    CK_CHECK(refused("\x1B]8;;https://example.test/docs\x1B\\a" + close));
    CK_CHECK(refused("\x1B]8;id=0000000000000000;https://example.test/docs\x1B\\a" + close));
    CK_CHECK(refused("\x1B]8;id=34ad6241ce53a328:x=1;https://example.test/docs\x1B\\a" + close));
    // A target that is not a terminal hyperlink.
    CK_CHECK(refused("\x1B]8;id=" + hyperlink_id("guide.md") + ";guide.md\x1B\\a" + close));
    CK_CHECK(refused("\x1B]8;id=" + hyperlink_id("https://x.test/a b") + ";https://x.test/a b\x1B\\a" + close));
    // No separator; a close with parameters; a close with nothing open;
    // an open over an open one.
    CK_CHECK(refused("\x1B]8\x1B\\"));
    CK_CHECK(refused(open + "a\x1B]8;id=1;\x1B\\"));
    CK_CHECK(refused(close));
    CK_CHECK(refused(open + "a" + open + "b" + close));
    // Anything but text and SGR while a hyperlink is open.
    CK_CHECK(refused(open + "a\x1B[2;1Hb" + close));
    CK_CHECK(refused(open + "a\x1B[K" + close));
    CK_CHECK(refused(open + "a\x1B[S" + close));
    CK_CHECK(refused(open + "a\x1B[?25l" + close));
    CK_CHECK(refused(open + "a\x1B]0;title\x07" + close));
    CK_CHECK(refused(open + "a\x1BPq#0;2;0;0;0~\x1B\\" + close));
    // A stream that ends with its hyperlink still open.
    CK_CHECK(refused(open + "a"));
    // The accepted form, for contrast.
    CK_CHECK(!refused(open + "a\x1B[0;1mb" + close + "\x1B[2;1H"));
}
