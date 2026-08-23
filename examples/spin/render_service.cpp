// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "render_service.hpp"

#include <algorithm>
#include <utility>

namespace ckv::spin {

RenderService::RenderService(ui::Application& app, const MeshLibrary& meshes, unsigned workers)
    : app_(app), meshes_(meshes) {
    workers_.reserve(std::max(1u, workers));
    for (unsigned worker = 0; worker < std::max(1u, workers); ++worker)
        workers_.emplace_back([this] { run_worker(); });
}

RenderService::~RenderService() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    queued_.notify_all();
    idle_.notify_all();
    for (std::thread& worker : workers_) worker.join();
}

unsigned RenderService::default_worker_count() noexcept {
    const unsigned reported = std::thread::hardware_concurrency();
    return std::clamp(reported, 1u, 4u);
}

void RenderService::submit(ShapeId shape, const FrameSpec& spec, std::weak_ptr<void> subscriber,
                           FrameHandler on_frame) {
    if (!on_frame) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        queue_.push_back(Job{shape, spec, std::move(subscriber), std::move(on_frame)});
    }
    queued_.notify_one();
}

void RenderService::wait_until_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return stopping_ || (queue_.empty() && active_ == 0); });
}

std::size_t RenderService::frames_rendered() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_rendered_;
}

void RenderService::run_worker() {
    // One renderer per worker, living as long as the thread does. Its
    // scratch buffers are the reason a worker is a loop rather than a task
    // started per frame: after the first frame at a given size, rendering
    // the next one allocates only the image it hands back.
    Renderer renderer;
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queued_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
            ++active_;
        }

        bool rendered = false;
        // A window closed between asking and being served has nothing to
        // show, and rendering for it would be work nobody will ever see.
        if (!job.subscriber.expired()) {
            auto frame = std::make_shared<const Image>(renderer.render(meshes_.mesh(job.shape), job.spec));
            rendered = true;
            // The frame crosses back into the application here and nowhere
            // else. It is const and freshly allocated, so publishing it is
            // a pointer hand-off rather than shared mutable state.
            app_.post([subscriber = std::move(job.subscriber), handler = std::move(job.on_frame),
                       held_frame = std::move(frame)] {
                if (subscriber.expired()) return;
                handler(held_frame);
            });
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            --active_;
            if (rendered) ++frames_rendered_;
            if (queue_.empty() && active_ == 0) idle_.notify_all();
        }
    }
}

}  // namespace ckv::spin
