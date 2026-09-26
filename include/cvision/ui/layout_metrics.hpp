// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "cvision/ui/view.hpp"

namespace ckv::ui::detail {

// The height, in rows, a layout gives `view` when it is `width` columns wide: the largest of its
// vertical minimum, its vertical preferred size and its height_for_width() answer. A negative
// width is treated as zero. Shared by the containers so a wrapped child is never squeezed below
// the height its text needs, nor below a height its hint asks for.
inline int preferred_height_for_width(const View& view, int width) {
    const SizeHint vertical = view.vertical_size_hint();
    const int measured = view.height_for_width(std::max(0, width));
    return std::max(vertical.min, std::max(vertical.preferred, measured));
}

// The children a layout actually places. A hidden child is not drawn, so
// reserving main-axis space for it would leave a gap the reader cannot
// account for — and asking it for a size hint would let something invisible
// decide how wide the container is. Hiding a child is therefore how an
// application removes it from a layout without removing it from the tree,
// which is what a container whose contents depend on state (a help viewer's
// cross-links, a form's conditional field) needs in order to keep the child
// alive across the moments it has nothing to show.
inline std::vector<View*> visible_children(const std::vector<std::unique_ptr<View>>& children) {
    std::vector<View*> visible;
    visible.reserve(children.size());
    for (const std::unique_ptr<View>& child : children)
        if (child->visible()) visible.push_back(child.get());
    return visible;
}

}  // namespace ckv::ui::detail
