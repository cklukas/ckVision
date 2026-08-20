// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/sixel_encoder.hpp"

#include "cvision/testing/cktest.hpp"

using namespace ckv;
using namespace ckv::term;

namespace {

std::size_t count_occurrences(std::string_view haystack, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += 1;
    }
    return count;
}

}  // namespace

CK_TEST(empty_image_encodes_to_empty_string) {
    CK_CHECK(encode_sixel(Image()).empty());
}

CK_TEST(encoded_sequence_has_the_correct_dcs_and_string_terminator) {
    Image img(4, 4);
    const std::string sixel = encode_sixel(img);
    CK_CHECK(sixel.starts_with("\x1BP0;0;0q\"1;1;4;4"));
    CK_CHECK(sixel.substr(sixel.size() - 2) == "\x1B\\");
}

CK_TEST(source_alpha_is_ignored_and_the_encoded_raster_is_explicitly_opaque) {
    Image img(2, 1);
    img.set_pixel(0, 0, Image::Rgba{255, 0, 0, 0});
    img.set_pixel(1, 0, Image::Rgba{0, 255, 0, 127});
    const std::string sixel = encode_sixel(img);
    CK_CHECK(sixel.starts_with("\x1BP0;0;0q\"1;1;2;1"));
    CK_CHECK(sixel.find("#0;2;100;0;0") != std::string::npos);
    CK_CHECK(sixel.find("#1;2;0;100;0") != std::string::npos);
}

CK_TEST(single_color_image_defines_exactly_one_palette_register) {
    Image img(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) img.set_pixel(x, y, Image::Rgba{200, 50, 50, 255});
    const std::string sixel = encode_sixel(img);
    CK_CHECK(count_occurrences(sixel, "#0;2;") == 1);
    CK_CHECK(sixel.find("#1;2;") == std::string::npos);  // no second register
}

CK_TEST(distinct_colors_get_distinct_palette_registers) {
    Image img(2, 1);
    img.set_pixel(0, 0, Image::Rgba{255, 0, 0, 255});
    img.set_pixel(1, 0, Image::Rgba{0, 255, 0, 255});
    const std::string sixel = encode_sixel(img);
    CK_CHECK(sixel.find("#0;2;") != std::string::npos);
    CK_CHECK(sixel.find("#1;2;") != std::string::npos);
}

CK_TEST(percent_encoding_matches_known_reference_values) {
    Image img(1, 1);
    img.set_pixel(0, 0, Image::Rgba{255, 128, 0, 255});
    const std::string sixel = encode_sixel(img);
    // 255 -> 100%, 128 -> 50%, 0 -> 0% (rounded).
    CK_CHECK(sixel.find("#0;2;100;50;0") != std::string::npos);
}

CK_TEST(image_taller_than_one_band_emits_a_newline_separator) {
    Image img(2, 10);  // 10 rows -> 2 bands (0-5, 6-9)
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 2; ++x)
            img.set_pixel(x, y, Image::Rgba{static_cast<std::uint8_t>(y * 20), 0, 0, 255});
    const std::string sixel = encode_sixel(img);
    CK_CHECK(sixel.find('-') != std::string::npos);  // band separator present
}

CK_TEST(single_band_image_has_no_newline_separator) {
    Image img(4, 4);  // fits entirely in one band (<=6 rows)
    img.set_pixel(0, 0, Image::Rgba{10, 20, 30, 255});
    const std::string sixel = encode_sixel(img);
    // No '-' should appear as a band separator (only inside the DCS
    // body — none expected here since there is exactly one band).
    const std::size_t q = sixel.find('q');
    const std::string body = sixel.substr(q + 1, sixel.size() - q - 1 - 2);  // strip DCS header/ST
    CK_CHECK(body.find('-') == std::string::npos);
}

CK_TEST(more_than_256_unique_colors_falls_back_to_the_quantized_cube) {
    // A gradient image with far more than 256 unique RGB combinations.
    Image img(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            img.set_pixel(x, y,
                          Image::Rgba{static_cast<std::uint8_t>(x * 4), static_cast<std::uint8_t>(y * 4),
                                      static_cast<std::uint8_t>((x + y) * 2), 255});
    const std::string sixel = encode_sixel(img);
    // Must never exceed the 6-level cube's 216-register ceiling.
    CK_CHECK(count_occurrences(sixel, ";2;") <= 216);
    CK_CHECK(!sixel.empty());
}

CK_TEST(verified_sixel_color_register_limit_bounds_the_emitted_palette) {
    Image img(32, 1);
    for (int x = 0; x < img.width(); ++x)
        img.set_pixel(x, 0, Image::Rgba{static_cast<std::uint8_t>(x * 8),
                                        static_cast<std::uint8_t>(255 - x * 8),
                                        static_cast<std::uint8_t>(x * 4), 255});
    const std::string sixel = encode_sixel(img, 4);
    CK_CHECK(count_occurrences(sixel, ";2;") <= 4);
    CK_CHECK(!sixel.empty());
}

CK_TEST(sixel_data_characters_stay_within_the_valid_range) {
    Image img(3, 3);
    img.set_pixel(0, 0, Image::Rgba{1, 2, 3, 255});
    img.set_pixel(1, 1, Image::Rgba{4, 5, 6, 255});
    img.set_pixel(2, 2, Image::Rgba{7, 8, 9, 255});
    const std::string sixel = encode_sixel(img);
    // Every raw sixel data character (outside escape/control sequences
    // and digits/punctuation used for registers/repeat counts) must be
    // in the documented 63-126 range when it IS a data character; this
    // is a coarse sanity sweep for any accidental out-of-range byte.
    for (const unsigned char c : sixel) {
        CK_CHECK(c == 0x1B || c == '\\' || c == 'P' || c == 'q' || (c >= 0x20 && c <= 0x7E));
    }
}
