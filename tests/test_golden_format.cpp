// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/golden.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A minimal canonical document used as the base for mutation tests.
const char* minimal_doc = "ckvision-golden 1\n"
                          "frame 2 1\n"
                          "cursor hidden\n"
                          "styles 1\n"
                          "0 fg default bg default attrs -\n"
                          "grid\n"
                          "|ab|\n"
                          "stylemap\n"
                          "|00|\n"
                          "end\n";

}  // namespace

CK_TEST(hand_authored_dump_round_trips_byte_exactly) {
    const std::string original = read_file("golden/hello.dump");
    CK_CHECK(!original.empty());
    const ckv::golden::ParseResult result = ckv::golden::parse(original);
    CK_CHECK(static_cast<bool>(result));
    if (result) {
        const ckv::golden::Document& doc = *result.document;
        CK_CHECK(doc.cols == 12);
        CK_CHECK(doc.rows == 3);
        CK_CHECK(doc.styles.size() == 2);
        CK_CHECK(doc.cursor.visible);
        CK_CHECK(doc.rasters.size() == 1);
        CK_CHECK(ckv::golden::serialize(doc) == original);
    }
}

CK_TEST(minimal_doc_round_trips) {
    const ckv::golden::ParseResult result = ckv::golden::parse(minimal_doc);
    CK_CHECK(static_cast<bool>(result));
    if (result) CK_CHECK(ckv::golden::serialize(*result.document) == minimal_doc);
}

CK_TEST(non_canonical_color_normalizes_to_uppercase) {
    std::string text = minimal_doc;
    const std::string from = "0 fg default bg default attrs -";
    text.replace(text.find(from), from.size(), "0 fg #c0ffee bg default attrs bold,dim");
    const ckv::golden::ParseResult result = ckv::golden::parse(text);
    CK_CHECK(static_cast<bool>(result));
    if (result) {
        const std::string canonical = ckv::golden::serialize(*result.document);
        CK_CHECK(canonical.find("#C0FFEE") != std::string::npos);
        CK_CHECK(canonical.find("bold,dim") != std::string::npos);
    }
}

CK_TEST(a_palette_colour_is_written_as_the_index_it_is) {
    std::string text = minimal_doc;
    const std::string from = "0 fg default bg default attrs -";
    text.replace(text.find(from), from.size(), "0 fg @9 bg @0 attrs -");
    const ckv::golden::ParseResult result = ckv::golden::parse(text);
    CK_CHECK(static_cast<bool>(result));
    if (result) {
        CK_CHECK(result.document->styles[0].fg.kind == ckv::golden::Color::Kind::Indexed);
        CK_CHECK(result.document->styles[0].fg.index == 9);
        CK_CHECK(ckv::golden::serialize(*result.document) == text);
    }
}

CK_TEST(an_underline_shape_and_colour_are_written_only_where_they_say_something) {
    std::string text = minimal_doc;
    const std::string from = "0 fg default bg default attrs -";
    text.replace(text.find(from), from.size(),
                 "0 fg default bg default attrs underline underline curly ulcolor @1");
    const ckv::golden::ParseResult result = ckv::golden::parse(text);
    CK_CHECK(static_cast<bool>(result));
    if (result) {
        CK_CHECK(result.document->styles[0].underline == "curly");
        CK_CHECK(result.document->styles[0].underline_color.index == 1);
        CK_CHECK(ckv::golden::serialize(*result.document) == text);
    }

    // The plain rule and a rule that follows the text are what an underline
    // is unless something says otherwise, so neither is ever written: one
    // appearance has exactly one spelling.
    std::string plain = minimal_doc;
    plain.replace(plain.find(from), from.size(), "0 fg default bg default attrs underline");
    const ckv::golden::ParseResult unadorned = ckv::golden::parse(plain);
    CK_CHECK(static_cast<bool>(unadorned));
    if (unadorned) CK_CHECK(ckv::golden::serialize(*unadorned.document) == plain);
}

CK_TEST(rejects_bad_input) {
    using ckv::golden::parse;

    // Wrong version / magic.
    CK_CHECK(!parse("ckvision-golden 2\n"));
    // Missing final newline.
    CK_CHECK(!parse("ckvision-golden 1"));

    const auto mutate = [](const std::string& from, const std::string& to) {
        std::string text = minimal_doc;
        text.replace(text.find(from), from.size(), to);
        return text;
    };

    // Cursor outside the frame.
    CK_CHECK(!parse(mutate("cursor hidden", "cursor 2 0 block")));
    // Unknown cursor shape.
    CK_CHECK(!parse(mutate("cursor hidden", "cursor 0 0 beam")));
    // Style count above the version-1 limit.
    CK_CHECK(!parse(mutate("styles 1", "styles 63")));
    // Non-contiguous style index.
    CK_CHECK(!parse(mutate("0 fg default", "1 fg default")));
    // Duplicate attribute.
    CK_CHECK(!parse(mutate("attrs -", "attrs bold,bold")));
    // Stylemap references an undeclared style.
    CK_CHECK(!parse(mutate("|00|", "|01|")));
    // Stylemap width mismatch.
    CK_CHECK(!parse(mutate("|00|", "|0|")));
    // Raster region outside the frame.
    CK_CHECK(!parse(mutate(
        "end", "raster 1 anchor 1 0 span 2 1 pixels 8 8 hash ab\nend")));
    // A raster line ends at its hash; the fallback state it once carried is gone (D-080).
    CK_CHECK(!parse(mutate(
        "end", "raster 1 anchor 0 0 span 1 1 pixels 8 8 hash ab fallback active\nend")));
    // Signed-overflow probe: an absurd anchor must be rejected, not wrap
    // around the int range into acceptance.
    CK_CHECK(!parse(mutate(
        "end",
        "raster 1 anchor 2147483647 0 span 1 1 pixels 8 8 hash ab\nend")));
    // Huge frame dimensions must fail cleanly without a giant allocation.
    CK_CHECK(!parse("ckvision-golden 1\n"
                    "frame 1 2147483647\n"
                    "cursor hidden\n"
                    "styles 1\n"
                    "0 fg default bg default attrs -\n"
                    "grid\n"
                    "|a|\n"
                    "end\n"));
    // Non-canonical integer spellings are rejected.
    CK_CHECK(!parse(mutate("frame 2 1", "frame 02 1")));
    CK_CHECK(!parse(mutate("cursor hidden", "cursor -0 0 block")));
    CK_CHECK(!parse(mutate("cursor hidden", "cursor +1 0 block")));
    // Raster hash must be lowercase hex.
    CK_CHECK(!parse(mutate(
        "end", "raster 1 anchor 0 0 span 1 1 pixels 8 8 hash AB\nend")));
    // A palette index has to be a palette index.
    CK_CHECK(!parse(mutate("fg default", "fg @256")));
    CK_CHECK(!parse(mutate("fg default", "fg @")));
    CK_CHECK(!parse(mutate("fg default", "fg @01")));
    // An underline shape or colour with no underline to describe.
    CK_CHECK(!parse(mutate("attrs -", "attrs bold underline curly")));
    CK_CHECK(!parse(mutate("attrs -", "attrs bold ulcolor @1")));
    // The plain rule and a default colour are spelled by their absence.
    CK_CHECK(!parse(mutate("attrs -", "attrs underline underline straight")));
    CK_CHECK(!parse(mutate("attrs -", "attrs underline ulcolor default")));
    // Trailing tokens nobody defined.
    CK_CHECK(!parse(mutate("attrs -", "attrs underline blink yes")));
    // Content after 'end'.
    CK_CHECK(!parse(mutate("end", "end\nextra")));
}

CK_TEST(content_after_end_reports_the_offending_line) {
    std::string text = minimal_doc;
    const std::string from = "end";
    text.replace(text.find(from), from.size(), "end\nextra");
    const ckv::golden::ParseResult result = ckv::golden::parse(text);
    CK_CHECK(!result);
    CK_CHECK(result.error.line == 11);
}

CK_TEST(error_reports_a_line_number) {
    std::string text = minimal_doc;
    const std::string from = "|00|";
    text.replace(text.find(from), from.size(), "|0!|");
    const ckv::golden::ParseResult result = ckv::golden::parse(text);
    CK_CHECK(!result);
    CK_CHECK(result.error.line == 9);
    CK_CHECK(!result.error.message.empty());
}


// --- Link records (D-088) -----------------------------------------------------

namespace {

const char* linked_doc = "ckvision-golden 1\n"
                         "frame 6 2\n"
                         "cursor hidden\n"
                         "styles 1\n"
                         "0 fg default bg default attrs -\n"
                         "grid\n"
                         "|abcdef|\n"
                         "|ghijkl|\n"
                         "stylemap\n"
                         "|000000|\n"
                         "|000000|\n"
                         "link 1 0 2 https://a.test/\n"
                         "link 3 0 1 https://b.test/\n"
                         "link 0 1 6 https://a.test/\n"
                         "raster 1 anchor 0 0 span 1 1 pixels 8 8 hash ab\n"
                         "end\n";

}  // namespace

CK_TEST(link_records_round_trip_between_the_stylemap_and_the_rasters) {
    const ckv::golden::ParseResult result = ckv::golden::parse(linked_doc);
    CK_CHECK(static_cast<bool>(result));
    if (!result) return;
    const ckv::golden::Document& doc = *result.document;
    CK_CHECK(doc.links.size() == 3);
    CK_CHECK(doc.links[0].col == 1 && doc.links[0].row == 0 && doc.links[0].cols == 2);
    CK_CHECK(doc.links[0].target == "https://a.test/");
    CK_CHECK(doc.links[2].cols == 6);
    CK_CHECK(doc.rasters.size() == 1);
    CK_CHECK(ckv::golden::serialize(doc) == linked_doc);
}

CK_TEST(a_dump_without_links_is_spelled_exactly_as_before) {
    const ckv::golden::ParseResult result = ckv::golden::parse(minimal_doc);
    CK_CHECK(static_cast<bool>(result));
    if (result) CK_CHECK(result.document->links.empty());
}

CK_TEST(link_records_have_one_spelling_and_stay_inside_the_frame) {
    using ckv::golden::parse;
    const auto mutate = [](const std::string& from, const std::string& to) {
        std::string text = linked_doc;
        text.replace(text.find(from), from.size(), to);
        return text;
    };
    // Outside the row or the frame, or empty.
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 5 0 2 https://b.test/")));
    CK_CHECK(!parse(mutate("link 0 1 6 https://a.test/", "link 0 2 1 https://a.test/")));
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 3 0 0 https://b.test/")));
    // A target that is not a terminal hyperlink cannot have been captured.
    CK_CHECK(!parse(mutate("https://b.test/", "guide.md")));
    // Out of order, overlapping, or split where one run would do.
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 0 0 1 https://b.test/")));
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 2 0 1 https://b.test/")));
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 3 0 1 https://a.test/")));
    // Missing or extra tokens.
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 3 0 1")));
    CK_CHECK(!parse(mutate("link 3 0 1 https://b.test/", "link 3 0 1 https://b.test/ extra")));
    // Links come before rasters.
    CK_CHECK(!parse(mutate("link 0 1 6 https://a.test/\nraster 1 anchor 0 0 span 1 1 pixels 8 8 hash ab",
                           "raster 1 anchor 0 0 span 1 1 pixels 8 8 hash ab\nlink 0 1 6 https://a.test/")));
}
