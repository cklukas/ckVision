// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Shared tree-visibility and teardown helpers for Scrollbars hosted on an
// enclosing Window's frame rather than embedded as a direct child of the
// scrollable view they control.
#pragma once

#include <memory>

#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets::detail {

inline bool is_view_ancestor(const ui::View* ancestor, const ui::View* descendant) noexcept {
    for (const ui::View* cur = descendant; cur != nullptr; cur = cur->parent()) {
        if (cur == ancestor) return true;
    }
    return false;
}

inline bool subject_visible_for_bar(const ui::View& subject, const Scrollbar& bar) noexcept {
    const ui::View* const host = bar.parent();
    if (host == &subject) return true;
    for (const ui::View* cur = &subject; cur != nullptr && cur != host; cur = cur->parent()) {
        if (!cur->visible()) return false;
    }
    return true;
}

inline void sever_scrollbar(ui::View& owner, Scrollbar* bar) {
    if (bar == nullptr) return;
    bar->on_change = nullptr;
    bar->bind_subject(nullptr);
    if (bar->parent() != &owner) bar->set_policy(ScrollbarPolicy::Hidden);
}

inline bool detach_orphaned_scrollbar(ui::View& owner, Scrollbar*& bar,
                                      std::weak_ptr<void>& liveness) {
    if (bar == nullptr || liveness.expired()) {
        bar = nullptr;
        liveness.reset();
        return false;
    }
    if (bar->parent() == nullptr || bar->parent() == &owner || is_view_ancestor(bar->parent(), &owner))
        return false;
    sever_scrollbar(owner, bar);
    bar = nullptr;
    liveness.reset();
    return true;
}

}  // namespace ckv::widgets::detail
