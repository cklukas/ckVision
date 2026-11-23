// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// A collapsible disclosure header ("[v] Title" / "[>] Title") framing one optional child view.
class CVISION_TYPE_API Expander final : public ui::View {
public:
    // Construct an Expander with a header `title` and initial `expanded` state.
    CVISION_MEMBER_API explicit Expander(std::string title = {}, bool expanded = false);

    // Replace the header title.
    CVISION_MEMBER_API void set_title(std::string title);
    // Return the header title.
    const std::string& title() const noexcept { return title_; }
    // Expand or collapse the content and notify layout parents.
    CVISION_MEMBER_API void set_expanded(bool expanded);
    // Return true if the content is currently expanded.
    bool expanded() const noexcept { return expanded_; }
    // Toggle the expanded state.
    void toggle() { set_expanded(!expanded_); }

    // Replace the single child view and return a non-owning pointer to it.
    CVISION_MEMBER_API ui::View* set_content(std::unique_ptr<ui::View> content);
    // Typed convenience overload of set_content().
    template <typename T>
        requires std::derived_from<T, ui::View>
    T* set_content(std::unique_ptr<T> content) {
        return static_cast<T*>(set_content(std::unique_ptr<ui::View>(std::move(content))));
    }
    // Return the current child view, or nullptr if empty.
    ui::View* content() const noexcept { return content_; }
    // Override the header theme role.
    CVISION_MEMBER_API void set_role_override(ui::RoleId role);

    // Invoked when the expanded state changes.
    std::function<void(bool)> on_change;

    // Resolve theme roles on attach.
    CVISION_MEMBER_API void on_attached() override;
    // Lay out the child view below the 1-row header when expanded.
    CVISION_MEMBER_API void on_resized() override;
    // Propagate size-hint changes from the child view.
    CVISION_MEMBER_API void on_child_size_hint_changed(ui::View&) override;
    // Paint the disclosure header.
    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Toggle on Space or Enter when focused.
    CVISION_MEMBER_API bool on_key(const KeyEvent& event) override;
    // Toggle on left mouse click in the header row.
    CVISION_MEMBER_API bool on_mouse(const MouseEvent& event) override;
    // Repaint on focus change.
    CVISION_MEMBER_API void on_focus(const FocusEvent& event) override;
    // Return PointerShape::Pointer over the header row.
    CVISION_MEMBER_API std::optional<PointerShape> pointer_shape_at(Point local) const override;
    // Return preferred horizontal extent.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    // Return 1 row when collapsed, or 1 + child height when expanded.
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;
    // Return 1 row when collapsed, or 1 + child height_for_width(width) when expanded.
    CVISION_MEMBER_API int height_for_width(int width) const override;

private:
    using ui::View::add;
    using ui::View::add_child;
    using ui::View::make;
    using ui::View::remove_child;

    void relayout();

    std::string title_;
    bool expanded_ = false;
    ui::View* content_ = nullptr;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId role_override_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
