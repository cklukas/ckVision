// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/scene/rect_ops.hpp"

#include <algorithm>

namespace ckv::scene {

std::vector<Rect> subtract_rect(Rect from, Rect cut) noexcept {
    const Rect overlap = from.intersected(cut);
    if (overlap.empty()) return {from};

    std::vector<Rect> result;
    if (overlap.top() > from.top())
        result.push_back(Rect{from.x, from.y, from.width, overlap.top() - from.top()});
    if (overlap.bottom() < from.bottom())
        result.push_back(
            Rect{from.x, overlap.bottom(), from.width, from.bottom() - overlap.bottom()});
    if (overlap.left() > from.left())
        result.push_back(Rect{from.x, overlap.top(), overlap.left() - from.left(),
                               overlap.bottom() - overlap.top()});
    if (overlap.right() < from.right())
        result.push_back(Rect{overlap.right(), overlap.top(), from.right() - overlap.right(),
                               overlap.bottom() - overlap.top()});
    return result;
}

std::vector<Rect> subtract_rects(Rect from, const std::vector<Rect>& cuts) noexcept {
    std::vector<Rect> remaining{from};
    for (const Rect& cut : cuts) {
        std::vector<Rect> next;
        for (const Rect& r : remaining) {
            const std::vector<Rect> pieces = subtract_rect(r, cut);
            next.insert(next.end(), pieces.begin(), pieces.end());
        }
        remaining = std::move(next);
    }
    remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
                                    [](const Rect& r) { return r.empty(); }),
                     remaining.end());
    return remaining;
}

}  // namespace ckv::scene
