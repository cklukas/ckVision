// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_stream.hpp"
#include "cvision/term/process_pump_internal.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ckv::term {
namespace {
using namespace core;
using namespace detail;
class Pulse final : public LogWake {
public:
    Pulse() {
#if defined(_WIN32)
        event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!event_) throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "process stream wake");
#else
        int pair[2];
#if defined(__linux__)
        if (::pipe2(pair, O_NONBLOCK | O_CLOEXEC) != 0)
#else
        if (::pipe(pair) != 0)
#endif
            throw std::system_error(errno, std::generic_category(), "process stream wake");
        reader_ = pair[0]; writer_ = pair[1];
        for (int descriptor : pair) {
            const int flags = ::fcntl(descriptor, F_GETFL);
            if (flags < 0 || ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0 ||
                ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
                const int error = errno; ::close(reader_); ::close(writer_);
                throw std::system_error(error, std::generic_category(), "process stream wake flags");
            }
        }
#endif
    }
    ~Pulse() override {
#if defined(_WIN32)
        ::CloseHandle(event_);
#else
        ::close(reader_); ::close(writer_);
#endif
    }
    void notify() const noexcept override {
#if defined(_WIN32)
        (void)::SetEvent(event_);
#else
        if (!armed_.exchange(true, std::memory_order_acq_rel)) {
            const char byte = 0;
            while (::write(writer_, &byte, 1) < 0 && errno == EINTR) {}
        }
#endif
    }
    void consume() noexcept {
#if defined(_WIN32)
        (void)::ResetEvent(event_);
#else
        char byte;
        while (::read(reader_, &byte, 1) < 0 && errno == EINTR) {}
        armed_.store(false, std::memory_order_release);
#endif
    }
    WaitHandle handle() const noexcept {
#if defined(_WIN32)
        return {WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(event_)};
#else
        return {WaitHandleKind::PosixFileDescriptor, static_cast<std::uintptr_t>(reader_)};
#endif
    }
private:
#if defined(_WIN32)
    HANDLE event_ = nullptr;
#else
    int reader_ = -1, writer_ = -1;
    mutable std::atomic<bool> armed_{false};
#endif
};
void increment(std::uint64_t& value, std::uint64_t count = 1) noexcept {
    value += std::min(count, std::numeric_limits<std::uint64_t>::max() - value);
}
struct StreamState final : ProcessPumpObserver {
    const Clock& clock;
    ProcessStreamRequest request;
    std::shared_ptr<Pulse> pulse;
    std::shared_ptr<const LogWake> work_wake;
    LogMailbox out, err;
    std::array<LogMessage, 2> packets;
    std::array<bool, 2> held{};
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> publication_failed{false};
    mutable std::mutex status_mutex;
    ProcessStreamStatus current;
    StreamState(const Clock& time, ProcessStreamRequest data, std::shared_ptr<Pulse> signal,
                std::shared_ptr<const LogWake> wake)
        : clock(time), request(std::move(data)), pulse(std::move(signal)), work_wake(std::move(wake)),
          out(request.stdout_source, request.epoch, request.stdout_queue, work_wake, pulse),
          err(request.stderr_source, request.epoch, request.stderr_queue, work_wake, pulse) {}
    bool cancelled() const noexcept override { return cancel_requested.load(std::memory_order_acquire); }
    void cancel() noexcept { cancel_requested.store(true, std::memory_order_release); pulse->notify(); }
    std::size_t chunk_bytes() const noexcept override { return request.chunk_bytes; }
    WaitHandle wake_handle() const noexcept override { return pulse->handle(); }
    void consume_wake() noexcept override { pulse->consume(); }
    PipePublication publish(ProcessPipe pipe, std::string_view bytes) noexcept override {
        if (cancelled()) return PipePublication::Cancelled;
        const auto index = pipe == ProcessPipe::Stdout ? 0U : 1U;
        auto& packet = packets[index];
        if (!held[index]) {
            packet.source = index == 0 ? request.stdout_source : request.stderr_source;
            packet.epoch = request.epoch; packet.arrival_nanos = clock.now_nanos();
            if (!packet.set_bytes(bytes)) { publication_failed = true; cancel(); return PipePublication::Cancelled; }
            held[index] = true;
            { std::lock_guard lock(status_mutex);
              increment(index == 0 ? current.stdout_bytes : current.stderr_bytes, bytes.size());
              (index == 0 ? current.stdout_pending_bytes : current.stderr_pending_bytes) = bytes.size();
              increment(current.revision); }
        }
        const auto offered = (index == 0 ? out : err).offer(packet);
        if (offered == LogOfferResult::Busy) return PipePublication::Busy;
        if (offered == LogOfferResult::WouldBlock) return PipePublication::Retry;
        if (offered != LogOfferResult::Accepted && offered != LogOfferResult::Dropped) {
            if (offered != LogOfferResult::Closed) publication_failed = true;
            cancel(); return PipePublication::Cancelled;
        }
        held[index] = false;
        {
            std::lock_guard lock(status_mutex);
            (index == 0 ? current.stdout_pending_bytes : current.stderr_pending_bytes) = 0; increment(current.revision);
        }
        return PipePublication::Accepted;
    }
    void started(std::uint64_t pid, bool notifications) noexcept override {
        { std::lock_guard lock(status_mutex); current.root_process_id = pid; current.root_notifications = notifications;
          current.state = ProcessStreamState::Running; increment(current.revision); }
        if (work_wake) work_wake->notify();
    }
    void root_exited(ProcessExitStatus exit) noexcept override {
        { std::lock_guard lock(status_mutex); current.exit = exit; current.state = ProcessStreamState::Draining; increment(current.revision); }
        if (work_wake) work_wake->notify();
    }
    void pipe_closed(ProcessPipe pipe, bool unverified) noexcept override {
        const auto now = clock.now_nanos();
        auto& queue = pipe == ProcessPipe::Stdout ? out : err;
        { std::lock_guard lock(status_mutex);
          (pipe == ProcessPipe::Stdout ? current.stdout_eof : current.stderr_eof) = !unverified;
          current.tail_unverified = current.tail_unverified || unverified; increment(current.revision); }
        while (!queue.close(LogSourceState::Ended, 0, now)) std::this_thread::yield();
    }
    void run() noexcept {
        ProcessPumpResult result;
        bool execution_exception = false;
        try {
            ProcessRunRequest invocation; invocation.launch = std::move(request.launch);
            invocation.descendants = ProcessDescendantPolicy::TerminateOnCompletion;
            invocation.max_stdout_bytes = 0; invocation.max_stderr_bytes = 0;
            result = run_private_process(invocation, clock, this);
        } catch (...) {
            execution_exception = true;
            result.run.state = ProcessRunState::IoFailed;
            result.run.error = {ProcessErrorDomain::None, 0};
        }
        const bool protocol_failure = publication_failed.load(std::memory_order_acquire);
        const bool was_cancelled = (result.cancelled || cancelled()) && !protocol_failure && !execution_exception;
        const bool failed = result.run.state != ProcessRunState::Completed || protocol_failure || execution_exception;
        const auto source_state = was_cancelled ? LogSourceState::Cancelled : failed ? LogSourceState::Failed : LogSourceState::Ended;
        const auto now = clock.now_nanos();
        for (auto* queue : {&out, &err}) {
            // Only worker-side short contention retries; UI calls never wait.
            const auto error = static_cast<std::uint32_t>(std::min<std::uint64_t>(result.run.error.code, std::numeric_limits<std::uint32_t>::max()));
            while (!(was_cancelled ? queue->cancel(now) : queue->close(source_state, error, now))) std::this_thread::yield();
        }
        { std::lock_guard lock(status_mutex);
          current.state = was_cancelled ? ProcessStreamState::Cancelled : !failed ? ProcessStreamState::Completed :
              result.run.state == ProcessRunState::LaunchFailed ? ProcessStreamState::LaunchFailed : ProcessStreamState::IoFailed;
          current.error = result.run.error; current.exit = result.run.exit;
          current.failure = protocol_failure ? ProcessStreamFailure::Publication :
              execution_exception ? ProcessStreamFailure::ExecutionException : ProcessStreamFailure::None;
          increment(current.retired_staging_bytes, current.stdout_pending_bytes + current.stderr_pending_bytes);
          current.stdout_pending_bytes = 0; current.stderr_pending_bytes = 0;
          current.tail_unverified = current.tail_unverified || result.tail_unverified; increment(current.revision); }
        if (work_wake) work_wake->notify();
    }
};
class StreamSession final : public ProcessStreamSession {
public:
    explicit StreamSession(std::shared_ptr<StreamState> state) : state_(std::move(state)) {}
    ~StreamSession() override { state_->cancel(); }
    LogMailbox& stdout_mailbox() noexcept override { return state_->out; }
    LogMailbox& stderr_mailbox() noexcept override { return state_->err; }
    bool status(ProcessStreamStatus& result) const noexcept override {
        std::unique_lock lock(state_->status_mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        result = state_->current; return true;
    }
    void cancel() noexcept override { state_->cancel(); }
    std::size_t allocated_bytes() const noexcept override {
        return sizeof(*this) + sizeof(StreamState) + state_->out.allocated_bytes() + state_->err.allocated_bytes()
            - 2 * sizeof(LogMailbox) +
#if defined(_WIN32)
            3 * 16 * 1024;
#else
            2 * 16 * 1024;
#endif
    }
private:
    std::shared_ptr<StreamState> state_;
};
struct Worker {
    std::shared_ptr<Pulse> pulse = std::make_shared<Pulse>();
    std::mutex mutex;
    std::condition_variable ready;
    bool stopping = false;
    std::shared_ptr<StreamState> job, active;
    std::thread thread;
    Worker() : thread([this] { loop(); }) {}
    ~Worker() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            if (job) job->cancel();
            if (active) active->cancel();
        }
        ready.notify_one();
        if (thread.joinable()) thread.join();
    }
    void loop() noexcept {
        for (;;) {
            std::shared_ptr<StreamState> state;
            { std::unique_lock lock(mutex); ready.wait(lock, [this] { return stopping || job; });
              if (stopping && !job) return;
              active = std::move(job); state = active; }
            state->run();
            { std::lock_guard lock(mutex); active.reset(); if (stopping) return; }
        }
    }
};
}  // namespace
struct NativeProcessStreamService::Impl {
    const Clock& clock;
    std::shared_ptr<const LogWake> wake;
    std::vector<std::unique_ptr<Worker>> workers;
    Impl(const Clock& time, std::size_t slots, std::shared_ptr<const LogWake> target) : clock(time), wake(std::move(target)) {
        if (!slots || slots > 8) throw std::invalid_argument("invalid process stream capacity");
        workers.reserve(slots); for (std::size_t i = 0; i < slots; ++i) workers.push_back(std::make_unique<Worker>());
    }
};
NativeProcessStreamService::NativeProcessStreamService(const Clock& clock, std::size_t slots, std::shared_ptr<const LogWake> wake)
    : impl_(std::make_unique<Impl>(clock, slots, std::move(wake))) {}
NativeProcessStreamService::~NativeProcessStreamService() = default;
ProcessStreamOpenResult NativeProcessStreamService::start(ProcessStreamRequest request) {
    const auto validation = validate_process_stream(request);
    if (validation != ProcessStreamValidation::Valid) return {ProcessStreamOpenState::Invalid, validation, {}};
    for (auto& worker : impl_->workers) {
        std::unique_lock lock(worker->mutex, std::try_to_lock);
        if (!lock.owns_lock() || worker->stopping || worker->job || worker->active) continue;
        auto state = std::make_shared<StreamState>(impl_->clock, std::move(request), worker->pulse, impl_->wake);
        auto session = std::make_unique<StreamSession>(state);
        worker->job = std::move(state); lock.unlock(); worker->ready.notify_one();
        return {ProcessStreamOpenState::Opened, ProcessStreamValidation::Valid, std::move(session)};
    }
    return {ProcessStreamOpenState::Busy, ProcessStreamValidation::Valid, {}};
}
}  // namespace ckv::term
