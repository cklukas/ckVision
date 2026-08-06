// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/golden.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "cktest.hpp"

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
        CK_CHECK(doc.rasters[0].fallback_active);
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
        "end", "raster 1 anchor 1 0 span 2 1 pixels 8 8 hash ab fallback active\nend")));
    // Signed-overflow probe: an absurd anchor must be rejected, not wrap
    // around the int range into acceptance.
    CK_CHECK(!parse(mutate(
        "end",
        "raster 1 anchor 2147483647 0 span 1 1 pixels 8 8 hash ab fallback active\nend")));
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
        "end", "raster 1 anchor 0 0 span 1 1 pixels 8 8 hash AB fallback active\nend")));
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

