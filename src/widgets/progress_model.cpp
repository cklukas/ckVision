// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress_tasks.hpp"
#include <algorithm>
#include <cmath>
#include <list>
#include <limits>
#include <mutex>
#include <utility>

namespace ckv::widgets {
namespace detail {
struct ProgressObservers {
    struct Entry { std::uint64_t id; std::function<void(bool)> callback; bool active = true; };
    std::list<Entry> entries;
    std::uint64_t next = 1;
    bool notifying = false;
    void notify(bool phase) {
        if (notifying) return;
        notifying = true;
        const auto boundary = next;
        for (auto& entry : entries)
            if (entry.active && entry.id < boundary) entry.callback(phase);
        notifying = false;
        entries.remove_if([](const Entry& entry) { return !entry.active; });
    }
};
}

ProgressSubscription::~ProgressSubscription() { reset(); }
ProgressSubscription::ProgressSubscription(ProgressSubscription&& other) noexcept
    : observers_(std::move(other.observers_)), id_(std::exchange(other.id_, 0)) {}
ProgressSubscription& ProgressSubscription::operator=(ProgressSubscription&& other) noexcept {
    if (this != &other) { reset(); observers_ = std::move(other.observers_); id_ = std::exchange(other.id_, 0); }
    return *this;
}
void ProgressSubscription::reset() noexcept {
    if (auto observers = observers_.lock()) {
        for (auto& entry : observers->entries) if (entry.id == id_) entry.active = false;
        if (!observers->notifying)
            observers->entries.remove_if([](const auto& entry) { return !entry.active; });
    }
    id_ = 0;
    observers_.reset();
}

ProgressModel::ProgressModel() : observers_(std::make_shared<detail::ProgressObservers>()),
    lifetime_(std::make_shared<int>(0)) {}
ProgressModel::~ProgressModel() { lifetime_.reset(); observers_->notify(false); }
ProgressSubscription ProgressModel::subscribe(std::function<void(bool)> observer) {
    ProgressSubscription result;
    if (!observer) return result;
    result.observers_ = observers_;
    result.id_ = observers_->next++;
    observers_->entries.push_back({result.id_, std::move(observer), true});
    return result;
}
void ProgressModel::changed(bool phase_only) {
    if (batch_ != 0) { pending_ = true; return; }
    observers_->notify(phase_only);
}
void ProgressModel::end_update() {
    if (batch_ == 0) return;
    if (--batch_ == 0 && std::exchange(pending_, false)) changed();
}
const ProgressTask* ProgressModel::find(ProgressTaskId id) const noexcept {
    const auto found = std::find_if(tasks_.begin(), tasks_.end(), [id](const auto& task) { return task.id == id; });
    return found == tasks_.end() ? nullptr : &*found;
}
ProgressTask* ProgressModel::mutable_task(ProgressTaskId id) noexcept {
    return const_cast<ProgressTask*>(std::as_const(*this).find(id));
}
namespace {
bool valid_total(std::optional<double> total) { return !total || (std::isfinite(*total) && *total >= 0); }
bool terminal(ProgressTaskState state) {
    return state == ProgressTaskState::Completed || state == ProgressTaskState::Failed || state == ProgressTaskState::Cancelled;
}
}
ProgressTaskId ProgressModel::add_task(ProgressTaskOptions options) {
    if (!valid_total(options.total) || !std::isfinite(options.weight) || options.weight <= 0 ||
        next_id_ == std::numeric_limits<ProgressTaskId>::max()) return 0;
    int depth = 0;
    for (auto id = options.parent; id != 0;) {
        const auto* parent = find(id);
        if (!parent || ++depth >= 64) return 0;
        id = parent->options.parent;
    }
    if (options.parent != 0) {
        const auto* parent = find(options.parent);
        if (!parent || parent->children_finalized) return 0;
    }
    ProgressTask task;
    task.id = next_id_++;
    task.options = std::move(options);
    tasks_.push_back(std::move(task));
    changed();
    return tasks_.back().id;
}
bool ProgressModel::remove(ProgressTaskId id) {
    const auto* task = find(id);
    if (!task) return false;
    if (const auto* parent = find(task->options.parent); parent && parent->children_finalized) return false;
    std::vector<ProgressTaskId> removed{id};
    for (std::size_t i = 0; i < removed.size(); ++i)
        for (const auto& child : tasks_) if (child.options.parent == removed[i]) removed.push_back(child.id);
    std::erase_if(tasks_, [&](const auto& item) { return std::find(removed.begin(), removed.end(), item.id) != removed.end(); });
    changed();
    return true;
}
bool ProgressModel::set_parent(ProgressTaskId id, ProgressTaskId parent) {
    auto* task = mutable_task(id);
    if (!task || id == parent || (parent != 0 && !find(parent))) return false;
    if (task->options.parent == parent) return true;
    int parent_depth = 0;
    for (auto current = parent; current != 0;) {
        ++parent_depth;
        if (parent_depth > 64) return false;
        if (current == id) return false;
        current = find(current)->options.parent;
    }
    for (const auto& candidate : tasks_) {
        int depth = 0;
        for (auto current = candidate.id; current != 0; current = find(current)->options.parent) {
            if (current == id && depth + parent_depth >= 64) return false;
            ++depth;
        }
    }
    const auto* old = find(task->options.parent);
    const auto* next = find(parent);
    if ((old && old->children_finalized) || (next && next->children_finalized)) return false;
    task->options.parent = parent;
    changed();
    return true;
}
bool ProgressModel::set_title(ProgressTaskId id, std::string title) {
    auto* task = mutable_task(id); if (!task) return false;
    if (task->options.title != title) { task->options.title = std::move(title); changed(); }
    return true;
}
bool ProgressModel::set_message(ProgressTaskId id, std::string message) {
    auto* task = mutable_task(id); if (!task) return false;
    if (task->message != message) { task->message = std::move(message); changed(); }
    return true;
}
bool ProgressModel::set_unit(ProgressTaskId id, std::string unit) {
    auto* task = mutable_task(id); if (!task) return false;
    if (task->options.unit != unit) { task->options.unit = std::move(unit); changed(); }
    return true;
}
bool ProgressModel::set_total(ProgressTaskId id, std::optional<double> total) {
    auto* task = mutable_task(id); if (!task || !valid_total(total)) return false;
    if (task->options.total != total) { task->options.total = total; changed(); }
    return true;
}
void ProgressModel::sample(ProgressTask& task) {
    const auto elapsed = task.elapsed_nanos + (task.state == ProgressTaskState::Running ? now_ - task.running_since : 0);
    if (task.sample_count != 0 && task.samples[task.sample_count - 1].time == elapsed) {
        task.samples[task.sample_count - 1].amount = task.completed;
        return;
    }
    if (task.sample_count == task.samples.size()) {
        std::move(task.samples.begin() + 1, task.samples.end(), task.samples.begin());
        --task.sample_count;
    }
    task.samples[task.sample_count++] = {elapsed, task.completed};
}
bool ProgressModel::set_completed(ProgressTaskId id, double completed) {
    auto* task = mutable_task(id);
    if (!task || !std::isfinite(completed) || completed < 0) return false;
    if (task->completed == completed) return true;
    if (completed < task->completed) { task->sample_count = 0; task->elapsed_nanos = 0; task->running_since = now_; }
    task->completed = completed;
    task->last_advance = now_;
    if (task->state == ProgressTaskState::Running) sample(*task);
    changed();
    return true;
}
bool ProgressModel::advance(ProgressTaskId id, double amount) {
    const auto* task = find(id);
    return task && std::isfinite(amount) && amount >= 0 && set_completed(id, task->completed + amount);
}
bool ProgressModel::set_weight(ProgressTaskId id, double weight) {
    auto* task = mutable_task(id);
    if (!task || !std::isfinite(weight) || weight <= 0) return false;
    if (const auto* parent = find(task->options.parent); parent && parent->children_finalized) return false;
    task->options.weight = weight; changed(); return true;
}
bool ProgressModel::set_aggregation(ProgressTaskId id, ProgressAggregation aggregation) {
    auto* task = mutable_task(id); if (!task) return false;
    task->options.aggregation = aggregation; changed(); return true;
}
bool ProgressModel::finalize_children(ProgressTaskId id) {
    auto* task = mutable_task(id); if (!task) return false;
    task->children_finalized = true; changed(); return true;
}
bool ProgressModel::set_visible(ProgressTaskId id, bool visible) {
    auto* task = mutable_task(id); if (!task) return false;
    task->visible = visible; changed(); return true;
}
bool ProgressModel::transition(ProgressTaskId id, ProgressTaskState state) {
    auto* task = mutable_task(id);
    if (!task || terminal(task->state)) return false;
    if (state == task->state) return true;
    if (state == ProgressTaskState::Paused && task->state != ProgressTaskState::Running) return false;
    if (task->state == ProgressTaskState::Running) task->elapsed_nanos += now_ - task->running_since;
    task->state = state;
    if (state == ProgressTaskState::Running) {
        task->running_since = now_; task->sample_count = 0; task->last_advance = now_; sample(*task);
    }
    changed(); return true;
}
bool ProgressModel::start(ProgressTaskId id) { return transition(id, ProgressTaskState::Running); }
bool ProgressModel::pause(ProgressTaskId id) { return transition(id, ProgressTaskState::Paused); }
bool ProgressModel::complete(ProgressTaskId id) { return transition(id, ProgressTaskState::Completed); }
bool ProgressModel::fail(ProgressTaskId id) { return transition(id, ProgressTaskState::Failed); }
bool ProgressModel::cancel(ProgressTaskId id) { return transition(id, ProgressTaskState::Cancelled); }
bool ProgressModel::reset(ProgressTaskId id) {
    auto* task = mutable_task(id); if (!task) return false;
    task->state = ProgressTaskState::Queued; task->completed = 0; task->elapsed_nanos = 0;
    task->running_since = now_; task->last_advance = now_; task->sample_count = 0;
    changed(); return true;
}
void ProgressModel::set_time(std::int64_t now) {
    if (now <= now_) return;
    const bool second_changed = now / 1'000'000'000 != now_ / 1'000'000'000;
    const bool rate_expired = std::any_of(tasks_.begin(), tasks_.end(), [&](const auto& task) {
        return task.state == ProgressTaskState::Running && task.options.aggregation == ProgressAggregation::Manual && task.sample_count >= 2 &&
               now_ - task.last_advance < 3'000'000'000 && now - task.last_advance >= 3'000'000'000;
    });
    now_ = now;
    changed(!second_changed && !rate_expired);
}
ProgressMetrics ProgressModel::metrics(ProgressTaskId id) const {
    ProgressMetrics result;
    const auto* task = find(id); if (!task) return result;
    result.elapsed_nanos = task->elapsed_nanos + (task->state == ProgressTaskState::Running ? now_ - task->running_since : 0);
    if (task->state == ProgressTaskState::Completed) result.fraction = 1;
    else if (task->options.aggregation == ProgressAggregation::WeightedChildren) {
        // Normalize before multiplication to avoid overflow with very large weights.
        double largest = 0;
        for (const auto& child : tasks_) if (child.options.parent == id) largest = std::max(largest, child.options.weight);
        double weights = 0, known = 0; bool unknown = false;
        for (const auto& child : tasks_) if (child.options.parent == id) {
            const auto child_metrics = metrics(child.id);
            const double weight = child.options.weight / largest;
            weights += weight;
            known += weight * child_metrics.fraction.value_or(child_metrics.known_lower_bound);
            unknown |= !child_metrics.fraction.has_value();
            result.child_exception |= child_metrics.child_exception || child.state == ProgressTaskState::Failed || child.state == ProgressTaskState::Cancelled;
        }
        if (weights > 0) { result.known_lower_bound = known / weights; if (!unknown) result.fraction = result.known_lower_bound; }
    } else if (task->options.total) result.fraction = *task->options.total == 0 ? 0 : std::clamp(task->completed / *task->options.total, 0.0, 1.0);
    if (task->options.aggregation != ProgressAggregation::WeightedChildren || task->state == ProgressTaskState::Completed)
        for (const auto& child : tasks_) if (child.options.parent == id) {
            const auto child_metrics = metrics(child.id);
            result.child_exception |= child_metrics.child_exception || child.state == ProgressTaskState::Failed || child.state == ProgressTaskState::Cancelled;
        }
    if (result.fraction) result.known_lower_bound = *result.fraction;
    if (task->state == ProgressTaskState::Running && task->sample_count >= 2 && now_ - task->last_advance < 3'000'000'000 &&
        task->options.aggregation == ProgressAggregation::Manual) {
        const auto first = task->samples[0]; const auto last = task->samples[task->sample_count - 1];
        const double duration = static_cast<double>(last.time - first.time) / 1e9;
        const double amount = last.amount - first.amount;
        if (duration > 0 && amount > 0) {
            const double rate = amount / duration;
            if (std::isfinite(rate) && rate > 0) {
                result.rate = rate;
                if (task->options.total) {
                    const double eta = std::max(0.0, *task->options.total - task->completed) / rate;
                    if (std::isfinite(eta)) result.remaining_seconds = eta;
                }
            }
        }
    }
    return result;
}

struct ProgressMailbox::State {
    struct Update { ProgressTaskId id; double completed; std::optional<double> total; };
    std::mutex mutex;
    std::vector<Update> updates, draining;
    std::size_t capacity;
    ui::Application* app;
    ProgressModel* model;
    std::weak_ptr<void> lifetime;
    bool stopped = false, posted = false;
};
ProgressMailbox::ProgressMailbox(ui::Application& app, ProgressModel& model, std::size_t capacity)
    : state_(std::make_shared<State>()) {
    state_->capacity = capacity; state_->updates.reserve(capacity); state_->draining.reserve(capacity);
    state_->app = &app; state_->model = &model; state_->lifetime = model.lifetime_token();
}
ProgressMailbox::~ProgressMailbox() { shutdown(); }
void ProgressMailbox::shutdown() noexcept {
    std::lock_guard lock(state_->mutex); state_->stopped = true; state_->updates.clear();
}
bool ProgressMailbox::submit(ProgressTaskId id, double completed, std::optional<double> total) {
    if (id == 0 || !std::isfinite(completed) || completed < 0 || !valid_total(total)) return false;
    const auto state = state_;
    std::lock_guard lock(state->mutex);
    if (state->stopped || state->lifetime.expired()) return false;
    auto item = std::find_if(state->updates.begin(), state->updates.end(), [id](const auto& update) { return update.id == id; });
    if (item == state->updates.end()) {
        if (state->updates.size() >= state->capacity) return false;
        state->updates.push_back({id, completed, total});
    } else *item = {id, completed, total};
    if (!state->posted) {
        state->posted = true;
        state->app->post([state] {
            {
                std::lock_guard apply_lock(state->mutex);
                state->posted = false;
                if (state->stopped || state->lifetime.expired()) { state->updates.clear(); return; }
                state->updates.swap(state->draining);
            }
            state->model->begin_update();
            state->model->set_time(state->app->clock().now_nanos());
            for (const auto& update : state->draining) {
                state->model->set_total(update.id, update.total);
                state->model->set_completed(update.id, update.completed);
            }
            state->draining.clear();
            state->model->end_update();
        });
    }
    return true;
}
} // namespace ckv::widgets
