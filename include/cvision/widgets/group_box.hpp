// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <memory>
#include <string>

#include "cvision/scene/painter.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// A titled single-line bordered container that frames one child view.
class CVISION_TYPE_API GroupBox final : public ui::View {
public:
    // Construct a GroupBox with an optional title and border line style.
    CVISION_MEMBER_API explicit GroupBox(std::string title = {},
                      scene::LineStyle line_style = scene::LineStyle::Single);

    // Replace the frame title and schedule a repaint.
    CVISION_MEMBER_API void set_title(std::string title);
    // Return the current frame title.
    const std::string& title() const noexcept { return title_; }

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

    // Resolve frame and title theme roles on attach.
    CVISION_MEMBER_API void on_attached() override;
    // Lay out the child view within the 1-cell border inset.
    CVISION_MEMBER_API void on_resized() override;
    // Recompute layout and propagate size-hint changes from the child.
    CVISION_MEMBER_API void on_child_size_hint_changed(ui::View&) override;
    // Draw the border frame and optional embedded title.
    CVISION_MEMBER_API void draw(scene::Painter& painter) override;
    // Return child horizontal size hint expanded by 2 border cells.
    CVISION_MEMBER_API ui::SizeHint horizontal_size_hint() const override;
    // Return child vertical size hint expanded by 2 border rows.
    CVISION_MEMBER_API ui::SizeHint vertical_size_hint() const override;
    // Return child height_for_width(width - 2) + 2.
    CVISION_MEMBER_API int height_for_width(int width) const override;

private:
    void relayout();

    std::string title_;
    scene::LineStyle line_style_ = scene::LineStyle::Single;
    ui::View* content_ = nullptr;
    ui::RoleId frame_role_ = ui::kInvalidRole;
    ui::RoleId title_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
