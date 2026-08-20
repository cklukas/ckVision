// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Sixel decoding, on its own, at the size of the picture.
//
// A Sixel is a picture; the terminal it arrives in is not part of it. Decoding
// one by painting it into a plane the size of the whole window and then
// looking through that window for the pixels that were touched makes the cost
// of a small logo a function of how large the reader happened to make their
// terminal — a picture that takes 0.7 ms in a small window takes 15 ms in a
// large one, for the same bytes. Here the sequence is read twice: once to
// learn how large the picture is, then once to draw it, into exactly that
// many pixels.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "cvision/core/geometry.hpp"
#include "cvision/core/image.hpp"

namespace ckv::term {

// The Sixel colour registers a terminal holds. Sixel numbers its registers
// `#0` … `#255`, and a child that asks XTSMGRAPHICS how many the terminal it
// is talking to has is told this number.
inline constexpr int kSixelColorRegisters = 256;

// The colour registers a Sixel stream draws from. They belong to the terminal
// and outlive any one sequence — a program may define a palette in one image
// and use it in the next — so the caller holds them.
struct SixelPalette {
    std::array<Image::Rgba, kSixelColorRegisters> colors{};
    std::array<bool, kSixelColorRegisters> defined{};
};

struct DecodedSixel {
    // Exactly the picture: its own declared raster size where it gives one,
    // otherwise the extent it actually drew. Untouched pixels are transparent.
    Image image;
    // P2 = 0: the picture's own area is cleared before it is drawn, so it
    // replaces whatever was under it rather than being laid over it.
    bool erases_background = true;
    // The size the sequence declared for itself in its raster attributes, or
    // {0,0} when it declared none. A declared size can exceed what was drawn,
    // and it is that whole rectangle a P2=0 picture clears.
    Size declared;
};

// Decodes one DCS Sixel body — the bytes between `ESC P` and the string
// terminator, parameters and `q` included.
//
// `visible` is how much room the destination has for it, in pixels, measured
// from where the picture starts: a picture wider or taller than that is cut
// off there, the way a terminal cuts one off at the edge of the screen rather
// than refusing to show it. Zero in either dimension means there is no room
// at all — the sequence is still read, and the picture comes back empty.
//
// `max_pixels` is a resource ceiling on what is kept, not on what was asked
// for. Returns nullopt and sets `error` for a malformed sequence, one this
// decoder does not implement, or a picture past that ceiling.
std::optional<DecodedSixel> decode_sixel(std::string_view body, Size visible, std::size_t max_pixels,
                                         SixelPalette& palette, std::string& error);

}  // namespace ckv::term
