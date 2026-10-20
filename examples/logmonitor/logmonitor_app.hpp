// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "export_writer.hpp"
#include "cvision/widgets/log_controller.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::logmonitor {
enum class SampleMode { Steady, Burst, Overload, Rotation, Truncation };
enum class ExportScope { Selection, FilteredRetained };
// App-owned OS schema/discovery seam; native adapters arrive through WP82.
// One bounded publication/parse quantum, explicit time, no view ownership.
class StreamFeed {
public:
    virtual ~StreamFeed() = default;
    virtual LogMailbox& publications() noexcept = 0;
    virtual void step(std::int64_t now) = 0;
    virtual std::optional<std::int64_t> next_deadline() const noexcept = 0;
    virtual widgets::LogPanelSourceStatus diagnostic() const = 0;
    virtual std::string_view explanation() const noexcept { return {}; }
    virtual void stop(std::int64_t now) noexcept = 0;
};
struct NativeSourceDescriptor {
    std::string key, title, explanation;
    bool available = false;
    std::function<std::unique_ptr<StreamFeed>(LogSourceId, std::shared_ptr<const LogWake>)> open;
};
class LogMonitorApp {
public:
    LogMonitorApp(ui::Application& app, LogFileService& files, FileSystem& filesystem,
                  ExportWriter& exports, std::shared_ptr<LogActivity> activity,
                  std::vector<NativeSourceDescriptor> catalog = {});
    ~LogMonitorApp();
    bool open_file(std::string path);
    bool open_sample(SampleMode mode);
    bool open_native(std::size_t descriptor);
    bool close_source(std::size_t index);
    void select_source(std::size_t index);
    void show_sources();
    void stop_active();
    void retry_active();
    void set_filter(std::string include, std::string exclude = {});
    void set_severity(std::uint16_t mask);
    void set_source_filter(std::optional<LogSourceId> source);
    void set_wrap(bool wrap);
    bool begin_export(std::string path, ExportScope scope);
    std::string detail_text() const;
    void show_details();
    // Drive from the outer loop between Application steps; explicit time.
    void step(std::int64_t now);
    std::optional<std::int64_t> next_deadline() const noexcept;
    std::size_t source_count() const noexcept { return sources_.size(); }
    std::optional<std::size_t> active_source() const noexcept;
    widgets::LogPanel* panel(std::size_t index) const noexcept;
    const widgets::LogModel* model(std::size_t index) const noexcept;
    widgets::InputLine& path_field() noexcept { return *path_; }
    widgets::ComboBox& sample_choice() noexcept { return *sample_; }
    widgets::TabControl& tabs() noexcept { return *tabs_; }
    std::string_view notice() const noexcept { return notice_text_; }
    bool exporting() const noexcept;
    const std::vector<NativeSourceDescriptor>& catalog() const noexcept { return catalog_; }
private:
    struct Source;
    struct Snapshot;
    struct OpenIntent;
    bool install(std::unique_ptr<Source> source);
    bool start_source(Source& source, std::int64_t now);
    void stop(Source& source, std::int64_t now);
    void synchronize_queries();
    void synchronize_controls();
    void schedule();
    void export_step();
    void export_prompt(ExportScope scope);
    void set_notice(std::string text);
    Source* active() const noexcept;
    std::optional<LogRecordId> inspected_id() const;
    ui::Application& app_;
    LogFileService& files_;
    FileSystem& filesystem_;
    ExportWriter& exports_;
    std::shared_ptr<LogActivity> activity_;
    ui::StandardRoles roles_;
    std::unique_ptr<widgets::ApplicationShell> shell_;
    widgets::Window* window_ = nullptr;
    widgets::TabControl* tabs_ = nullptr;
    widgets::InputLine *path_ = nullptr, *exclude_ = nullptr;
    widgets::ComboBox *sample_ = nullptr, *severity_ = nullptr, *source_choice_ = nullptr;
    widgets::StaticText* notice_view_ = nullptr;
    std::vector<NativeSourceDescriptor> catalog_;
    std::vector<std::unique_ptr<Source>> sources_;
    widgets::LogController controller_;
    widgets::LogFilterQuery shared_query_;
    std::unique_ptr<Snapshot> snapshot_;
    std::unique_ptr<OpenIntent> opening_;
    std::vector<ui::CommandId> commands_;
    LogSourceId next_source_ = 1;
    std::uint64_t next_export_ = 1;
    std::int64_t now_ = 0;
    std::size_t producer_cursor_ = 0;
    std::string notice_text_;
    bool syncing_ = false, wrap_ = false, export_write_pending_ = false;
};
}  // namespace ckv::logmonitor
