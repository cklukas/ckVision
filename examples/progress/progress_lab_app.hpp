// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cvision/widgets/progress_tasks.hpp"
#include "cvision/ui/standard_roles.hpp"
namespace ckv::widgets { class Desktop; class Window; class ComboBox; class SpinBox; class InputLine; class CheckGroup; }
namespace ckv::progress_lab {
// Interactive public-API example; its real object graph is also captured/tested.
class ProgressLabApp {
public:
    explicit ProgressLabApp(ui::Application& app);
    ~ProgressLabApp();
    widgets::ProgressModel& model() noexcept { return model_; }
    widgets::ProgressView& display() noexcept { return *display_; }
    widgets::ActivityIndicator& indicator() noexcept { return *indicator_; }
    widgets::Progress& meter() noexcept { return *meter_; }
    widgets::ProgressTaskId selected_task() const noexcept { return selected_; }
    widgets::ComboBox* selector(std::string_view name) const noexcept;
    // Drive/reset the same simulated operation the Run/Reset buttons use.
    void reset_tasks();
    void set_running(bool running);
private:
    void tick();
    void sync_selected();
    ui::Application& app_;
    ui::StandardRoles roles_;
    widgets::ProgressModel model_;
    widgets::ProgressController controller_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* window_ = nullptr;
    widgets::ProgressView* display_ = nullptr;
    widgets::ActivityIndicator* indicator_ = nullptr;
    widgets::Progress* meter_ = nullptr;
    widgets::ProgressTaskId selected_ = 0;
    std::vector<widgets::ProgressTaskId> tasks_;
    std::vector<std::pair<std::string, widgets::ComboBox*>> selectors_;
    std::vector<std::pair<std::string, widgets::SpinBox*>> numbers_;
    std::vector<std::pair<std::string, widgets::InputLine*>> fields_;
    widgets::CheckGroup* flags_ = nullptr;
    widgets::ProgressSubscription fields_subscription_;
    bool syncing_ = false;
    ui::Application::TimerId simulation_ = 0;
    bool running_ = false;
    bool weighted_ = false;
    bool frozen_ = false;
    std::int64_t tick_interval_ = 250'000'000;
    double increment_ = 1;
    int manual_phase_ = 0;
};
} // namespace ckv::progress_lab
