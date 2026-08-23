// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "benchmark_service.hpp"

#include <utility>

namespace ckv::sysinfo {

BenchmarkService::BenchmarkService(ui::Application& app, const BenchmarkRunner& runner)
    : app_(app), runner_(runner), worker_([this] { run_worker(); }) {}

BenchmarkService::~BenchmarkService() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cancelled_.store(true, std::memory_order_relaxed);
    queued_.notify_all();
    idle_.notify_all();
    worker_.join();
}

bool BenchmarkService::start(std::vector<BenchmarkId> plan, std::weak_ptr<void> subscriber,
                             ProgressHandler on_progress, ResultHandler on_result,
                             FinishedHandler on_finished) {
    if (plan.empty()) return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || running_ || !pending_.empty()) return false;
        pending_.push_back(Job{std::move(plan), std::move(subscriber), std::move(on_progress),
                               std::move(on_result), std::move(on_finished)});
        running_ = true;
    }
    cancelled_.store(false, std::memory_order_relaxed);
    queued_.notify_one();
    return true;
}

void BenchmarkService::cancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }

bool BenchmarkService::running() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void BenchmarkService::wait_until_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return stopping_ || !running_; });
}

void BenchmarkService::deliver(const std::weak_ptr<void>& subscriber, std::function<void()> work) {
    // The crossing, and the only one. A window closed between the
    // measurement and its delivery has nothing to show, and its handler is
    // not called at all rather than called on a destroyed view.
    app_.post([subscriber, held_work = std::move(work)] {
        if (subscriber.expired()) return;
        held_work();
    });
}

void BenchmarkService::run_worker() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queued_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
            if (stopping_) return;
            job = std::move(pending_.front());
            pending_.clear();
        }

        const std::size_t total = job.plan.size();
        std::size_t completed = 0;
        bool cancelled = false;
        for (const BenchmarkId id : job.plan) {
            if (cancelled_.load(std::memory_order_relaxed)) {
                cancelled = true;
                break;
            }
            if (job.on_progress)
                deliver(job.subscriber, [handler = job.on_progress, progress = Progress{completed, total, id}] {
                    handler(progress);
                });

            const BenchmarkResult result = runner_.run(id, cancelled_);

            // A cancelled kernel returns whatever it had, which is not a
            // measurement of anything; it is dropped rather than charted.
            if (cancelled_.load(std::memory_order_relaxed)) {
                cancelled = true;
                break;
            }
            ++completed;
            if (job.on_result)
                deliver(job.subscriber, [handler = job.on_result, held_result = result] { handler(held_result); });
            if (job.on_progress)
                deliver(job.subscriber, [handler = job.on_progress, progress = Progress{completed, total, id}] {
                    handler(progress);
                });
        }

        if (job.on_finished)
            deliver(job.subscriber, [handler = job.on_finished, cancelled] { handler(cancelled); });

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        idle_.notify_all();
    }
}

}  // namespace ckv::sysinfo
