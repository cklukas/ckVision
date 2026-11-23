// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Overlay: the layered-composition container from D-006's remaining
// set (M10/WP-19) — a stack of children sharing the same footprint
// (a background layer, content above it, a toast/tooltip layer above
// that). Z-order is NOT a separate concept Overlay invents: it is
// exactly View::children()' own list order, the same order every
// paint_children()/topmost_view_at() walk already honors (later
// children on top) — a later layer is added later, and an existing
// one is restacked with the already-public View::raise_to_front(),
// with no Overlay-specific wrapper needed for either.
//
// Each child is either Fill (resized to the overlay's own full extent
// on every insertion and every later resize — the common case: a
// layer that covers the whole stack) or Manual (keeps whatever bounds
// it was given, completely untouched by the overlay's own bounds
// changing — a small positioned badge over otherwise-Fill layers
// below it). Overrides on_child_size_hint_changed() and aggregates
// Fill children's size hints so enclosing containers adapt cleanly.
#pragma once

#include "cvision/core/export.hpp"

#include <unordered_map>

#include "cvision/ui/view.hpp"

namespace ckv::ui {

// How an Overlay places one layer: Fill resizes it to the overlay's whole extent, Manual leaves
// its bounds entirely to the caller.
enum class OverlayMode { Fill, Manual };

// A container that stacks its children over the same footprint, later children on top. Fill
// layers are resized on every add_item() and every resize; children added with add_child() are
// treated as Fill.
class CVISION_API Overlay : public View {
public:
    // Same constructor as View: parent-local bounds and a focus policy, both usually defaulted.
    using View::View;

    // Adds `child` as the new top layer in `mode` and re-places the Fill layers. `child` must not
    // be null. Returns the child as add_child() does, nullptr when its attachment callback
    // detached or destroyed it.
    View* add_item(std::unique_ptr<View> child, OverlayMode mode = OverlayMode::Fill);
    template <typename T>
        requires std::derived_from<T, View>
    T* add_item(std::unique_ptr<T> child, OverlayMode mode = OverlayMode::Fill) {
        return static_cast<T*>(add_item(std::unique_ptr<View>(std::move(child)), mode));
    }
    // Construct a child of type T in place and attach it as the top layer.
    template <typename T, typename... Args>
        requires std::derived_from<T, View>
    T* make_item(OverlayMode mode, Args&&... args) {
        return add_item(std::make_unique<T>(std::forward<Args>(args)...), mode);
    }
    // Detaches `child` exactly as View::remove_child() does, then forgets its mode and returns
    // ownership; nullptr when `child` is not a layer of this overlay. The remaining layers are
    // not moved. Every way a layer leaves comes through here, so a view added back with
    // add_child() is a Fill layer again.
    std::unique_ptr<View> remove_child(View* child) override;

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    void on_child_size_hint_changed(View&) override {
        relayout();
        size_hint_changed();
    }
    void on_resized() override { relayout(); }

protected:
    using View::add_child;
    using View::add;
    using View::make;

private:
    void relayout();

    std::unordered_map<View*, OverlayMode> specs_;
};

}  // namespace ckv::ui
