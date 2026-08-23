// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class TabControl : public ui::View {
public:
    struct Tab {
        std::string label;
        ui::View* page = nullptr;
        std::optional<std::string> mnemonic;
    };

    TabControl();

    ui::View* add_tab(std::string label, std::unique_ptr<ui::View> page);
    std::size_t tab_count() const noexcept { return tabs_.size(); }
    const Tab& tab(std::size_t index) const { return tabs_[index]; }

    void set_active_index(std::size_t index);
    std::size_t active_index() const noexcept { return active_index_; }
    ui::View* active_page() const noexcept;

    void on_attached() override;
    void on_resized() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // The strip of tabs switches pages when clicked.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_focus(const FocusEvent& event) override;

private:
    int tab_start_x(std::size_t index) const;
    int tab_at_x(int local_x) const;
    void activate_delta(int delta);

    std::vector<Tab> tabs_;
    std::size_t active_index_ = 0;
    bool has_focus_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::RoleId page_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
