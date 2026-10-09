// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "progress_lab_app.hpp"
#include "../example_about.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"
#include <algorithm>
#include <cmath>

namespace ckv::progress_lab {
namespace {
class SettingsViewport : public widgets::ScrollViewport {
public:
    SettingsViewport() : widgets::ScrollViewport(ui::FocusPolicy::TabStop) {}
    void on_descendant_focused(ui::View& target) override { ensure_visible(target); }
};
class LabPane : public ui::View {
public:
    widgets::ScrollViewport* settings = nullptr;
    ui::View* preview = nullptr;
    widgets::ProgressView* display = nullptr;
    void on_resized() override {
        if (!settings || !preview) return;
        const int width = bounds().width, height = bounds().height;
        const int left = width >= 60 ? std::min(36, width / 2) : width;
        settings->set_bounds(Rect{0, 0, left, height});
        preview->set_bounds(Rect{left + 1, 0, std::max(0, width - left - 1), height});
        preview->set_visible(preview->bounds().width > 0);
        if (display) display->set_bounds(Rect{0, 6, preview->bounds().width, std::max(0, height - 6)});
    }
};
}

ProgressLabApp::ProgressLabApp(ui::Application& app)
    : app_(app), roles_(ui::intern_standard_roles(app.roles())), controller_(app, model_) {
    const auto quit = app.commands().standard().quit;
    app.commands().set_handler(quit, [&app] { app.request_quit(); });
    const auto run = app.commands().declare({.key = "progress.run", .title = "&Run / pause", .category = "Progress", .chord = "F5", .handler = [this] { set_running(!running_); }});
    const auto reset = app.commands().declare({.key = "progress.reset", .title = "&Reset", .category = "Progress", .chord = "F6", .handler = [this] { reset_tasks(); }});
    widgets::ApplicationShell shell(app, {.theme = ui::make_classic_theme(app.roles(), roles_),
        .menus = {{"&File", {widgets::MenuItem::command(widgets::CommandPresentation{run}),
                             widgets::MenuItem::command(widgets::CommandPresentation{reset}),
                             widgets::MenuItem::command(widgets::CommandPresentation{app.commands().standard().help, "&About..."}),
                             widgets::MenuItem::command(widgets::CommandPresentation{quit})}}},
        .status_items = {widgets::StatusLineItem{widgets::CommandPresentation{run}},
                         widgets::StatusLineItem{widgets::CommandPresentation{reset}},
                         widgets::StatusLineItem{widgets::CommandPresentation{quit}}}});
    desktop_ = &shell.desktop();
    widgets::install_about_help(app, *desktop_, roles_, "ckVision Progress Lab",
        examples::about_text("Configure meters, activity, task hierarchy and timing. Tab reaches settings; wheel scrolls the settings panel. Use the Run/pause and Reset commands. Narrow windows keep settings accessible; widen to compare the live preview."));
    auto window = std::make_unique<widgets::Window>("Progress Lab");
    const Size screen{app.root().bounds().width, app.root().bounds().height};
    window->set_bounds(Rect{1, 1, std::max(40, screen.width - 2), std::max(12, screen.height - 3)});
    window->set_min_size(Size{40, 12}); window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);
    auto pane = std::make_unique<LabPane>(); auto* layout = pane.get();
    auto* settings = pane->make<SettingsViewport>();
    settings->set_horizontal_scrollbar_policy(widgets::ScrollbarPolicy::Hidden);
    auto options = std::make_unique<ui::View>(); options->set_preferred_size(Size{35, 72}); auto* controls = options.get();
    settings->set_content(std::move(options));
    auto* preview = pane->make<ui::View>();
    preview->make<widgets::StaticText>("Live preview — passive progress")->set_bounds(Rect{0, 0, 45, 1});
    // ckvision-doc: progress-lab-display
    display_ = preview->make<widgets::ProgressView>(model_);
    controller_.bind(*display_);
    meter_ = preview->make<widgets::Progress>();
    meter_->set_bounds(Rect{0, 2, 24, 1});
    meter_->set_presentation(widgets::ProgressPresentation::Smooth);
    meter_->set_fraction(0.375);
    meter_->set_show_percentage(true);
    controller_.bind(*meter_);
    indicator_ = preview->make<widgets::ActivityIndicator>();
    indicator_->set_bounds(Rect{27, 2, 8, 1});
    controller_.bind(*indicator_);
    // ckvision-doc-end: progress-lab-display
    preview->make<widgets::StaticText>("Standalone meter / indicator")->set_bounds(Rect{0, 4, 40, 1});
    layout->settings = settings; layout->preview = preview; layout->display = display_;
    int y = 0;
    const auto choose = [&](std::string name, std::vector<std::string> items, std::size_t selected,
                            std::function<void(std::size_t)> callback) {
        auto* label = controls->make<widgets::Label>(name); label->set_bounds(Rect{0, y, 14, 1});
        auto* combo = controls->make<widgets::ComboBox>(); combo->set_bounds(Rect{15, y, 18, 1});
        combo->set_items(std::move(items)); combo->set_selected_index(selected); combo->on_select = std::move(callback);
        label->set_buddy(combo); selectors_.emplace_back(std::move(name), combo); y += 2;
        return combo;
    };
    choose("Meter", {"Solid", "Block", "Segmented", "Smooth"}, 3, [this](std::size_t i) {
        const auto value = static_cast<widgets::ProgressPresentation>(i); display_->set_meter_presentation(value); meter_->set_presentation(value);
    });
    choose("Activity", {"Sweep", "Bounce", "Pulse"}, 0, [this](std::size_t i) {
        const auto value = static_cast<widgets::ProgressActivityStyle>(i); display_->set_activity_style(value); meter_->set_activity_style(value);
    });
    choose("Spinner", {"Dots", "Braille", "ASCII"}, 1, [this](std::size_t i) {
        const auto value = static_cast<widgets::ActivityPresentation>(i); display_->set_spinner_presentation(value); indicator_->set_presentation(value);
    });
    choose("Glyphs", {"Unicode", "ASCII"}, 0, [this](std::size_t i) {
        const auto value = static_cast<widgets::ProgressGlyphs>(i); display_->set_glyphs(value); meter_->set_glyphs(value); indicator_->set_glyphs(value);
    });
    choose("Motion", {"Animated", "Static"}, 0, [this](std::size_t i) {
        const auto value = static_cast<widgets::ProgressMotion>(i); display_->set_motion(value); indicator_->set_motion(value); meter_->set_motion(value);
    });
    choose("Rows", {"Compact", "Detailed"}, 0, [this](std::size_t i) { display_->set_presentation(static_cast<widgets::ProgressViewPresentation>(i)); });
    choose("Unit scale", {"None", "Decimal", "Binary"}, 0, [this](std::size_t i) { display_->set_unit_scale(static_cast<widgets::ProgressUnitScale>(i)); });
    choose("Aggregation", {"Manual", "Weighted"}, 0, [this](std::size_t i) {
        weighted_ = i != 0; model_.set_aggregation(tasks_[0], weighted_ ? widgets::ProgressAggregation::WeightedChildren : widgets::ProgressAggregation::Manual);
    });
    choose("Task", {"Import", "Pictures", "Index", "Download"}, 1, [this](std::size_t i) { selected_ = tasks_[i]; sync_selected(); });
    choose("State", {"Queued/reset", "Running", "Paused", "Completed", "Failed", "Cancelled"}, 1, [this](std::size_t i) {
        model_.set_time(app_.clock().now_nanos());
        if (i == 0) model_.reset(selected_);
        else if (i == 1) model_.start(selected_);
        else if (i == 2) model_.pause(selected_);
        else if (i == 3) model_.complete(selected_);
        else if (i == 4) model_.fail(selected_);
        else model_.cancel(selected_);
        sync_selected();
    });
    auto* columns = controls->make<widgets::CheckGroup>(std::vector<std::string>{"Percent", "Counts", "Elapsed", "Rate", "ETA", "Spinner", "Retain done", "Unknown", "Hide task", "Freeze children", "Meter unknown", "Meter percent", "Disabled"});
    flags_ = columns;
    columns->set_bounds(Rect{0, y, 33, 14}); columns->set_checked(0, true); columns->set_checked(6, true); columns->set_checked(11, true);
    columns->on_changed = [this, columns](std::size_t index, bool checked) {
        if (syncing_) return;
        display_->set_columns({columns->checked(0), columns->checked(1), columns->checked(2), columns->checked(3), columns->checked(4), columns->checked(5)});
        if (index == 6) display_->set_retain_completed(checked);
        if (index == 7) {
            double total = 200;
            for (const auto& [name, spin] : numbers_) if (name == "Total") total = spin->value();
            model_.set_total(selected_, checked ? std::nullopt : std::optional<double>{total});
        }
        if (index == 8) model_.set_visible(selected_, !checked);
        if (index == 9) { frozen_ = checked; if (checked) model_.finalize_children(tasks_[0]); else reset_tasks(); }
        if (index == 10) meter_->set_indeterminate(checked);
        if (index == 11) meter_->set_show_percentage(checked);
        if (index == 12) { display_->set_enabled(!checked); meter_->set_enabled(!checked); indicator_->set_enabled(!checked); }
    }; y += 15;
    const auto numeric = [&](std::string title, int min, int max, int initial, std::function<void(int)> callback) {
        auto* label = controls->make<widgets::Label>(title); label->set_bounds(Rect{0, y, 14, 1});
        auto* spin = controls->make<widgets::SpinBox>(); spin->set_bounds(Rect{15, y, 18, 1}); spin->set_range(min, max); spin->set_value(initial); spin->on_change = [this, callback](int value) { if (!syncing_) { callback(value); sync_selected(); } };
        numbers_.emplace_back(std::move(title), spin); label->set_buddy(spin); y += 2;
    };
    numeric("Meter %", 0, 100, 38, [this](int value) { meter_->set_fraction(value / 100.0); });
    numeric("Completed", 0, 1000000, 74, [this](int value) { model_.set_time(app_.clock().now_nanos()); model_.set_completed(selected_, value); });
    numeric("Total", 0, 1000000, 200, [this](int value) { model_.set_total(selected_, value); });
    numeric("Weight", 1, 1000, 1, [this](int value) { model_.set_weight(selected_, value); });
    numeric("Refresh ms", 20, 2000, 100, [this](int value) { controller_.set_interval(value * 1'000'000LL); });
    numeric("Step size", 1, 1000, 1, [this](int value) { increment_ = value; });
    numeric("Step ms", 20, 5000, 250, [this](int value) { tick_interval_ = value * 1'000'000LL; });
    numeric("First row", 0, 100, 0, [this](int value) { display_->set_first_row(static_cast<std::size_t>(value)); });
    numeric("Max rows", 0, 100, 100, [this](int value) { display_->set_max_rows(static_cast<std::size_t>(value)); });
    auto* custom = controls->make<widgets::InputLine>(); custom->set_bounds(Rect{0, y, 23, 1}); custom->set_text("|,/,-,\\");
    auto* apply = controls->make<widgets::Button>("Frames"); apply->set_flat(true); apply->set_bounds(Rect{24, y, 9, 1});
    apply->on_press = [this, custom] {
        std::vector<std::string> frames; const auto& text = custom->text(); std::size_t pos = 0;
        while (pos <= text.size()) { const auto end = text.find(',', pos); frames.push_back(text.substr(pos, end == std::string::npos ? end : end - pos)); if (end == std::string::npos) break; pos = end + 1; }
        custom->set_valid(indicator_->set_frames(std::move(frames)));
    }; y += 2;
    const auto edit = [&](std::string caption, std::string value, std::function<void(const std::string&)> callback) {
        auto* label = controls->make<widgets::Label>(caption); label->set_bounds(Rect{0, y, 14, 1});
        auto* input = controls->make<widgets::InputLine>(); input->set_bounds(Rect{15, y, 18, 1}); input->set_text(std::move(value));
        input->on_edited = [this, input, callback] { if (!syncing_) callback(input->text()); }; fields_.emplace_back(std::move(caption), input); label->set_buddy(input); y += 2;
    };
    edit("Task title", "Pictures", [this](const auto& text) { model_.set_title(selected_, text); });
    edit("Task unit", "files", [this](const auto& text) { model_.set_unit(selected_, text); });
    edit("Meter label", "", [this](const auto& text) { meter_->set_label(text); });
    auto* message = controls->make<widgets::InputLine>(); message->set_bounds(Rect{0, y, 33, 1}); message->set_text("holiday-2026.jpg");
    fields_.emplace_back("Message", message);
    message->on_edited = [this, message] { model_.set_message(selected_, message->text()); }; y += 2;
    const auto button = [&](std::string caption, int x, std::function<void()> callback) {
        auto* action = controls->make<widgets::Button>(std::move(caption)); action->set_flat(true); action->set_bounds(Rect{x, y, 10, 1}); action->on_press = std::move(callback);
    };
    button("Run/pause", 0, [this] { set_running(!running_); }); button("Reset", 12, [this] { reset_tasks(); });
    button("Step", 24, [this] { model_.set_time(app_.clock().now_nanos()); model_.advance(selected_, increment_); meter_->set_pulse(++manual_phase_); indicator_->set_phase(manual_phase_); });
    controls->set_preferred_size(Size{35, y + 2});
    window->set_content(std::move(pane)); window_ = desktop_->add<widgets::Window>(std::move(window));
    fields_subscription_ = model_.subscribe([this](bool phase) { if (!phase) sync_selected(); });
    reset_tasks(); app.set_focus(selector("Meter"));
}
ProgressLabApp::~ProgressLabApp() {
    fields_subscription_.reset();
    if (simulation_ != 0) app_.cancel_timer(simulation_);
    if (window_) desktop_->remove_window(window_);
}
widgets::ComboBox* ProgressLabApp::selector(std::string_view name) const noexcept {
    for (const auto& item : selectors_) if (item.first == name) return item.second;
    return nullptr;
}
void ProgressLabApp::reset_tasks() {
    set_running(false); model_.begin_update();
    for (const auto id : tasks_) model_.remove(id);
    tasks_.clear(); model_.set_time(app_.clock().now_nanos());
    tasks_.push_back(model_.add_task({.title = "Import", .total = 5, .unit = "stages", .aggregation = weighted_ ? widgets::ProgressAggregation::WeightedChildren : widgets::ProgressAggregation::Manual}));
    tasks_.push_back(model_.add_task({.title = "Pictures", .parent = tasks_[0], .total = 200, .unit = "files", .weight = 3}));
    tasks_.push_back(model_.add_task({.title = "Index", .parent = tasks_[0], .weight = 1}));
    tasks_.push_back(model_.add_task({.title = "Download", .total = 1000000, .unit = "B"}));
    model_.set_completed(tasks_[0], 3); model_.set_completed(tasks_[1], 74); model_.set_completed(tasks_[3], 520000);
    for (const auto id : tasks_) model_.start(id);
    model_.set_message(tasks_[1], "holiday-2026.jpg"); model_.set_message(tasks_[2], "Discovering files…");
    if (frozen_) model_.finalize_children(tasks_[0]);
    selected_ = tasks_[1];
    selector("Task")->set_selected_index(1); selector("State")->set_selected_index(1);
    indicator_->set_state(widgets::ProgressTaskState::Running);
    model_.end_update(); sync_selected();
}
void ProgressLabApp::sync_selected() {
    const auto* task = model_.find(selected_);
    if (!task || syncing_ || !flags_) return;
    syncing_ = true;
    selector("State")->set_selected_index(static_cast<std::size_t>(task->state));
    indicator_->set_state(task->state);
    flags_->set_checked(7, !task->options.total); flags_->set_checked(8, !task->visible);
    for (const auto& [name, input] : fields_) {
        if (name == "Task title" && input->text() != task->options.title) input->set_text(task->options.title);
        if (name == "Task unit" && input->text() != task->options.unit) input->set_text(task->options.unit);
        if (name == "Message" && input->text() != task->message) input->set_text(task->message);
    }
    for (const auto& [name, spin] : numbers_) {
        std::optional<double> value;
        if (name == "Completed") value = task->completed;
        if (name == "Total") value = task->options.total;
        if (name == "Weight") value = task->options.weight;
        if (value) {
            const int shown = static_cast<int>(std::clamp(*value, 0.0, 1'000'000.0));
            if (spin->value() != shown) spin->set_value(shown);
        }
    }
    syncing_ = false;
}
void ProgressLabApp::set_running(bool running) {
    model_.set_time(app_.clock().now_nanos());
    const auto* task = model_.find(selected_);
    if (running && task && task->state == widgets::ProgressTaskState::Paused) model_.start(selected_);
    if (!running && running_ && task && task->state == widgets::ProgressTaskState::Running) model_.pause(selected_);
    task = model_.find(selected_);
    running_ = running && task && task->state == widgets::ProgressTaskState::Running;
    if (task) indicator_->set_state(task->state);
    if (simulation_ != 0) { app_.cancel_timer(simulation_); simulation_ = 0; }
    if (running_) simulation_ = app_.start_timer(tick_interval_, false, [this] { tick(); });
}
void ProgressLabApp::tick() {
    simulation_ = 0; if (!running_) return;
    model_.set_time(app_.clock().now_nanos());
    const auto* task = model_.find(selected_);
    if (task && task->state == widgets::ProgressTaskState::Running) {
        model_.advance(selected_, increment_);
        task = model_.find(selected_);
        if (task->options.total && task->completed >= *task->options.total) { model_.complete(selected_); set_running(false); return; }
    }
    meter_->set_pulse(++manual_phase_);
    simulation_ = app_.start_timer(tick_interval_, false, [this] { tick(); });
}
} // namespace ckv::progress_lab
