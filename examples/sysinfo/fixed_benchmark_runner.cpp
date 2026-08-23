// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fixed_benchmark_runner.hpp"

#include <string>

#include "report_format.hpp"

namespace ckv::sysinfo {

FixedBenchmarkRunner::FixedBenchmarkRunner() {
    // Plausible for a machine of this era, and obviously round, so nobody
    // reads a screenshot of this fixture as a measurement of anything.
    const struct {
        BenchmarkId id;
        double rate;
    } scripted[] = {
        {BenchmarkId::IntegerMix, 4.8e8},
        {BenchmarkId::FloatingPoint, 2.4e9},
        {BenchmarkId::MemoryBandwidth, 2.0e10},
    };
    for (const auto& entry : scripted) {
        BenchmarkResult result;
        result.id = entry.id;
        result.rate = entry.rate;
        result.index = entry.rate / unit_rate(entry.id);
        result.index_text = format_decimal(result.index, 1);
        result.rate_text = format_rate(entry.id, entry.rate);
        results.push_back(result);
    }

    // A stair-step with three treads, which is what a machine with three
    // levels of cache looks like from inside a dependent walk.
    BenchmarkResult latency;
    latency.id = BenchmarkId::CacheLatency;
    latency.series_caption = "nanoseconds per dependent access - shorter is better";
    const struct {
        std::uint64_t bytes;
        double nanos;
    } steps[] = {
        {4u * 1024, 1.1},        {16u * 1024, 1.1},        {64u * 1024, 3.2},
        {256u * 1024, 3.4},      {1024u * 1024, 9.0},      {4u * 1024 * 1024, 9.5},
        {16u * 1024 * 1024, 45.0}, {64u * 1024 * 1024, 96.0},
    };
    for (const auto& step : steps)
        latency.series.push_back(
            SeriesPoint{format_bytes(step.bytes), step.nanos, format_decimal(step.nanos, 1) + " ns", 0.0});
    latency.rate_text = "1.1 ns at 4.0 KiB, 96.0 ns at 64.0 MiB";
    results.push_back(latency);

    // Scaling that is nearly perfect to four threads and then is not,
    // which is the shape worth being able to see.
    BenchmarkResult scaling;
    scaling.id = BenchmarkId::ThreadScaling;
    scaling.series_caption = "speedup over one thread - shaded is perfect scaling";
    const struct {
        int threads;
        double speedup;
    } lanes[] = {{1, 1.0}, {2, 1.95}, {4, 3.70}, {8, 6.40}};
    for (const auto& lane : lanes)
        scaling.series.push_back(SeriesPoint{std::to_string(lane.threads) +
                                                 (lane.threads == 1 ? " thread" : " threads"),
                                             lane.speedup, format_decimal(lane.speedup, 2) + "x",
                                             static_cast<double>(lane.threads)});
    scaling.rate_text = "6.40x on 8 threads";
    results.push_back(scaling);
}

BenchmarkResult FixedBenchmarkRunner::run(BenchmarkId id, const std::atomic<bool>& cancelled) const {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++runs_;
        entered_.notify_all();
        gate_.wait(lock, [this] { return !held_; });
        if (cancelled.load(std::memory_order_relaxed)) ++cancelled_runs_;
    }
    for (const BenchmarkResult& result : results)
        if (result.id == id) return result;
    BenchmarkResult empty;
    empty.id = id;
    return empty;
}

void FixedBenchmarkRunner::hold() {
    std::lock_guard<std::mutex> lock(mutex_);
    held_ = true;
}

void FixedBenchmarkRunner::release() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        held_ = false;
    }
    gate_.notify_all();
}

void FixedBenchmarkRunner::wait_until_entered(int count) {
    std::unique_lock<std::mutex> lock(mutex_);
    entered_.wait(lock, [this, count] { return runs_ >= count; });
}

int FixedBenchmarkRunner::runs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runs_;
}

int FixedBenchmarkRunner::cancelled_runs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancelled_runs_;
}

}  // namespace ckv::sysinfo
