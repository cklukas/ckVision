// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fixed_benchmark_runner.hpp"

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
        switch (entry.id) {
            case BenchmarkId::IntegerMix:
                result.rate_text = format_decimal(entry.rate / 1e6, 1) + " M steps/s";
                break;
            case BenchmarkId::FloatingPoint:
                result.rate_text = format_decimal(entry.rate / 1e6, 0) + " MFLOPS";
                break;
            case BenchmarkId::MemoryBandwidth:
                result.rate_text = format_decimal(entry.rate / 1e9, 2) + " GB/s";
                break;
        }
        results.push_back(result);
    }
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
