// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Run interactively on an idle host. DSR completion measures terminal receipt,
// never pixels. The producer and posted-work probes run independently of UI work.
#include "cvision/widgets/log_controller.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/term/log_file_service.hpp"
#include "cvision/term/headless_terminal.hpp"
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include <sys/resource.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace {
using namespace ckv;
constexpr std::int64_t second = 1'000'000'000;
enum class Workload { Short, Long, Unicode, LowMatch, QueryChurn, MultipleSources };
constexpr std::array workloads{Workload::Short, Workload::Long, Workload::Unicode,
    Workload::LowMatch, Workload::QueryChurn, Workload::MultipleSources};
constexpr std::array<std::string_view, 6> workload_names{"short", "long", "unicode", "low-match", "query-churn", "multiple-sources"};
std::string_view workload_name(Workload workload) { return workload_names[static_cast<std::size_t>(workload)]; }
LogLimits workload_limits(Workload workload) {
    LogLimits limits; limits.records = 1024; limits.retained_bytes = 512 * 1024;
    limits.record_bytes = workload == Workload::Long || workload == Workload::QueryChurn ? 4096 : 512;
    return limits;
}
struct Samples {
    std::vector<std::int64_t> values = std::vector<std::int64_t>(8192);
    std::size_t count = 0, excess = 0;
    void add(std::int64_t value) {
        if (value < 0) return;
        if (count < values.size()) values[count++] = value; else ++excess;
    }
    std::int64_t percentile(std::size_t percent) {
        if (!count) return -1;
        std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
        return values[(count - 1) * percent / 100];
    }
};
// One outstanding frame is the bounded policy's contract. Remember the record
// included when write occurs, rather than a newer model state at acknowledgement.
class ReceiptTerminal final : public term::Terminal {
public:
    ReceiptTerminal(term::Terminal& host, const Clock& clock, widgets::LogFilter& filter)
        : host_(host), clock_(clock), filter_(filter), acknowledgements_(host.frame_acknowledgements()) {}
    term::Capabilities capabilities() const noexcept override { return host_.capabilities(); }
    Size size() const noexcept override { return host_.size(); }
    std::size_t frame_acknowledgements() const noexcept override { return host_.frame_acknowledgements(); }
    std::vector<term::TerminalEvent> poll(std::int64_t deadline) override {
        auto events = host_.poll(deadline);
        if (frame_acknowledgements() != acknowledgements_) {
            acknowledgements_ = frame_acknowledgements();
            if (pending_) {
                receipt.add(clock_.now_nanos() - sent_);
                if (arrival_ > 0) source_receipt.add(clock_.now_nanos() - arrival_);
                pending_ = false;
            }
        }
        return events;
    }
    void write(std::string_view bytes) override {
        if (bytes.find("\x1b[5n") != std::string_view::npos) {
            if (pending_) ++overlaps;
            sent_ = clock_.now_nanos(); arrival_ = 0;
            if (filter_.size()) {
                const auto record = filter_.at(filter_.size() - 1);
                if (record && record->timestamp_nanos) arrival_ = *record->timestamp_nanos;
            }
            pending_ = true; ++frames;
        }
        bytes_written += bytes.size(); host_.write(bytes);
    }
    void wake() noexcept override { host_.wake(); }
    // The outer session encloses all four Applications and restores once.
    void restore() noexcept override {}
    void set_title(std::string_view value) override { host_.set_title(value); }
    void bell() override { host_.bell(); }
    ClipboardWriteResult write_clipboard(std::string_view value) override { return host_.write_clipboard(value); }
    Samples receipt, source_receipt;
    std::uint64_t frames = 0, overlaps = 0, bytes_written = 0;
private:
    term::Terminal& host_;
    const Clock& clock_;
    widgets::LogFilter& filter_;
    std::size_t acknowledgements_ = 0;
    std::int64_t sent_ = 0, arrival_ = 0;
    bool pending_ = false;
};
struct Trial {
    int multiplier = 0;
    Workload workload = Workload::Short;
    std::uint64_t offered = 0, rejected = 0, scheduled_missed = 0, dropped = 0, frames = 0;
    std::uint64_t ingested_records = 0, ingested_bytes = 0, output_bytes = 0, refreshes = 0, ingress_deferrals = 0;
    std::uint64_t secondary_ingested = 0, secondary_dropped = 0, query_changes = 0;
    std::size_t retained = 0, high_water = 0, storage = 0;
    Samples dispatch, receipt, source_receipt;
    std::int64_t elapsed = 0;
    double rate = 0;
    std::uint64_t peak_process_bytes = 0;
    std::uint64_t cpu_user_nanos = 0, cpu_system_nanos = 0;
    std::int64_t cpu_interval_nanos = 0;
    std::optional<std::uint64_t> voluntary_switches, involuntary_switches;
    bool cpu_available = false;
    bool bounded = false, cancelled = false;
};
struct CpuUsage {
    std::uint64_t user = 0, system = 0, voluntary = 0, involuntary = 0;
    bool available = false;
};
CpuUsage cpu_usage() {
#if defined(_WIN32)
    FILETIME created{}, exited{}, system{}, user{};
    if (!::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &system, &user)) return {};
    const auto nanos = [](FILETIME value) {
        return ((static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) | value.dwLowDateTime) * 100U;
    };
    return {nanos(user), nanos(system), 0, 0, true};
#else
    rusage usage{}; if (::getrusage(RUSAGE_SELF, &usage) != 0) return {};
    const auto nanos = [](timeval value) { return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000U + static_cast<std::uint64_t>(value.tv_usec) * 1000U; };
    return {nanos(usage.ru_utime), nanos(usage.ru_stime), static_cast<std::uint64_t>(usage.ru_nvcsw), static_cast<std::uint64_t>(usage.ru_nivcsw), true};
#endif
}
struct BackgroundSource {
    explicit BackgroundSource(LogLimits limits, std::shared_ptr<const LogWake> wake)
        : model(limits), filter(model, {16, 4096}), queue(2, 1, {8, limits.record_bytes, LogOverflowPolicy::DropOldest}, std::move(wake)),
          receiver(model, queue, {1024, 16}) {}
    std::size_t allocated_bytes() const {
        return model.allocated_bytes() + filter.allocated_bytes() + queue.allocated_bytes() + receiver.allocated_bytes();
    }
    widgets::LogModel model;
    widgets::LogFilter filter;
    LogMailbox queue;
    widgets::LogStreamReceiver receiver;
};
// Exercises the measurement path in CI without claiming native timing evidence.
// Decode real emitted VT and answer DSR after a controlled 10ms receipt delay.
class SyntheticTerminal final : public term::Terminal {
public:
    explicit SyntheticTerminal(const Clock& clock) : clock_(clock) {}
    term::Capabilities capabilities() const noexcept override { return host_.capabilities(); }
    Size size() const noexcept override { return host_.size(); }
    std::size_t frame_acknowledgements() const noexcept override { return host_.frame_acknowledgements(); }
    std::vector<term::TerminalEvent> poll(std::int64_t deadline) override {
        const auto until = reply_ ? std::min(deadline, *reply_) : deadline;
        {
            std::unique_lock lock(mutex_);
            if (!woken_) condition_.wait_for(lock, std::chrono::nanoseconds(std::max<std::int64_t>(0, until - clock_.now_nanos())), [&] { return woken_; });
            woken_ = false;
        }
        if (reply_ && clock_.now_nanos() >= *reply_) { host_.inject_bytes("\x1b[0n", clock_.now_nanos()); reply_.reset(); }
        return host_.poll(deadline);
    }
    void wake() noexcept override { std::lock_guard lock(mutex_); woken_ = true; condition_.notify_one(); }
    void write(std::string_view bytes) override {
        host_.write(bytes); host_.clear_written();
        if (bytes.find("\x1b[5n") != std::string_view::npos) reply_ = clock_.now_nanos() + 10'000'000;
    }
    void set_title(std::string_view) override {}
    void bell() override {}
    ClipboardWriteResult write_clipboard(std::string_view) override { return {ClipboardWriteStatus::Unsupported}; }
private:
    const Clock& clock_;
    term::HeadlessTerminal host_{{80, 24}};
    std::optional<std::int64_t> reply_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool woken_ = false;
};
Trial run(term::Terminal& native, Clock& clock, double rate, int multiplier,
          Workload workload = Workload::Short, std::int64_t duration = 3 * second) {
    const auto limits = workload_limits(workload);
    widgets::LogModel model(limits); widgets::LogFilter filter(model, {16, 4096});
    ReceiptTerminal terminal(native, clock, filter);
    auto wake = std::make_shared<term::TerminalLogWake>(terminal);
    auto activity = std::make_shared<LogActivity>(wake);
    LogMailbox queue(1, 1, {8, limits.record_bytes, LogOverflowPolicy::DropOldest}, activity);
    widgets::LogStreamReceiver receiver(model, queue, {1024, 16});
    std::unique_ptr<BackgroundSource> background;
    if (workload == Workload::MultipleSources) background = std::make_unique<BackgroundSource>(limits, activity);
    if (workload == Workload::LowMatch) filter.set_include("rare-match");
    ui::Application app(terminal, clock);
    app.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded);
    const auto roles = ui::intern_standard_roles(app.roles()); app.set_theme(ui::make_classic_theme(app.roles(), roles));
    auto* panel = app.root().make<widgets::LogPanel>(filter);
    widgets::LogController controller({2, 1, 1'000'000, multiplier == 0 ? 1 : 33'333'334}, activity.get());
    const auto primary_lane = controller.attach({&model, &filter, nullptr, panel, nullptr, &receiver});
    const auto background_lane = background ? controller.attach({&background->model, &background->filter, nullptr, nullptr, nullptr, &background->receiver}) : 0;
    Trial result; result.multiplier = multiplier; result.rate = rate; result.workload = workload;
    const auto storage = [&] { return model.allocated_bytes() + filter.allocated_bytes() + queue.allocated_bytes() +
        receiver.allocated_bytes() + controller.allocated_bytes() + panel->view().allocated_bytes() +
        (background ? background->allocated_bytes() : 0); };
    result.storage = storage();
    app.commands().declare({.key = "load.quit", .title = "Quit", .chord = "Ctrl+q", .handler = [&] { app.request_quit(); }});
    const auto start = clock.now_nanos(), end = start + duration;
    const auto cpu_before = cpu_usage();
    std::atomic<bool> stop{false};
    // Thread ownership encloses every post and borrow; joining happens after the
    // timed loop, never from a UI callback. Probes are 10/s, not one per record.
    std::jthread producer([&] {
        LogMessage message; message.source = 1; message.epoch = 1;
        const std::string suffix = workload == Workload::Long || workload == Workload::QueryChurn ? std::string(3072, 'x') :
            workload == Workload::Unicode ? " 中 é 🙂 العربية" : "";
        std::array<char, 4096> text{};
        std::int64_t next_probe = start;
        std::uint64_t scheduled = 0;
        while (!stop.load(std::memory_order_acquire)) {
            const auto now = clock.now_nanos();
            if (now >= end) break;
            if (now >= next_probe) {
                app.post([&, now] {
                    result.dispatch.add(clock.now_nanos() - now);
                    if (workload == Workload::QueryChurn) {
                        ++result.query_changes;
                        filter.set_include(result.query_changes % 2 ? "xxxxxx" : "xxxxxx-missing");
                        controller.kick(primary_lane);
                    }
                });
                next_probe = now + second / 10;
            }
            const auto due = static_cast<std::uint64_t>(static_cast<double>(now - start) * rate / static_cast<double>(second));
            const auto count = std::min<std::uint64_t>(due - scheduled, 4096);
            result.scheduled_missed += due - scheduled - count; scheduled = due;
            for (std::uint64_t index = 0; index < count; ++index) {
                constexpr std::string_view prefix = "load record ";
                std::copy(prefix.begin(), prefix.end(), text.begin());
                const auto digits = std::to_chars(text.data() + prefix.size(), text.data() + text.size(), result.offered);
                auto* tail = std::copy(suffix.begin(), suffix.end(), digits.ptr);
                if (workload == Workload::LowMatch && result.offered % 32 == 0) {
                    constexpr std::string_view rare = " rare-match"; tail = std::copy(rare.begin(), rare.end(), tail);
                }
                message.source = 1; message.arrival_nanos = clock.now_nanos();
                message.set_record({text.data(), static_cast<std::size_t>(tail - text.data())},
                                   {LogSeverity::Info, message.arrival_nanos, {}});
                ++result.offered;
                if (queue.offer(message) != LogOfferResult::Accepted) ++result.rejected;
                if (background) {
                    message.source = 2;
                    if (background->queue.offer(message) != LogOfferResult::Accepted) ++result.rejected;
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    while (clock.now_nanos() < end && !app.quit_requested()) {
        const auto now = clock.now_nanos(); controller.step(now);
        const auto deadline = std::min(end, std::min(now + second / 100, controller.next_deadline().value_or(end)));
        app.step(deadline);
    }
    stop.store(true, std::memory_order_release); producer.join();
    result.elapsed = clock.now_nanos() - start;
    result.cancelled = app.quit_requested();
    // Finish the last emitted frame before another Application owns the session.
    const auto drain_end = clock.now_nanos() + second / 2;
    while (app.frames_awaiting_terminal() && clock.now_nanos() < drain_end) app.step(drain_end);
    const auto cpu_after = cpu_usage(); result.cpu_interval_nanos = clock.now_nanos() - start;
    result.cpu_available = cpu_before.available && cpu_after.available;
    if (result.cpu_available) {
        result.cpu_user_nanos = cpu_after.user - cpu_before.user; result.cpu_system_nanos = cpu_after.system - cpu_before.system;
#if !defined(_WIN32)
        result.voluntary_switches = cpu_after.voluntary - cpu_before.voluntary;
        result.involuntary_switches = cpu_after.involuntary - cpu_before.involuntary;
#endif
    }
    LogMailboxStatus status; queue.status(status);
    result.dropped = status.dropped.records; result.high_water = status.high_water_batches;
    result.retained = model.size(); result.frames = terminal.frames;
    result.ingested_records = receiver.status().records_started; result.ingested_bytes = receiver.status().bytes_delivered;
    result.output_bytes = terminal.bytes_written; result.refreshes = controller.status().refreshes;
    result.ingress_deferrals = controller.status().ingress_deferrals;
    result.receipt = terminal.receipt; result.source_receipt = terminal.source_receipt;
    result.bounded = storage() == result.storage && model.size() <= limits.records && status.high_water_batches <= 8 &&
        terminal.overlaps == 0 && app.frame_completion_tracking() && terminal.receipt.count > 0 && !app.frames_awaiting_terminal();
    if (background) {
        LogMailboxStatus other; background->queue.status(other);
        result.secondary_ingested = background->receiver.status().records_started; result.secondary_dropped = other.dropped.records;
        result.bounded = result.bounded && other.high_water_batches <= 8 && background->model.size() <= limits.records && result.secondary_ingested > 0;
    }
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS memory{}; memory.cb = static_cast<DWORD>(sizeof(memory));
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &memory, memory.cb)) result.peak_process_bytes = memory.PeakWorkingSetSize;
#else
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        result.peak_process_bytes = static_cast<std::uint64_t>(usage.ru_maxrss);
#else
        result.peak_process_bytes = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
    }
#endif
    if (background_lane) controller.detach(background_lane);
    controller.detach(primary_lane);
    return result;
}
void samples(std::ostream& output, Samples& values) {
    output << "{\"count\":" << values.count << ",\"excess\":" << values.excess << ",\"p50_ns\":" << values.percentile(50)
           << ",\"p95_ns\":" << values.percentile(95) << ",\"p99_ns\":" << values.percentile(99) << '}';
}
}
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) { std::fprintf(stderr, "usage: cvision_log_terminal_load /absolute/result.json [short|long|unicode|low-match|query-churn|multiple-sources] | --verify-only (native mode is interactive; Ctrl+Q quits)\n"); return 2; }
    try {
        if (std::string_view(argv[1]) == "--verify-only") {
#if defined(_WIN32)
            term::WindowsClock clock;
#else
            term::PosixClock clock;
#endif
            SyntheticTerminal terminal(clock);
            bool valid = true;
            for (const auto workload : workloads) {
                const auto result = run(terminal, clock, 10'000, 100, workload, second / 2);
                std::printf("synthetic %.*s: offered %llu, samples %zu, bounded %d\n", static_cast<int>(workload_name(workload).size()),
                    workload_name(workload).data(), static_cast<unsigned long long>(result.offered), result.receipt.count, result.bounded);
                valid = valid && result.bounded && result.offered > 0 && result.dispatch.count > 0;
            }
            return valid ? 0 : 1;
        }
        Workload selected = Workload::Short;
        if (argc == 3) {
            const auto requested = std::string_view(argv[2]);
            const auto found = std::find(workload_names.begin(), workload_names.end(), requested);
            if (found == workload_names.end()) return 2;
            selected = workloads[static_cast<std::size_t>(found - workload_names.begin())];
        }
        std::array<Trial, 4> trials;
        {
#if defined(_WIN32)
            term::WindowsClock clock; term::WindowsTerminal native(clock);
#else
            term::PosixClock clock; term::PosixTerminal native(clock);
#endif
            // A distinct rapidly paced calibration uses the same scene/receipt
            // marker, with one record per frame opportunity. No assumed FPS.
            trials[0] = run(native, clock, 1000, 0);
            if (trials[0].cancelled) return 1;
            if (trials[0].bounded) {
                const auto baseline = static_cast<double>(trials[0].receipt.count) * static_cast<double>(second) /
                                      static_cast<double>(trials[0].elapsed);
                std::size_t index = 1;
                for (const auto multiplier : {1, 10, 100}) {
                    trials[index] = run(native, clock, baseline * multiplier, multiplier, selected);
                    if (trials[index++].cancelled) return 1;
                }
            }
        }
        std::ofstream output(argv[1], std::ios::out | std::ios::trunc); if (!output) return 2;
        output << "{\"schema\":1,\"completion\":\"DSR terminal receipt; physical pixels not measured\",\"calibration\":\"observed acknowledged frames/s, producer ceiling 1000 records/s, UI yielding 1ms\",\"input\":\"worker-post to UI dispatch; keyboard transport not measured\",\"memory\":\"process-lifetime peak RSS/working set includes all preceding trials\",\"trials\":[";
        bool valid = true;
        for (std::size_t index = 0; index < trials.size(); ++index) {
            auto& trial = trials[index]; if (index) output << ',';
            valid = valid && trial.bounded;
            output << "{\"multiplier\":" << trial.multiplier << ",\"producer_records_per_second\":" << trial.rate
                   << ",\"workload\":\"" << workload_name(trial.workload) << "\""
                   << ",\"elapsed_ns\":" << trial.elapsed << ",\"offered\":" << trial.offered << ",\"rejected\":" << trial.rejected
                   << ",\"scheduled_missed\":" << trial.scheduled_missed << ",\"dropped_records\":" << trial.dropped
                   << ",\"ingested_records\":" << trial.ingested_records << ",\"ingested_bytes\":" << trial.ingested_bytes
                   << ",\"output_bytes\":" << trial.output_bytes << ",\"refreshes\":" << trial.refreshes
                   << ",\"ingress_deferrals\":" << trial.ingress_deferrals
                   << ",\"secondary_ingested\":" << trial.secondary_ingested << ",\"secondary_dropped\":" << trial.secondary_dropped
                   << ",\"query_changes\":" << trial.query_changes
                   << ",\"queue_high_water\":" << trial.high_water << ",\"retained\":" << trial.retained
                   << ",\"reserved_library_bytes\":" << trial.storage << ",\"frames\":" << trial.frames
                   << ",\"peak_process_bytes\":" << trial.peak_process_bytes
                   << ",\"cpu_available\":" << (trial.cpu_available ? "true" : "false")
                   << ",\"cpu_interval_ns\":" << trial.cpu_interval_nanos
                   << ",\"cpu_user_ns\":" << trial.cpu_user_nanos << ",\"cpu_system_ns\":" << trial.cpu_system_nanos
                   << ",\"voluntary_switches\":" << (trial.voluntary_switches ? std::to_string(*trial.voluntary_switches) : "null")
                   << ",\"involuntary_switches\":" << (trial.involuntary_switches ? std::to_string(*trial.involuntary_switches) : "null")
                   << ",\"bounded\":" << (trial.bounded ? "true" : "false") << ",\"dispatch\":";
            samples(output, trial.dispatch); output << ",\"frame_receipt\":"; samples(output, trial.receipt);
            output << ",\"latest_record_receipt\":"; samples(output, trial.source_receipt); output << '}';
        }
        output << "]}"; output.close(); return valid && output ? 0 : 1;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
