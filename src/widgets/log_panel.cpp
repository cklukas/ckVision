// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_panel.hpp"

#include <algorithm>
#include <charconv>

#include "cvision/scene/painter.hpp"

namespace ckv::widgets {
namespace {
std::string_view source_name(LogSourceState state) noexcept {
    switch (state) {
        case LogSourceState::Pending: return "Opening";
        case LogSourceState::Following: return "Live";
        case LogSourceState::Missing: return "Missing";
        case LogSourceState::Denied: return "Access denied";
        case LogSourceState::Failed: return "Source failed";
        case LogSourceState::Ended: return "Ended";
        case LogSourceState::Cancelled: return "Stopped";
    }
    return "Unknown";
}
}

LogPanel::LogPanel(const LogRecordProvider& provider, std::size_t max_viewport_rows)
    : provider_(provider) {
    search_ = make<SearchBox>();
    resume_ = make<Button>("&Resume");
    resume_->set_presentation(ButtonPresentation::Flat);
    view_ = make<LogView>(provider, max_viewport_rows);
    search_->on_change = [this](const std::string& query) {
        if (on_query_changed) on_query_changed(query);
    };
    resume_->on_press = [this] { view_->resume(); };
    view_->on_state_changed = [this](const LogViewState& state) {
        status_dirty_ = true;
        resume_->set_enabled(!state.following);
        invalidate();
    };
    resume_->set_enabled(false);
    update_status();
}
void LogPanel::on_attached() {
    if (context().roles) role_ = context().roles->find("ckv.label.text");
}
void LogPanel::on_resized() {
    const int width = std::max(0, bounds().width), height = std::max(0, bounds().height);
    const bool button = width >= 20 && height >= 3;
    const int query_height = height >= 3 ? 1 : 0;
    const int status_height = height >= 2 ? 1 : 0;
    search_->set_visible(query_height != 0);
    resume_->set_visible(button);
    search_->set_bounds({0, 0, std::max(0, width - (button ? 8 : 0)), query_height});
    resume_->set_bounds({std::max(0, width - 8), 0, button ? 8 : 0, query_height});
    view_->set_bounds({0, query_height, width, std::max(0, height - query_height - status_height)});
    invalidate();
}
void LogPanel::set_source_status(LogPanelSourceStatus status) {
    if (source_ == status) return;
    source_ = status;
    status_dirty_ = true;
    update_status();
    invalidate();
}
void LogPanel::refresh() {
    view_->refresh();
    status_dirty_ = true;
    update_status();
    invalidate();
}
void LogPanel::update_status() {
    if (!status_dirty_) return;
    status_dirty_ = false;
    status_size_ = 0;
    const auto append = [this](std::string_view value) {
        const auto count = std::min(value.size(), status_.size() - status_size_);
        std::copy_n(value.data(), count, status_.data() + status_size_);
        status_size_ += count;
    };
    const auto number = [&append](std::uint64_t value) {
        std::array<char, 24> bytes{};
        const auto result = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value);
        append({bytes.data(), static_cast<std::size_t>(result.ptr - bytes.data())});
    };
    const auto& state = view_->state();
    append(source_name(source_.state));
    if (state.anchor_unavailable) append(" | Entry unavailable");
    if (state.selection_unavailable) append(" | Selection unavailable");
    if (state.viewport_exceeded || state.record_exceeded) append(" | Capacity exceeded");
    if (source_.uncertain) append(" | continuity uncertain");
    if (source_.native_error) { append(" | error "); number(source_.native_error); }
    if (source_.dropped) { append(" | dropped "); number(source_.dropped); }
    if (source_.backlog_bytes) { append(" | backlog "); number(source_.backlog_bytes); append(" B"); }
    if (state.following) append(" | Follow");
    else { append(" | Paused +"); number(state.unread); append(" | End: resume"); }
    append(" | records "); number(provider_.size());
    if (source_.query_pending) append(" (filter pending)");
    if (state.evicted) { append(" | evicted "); number(state.evicted); }
    if (source_.watch == LogWatchMode::Native) append(" | native watch");
    else if (source_.watch == LogWatchMode::Polling) append(" | polling");
}
bool LogPanel::invoke(LogPanelAction action) {
    switch (action) {
        case LogPanelAction::Pause: view_->pause(); return true;
        case LogPanelAction::Resume: view_->resume(); return true;
        case LogPanelAction::Copy: return view_->copy_selection();
        case LogPanelAction::ClearView: view_->clear_view(); return true;
        case LogPanelAction::ClearRetention: if (on_clear_retention) { on_clear_retention(); return true; } return false;
        case LogPanelAction::Stop: if (on_stop) { on_stop(); return true; } return false;
        case LogPanelAction::Retry: if (on_retry) { on_retry(); return true; } return false;
    }
    return false;
}
std::vector<ui::CommandDescriptor> LogPanel::commands(std::string prefix, std::string context_key) {
    const auto token = lifetime_token();
    set_command_context(context_key);
    std::vector<ui::CommandDescriptor> result;
    const auto add = [&](std::string_view suffix, std::string_view title, LogPanelAction action) {
        ui::CommandDescriptor descriptor;
        descriptor.key = prefix + "." + std::string(suffix);
        descriptor.title = title;
        descriptor.category = "Logs";
        descriptor.scope.contexts = {context_key};
        descriptor.handler = [this, token, action] { if (!token.expired()) invoke(action); };
        result.push_back(std::move(descriptor));
    };
    add("pause", "Pause log view", LogPanelAction::Pause);
    add("resume", "Resume live tail", LogPanelAction::Resume);
    add("copy", "Copy log selection", LogPanelAction::Copy);
    add("clear_view", "Clear log view", LogPanelAction::ClearView);
    add("clear_retention", "Clear retained logs", LogPanelAction::ClearRetention);
    add("stop", "Stop log source", LogPanelAction::Stop);
    add("retry", "Retry log source", LogPanelAction::Retry);
    return result;
}
void LogPanel::draw(scene::Painter& painter) {
    view_->refresh();
    update_status();
    const auto style = context().theme ? context().theme->resolve(role_) : Style{};
    painter.fill({0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    if (bounds().height >= 2) painter.draw_text({0, bounds().height - 1}, status_text(), style);
}

}  // namespace ckv::widgets
