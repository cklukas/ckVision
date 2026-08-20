// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Demonstrates why Cell::from_grapheme's full control-character
// neutralization (D-040) matters to the golden dump format itself: an
// un-neutralized control byte — a raw newline above all — embedded in a
// grid row would corrupt this line-oriented format
// (docs/golden-format.md).
#include "cvision/core/cell.hpp"
#include "cvision/core/golden.hpp"

#include "cvision/testing/cktest.hpp"

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
