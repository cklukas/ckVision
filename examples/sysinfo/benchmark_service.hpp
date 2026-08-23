// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example's boundary between the measuring thread and the
// application's owning thread (the architecture §9), and the reason the
// interface stays alive while a benchmark runs for several seconds.
//
// One worker takes a run of kernels; progress, each result, and the end of
// the run re-enter the application through Application::post, which is the
// one sanctioned crossing. Nothing on the worker touches a View, a Theme
// or a Terminal: it is handed a plan of BenchmarkIds and gives back plain
// values.
//
// One worker on purpose. A benchmark measures a machine, and a second
// thread measuring at the same time would be measuring a machine that is
// busy benchmarking. The multi-threaded scaling run (WP-53) is a
// measurement OF that, made deliberately, inside one kernel.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "cvision/ui/application.hpp"

#include "benchmark.hpp"

namespace ckv::sysinfo {

class BenchmarkService {
public:
    struct Progress {
        std::size_t completed = 0;
        std::size_t total = 0;
        BenchmarkId current = BenchmarkId::IntegerMix;
    };

    // All three run on the owning thread, during a later step().
    using ProgressHandler = std::function<void(Progress)>;
    using ResultHandler = std::function<void(BenchmarkResult)>;
    using FinishedHandler = std::function<void(bool cancelled)>;

    // `app` and `runner` must outlive this service; the destructor cancels
    // any run and joins the worker before returning, which is what makes
    // that a fact rather than a hope.
    BenchmarkService(ui::Application& app, const BenchmarkRunner& runner);
    ~BenchmarkService();

    BenchmarkService(const BenchmarkService&) = delete;
    BenchmarkService& operator=(const BenchmarkService&) = delete;

    // Queues one run. Returns false if a run is already in progress -- two
    // runs at once would measure each other. Callbacks fire only while
    // `subscriber` is still alive (ui::View::lifetime_token), so a window
    // closed mid-run takes its handlers with it.
    bool start(std::vector<BenchmarkId> plan, std::weak_ptr<void> subscriber, ProgressHandler on_progress,
               ResultHandler on_result, FinishedHandler on_finished);

    // Asks the run to stop. Observed between kernels and, for a kernel that
    // looks at it, inside one; the finished handler still fires, with
    // cancelled = true. Safe to call when nothing is running.
    void cancel() noexcept;

    bool running() const noexcept;

    // Blocks until no run is in progress. Every callback is then posted,
    // though not yet delivered: delivery is the owning thread's next
    // step(). For shutdown and for tests, never for the interface.
    void wait_until_idle();

private:
    struct Job {
        std::vector<BenchmarkId> plan;
        std::weak_ptr<void> subscriber;
        ProgressHandler on_progress;
        ResultHandler on_result;
        FinishedHandler on_finished;
    };

    void run_worker();
    void deliver(const std::weak_ptr<void>& subscriber, std::function<void()> work);

    ui::Application& app_;
    const BenchmarkRunner& runner_;

    mutable std::mutex mutex_;
    std::condition_variable queued_;
    std::condition_variable idle_;
    std::vector<Job> pending_;
    bool running_ = false;
    bool stopping_ = false;
    std::atomic<bool> cancelled_{false};
    std::thread worker_;
};

}  // namespace ckv::sysinfo
