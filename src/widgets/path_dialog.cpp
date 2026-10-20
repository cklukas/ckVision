// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/path_dialog_internal.hpp"

#include <algorithm>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::widgets {
namespace {
constexpr ListItemId kParentRow = UINT64_MAX;
constexpr ListItemId kRecentFirst = UINT64_MAX - 1;
constexpr std::size_t kRecentRows = 8;

class FileCompletionProvider final : public CompletionProvider {
public:
    std::function<void(const CompletionRequest&)> on_request;
    CompletionRequest query;
    CompletionSnapshot result;
    void request(const CompletionRequest& value) override {
        query = value;
        result = {query.revision, CompletionState::Pending, {}};
        const auto callback = on_request;
        if (callback) callback(query);
    }
    const CompletionSnapshot& snapshot() const noexcept override { return result; }
    void clear() { result = {query.revision, CompletionState::Pending, {}}; }
    void publish(const DirectorySnapshot& source, CompletionId preferred = 0) {
        result = {query.revision, source.state == DirectoryState::Ready ? CompletionState::Ready :
                                 source.state == DirectoryState::Failed ? CompletionState::Failed :
                                                                         CompletionState::Partial, {}};
        if (query.has_selection || query.text.size() > kMaxCompletionTextBytes || source.state == DirectoryState::Failed)
            return;
        const auto graphemes = text::split_graphemes(query.text);
        const auto end = graphemes.size();
        if (query.cursor != end) return;
        const auto separator = query.text.find_last_of("/\\");
        const auto bytes = separator == std::string::npos ? 0 : separator + 1;
        std::size_t begin = 0;
        std::size_t offset = 0;
        while (begin < end && offset + graphemes[begin].size() <= bytes) offset += graphemes[begin++].size();
        // The basename may start with Extend and share the separator's
        // cluster. Preserve the parent fragment while replacing that whole
        // cluster; counting a separately segmented parent would duplicate it.
        const auto parent_fragment = query.text.substr(offset, bytes - offset);
        result.candidates.reserve(source.entries.size());
        for (const auto& item : source.entries) {
            result.candidates.push_back({item.id, begin, end,
                                         parent_fragment + item.name + (item.is_directory ? "/" : "")});
        }
        const auto selected = std::find_if(result.candidates.begin(), result.candidates.end(),
            [preferred](const CompletionCandidate& candidate) { return candidate.id == preferred; });
        if (selected != result.candidates.end()) std::rotate(result.candidates.begin(), selected, selected + 1);
    }
};

class DirectoryRows final : public ListModel {
public:
    std::shared_ptr<const DirectorySnapshot> source;
    std::string parent;
    std::vector<std::string> recent;
    ListItemId suggestion = 0;
    std::vector<std::string> labels;
    std::vector<std::string> recent_labels;
    void prepare_labels() {
        labels.clear(); recent_labels.clear();
        for (const auto& path : recent) recent_labels.push_back("Recent: " + path);
        if (source) for (const auto& entry : source->entries)
            labels.push_back((entry.id == suggestion ? "> " : "  ") + entry.name +
                             (entry.is_directory ? "/" : ""));
    }
    void set_suggestion(ListItemId id) noexcept {
        suggestion = id;
        if (source) for (std::size_t i = 0; i < labels.size(); ++i)
            labels[i][0] = source->entries[i].id == id ? '>' : ' ';
    }
    std::size_t item_count() const noexcept override { return navigation_count() + (source ? source->entries.size() : 0); }
    ListItem item_at(std::size_t index) const override {
        if (!parent.empty()) {
            if (index == 0) return {kParentRow, "..", std::nullopt};
            --index;
        }
        if (index < recent.size()) return {kRecentFirst - index, {}, std::nullopt, recent_labels[index]};
        index -= recent.size();
        if (!source || index >= source->entries.size()) return {};
        const auto& entry = source->entries[index];
        return {entry.id, {}, std::nullopt, labels[index]};
    }
    std::optional<std::size_t> index_of(ListItemId id) const override {
        if (!parent.empty() && id == kParentRow) return 0;
        for (std::size_t i = 0; i < recent.size(); ++i)
            if (id == kRecentFirst - i) return (!parent.empty() ? 1U : 0U) + i;
        if (source) {
            for (std::size_t i = 0; i < source->entries.size(); ++i)
                if (id == source->entries[i].id) return navigation_count() + i;
        }
        return std::nullopt;
    }
    std::size_t navigation_count() const noexcept { return recent.size() + (!parent.empty() ? 1U : 0U); }
};

class PathDialogState final : public std::enable_shared_from_this<PathDialogState> {
public:
    PathDialogState(DirectoryService& service, ui::Application& application, FileDialogOptions options,
                    bool directory_only, std::string initial, std::function<void(FileDialogResult)> callback)
        : app(application), fs(service.filesystem()), session(service.open_session()), options_(std::move(options)),
          directories_only_(directory_only), base_(fs.normalize_path(initial)), on_result_(std::move(callback)) {}
    ~PathDialogState() {
        retire();
        if (!window_liveness.expired() && list) list->clear_model();
    }
    ui::Application& app;
    const FileSystem& fs;
    std::unique_ptr<DirectorySession> session;
    InputLine* field = nullptr;
    ListView* list = nullptr;
    StaticText* status = nullptr;
    Button* previous = nullptr;
    Button* next = nullptr;
    Button* filter = nullptr;
    Button* hidden = nullptr;
    Window* window = nullptr;
    std::weak_ptr<void> window_liveness;
    std::shared_ptr<FileCompletionProvider> provider = std::make_shared<FileCompletionProvider>();
    DirectoryRows rows;
    StandardStrings strings;
    ui::Application::TimerId timer = 0;

    void initialize() {
        if (!options_.filters.empty()) options_.active_filter = std::min(options_.active_filter, options_.filters.size() - 1);
        const auto weak = weak_from_this();
        provider->on_request = [weak](const CompletionRequest& query) {
            if (const auto self = weak.lock()) self->browse(query);
        };
        field->on_accept = [weak] { if (const auto self = weak.lock()) self->submit(false); };
        field->on_completion_changed = [weak] {
            if (const auto self = weak.lock()) self->suggestion_changed();
        };
        list->set_model(rows);
        list->on_cursor_changed = [weak](std::size_t index) {
            if (const auto self = weak.lock()) self->choose(index);
        };
        list->on_activate = [weak](std::size_t index) {
            if (const auto self = weak.lock()) self->activate(index);
        };
        previous->on_press = [weak] { if (const auto self = weak.lock()) self->page(true); };
        next->on_press = [weak] { if (const auto self = weak.lock()) self->page(false); };
        filter->on_press = [weak] {
            if (const auto self = weak.lock()) {
                if (self->options_.filters.empty()) return;
                self->options_.active_filter = (self->options_.active_filter + 1) % self->options_.filters.size();
                self->field->refresh_completion();
            }
        };
        hidden->on_press = [weak] {
            if (const auto self = weak.lock()) {
                self->options_.show_hidden = !self->options_.show_hidden;
                self->field->refresh_completion();
            }
        };
        field->set_text(options_.suggested_name.empty() ? directory_prefix(base_) : fs.join(base_, options_.suggested_name));
        field->set_completion_provider(provider);
        poll();
    }
    void retire() noexcept {
        if (timer != 0) { app.cancel_timer(timer); timer = 0; }
        session.reset();
    }
    void report(FileDialogResult result) {
        if (delivered_) return;
        delivered_ = true;
        retire();
        const auto callback = on_result_;
        const auto alive = window_liveness;
        Window* const target = window;
        if (result.accepted && !options_.recent_locations_key.empty())
            app.history().record(options_.recent_locations_key, base_);
        if (callback) callback(std::move(result));
        if (!alive.expired()) target->close();
    }
    void submit(bool select_directory) {
        if (!session || delivered_) return;
        std::string value = field->text();
        if (value.empty()) { if (!select_directory) return; value = base_; }
        if (resolving_ && submitted_ == value && select_directory_ == select_directory) return;
        submitted_ = value;
        select_directory_ = select_directory;
        submitted_filter_ = options_.active_filter;
        submitted_hidden_ = options_.show_hidden;
        resolving_ = true;
        provider->clear(); field->completion_changed();
        query_ = {}; query_.revision = ++revision_; query_.source_generation = generation_;
        query_.operation = DirectoryOperation::Resolve; query_.directory = base_;
        query_.for_navigation = !select_directory;
        query_.path = fs.is_absolute_path(value) ? fs.normalize_path(value) : fs.join(base_, value);
        status->set_text("Checking path...");
        request();
    }
    void poll() {
        if (!session || window_liveness.expired() || delivered_) return;
        const auto result = session->snapshot();
        if (!result || result->revision != query_.revision || result->source_generation != query_.source_generation ||
            result == publication_) { schedule_poll(); return; }
        publication_ = result;
        awaiting_ = result->state == DirectoryState::Pending || result->state == DirectoryState::Partial;
        if (!awaiting_ && timer != 0) { app.cancel_timer(timer); timer = 0; }
        if (resolving_) {
            if (result->state == DirectoryState::Pending || result->state == DirectoryState::Partial) return;
            resolving_ = false;
            if (result->state == DirectoryState::Failed) { status->set_text(result->diagnostic); return; }
            if (result->resolved_directory) {
                if (select_directory_) { report({true, result->path}); return; }
                navigate(result->path);
                return;
            }
            const bool directory_form = !submitted_.empty() && (submitted_.back() == '/' || submitted_.back() == '\\');
            if (directories_only_ || directory_form || submitted_ == "." || submitted_ == "..") {
                status->set_text("Directory is unavailable"); return;
            }
            report({true, result->path}); return;
        }
        updating_ = true;
        rows.source = result;
        rows.parent = result->state == DirectoryState::Failed ? std::string{} : fs.parent(result->directory);
        if (rows.parent == result->directory) rows.parent.clear();
        rows.prepare_labels();
        previous->set_enabled(result->has_previous);
        next->set_enabled(result->has_next);
        provider->publish(*result, query_.preferred ? query_.preferred->id : 0);
        field->completion_changed();
        list->model_changed();
        updating_ = false;
        suggestion_changed();
        status->set_text(result->state == DirectoryState::Failed ? result->diagnostic :
            result->directory + "  " + std::to_string(result->matching) + " matching" +
            (result->state == DirectoryState::Ready ? "" : " (discovering...)"));
    }
private:
    static std::string directory_prefix(const std::string& path) { return path.ends_with('/') ? path : path + "/"; }
    void browse(const CompletionRequest& input) {
        if (resolving_ && input.text == submitted_ && generation_ == query_.source_generation &&
            options_.active_filter == submitted_filter_ && options_.show_hidden == submitted_hidden_) {
            provider->clear();
            return;
        }
        resolving_ = false;
        submitted_.clear();
        const auto previous_query = query_;
        query_ = {}; query_.revision = ++revision_; query_.source_generation = generation_;
        query_.show_hidden = options_.show_hidden; query_.directories_only = directories_only_;
        if (!options_.filters.empty()) query_.extensions = options_.filters[options_.active_filter].extensions;
        const auto separator = input.text.find_last_of("/\\");
        if (separator == std::string::npos) { query_.directory = base_; query_.prefix = input.text; }
        else {
            const auto parent = input.text.substr(0, separator + 1);
            query_.directory = fs.is_absolute_path(parent) ? fs.normalize_path(parent) : fs.join(base_, parent);
            query_.prefix = input.text.substr(separator + 1);
        }
        if (previous_query.operation == DirectoryOperation::Browse &&
            previous_query.directory == query_.directory && previous_query.prefix == query_.prefix &&
            previous_query.extensions == query_.extensions && previous_query.show_hidden == query_.show_hidden &&
            previous_query.directories_only == query_.directories_only &&
            previous_query.source_generation == query_.source_generation) {
            // Caret/focus changes do not change filename eligibility. Reuse
            // the current model/query instead of rescanning a huge index, and
            // retain an outstanding page request. No field callback occurs
            // inside the provider call; InputLine reads this result afterward.
            query_ = previous_query;
            if (rows.source && rows.source->revision == query_.revision &&
                rows.source->source_generation == query_.source_generation)
                provider->publish(*rows.source, query_.preferred ? query_.preferred->id : 0);
            else provider->clear();
            return;
        }
        const auto cached = rows.source;
        updating_ = true;
        rows.source.reset(); rows.parent.clear(); rows.recent.clear();
        if (!options_.recent_locations_key.empty()) {
            const auto& history = app.history().entries(options_.recent_locations_key);
            for (std::size_t i = 0; i < std::min(history.size(), kRecentRows); ++i) {
                if (history[i].size() <= kDirectoryNameBytes) rows.recent.push_back(history[i]);
            }
        }
        rows.suggestion = 0;
        if (cached && (cached->state == DirectoryState::Ready || cached->state == DirectoryState::Partial) &&
            cached->directory == query_.directory && cached->source_generation == query_.source_generation) {
            // A fresh query over the bounded known subset, not a relabeled
            // worker reply. The complete index is still matched off-thread.
            auto known = std::make_shared<DirectorySnapshot>();
            known->revision = query_.revision; known->source_generation = query_.source_generation;
            known->state = DirectoryState::Partial; known->directory = cached->directory;
            known->discovered = cached->entries.size();
            for (const auto& entry : cached->entries)
                if (directory_item_matches(entry, query_)) known->entries.push_back(entry);
            known->matching = known->entries.size();
            rows.source = std::move(known);
            rows.parent = fs.parent(rows.source->directory);
            if (rows.parent == rows.source->directory) rows.parent.clear();
            provider->publish(*rows.source);
        }
        rows.prepare_labels();
        list->model_changed();
        updating_ = false;
        filter->set_text(options_.filters.empty() ? strings.filter :
                         strings.filter + ": " + options_.filters[options_.active_filter].label);
        hidden->set_text(options_.show_hidden ? strings.hide_hidden : strings.show_hidden);
        previous->set_enabled(false); next->set_enabled(false);
        status->set_text(!session ? "Directory workers are busy; close and retry" :
                         rows.source ? "Finding directory matches..." : "Discovering directory...");
        if (session) request(false);
    }
    void suggestion_changed() {
        if (window_liveness.expired() || updating_) return;
        const auto candidate = field->completion_candidate();
        const auto id = candidate ? candidate->id : 0;
        if (rows.suggestion == id) return;
        rows.set_suggestion(id);
        updating_ = true;
        list->model_changed();
        if (const auto index = rows.index_of(id)) list->set_cursor(*index);
        updating_ = false;
    }
    void choose(std::size_t index) {
        if (updating_ || resolving_ || !rows.source || index < rows.navigation_count()) return;
        index -= rows.navigation_count();
        if (index >= rows.source->entries.size()) return;
        const auto item = rows.source->entries[index];
        field->select_completion(item.id);
        // Preserve this explicit choice even if a later partial batch sorts
        // earlier entries onto the page. The source verifies its identity.
        query_.preferred = item; query_.revision = ++revision_;
        if (session) request();
    }
    void activate(std::size_t index) {
        if (!rows.source || resolving_) return;
        const auto item = rows.item_at(index);
        if (item.id == kParentRow) { field->set_text(rows.parent); submit(false); return; }
        for (std::size_t i = 0; i < rows.recent.size(); ++i)
            if (item.id == kRecentFirst - i) { field->set_text(rows.recent[i]); submit(false); return; }
        if (index < rows.navigation_count()) return;
        index -= rows.navigation_count();
        if (index >= rows.source->entries.size()) return;
        const auto entry = rows.source->entries[index];
        const auto path = fs.join(rows.source->directory, entry.name);
        if (entry.is_directory) { field->set_text(path); submit(false); }
        else { field->set_text(path); app.set_focus(field); }
    }
    void navigate(std::string path) {
        base_ = std::move(path);
        field->set_text(directory_prefix(base_));
        app.set_focus(field);
    }
    void page(bool backward) {
        if (!session || !rows.source || resolving_) return;
        if (backward ? !rows.source->has_previous : !rows.source->has_next) return;
        const auto boundary = backward ? rows.source->first : rows.source->last;
        // A byte-limited publication may contain only its out-of-page pin.
        // Clearing that pin exposes the ordinary page at the same boundary.
        if (boundary) query_.boundary = boundary;
        query_.backward = backward; query_.preferred.reset(); query_.revision = ++revision_;
        provider->clear(); field->completion_changed();
        status->set_text("Loading directory page...");
        request();
    }
    void request(bool immediate_poll = true) {
        awaiting_ = true;
        session->request(query_);
        // browse() runs inside CompletionProvider::request: publishing into
        // its InputLine is deferred until that provider call has returned.
        if (immediate_poll) poll();
        schedule_poll();
    }
    void schedule_poll() {
        if (!awaiting_ || !session || timer != 0 || delivered_) return;
        const auto weak = weak_from_this();
        timer = app.start_timer(20'000'000, false, [weak] {
            if (const auto self = weak.lock()) {
                self->timer = 0;
                self->poll();
                self->schedule_poll();
            }
        });
    }
public:
    void refresh() { ++generation_; field->refresh_completion(); }
private:
    FileDialogOptions options_;
    bool directories_only_ = false;
    std::string base_;
    std::function<void(FileDialogResult)> on_result_;
    DirectoryQuery query_;
    std::shared_ptr<const DirectorySnapshot> publication_;
    std::uint64_t revision_ = 0;
    std::uint64_t generation_ = 0;
    bool updating_ = false;
    bool awaiting_ = false;
    bool resolving_ = false;
    bool select_directory_ = false;
    std::size_t submitted_filter_ = 0;
    bool submitted_hidden_ = false;
    bool delivered_ = false;
    std::string submitted_;
};
}  // namespace

WindowHandle detail::make_path_dialog(FileDialogMode mode, bool directory_only, std::string initial_directory,
                              DirectoryService& directories, FileDialogOptions options,
                              const ui::StandardRoles& roles, ui::Application& app, ui::View* restore_focus_to,
                              std::function<void(FileDialogResult)> on_result, const StandardStrings& strings) {
    auto state = std::make_shared<PathDialogState>(directories, app, std::move(options), directory_only,
                                                 std::move(initial_directory), std::move(on_result));
    state->strings = strings;
    auto window = std::make_unique<Window>(directory_only ? strings.select_directory_title :
                                          mode == FileDialogMode::Open ? strings.open_file_title : strings.save_file_title);
    state->window = window.get(); state->window_liveness = window->lifetime_token();
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame, roles.dialog_background);
    window->set_resizable(false);
    auto column = std::make_unique<ui::Column>(); column->set_spacing(1);
    state->field = static_cast<InputLine*>(column->add_item(std::make_unique<InputLine>(), {ui::SizePolicy::Fixed, 1}));
    state->status = static_cast<StaticText*>(column->add_item(std::make_unique<StaticText>(""), {ui::SizePolicy::Fixed, 1}));
    state->status->set_preformatted(true);
    state->list = static_cast<ListView*>(column->add_item(std::make_unique<ListView>(), {ui::SizePolicy::Expanding, 1}));
    state->list->set_preferred_size({64, 10});
    auto pages = std::make_unique<ui::Row>(); pages->set_spacing(2);
    state->previous = static_cast<Button*>(pages->add_item(std::make_unique<Button>(strings.previous_page), {ui::SizePolicy::Fixed, 1}));
    state->next = static_cast<Button*>(pages->add_item(std::make_unique<Button>(strings.next_page), {ui::SizePolicy::Fixed, 1}));
    auto* refresh = static_cast<Button*>(pages->add_item(std::make_unique<Button>(strings.refresh_directory), {ui::SizePolicy::Fixed, 1}));
    refresh->on_press = [weak_state = std::weak_ptr<PathDialogState>(state)] {
        if (const auto held = weak_state.lock()) held->refresh();
    };
    column->add_item(std::move(pages), {ui::SizePolicy::Fixed, 1});
    auto buttons = std::make_unique<ui::Row>(); buttons->set_spacing(2);
    state->filter = static_cast<Button*>(buttons->add_item(std::make_unique<Button>(strings.filter), {ui::SizePolicy::Fixed, 1}));
    state->hidden = static_cast<Button*>(buttons->add_item(std::make_unique<Button>(strings.show_hidden), {ui::SizePolicy::Fixed, 1}));
    if (directory_only) state->filter->set_visible(false);
    auto* accept = static_cast<Button*>(buttons->add_item(std::make_unique<Button>(directory_only ? strings.select :
                                        mode == FileDialogMode::Open ? strings.open : strings.save), {ui::SizePolicy::Fixed, 1}));
    accept->set_default(true);
    auto* cancel = static_cast<Button*>(buttons->add_item(std::make_unique<Button>(strings.cancel), {ui::SizePolicy::Fixed, 1}));
    column->add_item(std::move(buttons), {ui::SizePolicy::Fixed, 1});
    auto help = std::make_unique<StaticText>(strings.completion_help);
    help->set_preformatted(true);
    column->add_item(std::move(help), {ui::SizePolicy::Fixed, 1});
    window->set_content(std::move(column));
    const auto weak = std::weak_ptr<PathDialogState>(state);
    accept->on_press = [weak, directory_only] { if (const auto held = weak.lock()) held->submit(directory_only); };
    cancel->on_press = [weak] { if (const auto held = weak.lock()) held->report({}); };
    window->accept_request = accept->on_press;
    window->cancel_request = cancel->on_press;
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    Window* const pointer = window.get();
    const auto alive = window->lifetime_token();
    // This closure is the sole owner at rest. Every callback takes a strong
    // local reference before invoking client code that may destroy the window.
    window->on_closed = [state, focus_restore, pointer, alive, &app] {
        state->retire();
        focus_restore.restore(app);
        if (!alive.expired()) schedule_self_detach(*pointer, app);
    };
    window->on_detached = [weak] { if (const auto held = weak.lock()) held->retire(); };
    state->initialize();
    return {std::move(window), state->field};
}

WindowHandle make_file_dialog(FileDialogMode mode, std::string initial_directory, DirectoryService& directories,
                              FileDialogOptions options, const ui::StandardRoles& roles, ui::Application& app,
                              ui::View* restore_focus_to, std::function<void(FileDialogResult)> on_result,
                              const StandardStrings& strings) {
    return detail::make_path_dialog(mode, false, std::move(initial_directory), directories, std::move(options), roles,
                                    app, restore_focus_to, std::move(on_result), strings);
}
FileDialogPresentation present_modal_file_dialog(FileDialogMode mode, std::string initial_directory,
                              DirectoryService& directories, FileDialogOptions options, ui::Application& app,
                              Desktop& desktop, const ui::StandardRoles& roles, const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<FileDialogResult>;
    auto parts = Access::make();
    auto handle = make_file_dialog(mode, std::move(initial_directory), directories, std::move(options), roles, app,
                                  app.focused(), [state = parts.state](FileDialogResult result) { Access::record(state, std::move(result)); }, strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state] {
        if (previous) previous();
        Access::finish(state, {});
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}
}  // namespace ckv::widgets
