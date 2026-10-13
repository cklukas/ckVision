// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include "cvision/ui/command.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/log_view.hpp"
#include "cvision/widgets/log_search.hpp"

namespace ckv::widgets {

// App-supplied source/transport diagnostics, separate from provider retention.
struct LogPanelSourceStatus {
    // Lifecycle/mode, known queued bytes/records lost, native code and continuity.
    LogSourceState state = LogSourceState::Pending;
    LogWatchMode watch = LogWatchMode::Unavailable;
    std::uint64_t backlog_bytes = 0, dropped = 0;
    std::uint32_t native_error = 0;
    bool uncertain = false;
    // dropped is exact known structured-record loss; byte streams may have
    // additional unknown record loss. Never infer records from arbitrary chunks.
    std::uint64_t dropped_bytes = 0, dropped_batches = 0;
    std::int64_t lag_nanos = 0;
    bool dropped_records_unknown = false;
    friend bool operator==(const LogPanelSourceStatus&, const LogPanelSourceStatus&) = default;
};
// Presentation, clipboard, source-lifecycle and independent Find commands.
enum class LogPanelAction { Pause, Resume, Copy, ClearView, ClearRetention, Stop, Retry, Find, FindNext, FindPrevious };

// A pure composition, borrowing the same provider as its LogView. Query changes
// are delivered to the application's filter controller; it publishes a coherent
// provider revision then calls refresh. Source commands are explicit callbacks.
// At narrow sizes the search gets its full row and End still resumes the view.
class LogPanel : public ui::View {
public:
    // Borrow a provider, or bind a filter include control automatically; reserve
    // the view workspace for a positive bounded viewport row capacity.
    explicit LogPanel(const LogRecordProvider& provider, std::size_t max_viewport_rows = 4096);
    explicit LogPanel(LogFilter& filter, std::size_t max_viewport_rows = 4096);
    LogView& view() noexcept { return *view_; }
    const LogView& view() const noexcept { return *view_; }
    const LogRecordProvider& provider() const noexcept { return provider_; }
    SearchBox& filter_box() noexcept { return *search_; }
    // Optional independent Find field. The bound search/provider must outlive
    // the panel and use this panel's provider. The application drives step().
    void set_search(LogSearch* search);
    // Borrow Find control, show/hide it, or request the bound search direction.
    SearchBox& find_box() noexcept { return *find_; }
    void show_find(bool visible);
    bool find(LogSearchDirection direction);
    // Publish query/source diagnostics and refresh coherent provider/view state.
    void set_query_pending(bool pending) { query_pending_ = pending; status_dirty_ = true; invalidate(); }
    void refresh();
    void set_source_status(LogPanelSourceStatus status);
    const LogPanelSourceStatus& source_status() const noexcept { return source_; }
    std::string_view status_text() const noexcept { return {status_.data(), status_size_}; }
    bool invoke(LogPanelAction action);
    // Registry-compatible descriptors for menus, palette and status legends.
    // Keys/context belong to the caller; callbacks become inert on destruction.
    std::vector<ui::CommandDescriptor> commands(std::string key_prefix, std::string context_key);
    // App-owned query/retention/source actions; callbacks run on the UI thread.
    std::function<void(const std::string&)> on_query_changed;
    std::function<void()> on_clear_retention, on_stop, on_retry;
    void on_resized() override;
    void on_attached() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
private:
    void update_status();
    const LogRecordProvider& provider_;
    LogView* view_ = nullptr;
    SearchBox* search_ = nullptr;
    SearchBox* find_ = nullptr;
    Button *next_ = nullptr, *previous_ = nullptr;
    Button* resume_ = nullptr;
    LogFilter* filter_ = nullptr;
    LogSearch* finder_ = nullptr;
    LogPanelSourceStatus source_;
    std::array<char, 512> status_{};
    std::size_t status_size_ = 0;
    bool status_dirty_ = true;
    bool find_visible_ = false, query_pending_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
