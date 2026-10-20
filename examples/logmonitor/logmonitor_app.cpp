// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "logmonitor_app.hpp"
#include "sample_file.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/core/text.hpp"
#include <algorithm>
#include <charconv>
#include <limits>

namespace ckv::logmonitor {
namespace {
using namespace widgets;
constexpr std::size_t source_limit = 8, export_limit = 2 * 1024 * 1024;
constexpr std::int64_t sample_period = 100'000'000, yield_period = 1'000'000;
constexpr std::array<std::string_view, 5> sample_names{"Steady", "Burst", "Overload", "Rotation", "Truncation"};
constexpr std::array<std::string_view, 8> severity_names{"Unknown", "Trace", "Debug", "Info", "Notice", "Warning", "Error", "Critical"};
std::int64_t after(std::int64_t now, std::int64_t delay) { return now > std::numeric_limits<std::int64_t>::max() - delay ? std::numeric_limits<std::int64_t>::max() : now + delay; }
LogLimits demo_limits() { LogLimits limits; limits.records = 2048; limits.record_bytes = 8192; limits.retained_bytes = 1024 * 1024; return limits; }
std::string id_text(LogRecordId id) { return std::to_string(id.source) + "/" + std::to_string(id.generation) + "/" + std::to_string(id.sequence); }
std::string safe_raw(std::string_view raw) {
    std::string output; output.reserve(raw.size()); constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char byte : raw) {
        if (byte < 32 || byte == 127 || byte >= 128) {
            output += "\\x"; output += hex[byte >> 4]; output += hex[byte & 15];
        } else output += static_cast<char>(byte);
    }
    return output;
}
class MonitorPane final : public ui::View {
public:
    std::vector<std::pair<ui::View*, int>> controls;
    TabControl* tabs = nullptr; StaticText* notice = nullptr;
    void on_resized() override {
        const int width = std::max(1, bounds().width), height = bounds().height;
        int x = 0, y = 0;
        for (std::size_t index = 0; index < controls.size(); ++index) {
            auto [control, wanted] = controls[index];
            const bool visible = height >= 13 || index < 3;
            control->set_visible(visible); if (!visible) continue;
            const int size = std::min(wanted, width);
            if (x && x + size > width) { x = 0; y += 2; }
            control->set_bounds({x, y, size, 1}); x += size + 1;
        }
        if (tabs) tabs->set_bounds({0, y + 2, width, std::max(0, height - y - 3)});
        if (notice) notice->set_bounds({0, std::max(0, height - 1), width, 1});
    }
};
class PickerPane final : public ui::View {
public:
    InputLine* path = nullptr; Button* open = nullptr; ComboBox* mode = nullptr; Button* sample = nullptr;
    std::vector<ui::View*> native;
    void on_resized() override {
        const int width = std::max(1, bounds().width);
        if (path) path->set_bounds({0, 2, std::max(1, width - 10), 1});
        if (open) open->set_bounds({std::max(0, width - 9), 2, std::min(9, width), 1});
        if (mode) mode->set_bounds({0, 5, std::min(22, width), 1});
        if (sample) sample->set_bounds({std::min(23, std::max(0, width - 13)), 5, std::min(13, width), 1});
        int row = 8; for (auto* view : native) { view->set_bounds({0, row, width, 1}); row += 2; }
    }
};
}
struct LogMonitorApp::Source {
    enum class Kind { File, Sample, External } kind = Kind::Sample;
    LogSourceId id;
    std::string name, path;
    SampleMode mode = SampleMode::Steady;
    std::size_t native_descriptor = 0;
    LogModel model{demo_limits()}; LogFilter filter{model}; LogSearch search{filter};
    std::unique_ptr<LogMailbox> queue;
    std::unique_ptr<SampleFile> sample_file;
    std::unique_ptr<StreamFeed> external;
    std::unique_ptr<LogFollower> follower;
    std::unique_ptr<LogStreamReceiver> receiver;
    LogPanel* panel = nullptr;
    LogLaneId lane = 0;
    std::uint64_t counter = 0, epoch = 1;
    std::int64_t next_event = 0, restart_at = 0;
    bool running = true, restarting = false, frozen = false;
    explicit Source(LogSourceId value) : id(value) {}
    LogLane bindings() { return {&model, &filter, &search, panel, follower.get(), receiver.get()}; }
};
struct LogMonitorApp::OpenIntent { Source::Kind kind; std::string path; SampleMode mode = SampleMode::Steady; std::size_t native = 0; };
struct LogMonitorApp::Snapshot {
    LogSourceId source = 0; ExportScope scope = ExportScope::FilteredRetained;
    std::size_t position = 0, end = 0, copied = 0;
    std::uint64_t query = 0; bool frozen = false, ready = false;
    ExportJob job;
};
LogMonitorApp::LogMonitorApp(ui::Application& app, LogFileService& files, FileSystem& filesystem,
                           ExportWriter& exports, std::shared_ptr<LogActivity> activity,
                           std::vector<NativeSourceDescriptor> catalog)
    : app_(app), files_(files), filesystem_(filesystem), exports_(exports), activity_(std::move(activity)),
      roles_(ui::intern_standard_roles(app.roles())), catalog_(std::move(catalog)),
      controller_({source_limit, 1, yield_period, 33'333'334}, activity_.get()) {
    app_.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded);
    sources_.reserve(source_limit);
    const auto command = [&](ui::CommandDescriptor descriptor) {
        descriptor.category = "Log monitor";
        auto id = app_.commands().declare(std::move(descriptor));
        commands_.push_back(id); return id;
    };
    const auto sources = command({.key = "logmonitor.sources", .title = "&Sources", .chord = "Ctrl+o", .handler = [this] { show_sources(); }});
    const auto pause = command({.key = "logmonitor.pause", .title = "&Pause / resume", .chord = "F5", .handler = [this] { if (auto* source = active()) { if (source->panel->view().state().following) source->panel->invoke(LogPanelAction::Pause); else source->panel->invoke(LogPanelAction::Resume); } }});
    const auto details = command({.key = "logmonitor.details", .title = "&Details", .chord = "F4", .handler = [this] { show_details(); }});
    const auto copy = command({.key = "logmonitor.copy", .title = "&Copy selection", .chord = "Ctrl+c", .handler = [this] { if (auto* source = active()) source->panel->invoke(LogPanelAction::Copy); }});
    const auto stop_cmd = command({.key = "logmonitor.stop", .title = "S&top source", .chord = "F6", .handler = [this] { stop_active(); }});
    const auto retry = command({.key = "logmonitor.retry", .title = "&Retry source", .chord = "F7", .handler = [this] { retry_active(); }});
    const auto export_cmd = command({.key = "logmonitor.export", .title = "&Export filtered retained...", .chord = "Ctrl+e", .handler = [this] { export_prompt(ExportScope::FilteredRetained); }});
    const auto export_selection = command({.key = "logmonitor.export-selection", .title = "Export &selected records...", .chord = "", .handler = [this] { export_prompt(ExportScope::Selection); }});
    const auto close = command({.key = "logmonitor.close", .title = "&Close source", .chord = "Ctrl+w", .handler = [this] { if (auto index = active_source()) close_source(*index); }});
    const auto wrap_cmd = command({.key = "logmonitor.wrap", .title = "&Wrap lines", .chord = "", .handler = [this] { set_wrap(!wrap_); }});
    const auto quit = app.commands().standard().quit; app.commands().set_handler(quit, [&app] { app.request_quit(); });
    std::vector<MenuItem> items;
    for (auto id : {sources, pause, details, copy, stop_cmd, retry, export_cmd, export_selection, close, wrap_cmd, quit}) items.push_back(MenuItem::command(CommandPresentation{id}));
    shell_ = std::make_unique<ApplicationShell>(app_, ApplicationShellOptions{.theme = ui::make_classic_theme(app.roles(), roles_),
        .menus = {{"&Logs", std::move(items)}}, .status_items = {StatusLineItem{CommandPresentation{sources}}, StatusLineItem{CommandPresentation{pause}}, StatusLineItem{CommandPresentation{details}}, StatusLineItem{CommandPresentation{quit}}}});
    auto window = std::make_unique<Window>("ckVision Log Monitor"); window_ = window.get();
    const auto area = shell_->desktop().content_area();
    window->set_bounds(area); window->set_min_size({20, 8}); window->set_grow_policy(DesktopGrowPolicy::AnchorEdges);
    window->set_minimizable(false); window->close_request = [&app] { app.request_quit(); return false; };
    auto pane = std::make_unique<MonitorPane>(); auto* layout = pane.get();
    const auto button = [&](std::string label, int width, ui::CommandId id) {
        auto* result = pane->make<Button>(std::move(label)); result->set_flat(true);
        result->on_press = [this, id] { app_.commands().execute(id); }; layout->controls.emplace_back(result, width); return result;
    };
    button("Sources", 9, sources); button("Pause/Live", 11, pause); button("Details", 9, details);
    button("Stop", 6, stop_cmd); button("Retry", 7, retry); button("Export", 8, export_cmd); button("Close", 7, close);
    severity_ = pane->make<ComboBox>(); severity_->set_items({"Level: all", "Warning +", "Error +", "Unknown", "Info"}); severity_->set_selected_index(0);
    severity_->on_select = [this](std::size_t index) {
        std::uint16_t mask = kLogAllSeverities;
        if (index == 1) mask = static_cast<std::uint16_t>((1U << 5) | (1U << 6) | (1U << 7));
        else if (index == 2) mask = static_cast<std::uint16_t>((1U << 6) | (1U << 7));
        else if (index == 3) mask = 1; else if (index == 4) mask = 1U << 3;
        set_severity(mask);
    }; layout->controls.emplace_back(severity_, 16);
    source_choice_ = pane->make<ComboBox>(); layout->controls.emplace_back(source_choice_, 19);
    source_choice_->on_select = [this](std::size_t index) { set_source_filter(index && index <= sources_.size() ? std::optional{sources_[index - 1]->id} : std::nullopt); };
    auto* exclude_label = pane->make<StaticText>("Exclude:"); layout->controls.emplace_back(exclude_label, 8);
    exclude_ = pane->make<InputLine>(); layout->controls.emplace_back(exclude_, 18);
    exclude_->on_edited = [this] { if (!syncing_) set_filter(shared_query_.include, exclude_->text()); };
    tabs_ = pane->make<TabControl>(); tabs_->set_presentation(TabPresentation::Compact); layout->tabs = tabs_;
    tabs_->on_change = [this](std::size_t) { if (!syncing_) synchronize_controls(); };
    notice_view_ = pane->make<StaticText>(""); layout->notice = notice_view_;
    auto viewport = std::make_unique<ScrollViewport>();
    auto picker = std::make_unique<PickerPane>(); auto* picker_layout = picker.get(); picker->set_preferred_size({60, static_cast<int>(12 + catalog_.size() * 2)});
    picker->make<StaticText>("Choose a growing file, a sample, or an available system source.")->set_bounds({0, 0, 70, 1});
    picker->make<StaticText>("Absolute log file path:")->set_bounds({0, 1, 45, 1});
    path_ = picker->make<InputLine>(); picker_layout->path = path_;
    auto* open = picker->make<Button>("&Open"); open->set_flat(true); open->on_press = [this] { open_file(path_->text()); }; picker_layout->open = open;
    picker->make<StaticText>("Sample Stream (no files or permissions required)")->set_bounds({0, 4, 58, 1});
    sample_ = picker->make<ComboBox>(); sample_->set_items({"Steady", "Burst", "Overload", "Rotation", "Truncation"}); sample_->set_selected_index(0); picker_layout->mode = sample_;
    auto* start = picker->make<Button>("&Start sample"); start->set_flat(true); start->on_press = [this] { open_sample(static_cast<SampleMode>(sample_->selected_index().value_or(0))); }; picker_layout->sample = start;
    if (catalog_.empty()) picker_layout->native.push_back(picker->make<StaticText>("System sources are not configured. File and Sample remain available."));
    for (std::size_t index = 0; index < catalog_.size(); ++index) {
        auto* native = picker->make<Button>(catalog_[index].title); native->set_flat(true);
        native->on_press = [this, index] { open_native(index); }; picker_layout->native.push_back(native);
    }
    viewport->set_content(std::move(picker)); tabs_->add_tab("&Sources", std::move(viewport));
    window->set_content(std::move(pane)); shell_->desktop().add_window(std::move(window));
    synchronize_controls(); set_notice("Open File or start a sample. Filter changes apply across source tabs."); app_.set_focus(path_);
}
LogMonitorApp::~LogMonitorApp() {
    for (auto& source : sources_) { if (source->lane) controller_.detach(source->lane); stop(*source, now_); }
    if (shell_) shell_->detach_desktop();
    for (auto id : commands_) app_.commands().withdraw(id);
    app_.commands().set_handler(app_.commands().standard().quit, {});
}
std::optional<std::size_t> LogMonitorApp::active_source() const noexcept {
    const auto index = tabs_->active_index(); return index && index <= sources_.size() ? std::optional{index - 1} : std::nullopt;
}
LogMonitorApp::Source* LogMonitorApp::active() const noexcept { const auto index = active_source(); return index ? sources_[*index].get() : nullptr; }
LogPanel* LogMonitorApp::panel(std::size_t index) const noexcept { return index < sources_.size() ? sources_[index]->panel : nullptr; }
const LogModel* LogMonitorApp::model(std::size_t index) const noexcept { return index < sources_.size() ? &sources_[index]->model : nullptr; }
void LogMonitorApp::set_notice(std::string text) { notice_text_ = std::move(text); notice_view_->set_text(notice_text_); }
void LogMonitorApp::show_sources() { tabs_->set_active_index(0); app_.set_focus(path_); }
void LogMonitorApp::select_source(std::size_t index) { if (index < sources_.size()) { tabs_->set_active_index(index + 1); app_.set_focus(&sources_[index]->panel->view()); } }
bool LogMonitorApp::open_file(std::string path) {
    if (opening_ || sources_.size() >= source_limit || path.empty() || path.size() > 32768 || !filesystem_.is_absolute_path(path)) {
        set_notice("Use an absolute file path; at most eight sources can be open."); return false;
    }
    opening_ = std::make_unique<OpenIntent>(OpenIntent{Source::Kind::File, std::move(path)}); return true;
}
bool LogMonitorApp::open_sample(SampleMode mode) {
    if (opening_ || sources_.size() >= source_limit || static_cast<unsigned>(mode) >= sample_names.size()) return false;
    opening_ = std::make_unique<OpenIntent>(OpenIntent{Source::Kind::Sample, {}, mode}); return true;
}
bool LogMonitorApp::open_native(std::size_t index) {
    if (index >= catalog_.size()) return false;
    if (!catalog_[index].available || !catalog_[index].open) { set_notice(catalog_[index].explanation + " Choose File or Sample, or retry when available."); return false; }
    if (opening_ || sources_.size() >= source_limit) return false;
    opening_ = std::make_unique<OpenIntent>(OpenIntent{Source::Kind::External, {}, SampleMode::Steady, index}); return true;
}
bool LogMonitorApp::start_source(Source& source, std::int64_t now) {
    source.next_event = now; source.running = true;
    if (source.kind == Source::Kind::File || (source.kind == Source::Kind::Sample && source.mode >= SampleMode::Rotation)) {
        auto session = source.sample_file ? source.sample_file->open_session() : files_.open_session();
        if (!session) return false;
        LogFollowOptions options; options.start = LogStartMode::RecentTail;
        source.follower = std::make_unique<LogFollower>(source.model, std::move(session), options);
        if (!source.follower->start(source.path, source.id, now)) return false;
    } else {
        if (source.kind == Source::Kind::External) {
            source.external = catalog_[source.native_descriptor].open(source.id, activity_);
            if (!source.external) return false;
            source.receiver = std::make_unique<LogStreamReceiver>(source.model, source.external->publications());
        } else {
            if (!source.queue) source.queue = std::make_unique<LogMailbox>(source.id, source.epoch, LogMailboxOptions{4, 1024, LogOverflowPolicy::DropOldest}, activity_);
            else if (!source.queue->reset(source.id, ++source.epoch)) return false;
            source.receiver = std::make_unique<LogStreamReceiver>(source.model, *source.queue);
        }
    }
    source.lane = controller_.attach(source.bindings()); return source.lane != 0;
}
bool LogMonitorApp::install(std::unique_ptr<Source> source) {
    auto panel = std::make_unique<LogPanel>(source->filter, 512); source->panel = panel.get(); panel->set_search(&source->search);
    const auto id = source->id;
    panel->on_query_changed = [this](const std::string& text) { if (!syncing_) set_filter(text, shared_query_.exclude); };
    panel->on_stop = [this] { stop_active(); }; panel->on_retry = [this] { retry_active(); };
    panel->on_clear_retention = [this, id] { for (auto& item : sources_) if (item->id == id) {
        if (item->frozen) { set_notice("Wait for the export snapshot before clearing retention."); return; }
        item->model.clear(); controller_.kick(item->lane);
    } };
    source->filter.set_query(shared_query_); panel->filter_box().set_query(shared_query_.include);
    panel->view().set_wrap_mode(wrap_ ? WrapMode::Word : WrapMode::None);
    if (!start_source(*source, now_)) { set_notice("Source could not be opened; retry or choose File/Sample."); return false; }
    const auto name = source->name; sources_.push_back(std::move(source));
    syncing_ = true; tabs_->add_tab(name, std::move(panel)); syncing_ = false;
    select_source(sources_.size() - 1); synchronize_controls(); return true;
}
void LogMonitorApp::stop(Source& source, std::int64_t now) {
    source.running = false; source.restarting = false;
    if (source.external) source.external->stop(now);
    if (source.receiver) source.receiver->stop(now);
    if (source.follower) source.follower->stop();
    if (source.lane) controller_.kick(source.lane);
}
void LogMonitorApp::stop_active() { if (auto* source = active()) { stop(*source, now_); set_notice("Source stopped; retained records remain. Retry reopens a bounded recent tail."); } }
void LogMonitorApp::retry_active() {
    auto* source = active(); if (!source) return;
    if (snapshot_ && snapshot_->source == source->id) { set_notice("Wait for the export snapshot before reopening this source."); return; }
    if (source->follower && source->running && source->follower->retry(now_)) { controller_.kick(source->lane); return; }
    if (source->lane) controller_.detach(source->lane);
    source->lane = 0;
    stop(*source, now_); source->follower.reset(); source->receiver.reset(); source->external.reset();
    source->restarting = true; source->restart_at = now_; set_notice("Reopening source; previous generations stay retained.");
}
bool LogMonitorApp::close_source(std::size_t index) {
    if (index >= sources_.size()) return false;
    auto& source = *sources_[index];
    if (snapshot_ && snapshot_->source == source.id) snapshot_.reset();
    if (source.lane) controller_.detach(source.lane);
    stop(source, now_);
    if (shared_query_.source_count && shared_query_.sources[0] == source.id) shared_query_.source_count = 0;
    syncing_ = true; auto removed = tabs_->remove_tab(index + 1); removed.reset(); sources_.erase(sources_.begin() + static_cast<std::ptrdiff_t>(index)); syncing_ = false;
    synchronize_queries(); synchronize_controls();
    if (auto current = active_source()) select_source(*current); else show_sources();
    return true;
}
void LogMonitorApp::synchronize_controls() {
    std::vector<std::string> names{"All sources"}; for (auto& source : sources_) names.push_back(source->name);
    source_choice_->set_items(std::move(names)); std::size_t selected = 0;
    if (shared_query_.source_count) for (std::size_t i = 0; i < sources_.size(); ++i) if (sources_[i]->id == shared_query_.sources[0]) selected = i + 1;
    source_choice_->set_selected_index(selected);
}
void LogMonitorApp::synchronize_queries() {
    syncing_ = true;
    for (auto& source : sources_) { source->panel->filter_box().set_query(shared_query_.include); source->filter.set_query(shared_query_); if (source->lane) controller_.kick(source->lane); }
    exclude_->set_text(shared_query_.exclude); syncing_ = false;
}
void LogMonitorApp::set_filter(std::string include, std::string exclude) {
    if (include.size() > kLogLiteralBytes || exclude.size() > kLogLiteralBytes) { set_notice("Filter literals are limited to 1024 UTF-8 bytes."); return; }
    shared_query_.include = std::move(include); shared_query_.exclude = std::move(exclude); synchronize_queries();
}
void LogMonitorApp::set_severity(std::uint16_t mask) { shared_query_.severity_mask = mask; synchronize_queries(); }
void LogMonitorApp::set_source_filter(std::optional<LogSourceId> source) { shared_query_.source_count = source ? 1 : 0; shared_query_.sources[0] = source.value_or(0); synchronize_queries(); }
void LogMonitorApp::set_wrap(bool wrap) { wrap_ = wrap; for (auto& source : sources_) source->panel->view().set_wrap_mode(wrap ? WrapMode::Word : WrapMode::None); }
std::optional<LogRecordId> LogMonitorApp::inspected_id() const {
    auto* source = active(); if (!source) return {};
    const auto& view = source->panel->view(); if (view.selection()) return view.selection()->cursor.id;
    if (!view.state().following && view.top()) return view.top()->id;
    const auto row = source->filter.size() ? source->filter.at(source->filter.size() - 1) : std::nullopt;
    return row ? std::optional{row->id} : std::nullopt;
}
std::string LogMonitorApp::detail_text() const {
    auto* source = active(); const auto id = inspected_id();
    if (!source) return "No retained entry is selected.";
    const auto diagnostic = source->external ? safe_raw(source->external->explanation()) : std::string{};
    if (!id) return diagnostic.empty() ? "No retained entry is selected." : "Source diagnostic:\n" + diagnostic + "\nNo retained entry is selected.";
    const auto record = source->filter.detail(*id); if (!record) return "This entry is no longer retained.";
    std::string text = "Source: " + source->name + "\nRecord: " + id_text(*id) + "\nLevel: " + std::string(severity_names[static_cast<std::size_t>(record->severity)]) + "\n";
    text += record->timestamp_nanos ? "Source timestamp: " + std::to_string(*record->timestamp_nanos) + " ns\n" : "Source timestamp: unknown\n";
    if (!diagnostic.empty()) text += "Source diagnostic:\n" + diagnostic + "\n";
    for (const auto& field : record->metadata().fields) text += std::string(field.name) + ": " + safe_raw(field.value) + "\n";
    text += "\nDisplay text:\n" + std::string(record->text) + "\n\nRaw bytes (hex escapes):\n" + safe_raw(record->raw);
    LogContextProvider context(source->model); context.set_anchor(*id, 3, 3);
    text += "\n\nRetained context (filter unchanged):\n";
    for (std::size_t index = 0; index < context.size(); ++index) if (const auto row = context.at(index)) text += id_text(row->id) + " " + std::string(row->text.substr(0, 2048)) + "\n";
    return text;
}
void LogMonitorApp::show_details() {
    auto window = std::make_unique<Window>("Retained entry details"); window->set_bounds({3, 2, 70, 18}); window->set_min_size({24, 8});
    auto text = std::make_unique<TextView>(); text->set_text(detail_text()); text->set_wrap_mode(WrapMode::Word); auto* focus = text.get(); window->set_content(std::move(text));
    auto* shown = shell_->desktop().add_window(std::move(window)); schedule_self_detach(*shown, app_); app_.set_focus(focus);
}
bool LogMonitorApp::exporting() const noexcept { return snapshot_ != nullptr || export_write_pending_; }
bool LogMonitorApp::begin_export(std::string path, ExportScope scope) {
    auto* source = active(); if (!source || exporting() || path.size() > 32768 || !filesystem_.is_absolute_path(path)) { set_notice("Choose an active source and a new absolute export filename."); return false; }
    snapshot_ = std::make_unique<Snapshot>(); snapshot_->source = source->id; snapshot_->scope = scope;
    snapshot_->job = {next_export_++, std::move(path), {}, {}};
    set_notice("Preparing a bounded snapshot; existing export files are preserved."); return true;
}
void LogMonitorApp::export_step() {
    ExportResult result;
    if (exports_.take(result)) { export_write_pending_ = false; set_notice(result.state == FileWriteStatus::Ok ? "Exported " + result.scope + " to " + result.path : result.state == FileWriteStatus::Conflict ? "Export target exists. Choose a new filename." : "Export failed; retained records are unchanged."); }
    if (!snapshot_) return;
    const auto found = std::find_if(sources_.begin(), sources_.end(), [this](const auto& source) { return source->id == snapshot_->source; });
    if (found == sources_.end()) { snapshot_.reset(); return; }
    auto& source = **found; auto& snapshot = *snapshot_;
    if (!snapshot.frozen) {
        if (source.filter.status().query_pending || source.filter.status().live_pending) return;
        snapshot.job.contents.reserve(export_limit);
        snapshot.query = source.filter.query_revision(); snapshot.position = 0; snapshot.end = source.filter.size();
        if (snapshot.scope == ExportScope::Selection) {
            const auto selection = source.panel->view().selection();
            const auto first = selection ? source.filter.index_of(selection->anchor.id) : std::nullopt;
            const auto last = selection ? source.filter.index_of(selection->cursor.id) : std::nullopt;
            if (!first || !last) { snapshot_.reset(); set_notice("Select retained eligible entries before exporting selection."); return; }
            snapshot.position = std::min(*first, *last); snapshot.end = std::max(*first, *last) + 1;
        }
        if (source.lane) controller_.detach(source.lane);
        source.lane = 0; source.frozen = true; snapshot.frozen = true;
        snapshot.job.scope = (snapshot.scope == ExportScope::Selection ? "selected whole records" : "filtered retained records") + std::string(" from ") + source.name + ", query " + std::to_string(snapshot.query);
        snapshot.job.contents = "# ckVision Log Monitor export\n# " + snapshot.job.scope + "\n# Retained window only; raw history outside retention is unavailable.\n";
        const auto& query = source.filter.published_query();
        snapshot.job.contents += "# Include: " + safe_raw(query.include) + "\n# Exclude: " + safe_raw(query.exclude) +
            "\n# Severity mask: " + std::to_string(query.severity_mask) + "\n# Source IDs:";
        for (std::size_t index = 0; index < query.source_count; ++index) snapshot.job.contents += " " + std::to_string(query.sources[index]);
        snapshot.job.contents += query.source_count ? "\n" : " all\n";
    }
    std::size_t bytes = 0, records = 0;
    while (snapshot.position < snapshot.end && records < 128 && bytes < 64 * 1024) {
        const auto record = source.filter.at(snapshot.position++); if (!record) continue;
        const auto prefix = "[" + id_text(record->id) + "] ";
        if (prefix.size() + record->text.size() + 1 > export_limit - snapshot.job.contents.size()) {
            source.frozen = false; source.lane = controller_.attach(source.bindings()); snapshot_.reset(); set_notice("Export exceeds the 2 MiB snapshot limit. Narrow the filter."); return;
        }
        snapshot.job.contents += prefix; snapshot.job.contents.append(record->text); snapshot.job.contents += '\n';
        bytes += prefix.size() + record->text.size() + 1; ++records; ++snapshot.copied;
    }
    if (snapshot.position < snapshot.end) return;
    if (!snapshot.ready) {
        source.frozen = false; source.lane = controller_.attach(source.bindings()); snapshot.ready = true;
        snapshot.job.scope += ", " + std::to_string(snapshot.copied) + " records";
    }
    if (exports_.submit(snapshot.job)) { snapshot_.reset(); export_write_pending_ = true; set_notice("Snapshot captured; source resumed while export writes."); }
}
void LogMonitorApp::export_prompt(ExportScope scope) {
    auto window = std::make_unique<Window>(scope == ExportScope::Selection ? "Export selected whole records" : "Export filtered retained records"); window->set_bounds({5, 3, 65, 8});
    auto pane = std::make_unique<ui::View>(); pane->make<StaticText>("New absolute filename (existing files are preserved)")->set_bounds({1, 0, 60, 1});
    auto* path = pane->make<InputLine>(); path->set_bounds({1, 2, 59, 1});
    auto* button = pane->make<Button>("&Export"); button->set_bounds({1, 4, 12, 1}); auto* raw = window.get();
    button->on_press = [this, path, scope, raw] { if (begin_export(path->text(), scope)) raw->close(); };
    window->set_content(std::move(pane)); auto* shown = shell_->desktop().add_window(std::move(window)); schedule_self_detach(*shown, app_); app_.set_focus(path);
}
void LogMonitorApp::step(std::int64_t now) {
    if (now < now_ || now < 0 || !app_.can_run_blocking()) return;
    now_ = now;
    // Creation/reopening occurs only in the outer app loop, not event dispatch.
    if (app_.can_run_blocking() && opening_) {
        if (next_source_ == std::numeric_limits<LogSourceId>::max()) { opening_.reset(); set_notice("Source identity exhausted."); return; }
        const auto intent = std::move(opening_); auto source = std::make_unique<Source>(next_source_++);
        source->kind = intent->kind; source->path = intent->path; source->mode = intent->mode; source->native_descriptor = intent->native;
        if (source->kind == Source::Kind::File) source->name = source->path.substr(source->path.find_last_of("/\\") + 1);
        else if (source->kind == Source::Kind::External) source->name = catalog_[source->native_descriptor].title;
        else {
            source->name = "Sample " + std::string(sample_names[static_cast<std::size_t>(source->mode)]) + " " + std::to_string(source->id);
            if (source->mode >= SampleMode::Rotation) { source->sample_file = std::make_unique<SampleFile>(activity_); source->path = "/sample/" + std::string(sample_names[static_cast<std::size_t>(source->mode)]) + ".log"; }
        }
        if (source->name.size() > 240) source->name = source->name.substr(0, 240) + "…";
        source->name = text::sanitize_display_text(source->name);
        install(std::move(source));
    }
    if (!sources_.empty()) {
        auto& source = *sources_[producer_cursor_++ % sources_.size()];
        if (source.restarting && app_.can_run_blocking() && now >= source.restart_at) {
            if (start_source(source, now)) source.restarting = false; else source.restart_at = after(now, 50'000'000);
        }
        if (source.running && source.external) source.external->step(now);
        if (source.running && source.kind == Source::Kind::Sample && now >= source.next_event) {
            const std::size_t count = source.mode == SampleMode::Burst ? 32 : source.mode == SampleMode::Overload ? 128 : 1;
            if (source.sample_file && source.counter && source.counter % 10 == 0) {
                if (source.mode == SampleMode::Rotation) source.sample_file->rotate(); else source.sample_file->truncate();
                source.next_event = after(now, sample_period); ++source.counter;
            } else {
                LogMessage message; message.source = source.id; message.epoch = source.epoch; message.arrival_nanos = now;
                for (std::size_t index = 0; index < count; ++index) {
                    std::array<char, 160> text{}; const std::string_view prefix = "sample event "; std::copy(prefix.begin(), prefix.end(), text.begin());
                    auto number = std::to_chars(text.data() + prefix.size(), text.data() + text.size(), source.counter++);
                    const std::string_view suffix = " 中 é"; std::copy(suffix.begin(), suffix.end(), number.ptr);
                    const std::string_view record(text.data(), static_cast<std::size_t>(number.ptr - text.data()) + suffix.size());
                    if (source.sample_file) { std::string line(record); line += '\n'; source.sample_file->append(line); }
                    else {
                        const auto severity = source.counter % 17 == 0 ? LogSeverity::Error : source.counter % 7 == 0 ? LogSeverity::Warning : LogSeverity::Info;
                        const std::array fields{LogField{"unit", "sample"}, LogField{"mode", sample_names[static_cast<std::size_t>(source.mode)]}};
                        message.set_record(record, {severity, {}, fields}); source.queue->offer(message);
                    }
                }
                source.next_event = after(now, sample_period);
            }
        }
    }
    controller_.step(now); export_step();
    for (auto& source : sources_) if (source->external) {
        auto status = source->panel->source_status(); const auto diagnostic = source->external->diagnostic();
        status.state = diagnostic.state; status.native_error = diagnostic.native_error; status.uncertain = diagnostic.uncertain;
        status.watch = diagnostic.watch;
        const auto& received = source->receiver->status();
        status.backlog_bytes = received.mailbox.queued_bytes + received.held_bytes + diagnostic.backlog_bytes;
        source->panel->set_source_status(status);
    }
}
std::optional<std::int64_t> LogMonitorApp::next_deadline() const noexcept {
    auto deadline = controller_.next_deadline(); const auto add = [&](std::int64_t time) { if (!deadline || time < *deadline) deadline = time; };
    if (opening_ || snapshot_) add(after(now_, yield_period));
    if (export_write_pending_ && !exports_.has_wake()) add(after(now_, 50'000'000));
    for (const auto& source : sources_) {
        if (source->restarting) add(source->restart_at);
        if (source->running && source->kind == Source::Kind::Sample) add(source->next_event <= now_ ? after(now_, yield_period) : source->next_event);
        if (source->running && source->external) if (auto value = source->external->next_deadline()) add(*value <= now_ ? after(now_, yield_period) : *value);
    }
    return deadline;
}
}  // namespace ckv::logmonitor
