// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "cvision/ui/application.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {
class ComboBox;
class InputLine;
class ListView;
class Memo;
class Progress;
class TabControl;
class Table;
class TextView;
class TreeView;
class Window;
}  // namespace ckv::widgets

namespace ckv::workbench {

class WorkbenchApp {
public:
    explicit WorkbenchApp(ui::Application& app);

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* window() const noexcept { return window_; }
    widgets::TabControl* tabs() const noexcept { return tabs_; }
    widgets::Memo* memo() const noexcept { return memo_; }
    widgets::InputLine* command_input() const noexcept { return command_input_; }
    widgets::TextView* text_view() const noexcept { return text_view_; }
    widgets::Table* table() const noexcept { return table_; }
    widgets::TreeView* tree() const noexcept { return tree_; }
    widgets::ListView* list() const noexcept { return list_; }
    widgets::ComboBox* combo() const noexcept { return combo_; }
    widgets::Progress* progress() const noexcept { return progress_; }
    const std::optional<std::string>& last_link() const noexcept { return last_link_; }

private:
    void build_chrome();
    void build_window();
    std::unique_ptr<ui::View> build_text_page();
    std::unique_ptr<ui::View> build_data_page();
    std::unique_ptr<ui::View> build_help_page();

    ui::Application& app_;
    ui::StandardRoles roles_;
    ui::HistoryRegistry history_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::TabControl* tabs_ = nullptr;
    widgets::Memo* memo_ = nullptr;
    widgets::InputLine* command_input_ = nullptr;
    widgets::TextView* text_view_ = nullptr;
    widgets::Table* table_ = nullptr;
    widgets::TreeView* tree_ = nullptr;
    widgets::ListView* list_ = nullptr;
    widgets::ComboBox* combo_ = nullptr;
    widgets::Progress* progress_ = nullptr;
    std::optional<std::string> last_link_;
};

}  // namespace ckv::workbench
