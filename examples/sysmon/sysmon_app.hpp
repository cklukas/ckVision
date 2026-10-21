// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "monitor_service.hpp"
#include "settings.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/common_components.hpp"
#include <array>
#include <memory>

namespace ckv::sysmon {
struct CalendarProviders {
    std::function<widgets::TimeValue()> time;
    std::function<widgets::DateValue()> date;
};
struct SettingsBinding {
    Settings value;
    std::function<std::string(const Settings&)> save;
    std::string diagnostic;
};
class SysmonApp {
public:
    SysmonApp(ui::Application& app, MonitorSource& source, CalendarProviders calendar, SettingsBinding settings = {});
    ~SysmonApp();
    SysmonApp(const SysmonApp&) = delete;
    SysmonApp& operator=(const SysmonApp&) = delete;
    widgets::Desktop& desktop() { return shell_->desktop(); }
    const std::vector<HostState>& hosts() const { return hosts_; }
    void wait_idle() { service_->wait_idle(); }
    void refresh();
    void open_calendar();
    // Public deterministic injection path used by replay/tests and live service.
    void accept(std::string endpoint, Sample sample);
private:
    void build_shell();
    void menus();
    void status();
    void paint(scene::Painter& painter, Rect area);
    void paint_host(scene::Painter& painter, Rect card, const HostState& host);
    void changed();
    void save_settings();
    std::string host_summary();
    ui::CommandId command(ui::CommandDescriptor descriptor);
    ui::Application& app_;
    CalendarProviders calendar_;
    ui::StandardRoles roles_;
    std::vector<HostState> hosts_;
    std::vector<ui::CommandId> commands_;
    std::array<ui::CommandId, 6> toggles_{};
    std::array<ui::CommandId, 2> theme_commands_{};
    SettingsBinding settings_;
    std::vector<ui::CommandId> host_commands_;
    ui::CommandId refresh_ = ui::kInvalidCommand, page_command_ = ui::kInvalidCommand;
    ui::CommandId detail_command_ = ui::kInvalidCommand, about_ = ui::kInvalidCommand, calendar_command_ = ui::kInvalidCommand;
    std::size_t page_ = 0, detail_page_ = 0;
    int ticks_ = 0;
    ui::Application::TimerId timer_ = 0;
    widgets::ClockView* clock_ = nullptr;
    ui::View* header_ = nullptr;
    std::unique_ptr<widgets::ApplicationShell> shell_;
    std::shared_ptr<void> lifetime_ = std::make_shared<int>(0);
    std::unique_ptr<MonitorService> service_;
};
} // namespace ckv::sysmon
