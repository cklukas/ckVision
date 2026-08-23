// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Spin example's boundary between the render threads and the
// application's owning thread (the architecture §9): worker threads take
// frame requests, and finished frames re-enter the application through
// `Application::post` — the one sanctioned crossing. Nothing on a worker
// thread ever touches a View, a Theme, or a Terminal; a request carries
// everything the renderer needs, and the answer comes back as an Image
// that nothing else has a reference to yet.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "cvision/core/image.hpp"
#include "cvision/ui/application.hpp"

#include "mesh.hpp"
#include "renderer.hpp"

namespace ckv::spin {

class RenderService {
public:
    // Runs on the owning thread with the finished frame.
    using FrameHandler = std::function<void(std::shared_ptr<const Image>)>;

    // `app` and `meshes` must outlive this service; its destructor joins
    // every worker before returning, which is what makes that a fact
    // rather than a hope. Declare a RenderService after everything its
    // workers read, and destruction order does the rest.
    RenderService(ui::Application& app, const MeshLibrary& meshes, unsigned workers);
    ~RenderService();

    RenderService(const RenderService&) = delete;
    RenderService& operator=(const RenderService&) = delete;

    // A small pool: one frame per window at a walking pace is not a
    // throughput problem, and leaving cores free is the point of moving
    // the work off the owning thread in the first place.
    static unsigned default_worker_count() noexcept;

    // Queues one frame. `on_frame` runs on the owning thread during a
    // later step(), and only while `subscriber` is still alive — the
    // idiom Application itself uses to hold a View across a callback
    // (ui::View::lifetime_token).
    //
    // The service imposes no queue limit of its own. Callers rate-limit
    // themselves by keeping at most one request outstanding, so the queue
    // is bounded by the number of animated views whatever the host's
    // speed; a service that instead accepted everything and dropped the
    // stale ones would be doing the work twice to throw half of it away.
    void submit(ShapeId shape, const FrameSpec& spec, std::weak_ptr<void> subscriber,
                FrameHandler on_frame);

    // Blocks until nothing is queued or in progress. Every frame is then
    // posted, though not yet delivered: delivery is the owning thread's
    // next step(). For shutdown and for tests, never for a frame loop.
    void wait_until_idle();

    std::size_t frames_rendered() const noexcept;
    std::size_t worker_count() const noexcept { return workers_.size(); }

private:
    struct Job {
        ShapeId shape = ShapeId::WireCube;
        FrameSpec spec;
        std::weak_ptr<void> subscriber;
        FrameHandler on_frame;
    };

    void run_worker();

    ui::Application& app_;
    const MeshLibrary& meshes_;

    mutable std::mutex mutex_;
    std::condition_variable queued_;
    std::condition_variable idle_;
    std::deque<Job> queue_;
    std::size_t active_ = 0;
    std::size_t frames_rendered_ = 0;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

}  // namespace ckv::spin
