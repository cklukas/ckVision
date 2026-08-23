// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/status_line.hpp"

namespace ckv::ui {
class AnchorPane;
class Column;
class Dock;
class Grid;
class Overlay;
class Row;
}  // namespace ckv::ui

namespace ckv::widgets {
class Label;
class Splitter;
class Window;
}  // namespace ckv::widgets

namespace ckv::layouts {

class LayoutsApp {
public:
    explicit LayoutsApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* window() const noexcept { return window_; }
    ui::Row* row() const noexcept { return row_; }
    ui::Column* column() const noexcept { return column_; }
    ui::Grid* grid() const noexcept { return grid_; }
    ui::Dock* dock() const noexcept { return dock_; }
    ui::Overlay* overlay() const noexcept { return overlay_; }
    widgets::Splitter* splitter() const noexcept { return splitter_; }
    widgets::Label* anchored_label() const noexcept { return anchored_label_; }

private:
    void build_chrome();
    void build_window();

    ui::Application& app_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    ui::Row* row_ = nullptr;
    ui::Column* column_ = nullptr;
    ui::Grid* grid_ = nullptr;
    ui::Dock* dock_ = nullptr;
    ui::Overlay* overlay_ = nullptr;
    widgets::Splitter* splitter_ = nullptr;
    widgets::Label* anchored_label_ = nullptr;
};

}  // namespace ckv::layouts
