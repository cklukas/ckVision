// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/history.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {
class BreadcrumbBar;
class CommandPalette;
class ComboBox;
class FlowView;
class InputLine;
class KeyChordCapture;
class ListView;
class Memo;
class NotificationCenter;
class PropertyInspector;
class Progress;
class SearchBox;
class TabControl;
class Table;
class TextView;
class ToolBar;
class Tooltip;
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
    widgets::FlowView* flow_view() const noexcept { return flow_view_; }
    widgets::Table* table() const noexcept { return table_; }
    widgets::TreeView* tree() const noexcept { return tree_; }
    widgets::ListView* list() const noexcept { return list_; }
    widgets::ComboBox* combo() const noexcept { return combo_; }
    widgets::Progress* progress() const noexcept { return progress_; }
    widgets::SearchBox* search_box() const noexcept { return search_box_; }
    widgets::ToolBar* tool_bar() const noexcept { return tool_bar_; }
    widgets::CommandPalette* command_palette() const noexcept { return command_palette_; }
    widgets::BreadcrumbBar* breadcrumb() const noexcept { return breadcrumb_; }
    widgets::PropertyInspector* property_inspector() const noexcept { return property_inspector_; }
    widgets::NotificationCenter* notifications() const noexcept { return notifications_; }
    widgets::Tooltip* tooltip() const noexcept { return tooltip_; }
    const std::optional<std::string>& last_link() const noexcept { return last_link_; }

    // The application's own commands: run a build, open (or bring forward)
    // the console window, and present the key bindings dialog.
    ui::CommandId build_command() const noexcept { return build_command_; }
    ui::CommandId console_command() const noexcept { return console_command_; }
    ui::CommandId keys_command() const noexcept { return keys_command_; }
    int builds_run() const noexcept { return builds_run_; }
    // The console window while it is open, else nullptr.
    widgets::Window* console_window() const noexcept { return console_window_; }
    // The Keys dialog while it is open, else nullptr.
    widgets::Window* keys_dialog() const noexcept { return keys_dialog_; }

private:
    // One row of the Keys dialog: a command and the control editing its chord.
    struct KeyBindingRow {
        ui::CommandId command = ui::kInvalidCommand;
        widgets::KeyChordCapture* capture = nullptr;
    };

    void build_chrome();
    void build_window();
    void open_console();
    void append_console_line(std::string line);
    void show_console_lines();
    void run_build();
    void present_keys_dialog();
    // Makes `chord` the command's only binding (or leaves it unbound), then
    // shows every row of the Keys dialog what is bound now: a chord taken
    // from another command has moved, and that row says so.
    void rebind(ui::CommandId command, const std::optional<KeyChord>& chord);
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
    widgets::FlowView* flow_view_ = nullptr;
    widgets::Table* table_ = nullptr;
    widgets::TreeView* tree_ = nullptr;
    widgets::ListView* list_ = nullptr;
    widgets::ComboBox* combo_ = nullptr;
    widgets::Progress* progress_ = nullptr;
    widgets::SearchBox* search_box_ = nullptr;
    widgets::ToolBar* tool_bar_ = nullptr;
    widgets::CommandPalette* command_palette_ = nullptr;
    widgets::BreadcrumbBar* breadcrumb_ = nullptr;
    widgets::PropertyInspector* property_inspector_ = nullptr;
    widgets::NotificationCenter* notifications_ = nullptr;
    widgets::Tooltip* tooltip_ = nullptr;
    std::optional<std::string> last_link_;

    ui::CommandId build_command_ = ui::kInvalidCommand;
    ui::CommandId console_command_ = ui::kInvalidCommand;
    ui::CommandId keys_command_ = ui::kInvalidCommand;
    int builds_run_ = 0;
    widgets::Window* console_window_ = nullptr;
    widgets::TextView* console_text_ = nullptr;
    std::vector<std::string> console_lines_;
    widgets::Window* keys_dialog_ = nullptr;
    std::vector<KeyBindingRow> key_rows_;
};

}  // namespace ckv::workbench
