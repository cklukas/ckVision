// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Hostile display text (the architecture §12, D-040): strings carrying
// terminal controls are painted through the public Painter, pinned as a
// checked-in golden, and presented, and the Presenter's raw bytes are checked
// to carry none of their controls. tests/golden/hostile_display_text.dump is
// regenerated from the same calls by
// tools/docgen/generate_hostile_content_goldens.cpp.
//
// It also shows why Cell::from_grapheme's full control-character
// neutralization matters to the golden dump format itself: an un-neutralized
// control byte — a raw newline above all — embedded in a grid row would
// corrupt this line-oriented format (docs/golden-format.md).
#include "cvision/core/cell.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"

namespace {

using namespace std::string_view_literals;

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// One hostile string per row: an erase-display CSI, an OSC 52 clipboard
// write, U+009B (CSI to a UTF-8 host), BEL/DEL/NUL, TAB and LF, raw 8-bit
// CSI and ST bytes, and malformed UTF-8.
constexpr std::string_view kHostileRows[] = {
    "A\x1B[2JB",
    "C\x1B]52;c;aGk=\x07" "D",
    "E\xC2\x9B" "31mF",
    "G\x07H\x7FI\x00J"sv,
    "K\tL\nM",
    "N\x9B" "2JO\x9CP",
    "Q\xFFR\xC3S\xE2\x82T",
};

ckv::scene::Surface paint_hostile_rows() {
    ckv::scene::Surface surface(ckv::Size{16, 7});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 16, 7});
    int row = 0;
    for (const std::string_view text : kHostileRows) painter.draw_text(ckv::Point{0, row++}, text, ckv::Style{});
    return surface;
}

}  // namespace

CK_TEST(hostile_grapheme_through_cell_produces_a_well_formed_dump) {
    // A raw '\n' reaching a grid row unneutralized would split it into
    // two lines and corrupt the whole document structure.
    const ckv::Cell hostile = ckv::Cell::from_grapheme("\n", ckv::Style{});
    CK_CHECK(hostile.grapheme() == "\xEF\xBF\xBD");  // U+FFFD, never a raw '\n'

    ckv::golden::Document doc;
    doc.cols = 3;
    doc.rows = 1;
    doc.grid.push_back(std::string("A") + std::string(hostile.grapheme()) + "B");
    doc.stylemap.push_back("000");
    doc.styles.push_back(ckv::golden::StyleSpec{});

    const std::string dump = ckv::golden::serialize(doc);
    // Exactly one grid line, with the hostile content neutralized in
    // place — the document did not gain an extra line.
    CK_CHECK(dump.find("|A\xEF\xBF\xBD" "B|\n") != std::string::npos);

    const ckv::golden::ParseResult reparsed = ckv::golden::parse(dump);
    CK_CHECK(static_cast<bool>(reparsed));
    if (reparsed) CK_CHECK(reparsed.document->grid[0] == std::string("A\xEF\xBF\xBD" "B"));
}

CK_TEST(hostile_display_text_painted_through_the_painter_matches_its_golden) {
    // Every control becomes one visible U+FFFD cell in place and the text
    // around it is kept, so the reader sees that something was there.
    const std::string actual = ckv::golden::serialize(ckv::scene::capture(paint_hostile_rows()));
    const std::string expected = read_file("golden/hostile_display_text.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);

    const ckv::golden::ParseResult reparsed = ckv::golden::parse(actual);
    CK_CHECK(static_cast<bool>(reparsed));
    if (reparsed) CK_CHECK(ckv::golden::serialize(*reparsed.document) == actual);
}

CK_TEST(presenting_hostile_display_text_emits_none_of_its_controls) {
    const ckv::scene::Surface surface = paint_hostile_rows();
    ckv::term::HeadlessTerminal terminal(ckv::Size{16, 7});
    ckv::term::Presenter presenter(terminal);
    presenter.present(surface.view(), ckv::CursorState{}, 0);
    const std::string_view bytes = terminal.written_bytes();
    CK_CHECK(!bytes.empty());

    // The Presenter's own CSI sequences are the only controls in the
    // stream: every C0 byte and DEL is an ESC, and every ESC introduces a
    // CSI. No NUL, BEL, TAB, LF or DEL of the text survives, no OSC is
    // opened, and the painted escape sequences arrive as inert text.
    for (std::size_t at = 0; at < bytes.size(); ++at) {
        const auto byte = static_cast<unsigned char>(bytes[at]);
        if (byte >= 0x20 && byte != 0x7F) continue;
        CK_CHECK(byte == 0x1B);
        CK_CHECK(at + 1 < bytes.size() && bytes[at + 1] == '[');
    }
    CK_CHECK(bytes.find("\x1B[2J") == std::string_view::npos);
    CK_CHECK(bytes.find("\x1B[31m") == std::string_view::npos);
    CK_CHECK(bytes.find("\x1B]") == std::string_view::npos);

    // No 8-bit control either: the stream is well-formed UTF-8 (so no lone
    // 0x9B or 0x9C byte) and holds no C1 code point (so no U+009B or U+009C).
    CK_CHECK(ckv::utf8::is_valid(bytes));
    for (std::size_t at = 0; at < bytes.size();) {
        const char32_t cp = ckv::utf8::decode(bytes, at);
        CK_CHECK(cp < 0x80 || cp > 0x9F);
    }

    // And the host shows exactly the neutralized frame.
    CK_CHECK(terminal.display().valid());
    const ckv::FrameView shown = terminal.display().frame();
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 16; ++x)
            CK_CHECK(shown.at(ckv::Point{x, y}).grapheme() == surface.view().at(ckv::Point{x, y}).grapheme());
}
