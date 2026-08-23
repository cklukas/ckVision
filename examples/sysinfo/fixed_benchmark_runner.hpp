// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A scripted run, for the tests and the generated screenshots — the
// benchmark counterpart of FixedSystemProbe. It returns the same numbers
// on every machine and returns them at once, so a suite can assert what
// the chart says without measuring anything, and a screenshot of this
// application shows the same bars on every host.
//
// It also carries the gate that makes cancellation testable: a run held at
// a known point can be cancelled at a known point, which is the difference
// between a test that proves cancellation and a test that sleeps and
// hopes.
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "benchmark.hpp"

namespace ckv::sysinfo {

class FixedBenchmarkRunner final : public BenchmarkRunner {
public:
    FixedBenchmarkRunner();

    // Scripted, in catalogue order, and public so a test can vary one.
    std::vector<BenchmarkResult> results;

    BenchmarkResult run(BenchmarkId id, const std::atomic<bool>& cancelled) const override;

    // Makes every subsequent run() block on entry until release().
    void hold();
    void release();

    // Blocks until run() has been entered `count` times. With hold() in
    // force, returning means the worker is parked inside the kernel and
    // will still be there when the caller acts.
    void wait_until_entered(int count);

    int runs() const;
    // How many runs saw the cancel flag already set when they were let go.
    int cancelled_runs() const;

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable entered_;
    mutable std::condition_variable gate_;
    bool held_ = false;
    mutable int runs_ = 0;
    mutable int cancelled_runs_ = 0;
};

}  // namespace ckv::sysinfo
