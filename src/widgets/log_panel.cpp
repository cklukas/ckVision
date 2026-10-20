// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_panel.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

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
    search_->set_prompt("Filter ");
    resume_ = make<Button>("&Resume");
    resume_->set_presentation(ButtonPresentation::Flat);
    view_ = make<LogView>(provider, max_viewport_rows);
    find_ = make<SearchBox>();
    find_->set_prompt("Find ");
    next_ = make<Button>("&Next"); previous_ = make<Button>("&Prev");
    next_->set_presentation(ButtonPresentation::Flat); previous_->set_presentation(ButtonPresentation::Flat);
    find_->set_visible(false); next_->set_visible(false); previous_->set_visible(false);
    find_->on_change = [this](const std::string& query) {
        if (finder_) finder_->set_query(query, finder_->case_sensitive());
        status_dirty_ = true; view_->invalidate(); invalidate();
    };
    find_->on_clear = [this] { show_find(false); };
    next_->on_press = [this] { find(LogSearchDirection::Next); };
    previous_->on_press = [this] { find(LogSearchDirection::Previous); };
    search_->on_change = [this](const std::string& query) {
        if (filter_) filter_->set_include(query);
        status_dirty_ = true; invalidate();
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
LogPanel::LogPanel(LogFilter& filter, std::size_t max_viewport_rows)
    : LogPanel(static_cast<const LogRecordProvider&>(filter), max_viewport_rows) {
    filter_ = &filter;
    search_->set_query(std::string(filter.requested_query().include));
    update_status();
}
void LogPanel::set_search(LogSearch* search) {
    if (search && &search->provider() != &provider_) throw std::invalid_argument("log search provider mismatch");
    finder_ = search;
    view_->set_highlighter(search);
    find_->set_query(search ? std::string(search->query()) : std::string{});
    if (!search) show_find(false);
    status_dirty_ = true; invalidate();
}
void LogPanel::show_find(bool visible) {
    const bool restore_focus = context().app && context().app->focused() == &find_->field();
    find_visible_ = visible && finder_;
    on_resized();
    if (find_visible_ && context().app && find_->visible()) context().app->set_focus(&find_->field());
    else if (!find_visible_ && restore_focus) context().app->set_focus(view_);
    status_dirty_ = true; invalidate();
}
bool LogPanel::find(LogSearchDirection direction) {
    if (!finder_) return false;
    std::optional<LogSearchOrigin> origin;
    if (const auto selected = view_->selection()) {
        const auto point = direction == LogSearchDirection::Next ? selected->cursor : selected->anchor;
        origin = LogSearchOrigin{point.id, point.byte};
    } else if (const auto top = view_->top()) origin = LogSearchOrigin{top->id, top->byte};
    const bool requested = finder_->request(direction, origin);
    status_dirty_ = true; invalidate();
    return requested;
}
void LogPanel::on_attached() {
    if (context().roles) role_ = context().roles->find("ckv.label.text");
}
void LogPanel::on_resized() {
    const int width = std::max(0, bounds().width), height = std::max(0, bounds().height);
    const bool button = width >= 20 && height >= 3;
    const int query_height = height >= 3 ? 1 : 0;
    const int status_height = height >= 2 ? 1 : 0;
    const int find_height = find_visible_ && height >= 4 ? 1 : 0;
    find_->set_visible(find_height != 0);
    next_->set_visible(find_height && width >= 28);
    previous_->set_visible(find_height && width >= 28);
    const int find_buttons = width >= 28 ? 12 : 0;
    find_->set_bounds({0, query_height, std::max(0, width - find_buttons), find_height});
    previous_->set_bounds({std::max(0, width - 12), query_height, 6, find_height});
    next_->set_bounds({std::max(0, width - 6), query_height, 6, find_height});
    search_->set_visible(query_height != 0);
    resume_->set_visible(button);
    search_->set_bounds({0, 0, std::max(0, width - (button ? 8 : 0)), query_height});
    resume_->set_bounds({std::max(0, width - 8), 0, button ? 8 : 0, query_height});
    view_->set_bounds({0, query_height + find_height, width, std::max(0, height - query_height - find_height - status_height)});
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
    if (finder_) {
        const auto match = finder_->take_result();
        if (match) view_->reveal(match->id, match->range);
    }
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
    if (filter_ && filter_->status().error != LogQueryError::None) {
        append(" | Filter: "); append(log_query_error_name(filter_->status().error));
    }
    if (finder_) {
        const auto& search = finder_->status();
        if (search.error != LogQueryError::None) { append(" | Find: "); append(log_query_error_name(search.error)); }
        else if (search.phase == LogSearchPhase::Pending) append(" | Find pending");
        else if (search.phase == LogSearchPhase::NotFound) append(search.incomplete ? " | No match (history changed)" : " | No match");
        else if (search.phase == LogSearchPhase::Stale) append(" | Find result stale");
    }
    if (filter_ && filter_->status().query_pending) {
        append(" | query "); number(filter_->status().published_revision);
        append(" rebuilding "); number(filter_->status().requested_revision);
    } else if (filter_ && filter_->status().live_pending) append(" | filter catching up");
    if (state.anchor_unavailable) append(" | Entry unavailable");
    if (state.selection_unavailable) append(" | Selection unavailable");
    if (state.viewport_exceeded || state.record_exceeded) append(" | Capacity exceeded");
    if (source_.uncertain) append(" | continuity uncertain");
    if (source_.native_error) { append(" | error "); number(source_.native_error); }
    if (source_.dropped) { append(" | dropped "); number(source_.dropped); append(" records"); }
    if (source_.dropped_bytes) { append(" | loss "); number(source_.dropped_bytes); append(" B"); }
    if (source_.dropped_records_unknown) append(" (record count unknown)");
    if (source_.lag_nanos >= 1'000'000) { append(" | queued lag "); number(static_cast<std::uint64_t>(source_.lag_nanos / 1'000'000)); append(" ms"); }
    if (source_.backlog_bytes) { append(" | backlog "); number(source_.backlog_bytes); append(" B"); }
    if (state.following) append(" | Follow");
    else { append(" | Paused +"); number(state.unread); append(" | End: resume"); }
    if (filter_) {
        append(" | retained "); number(provider_.retained_size());
        append(" matches "); number(provider_.size());
    } else { append(" | records "); number(provider_.size()); }
    if (query_pending_) append(" (filter pending)");
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
        case LogPanelAction::Find: if (!finder_) return false; show_find(true); return true;
        case LogPanelAction::FindNext: return find(LogSearchDirection::Next);
        case LogPanelAction::FindPrevious: return find(LogSearchDirection::Previous);
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
    if (finder_) {
        add("find", "Find in retained matching logs", LogPanelAction::Find);
        add("find_next", "Find next log occurrence", LogPanelAction::FindNext);
        add("find_previous", "Find previous log occurrence", LogPanelAction::FindPrevious);
    }
    return result;
}
bool LogPanel::on_key(const KeyEvent& event) {
    if (has_modifier(event.chord.modifiers, Modifier::Alt) || has_modifier(event.chord.modifiers, Modifier::Super)) return false;
    if (event.chord.key == Key::Char && has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        (event.chord.text == "f" || event.chord.text == "F")) return invoke(LogPanelAction::Find);
    if (event.chord.key == Key::F3 && !has_modifier(event.chord.modifiers, Modifier::Ctrl))
        return find(has_modifier(event.chord.modifiers, Modifier::Shift) ? LogSearchDirection::Previous : LogSearchDirection::Next);
    if (event.chord.key == Key::Escape && find_visible_) { show_find(false); return true; }
    return false;
}
void LogPanel::draw(scene::Painter& painter) {
    view_->refresh();
    update_status();
    const auto style = context().theme ? context().theme->resolve(role_) : Style{};
    painter.fill({0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    if (bounds().height >= 2) painter.draw_text({0, bounds().height - 1}, status_text(), style);
}

}  // namespace ckv::widgets
