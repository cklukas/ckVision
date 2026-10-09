// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "cvision/ui/application.hpp"
#include "cvision/widgets/progress.hpp"

namespace ckv::widgets {
// Stable, never reused within a model. Zero means no task/parent.
using ProgressTaskId = std::uint64_t;
// Explicit operation lifecycle, independent of numeric completion.
enum class ProgressTaskState { Queued, Running, Paused, Completed, Failed, Cancelled };
// Manual counts or normalized explicit child weights; never sum mixed units.
enum class ProgressAggregation { Manual, WeightedChildren };
// Spinner geometry, independent of Unicode/ASCII policy.
enum class ActivityPresentation { Dots, Braille, Ascii };
// Multi-task row height: one row or one row plus a message line.
enum class ProgressViewPresentation { Compact, Detailed };
// Counts may be displayed directly or with decimal/binary unit prefixes.
enum class ProgressUnitScale { None, Decimal, Binary };

// Owned task specification; absent total means unknown work.
struct ProgressTaskOptions {
    // User-facing description; owned UTF-8 text.
    std::string title;
    // Parent identity; zero makes a root task.
    ProgressTaskId parent = 0;
    // Known nonnegative amount; absent means unknown.
    std::optional<double> total;
    // Owned unit label, such as files or B.
    std::string unit;
    // Positive finite contribution to a weighted parent.
    double weight = 1.0;
    // Manual counts by default; weighted children require explicit selection.
    ProgressAggregation aggregation = ProgressAggregation::Manual;
};

// Immutable to clients; update through ProgressModel for coherent notifications.
struct ProgressTask {
    // Stable identity assigned by the owning model.
    ProgressTaskId id = 0;
    // Validated owned task configuration.
    ProgressTaskOptions options;
    // Measured nonnegative work; may exceed a revised total.
    double completed = 0;
    // Explicit operation state; completion is not inferred from counts.
    ProgressTaskState state = ProgressTaskState::Queued;
    // Optional second-line detail.
    std::string message;
    // False hides this row and all descendants.
    bool visible = true;
    // True prevents changes to child membership and weights.
    bool children_finalized = false;
private:
    friend class ProgressModel;
    std::int64_t elapsed_nanos = 0;
    std::int64_t running_since = 0;
    struct Sample { std::int64_t time = 0; double amount = 0; };
    std::array<Sample, 8> samples{};
    std::size_t sample_count = 0;
    std::int64_t last_advance = 0;
};

// Derived values; fraction/ETA absent when their prerequisites are unknown.
struct ProgressMetrics {
    // Known fraction clamped to zero through one; absent for unknown aggregate.
    std::optional<double> fraction;
    // Weighted known contribution; not an exact percentage when fraction is absent.
    double known_lower_bound = 0;
    // Estimated work units per active second; absent before warm-up or after a stall.
    std::optional<double> rate;
    // Estimated remaining duration, unavailable for weighted or unknown tasks.
    std::optional<double> remaining_seconds;
    // Accumulated active duration; pauses are excluded.
    std::int64_t elapsed_nanos = 0;
    // A descendant failed or was cancelled.
    bool child_exception = false;
};

namespace detail { struct ProgressObservers; }
// Move-only observer lifetime; safe to destroy after its model.
class ProgressSubscription {
public:
    // Construct a disconnected subscription.
    ProgressSubscription() = default;
    // Disconnect from a surviving model.
    ~ProgressSubscription();
    // Transfer observer ownership without reconnecting.
    ProgressSubscription(ProgressSubscription&&) noexcept;
    // Disconnect this observer and take ownership from another.
    ProgressSubscription& operator=(ProgressSubscription&&) noexcept;
    ProgressSubscription(const ProgressSubscription&) = delete;
    ProgressSubscription& operator=(const ProgressSubscription&) = delete;
    // Disconnect immediately; idempotent.
    void reset() noexcept;
private:
    friend class ProgressModel;
    std::weak_ptr<detail::ProgressObservers> observers_;
    std::uint64_t id_ = 0;
};

// UI-thread-owned data. Explicit time only, no clock reads or background work.
// Observers may inspect data/disconnect, but must not mutate the model in a callback.
class ProgressModel {
public:
    // Construct an empty model with explicit time zero.
    ProgressModel();
    // Expire the lifetime token before notifying observers of shutdown.
    ~ProgressModel();
    ProgressModel(const ProgressModel&) = delete;
    ProgressModel& operator=(const ProgressModel&) = delete;
    // Add a validated task; zero on invalid configuration, frozen children or depth over 64 levels.
    ProgressTaskId add_task(ProgressTaskOptions options);
    // Borrow tasks in stable insertion order; mutation may invalidate this span.
    std::span<const ProgressTask> tasks() const noexcept { return tasks_; }
    // Borrow one task; nullptr for a stale id.
    const ProgressTask* find(ProgressTaskId id) const noexcept;
    // Atomically remove a subtree; reject removal from a finalized child set.
    bool remove(ProgressTaskId id);
    // Reject cycles, stale parents and finalized old/new child sets.
    bool set_parent(ProgressTaskId id, ProgressTaskId parent);
    // Set owned text and unit configuration without resetting progress.
    bool set_title(ProgressTaskId id, std::string title);
    bool set_message(ProgressTaskId id, std::string message);
    bool set_unit(ProgressTaskId id, std::string unit);
    // Validate numeric inputs before any mutation; totals may be absent or zero.
    bool set_total(ProgressTaskId id, std::optional<double> total);
    bool set_completed(ProgressTaskId id, double completed);
    bool advance(ProgressTaskId id, double amount = 1);
    bool set_weight(ProgressTaskId id, double weight);
    // Select explicit aggregation; finalized children keep membership/weights fixed.
    bool set_aggregation(ProgressTaskId id, ProgressAggregation aggregation);
    bool finalize_children(ProgressTaskId id);
    // Hide a row and descendants without deleting its measured contribution.
    bool set_visible(ProgressTaskId id, bool visible);
    // Explicit state transitions; resume via start. Terminal states require reset.
    bool start(ProgressTaskId id);
    bool pause(ProgressTaskId id);
    bool complete(ProgressTaskId id);
    bool fail(ProgressTaskId id);
    bool cancel(ProgressTaskId id);
    // Begin a new queued measurement epoch; preserve title, total and hierarchy.
    bool reset(ProgressTaskId id);
    // Monotonic explicit time; negative/regressing readings are ignored.
    void set_time(std::int64_t now_nanos);
    std::int64_t time() const noexcept { return now_; }
    // Pure derived metrics at the last explicit time.
    ProgressMetrics metrics(ProgressTaskId id) const;
    // Coalesce mutations between begin/end into one observer notification.
    void begin_update() noexcept { ++batch_; }
    void end_update();
    // phase_only distinguishes animation from data/metadata changes.
    ProgressSubscription subscribe(std::function<void(bool phase_only)> observer);
    // Weak token expires before destruction notifies its remaining observers.
    std::weak_ptr<void> lifetime_token() const noexcept { return lifetime_; }
private:
    ProgressTask* mutable_task(ProgressTaskId id) noexcept;
    bool transition(ProgressTaskId id, ProgressTaskState state);
    void changed(bool phase_only = false);
    void sample(ProgressTask& task);
    std::vector<ProgressTask> tasks_;
    std::shared_ptr<detail::ProgressObservers> observers_;
    std::shared_ptr<void> lifetime_;
    ProgressTaskId next_id_ = 1;
    std::int64_t now_ = 0;
    unsigned batch_ = 0;
    bool pending_ = false;
};

// Noninteractive activity/status glyph. Advance phase explicitly or bind a controller.
class ActivityIndicator : public ui::View {
public:
    // Construct a running Braille indicator with a one-cell preference.
    ActivityIndicator();
    // Built-in spinner or validated owned custom frame sequence (false if empty/invalid).
    void set_presentation(ActivityPresentation presentation);
    bool set_frames(std::vector<std::string> frames);
    // Choose Unicode or ASCII explicitly.
    void set_glyphs(ProgressGlyphs glyphs);
    void set_state(ProgressTaskState state);
    // Choose animation or stable activity markers.
    void set_motion(ProgressMotion motion);
    void set_phase(std::int64_t phase);
    // True only when visible, running, and animated.
    bool needs_animation() const noexcept;
    void on_attached() override;
    void on_detaching() override;
    void on_effective_visibility_changed() override;
    // Scheduling observer installed by an explicit controller binding.
    std::function<void()> on_animation_changed;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
private:
    ActivityPresentation presentation_ = ActivityPresentation::Braille;
    ProgressGlyphs glyphs_ = ProgressGlyphs::Unicode;
    ProgressTaskState state_ = ProgressTaskState::Running;
    ProgressMotion motion_ = ProgressMotion::Animated;
    std::int64_t phase_ = 0;
    int frame_width_ = 1;
    std::vector<std::string> frames_;
    bool attached_ = false;
    ui::RoleId role_ = ui::kInvalidRole;
};

// Optional data columns; defaults deliberately retain a quiet three-column row.
struct ProgressColumns {
    // Show exact percentage when known; state remains visible independently.
    bool percentage = true;
    // Show completed/total counts in owned units.
    bool counts = false;
    // Show active elapsed seconds.
    bool elapsed = false;
    // Show measured throughput or an unavailable marker.
    bool rate = false;
    // Show estimated remaining seconds or an unavailable marker.
    bool eta = false;
    // Show activity beside the meter even for known totals.
    bool spinner = false;
};

// Responsive passive task display; safely clears when its borrowed model dies.
class ProgressView : public ui::View {
public:
    // Borrow the model with scoped observation and a safe weak lifetime.
    explicit ProgressView(ProgressModel& model);
    ~ProgressView() override;
    // Choose one-row or detail-line task geometry.
    void set_presentation(ProgressViewPresentation presentation);
    // Select meter fill independently of row geometry.
    void set_meter_presentation(ProgressPresentation presentation);
    // Select movement of unknown-total meters.
    void set_activity_style(ProgressActivityStyle style);
    // Select the optional activity-column glyph sequence.
    void set_spinner_presentation(ActivityPresentation presentation);
    // Choose Unicode or ASCII explicitly.
    void set_glyphs(ProgressGlyphs glyphs);
    // Choose animation or stable activity markers.
    void set_motion(ProgressMotion motion);
    // Enable metadata columns; narrow layouts drop optional fields first.
    void set_columns(ProgressColumns columns);
    // Format counts/rate without locale or environment access.
    void set_unit_scale(ProgressUnitScale scale);
    // Limit visible rows and select a first task index for large datasets.
    void set_first_row(std::size_t first);
    void set_max_rows(std::size_t count);
    // Collapse completed children; failed/cancelled rows remain visible.
    void set_retain_completed(bool retain);
    bool needs_animation() const noexcept;
    void on_attached() override;
    void on_resized() override;
    void on_detaching() override;
    void on_effective_visibility_changed() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    // Observable scheduling request; controller binding owns this callback.
    std::function<void()> on_animation_changed;
private:
    void prepare();
    struct Row {
        ProgressTaskId id = 0;
        int depth = 0;
        std::string title, message, percentage, counts, elapsed, rate, eta;
        ProgressTaskState state = ProgressTaskState::Queued;
        std::optional<double> fraction;
        bool parent = false;
        bool child_exception = false;
    };
    ProgressModel* model_;
    std::weak_ptr<void> model_lifetime_;
    ProgressSubscription subscription_;
    std::vector<Row> rows_;
    ProgressViewPresentation presentation_ = ProgressViewPresentation::Compact;
    ProgressPresentation meter_ = ProgressPresentation::Smooth;
    ProgressActivityStyle activity_ = ProgressActivityStyle::Sweep;
    ActivityPresentation spinner_ = ActivityPresentation::Braille;
    ProgressGlyphs glyphs_ = ProgressGlyphs::Unicode;
    ProgressMotion motion_ = ProgressMotion::Animated;
    ProgressColumns columns_;
    ProgressUnitScale scale_ = ProgressUnitScale::None;
    std::size_t first_ = 0, max_rows_ = 100;
    bool retain_completed_ = true;
    bool attached_ = false;
    ui::RoleId track_ = ui::kInvalidRole, fill_ = ui::kInvalidRole, label_ = ui::kInvalidRole;
};

// One one-shot timer for bound displays, using only Application's injected Clock.
// The Application must outlive this controller; destroying a display is safe.
class ProgressController {
public:
    // Bind explicit scheduling services; Application must outlive the controller.
    ProgressController(ui::Application& app, ProgressModel& model);
    // Cancel the timer and release surviving display callbacks.
    ~ProgressController();
    ProgressController(const ProgressController&) = delete;
    ProgressController& operator=(const ProgressController&) = delete;
    // Bind views; no timer when every bound view is hidden/static/inactive.
    void bind(ProgressView& view);
    void bind(ActivityIndicator& indicator);
    // Animate a standalone unknown-total Progress without client timer code.
    void bind(Progress& meter);
    // Positive interval; changes restart the single timer.
    bool set_interval(std::int64_t nanos);
    // Re-evaluate standalone indicator configuration/visibility after changing it.
    void refresh();
private:
    void tick();
    ui::Application& app_;
    ProgressModel* model_;
    std::weak_ptr<void> model_lifetime_;
    ProgressSubscription subscription_;
    ui::Application::TimerId timer_ = 0;
    std::int64_t interval_ = 100'000'000;
    std::shared_ptr<void> lifetime_;
    struct Binding { ui::View* view; std::weak_ptr<void> lifetime; unsigned kind; };
    std::vector<Binding> bindings_;
};

// Bounded latest-value worker bridge; submitting never touches a View/model.
// Construct on the UI thread. Each task has one slot, at most one wakeup pending.
// Model/Application must be alive when submitting; shutdown rejects future submits.
class ProgressMailbox {
public:
    // Create a bounded bridge; Application must outlive active worker submissions.
    ProgressMailbox(ui::Application& app, ProgressModel& model, std::size_t capacity = 64);
    // Stop accepting submissions; posted deliveries become inert.
    ~ProgressMailbox();
    ProgressMailbox(const ProgressMailbox&) = delete;
    ProgressMailbox& operator=(const ProgressMailbox&) = delete;
    // Replace one task's pending completion/total; false on invalid values/full/shutdown.
    bool submit(ProgressTaskId id, double completed, std::optional<double> total);
    // Reject future work; an already posted callback safely becomes inert.
    void shutdown() noexcept;
private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace ckv::widgets
