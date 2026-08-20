// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// M0 benchmark smoke: exercises the golden format on a realistic frame
// size so the harness wiring is proven end to end.
#include <cstdio>
#include <string>

#include "ckbench.hpp"
#include "cvision/core/golden.hpp"

namespace {

ckv::golden::Document make_document(int cols, int rows) {
    ckv::golden::Document doc;
    doc.cols = cols;
    doc.rows = rows;
    doc.cursor = {true, 1, 1, "block"};
    for (int i = 0; i < 4; ++i) {
        ckv::golden::StyleSpec style;
        style.fg = {ckv::golden::Color::Kind::Rgb, 0, static_cast<std::uint8_t>(64 * i), 255, 128};
        style.bg = {ckv::golden::Color::Kind::Rgb, 0, 0, 0, static_cast<std::uint8_t>(32 * i)};
        if (i % 2 == 1) style.attrs = {"bold"};
        doc.styles.push_back(style);
    }
    for (int r = 0; r < rows; ++r) {
        doc.grid.emplace_back(static_cast<std::size_t>(cols), 'x');
        std::string map(static_cast<std::size_t>(cols), '0');
        for (std::size_t c = 0; c < map.size(); ++c)
            map[c] = ckv::golden::style_alphabet[c % 4];
        doc.stylemap.push_back(map);
    }
    doc.rasters.push_back({1, 2, 2, 10, 5, 160, 80, "deadbeef", true});
    return doc;
}

}  // namespace

void run_golden_benchmarks() {
    const ckv::golden::Document doc = make_document(80, 25);
    std::size_t sink = 0;

    ckbench::run("golden_serialize_80x25", 1000, [&] {
        sink += ckv::golden::serialize(doc).size();
    });

    const std::string text = ckv::golden::serialize(doc);
    ckbench::run("golden_parse_80x25", 1000, [&] {
        const ckv::golden::ParseResult result = ckv::golden::parse(text);
        if (result) sink += result.document->grid.size();
    });

    std::printf("checksum %zu\n", sink);
}
