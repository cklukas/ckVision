// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress_tasks.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/progress_paint_internal.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <utility>

namespace ckv::widgets {
namespace {
std::string_view state_name(ProgressTaskState state) {
    switch (state) {
    case ProgressTaskState::Queued: return "queued";
    case ProgressTaskState::Running: return "working";
    case ProgressTaskState::Paused: return "paused";
    case ProgressTaskState::Completed: return "done";
    case ProgressTaskState::Failed: return "failed";
    case ProgressTaskState::Cancelled: return "cancelled";
    }
    return {};
}
std::string_view activity_frame(ActivityPresentation presentation, ProgressGlyphs glyphs,
                                ProgressTaskState state, std::int64_t phase, ProgressMotion motion) {
    if (state == ProgressTaskState::Queued) return ".";
    if (state == ProgressTaskState::Paused) return "=";
    if (state == ProgressTaskState::Completed) return glyphs == ProgressGlyphs::Ascii ? "+" : "✓";
    if (state == ProgressTaskState::Failed) return "!";
    if (state == ProgressTaskState::Cancelled) return glyphs == ProgressGlyphs::Ascii ? "x" : "×";
    if (motion == ProgressMotion::Static) return glyphs == ProgressGlyphs::Ascii ? "*" : "•";
    constexpr std::array<std::string_view, 4> ascii{"|", "/", "-", "\\"};
    constexpr std::array<std::string_view, 8> braille{"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧"};
    constexpr std::array<std::string_view, 4> dots{"·", "•", "●", "•"};
    const auto index = [](std::int64_t value, std::size_t size) {
        const auto count = static_cast<std::int64_t>(size);
        return static_cast<std::size_t>((value % count + count) % count);
    };
    if (glyphs == ProgressGlyphs::Ascii || presentation == ActivityPresentation::Ascii) return ascii[index(phase, ascii.size())];
    if (presentation == ActivityPresentation::Dots) return dots[index(phase, dots.size())];
    return braille[index(phase, braille.size())];
}
std::string number(double value, int precision = 0) {
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                    std::chars_format::fixed, precision);
    if (result.ec != std::errc{}) {
        const auto scientific = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                              std::chars_format::scientific, 2);
        return scientific.ec == std::errc{} ? std::string(buffer.data(), scientific.ptr) : "--";
    }
    return {buffer.data(), result.ptr};
}
std::string quantity(double amount, std::string_view unit, ProgressUnitScale scale) {
    constexpr std::array<std::string_view, 7> decimal{"", "k", "M", "G", "T", "P", "E"};
    constexpr std::array<std::string_view, 7> binary{"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};
    const double divisor = scale == ProgressUnitScale::Binary ? 1024 : 1000;
    std::size_t prefix = 0;
    if (scale != ProgressUnitScale::None)
        while (amount >= divisor && prefix + 1 < decimal.size()) { amount /= divisor; ++prefix; }
    std::string out = number(amount, prefix == 0 ? 0 : 1);
    if (!unit.empty() || prefix != 0) {
        out += ' '; out += scale == ProgressUnitScale::Binary ? binary[prefix] : decimal[prefix]; out += unit;
    }
    return out;
}
}

ActivityIndicator::ActivityIndicator() { set_preferred_size(Size{1, 1}); }
void ActivityIndicator::set_presentation(ActivityPresentation presentation) {
    presentation_ = presentation; frames_.clear(); frame_width_ = 1; size_hint_changed(); invalidate();
}
bool ActivityIndicator::set_frames(std::vector<std::string> frames) {
    if (frames.empty()) return false;
    int width = 0;
    for (const auto& frame : frames) {
        if (frame.empty() || frame.find_first_of("\r\n\t\x1b") != std::string::npos || !text::is_sanitized_display_text(frame)) return false;
        const int measured = text::text_width(frame);
        if (measured <= 0 || measured > 32) return false;
        width = std::max(width, measured);
    }
    frame_width_ = width; frames_ = std::move(frames); size_hint_changed(); invalidate(); return true;
}
void ActivityIndicator::set_glyphs(ProgressGlyphs glyphs) { glyphs_ = glyphs; invalidate(); }
void ActivityIndicator::set_state(ProgressTaskState state) {
    state_ = state; invalidate(); if (on_animation_changed) on_animation_changed();
}
void ActivityIndicator::set_motion(ProgressMotion motion) {
    motion_ = motion; invalidate(); if (on_animation_changed) on_animation_changed();
}
void ActivityIndicator::set_phase(std::int64_t phase) { if (phase_ != phase) { phase_ = phase; invalidate(); } }
bool ActivityIndicator::needs_animation() const noexcept {
    return attached_ && visible_in_tree() && state_ == ProgressTaskState::Running && motion_ == ProgressMotion::Animated;
}
void ActivityIndicator::on_attached() {
    attached_ = true; role_ = context().roles->find("ckv.progress.label");
    if (on_animation_changed) on_animation_changed();
}
void ActivityIndicator::on_detaching() { attached_ = false; if (on_animation_changed) on_animation_changed(); }
void ActivityIndicator::on_effective_visibility_changed() { if (on_animation_changed) on_animation_changed(); }
ui::SizeHint ActivityIndicator::horizontal_size_hint() const { return {frame_width_, frame_width_, ui::kUnboundedExtent}; }
ui::SizeHint ActivityIndicator::vertical_size_hint() const { return {1, 1, 1}; }
void ActivityIndicator::draw(scene::Painter& painter) {
    if (!context().valid()) return;
    auto style = context().theme->resolve(role_);
    if (!enabled_in_tree()) style.fg = context().theme->resolve(context().roles->find("ckv.progress.disabled")).fg;
    auto out = painter.clipped(Rect{0, 0, bounds().width, bounds().height});
    out.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    std::string_view frame = activity_frame(presentation_, glyphs_, state_, phase_, motion_);
    if (!frames_.empty() && glyphs_ != ProgressGlyphs::Ascii && motion_ == ProgressMotion::Animated && state_ == ProgressTaskState::Running) {
        const auto size = static_cast<std::int64_t>(frames_.size());
        frame = frames_[static_cast<std::size_t>((phase_ % size + size) % size)];
    }
    out.draw_text(Point{0, 0}, text::clip_to_width_view(frame, bounds().width), style);
}

ProgressView::ProgressView(ProgressModel& model) : model_(&model), model_lifetime_(model.lifetime_token()) {
    subscription_ = model.subscribe([this](bool phase) {
        if (!phase) prepare();
        if (visible_in_tree()) invalidate();
    });
    prepare();
}
ProgressView::~ProgressView() { subscription_.reset(); }
void ProgressView::set_presentation(ProgressViewPresentation presentation) { presentation_ = presentation; prepare(); size_hint_changed(); invalidate(); }
void ProgressView::set_meter_presentation(ProgressPresentation presentation) { meter_ = presentation; invalidate(); }
void ProgressView::set_activity_style(ProgressActivityStyle style) { activity_ = style; invalidate(); }
void ProgressView::set_spinner_presentation(ActivityPresentation presentation) { spinner_ = presentation; invalidate(); }
void ProgressView::set_glyphs(ProgressGlyphs glyphs) { glyphs_ = glyphs; invalidate(); }
void ProgressView::set_motion(ProgressMotion motion) {
    motion_ = motion; invalidate(); if (on_animation_changed) on_animation_changed();
}
void ProgressView::set_columns(ProgressColumns columns) { columns_ = columns; prepare(); invalidate(); if (on_animation_changed) on_animation_changed(); }
void ProgressView::set_unit_scale(ProgressUnitScale scale) { scale_ = scale; prepare(); invalidate(); }
void ProgressView::set_first_row(std::size_t first) { first_ = first; prepare(); invalidate(); }
void ProgressView::set_max_rows(std::size_t count) { max_rows_ = count; prepare(); size_hint_changed(); invalidate(); }
void ProgressView::set_retain_completed(bool retain) { retain_completed_ = retain; prepare(); size_hint_changed(); invalidate(); }
void ProgressView::on_attached() {
    attached_ = true;
    track_ = context().roles->find("ckv.progress.track");
    fill_ = context().roles->find("ckv.progress.fill");
    label_ = context().roles->find("ckv.progress.label");
    prepare();
}
void ProgressView::on_resized() { prepare(); }
void ProgressView::on_detaching() { attached_ = false; if (on_animation_changed) on_animation_changed(); }
void ProgressView::on_effective_visibility_changed() { if (on_animation_changed) on_animation_changed(); }
ui::SizeHint ProgressView::horizontal_size_hint() const { return {1, 60, ui::kUnboundedExtent}; }
ui::SizeHint ProgressView::vertical_size_hint() const {
    const auto count = std::min<std::size_t>(rows_.size(), static_cast<std::size_t>(std::numeric_limits<int>::max() / 2));
    return {1, std::max(1, static_cast<int>(count) * (presentation_ == ProgressViewPresentation::Detailed ? 2 : 1)), ui::kUnboundedExtent};
}
void ProgressView::prepare() {
    const auto previous = rows_.size();
    rows_.clear();
    if (!model_lifetime_.expired()) {
        std::size_t skipped = 0;
        const auto visit = [&](auto&& self, ProgressTaskId parent, int depth) -> void {
            if (rows_.size() >= max_rows_) return;
            for (const auto& task : model_->tasks()) {
                if (rows_.size() >= max_rows_) return;
                if (task.options.parent != parent || !task.visible) continue;
                const bool collapsed = !retain_completed_ && parent != 0 && task.state == ProgressTaskState::Completed;
                if (!collapsed && skipped++ >= first_ && rows_.size() < max_rows_) {
                    const auto metrics = model_->metrics(task.id);
                    Row row;
                    row.id = task.id; row.depth = depth; row.title = task.options.title; row.message = task.message;
                    row.state = task.state; row.fraction = metrics.fraction; row.child_exception = metrics.child_exception;
                    row.parent = std::any_of(model_->tasks().begin(), model_->tasks().end(), [&](const auto& child) { return child.options.parent == task.id; });
                    if (!retain_completed_ && row.parent) {
                        const auto done = std::count_if(model_->tasks().begin(), model_->tasks().end(), [&](const auto& child) {
                            return child.options.parent == task.id && child.visible && child.state == ProgressTaskState::Completed;
                        });
                        if (done > 0) row.title += " / " + std::to_string(done) + " done";
                    }
                    row.percentage = task.state == ProgressTaskState::Running
                        ? (metrics.fraction ? number(*metrics.fraction * 100) + "%" : std::string(state_name(task.state)))
                        : std::string(state_name(task.state));
                    if (row.child_exception) row.percentage += " !";
                    row.counts = quantity(task.completed, task.options.unit, scale_);
                    if (task.options.total) row.counts += "/" + quantity(*task.options.total, task.options.unit, scale_);
                    if (task.options.aggregation == ProgressAggregation::WeightedChildren) {
                        std::size_t total = 0, completed = 0;
                        for (const auto& child : model_->tasks()) if (child.options.parent == task.id) {
                            ++total;
                            if (child.state == ProgressTaskState::Completed) ++completed;
                        }
                        row.counts = std::to_string(completed) + "/" + std::to_string(total) + " tasks";
                    }
                    row.elapsed = number(static_cast<double>(metrics.elapsed_nanos) / 1e9) + " s";
                    row.rate = metrics.rate ? quantity(*metrics.rate, task.options.unit, scale_) + "/s" : "--/s";
                    row.eta = metrics.remaining_seconds ? number(*metrics.remaining_seconds) + " s left" : "-- left";
                    rows_.push_back(std::move(row));
                }
                if (!collapsed) self(self, task.id, depth + 1);
            }
        };
        visit(visit, 0, 0);
    }
    if (previous != rows_.size()) size_hint_changed();
    if (on_animation_changed) on_animation_changed();
}
bool ProgressView::needs_animation() const noexcept {
    if (!attached_ || model_lifetime_.expired() || !visible_in_tree() || motion_ == ProgressMotion::Static) return false;
    const int height = presentation_ == ProgressViewPresentation::Detailed ? 2 : 1;
    const auto visible = std::min(rows_.size(), static_cast<std::size_t>(std::max(0, bounds().height) / height));
    for (std::size_t i = 0; i < visible; ++i)
        if (rows_[i].state == ProgressTaskState::Running && (!rows_[i].fraction || columns_.spinner || columns_.elapsed || columns_.rate || columns_.eta)) return true;
    return false;
}
void ProgressView::draw(scene::Painter& painter) {
    if (!context().valid()) return;
    const int width = bounds().width;
    const int row_height = presentation_ == ProgressViewPresentation::Detailed ? 2 : 1;
    auto out = painter.clipped(Rect{0, 0, width, bounds().height});
    Style label = context().theme->resolve(label_);
    const Style track = context().theme->resolve(track_);
    Style fill = context().theme->resolve(fill_);
    if (!enabled_in_tree()) {
        const auto disabled = context().theme->resolve(context().roles->find("ckv.progress.disabled"));
        label.fg = disabled.fg; fill.bg = disabled.fg;
    }
    out.fill(Rect{0, 0, width, bounds().height}, Cell::from_grapheme(" ", label));
    if (model_lifetime_.expired() || width <= 0) return;
    const auto visible = std::min(rows_.size(), static_cast<std::size_t>(std::max(0, bounds().height) / row_height));
    // Widths are shared by the visible rows; strings were prepared on data updates.
    std::array<int, 6> widths{};
    int title_width = 0;
    for (std::size_t i = 0; i < visible; ++i) {
        const auto& row = rows_[i];
        title_width = std::max(title_width, text::text_width(row.title) + row.depth * 2);
        const std::array<std::string_view, 6> texts{row.percentage, row.counts, row.elapsed, row.rate, row.eta, "*"};
        for (std::size_t c = 0; c < widths.size(); ++c) widths[c] = std::max(widths[c], text::text_width(texts[c]));
    }
    std::array<bool, 6> shown{columns_.percentage, columns_.counts, columns_.elapsed, columns_.rate, columns_.eta, columns_.spinner};
    if (std::any_of(rows_.begin(), rows_.end(), [](const auto& row) { return !row.fraction || row.state != ProgressTaskState::Running || row.child_exception; })) shown[0] = true;
    const auto suffix_width = [&] { int sum = 0; for (std::size_t i = 0; i < shown.size(); ++i) if (shown[i]) sum += widths[i] + 1; return sum; };
    for (const auto index : {3U, 4U, 2U, 1U, 5U}) if (suffix_width() + 12 > width) shown[index] = false;
    if (suffix_width() + 3 > width) shown[0] = false;
    const int title_cells = std::min(title_width, std::max(0, (width - suffix_width()) / 3));
    const int meter_x = title_cells > 0 ? title_cells + 1 : 0;
    const int meter_width = std::max(0, width - suffix_width() - meter_x);
    const auto phase = motion_ == ProgressMotion::Static ? 0 : model_->time() / 100'000'000;
    for (std::size_t i = 0; i < visible; ++i) {
        const auto& row = rows_[i]; const int y = static_cast<int>(i) * row_height;
        Style text_style = label;
        if (row.parent || row.state == ProgressTaskState::Failed || row.child_exception) text_style.attrs |= Attr::Bold;
        if (width < 8) { out.draw_text(Point{0, y}, activity_frame(spinner_, glyphs_, row.state, phase, motion_), text_style); continue; }
        const int indent = std::min(row.depth * 2, title_cells);
        out.draw_text(Point{indent, y}, text::clip_to_width_view(row.title, title_cells - indent), text_style);
        const bool unknown = !row.fraction && row.state == ProgressTaskState::Running;
        detail::paint_progress(out, Rect{meter_x, y, meter_width, 1}, row.fraction.value_or(0), unknown,
                               phase, meter_, motion_ == ProgressMotion::Static ? ProgressActivityStyle::Bounce : activity_, glyphs_, track, fill);
        int x = meter_x + meter_width;
        const std::array<std::string_view, 6> texts{row.percentage, row.counts, row.elapsed, row.rate, row.eta,
            activity_frame(spinner_, glyphs_, row.state, phase, motion_)};
        for (std::size_t c = 0; c < shown.size(); ++c) if (shown[c]) {
            ++x; out.draw_text(Point{x, y}, texts[c], text_style); x += widths[c];
        }
        if (presentation_ == ProgressViewPresentation::Detailed)
            out.draw_text(Point{std::min(indent + 2, width), y + 1}, text::clip_to_width_view(row.message, width - std::min(indent + 2, width)), label);
    }
}

ProgressController::ProgressController(ui::Application& app, ProgressModel& model)
    : app_(app), model_(&model), model_lifetime_(model.lifetime_token()), lifetime_(std::make_shared<int>(0)) {
    model.set_time(app.clock().now_nanos());
    subscription_ = model.subscribe([this](bool) { refresh(); });
}
ProgressController::~ProgressController() {
    lifetime_.reset(); subscription_.reset(); if (timer_ != 0) app_.cancel_timer(timer_);
    for (auto& binding : bindings_) if (!binding.lifetime.expired()) {
        if (binding.kind == 1) static_cast<ActivityIndicator*>(binding.view)->on_animation_changed = {};
        else if (binding.kind == 2) static_cast<Progress*>(binding.view)->on_animation_changed = {};
        else static_cast<ProgressView*>(binding.view)->on_animation_changed = {};
    }
}
void ProgressController::bind(ProgressView& view) {
    if (std::any_of(bindings_.begin(), bindings_.end(), [&](const auto& entry) { return entry.view == &view && !entry.lifetime.expired(); })) return;
    bindings_.push_back({&view, view.lifetime_token(), 0});
    const std::weak_ptr<void> lifetime = lifetime_;
    view.on_animation_changed = [this, lifetime] { if (!lifetime.expired()) refresh(); };
    refresh();
}
void ProgressController::bind(ActivityIndicator& view) {
    if (std::any_of(bindings_.begin(), bindings_.end(), [&](const auto& entry) { return entry.view == &view && !entry.lifetime.expired(); })) return;
    bindings_.push_back({&view, view.lifetime_token(), 1});
    const std::weak_ptr<void> lifetime = lifetime_;
    view.on_animation_changed = [this, lifetime] { if (!lifetime.expired()) refresh(); };
    refresh();
}
void ProgressController::bind(Progress& view) {
    if (std::any_of(bindings_.begin(), bindings_.end(), [&](const auto& entry) { return entry.view == &view && !entry.lifetime.expired(); })) return;
    bindings_.push_back({&view, view.lifetime_token(), 2});
    const std::weak_ptr<void> lifetime = lifetime_;
    view.on_animation_changed = [this, lifetime] { if (!lifetime.expired()) refresh(); };
    refresh();
}
bool ProgressController::set_interval(std::int64_t nanos) {
    if (nanos <= 0) return false;
    interval_ = nanos;
    if (timer_ != 0) { app_.cancel_timer(timer_); timer_ = 0; }
    refresh(); return true;
}
void ProgressController::refresh() {
    std::erase_if(bindings_, [](const auto& binding) { return binding.lifetime.expired(); });
    bool active = false;
    if (!model_lifetime_.expired()) for (const auto& binding : bindings_)
        active |= binding.kind == 1 ? static_cast<ActivityIndicator*>(binding.view)->needs_animation()
                : binding.kind == 2 ? static_cast<Progress*>(binding.view)->needs_animation()
                                    : static_cast<ProgressView*>(binding.view)->needs_animation();
    if (!active && timer_ != 0) { app_.cancel_timer(timer_); timer_ = 0; }
    if (active && timer_ == 0) {
        const std::weak_ptr<void> lifetime = lifetime_;
        timer_ = app_.start_timer(interval_, false, [this, lifetime] { if (!lifetime.expired()) tick(); });
    }
}
void ProgressController::tick() {
    timer_ = 0;
    if (!model_lifetime_.expired()) {
        model_->set_time(app_.clock().now_nanos());
        for (const auto& binding : bindings_) if (!binding.lifetime.expired()) {
            if (binding.kind == 1) static_cast<ActivityIndicator*>(binding.view)->set_phase(model_->time() / 100'000'000);
            if (binding.kind == 2) static_cast<Progress*>(binding.view)->set_pulse(static_cast<int>((model_->time() / 100'000'000) % 1'000'000));
        }
    }
    refresh();
}
} // namespace ckv::widgets
