// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

class Progress : public ui::View {
public:
    Progress();

    void set_fraction(double fraction);
    double fraction() const noexcept { return fraction_; }

    void set_indeterminate(bool indeterminate);
    bool indeterminate() const noexcept { return indeterminate_; }
    void set_pulse(int offset);
    int pulse() const noexcept { return pulse_; }

    void set_label(std::string label);
    const std::string& label() const noexcept { return label_; }

    void on_attached() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

private:
    double fraction_ = 0.0;
    bool indeterminate_ = false;
    int pulse_ = 0;
    std::string label_;
    ui::RoleId track_role_ = ui::kInvalidRole;
    ui::RoleId fill_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
