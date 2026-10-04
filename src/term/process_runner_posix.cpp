// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_runner.hpp"
#include "cvision/term/process_launch_internal.hpp"

#if !defined(_WIN32)
#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <spawn.h>
#endif
#if defined(__linux__)
#include <sys/syscall.h>
#endif

namespace ckv::term {
namespace {
using namespace core;
class Descriptor {
public:
    int value = -1;
    ~Descriptor() { close(); }
    Descriptor() = default;
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    void close() noexcept { if (value >= 0) (void)::close(value); value = -1; }
};

void fail(ProcessRunResult& result, ProcessRunState state, const char* stage, int code) {
    if (result.error.domain != ProcessErrorDomain::None || result.state == ProcessRunState::IdleTimeout) return;
    result.state = state;
    result.error = {ProcessErrorDomain::Posix, static_cast<std::uint64_t>(code)};
    result.diagnostic = std::string(stage) + " (errno " + std::to_string(code) + ")";
}

bool pipe_pair(Descriptor& reader, Descriptor& writer) {
    int ends[2];
#if defined(__linux__)
    if (::pipe2(ends, O_CLOEXEC) != 0) return false;
#else
    if (::pipe(ends) != 0) return false;
#endif
    reader.value = ends[0]; writer.value = ends[1];
    for (Descriptor* end : {&reader, &writer}) {
        // stdio may be closed in the caller. Keep every private end above it
        // so dup2 cannot silently alias/close another stream in the child.
        if (end->value < 3) {
            const int moved = ::fcntl(end->value, F_DUPFD_CLOEXEC, 3);
            if (moved < 0) return false;
            end->close(); end->value = moved;
        } else if (::fcntl(end->value, F_SETFD, FD_CLOEXEC) < 0) return false;
    }
    return true;
}
bool nonblocking(const Descriptor& descriptor) {
    const int flags = ::fcntl(descriptor.value, F_GETFL);
    return flags >= 0 && ::fcntl(descriptor.value, F_SETFL, flags | O_NONBLOCK) == 0;
}

// Block SIGPIPE only in this calling thread, preserving global dispositions
// and previously pending/blocked signals. Consume our new synchronous EPIPE
// signal before restoring the mask. A failed mask setup refuses execution.
class PipeSignalGuard {
public:
    PipeSignalGuard() {
        sigemptyset(&pipe_); sigaddset(&pipe_, SIGPIPE);
        error = ::pthread_sigmask(SIG_BLOCK, &pipe_, &previous_);
        active_ = error == 0;
        if (active_) {
            sigset_t pending;
            if (::sigpending(&pending) == 0) already_pending_ = sigismember(&pending, SIGPIPE) == 1;
        }
    }
    ~PipeSignalGuard() {
        if (active_) (void)::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
    }
    void consume_epipe() noexcept {
        if (already_pending_) return;
        sigset_t pending;
        if (::sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1) {
            int signal = 0;
            (void)::sigwait(&pipe_, &signal);
        }
    }
    int error = 0;
private:
    sigset_t pipe_{}, previous_{};
    bool active_ = false;
    bool already_pending_ = false;
};

struct LaunchError { int code; int stage; };
#if !defined(__APPLE__)
[[noreturn]] void child_fail(int stage) noexcept {
    const LaunchError error{errno, stage};
    const char* bytes = reinterpret_cast<const char*>(&error);
    std::size_t offset = 0;
    while (offset < sizeof(error)) {
        const auto count = ::write(3, bytes + offset, sizeof(error) - offset);
        if (count > 0) offset += static_cast<std::size_t>(count);
        else if (count < 0 && errno == EINTR) continue;
        else break;
    }
    ::_exit(127);
}

void close_child_descriptors(int maximum) noexcept {
#if defined(__linux__) && defined(SYS_close_range)
    if (::syscall(SYS_close_range, 4U, ~0U, 0U) == 0) return;
#endif
    // This limit is obtained before fork. Only async-signal-safe close calls
    // occur here; arbitrary caller descriptors must not reach the helper.
    for (int fd = 4; fd < maximum; ++fd) (void)::close(fd);
}
#else
int spawn_directory(posix_spawn_file_actions_t& actions, const char* directory) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (__builtin_available(macOS 26.0, *)) return ::posix_spawn_file_actions_addchdir(&actions, directory);
    // Required only on supported pre-26 hosts. The replacement is unavailable
    // there; keep this narrowly scoped native SDK fallback, not a project-wide
    // warning exemption or a higher minimum OS requirement.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return ::posix_spawn_file_actions_addchdir_np(&actions, directory);
#pragma clang diagnostic pop
#else
    return ::posix_spawn_file_actions_addchdir_np(&actions, directory);
#endif
}

int spawn_private_mac(detail::PosixLaunchData& launch, pid_t& child,
                      const Descriptor& input, const Descriptor& output, const Descriptor& errors) {
    posix_spawnattr_t attributes;
    int code = ::posix_spawnattr_init(&attributes);
    if (code != 0) return code;
    posix_spawn_file_actions_t actions;
    code = ::posix_spawn_file_actions_init(&actions);
    if (code != 0) { (void)::posix_spawnattr_destroy(&attributes); return code; }
    sigset_t defaults, mask;
    sigfillset(&defaults); sigdelset(&defaults, SIGKILL); sigdelset(&defaults, SIGSTOP);
    sigemptyset(&mask);
    const short flags = POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETPGROUP |
                        POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK;
    const auto accept = [&](int operation) { if (code == 0) code = operation; };
    accept(::posix_spawnattr_setflags(&attributes, flags));
    accept(::posix_spawnattr_setpgroup(&attributes, 0));
    accept(::posix_spawnattr_setsigdefault(&attributes, &defaults));
    accept(::posix_spawnattr_setsigmask(&attributes, &mask));
    accept(::posix_spawn_file_actions_adddup2(&actions, input.value, 0));
    accept(::posix_spawn_file_actions_adddup2(&actions, output.value, 1));
    accept(::posix_spawn_file_actions_adddup2(&actions, errors.value, 2));
    // A dup2 source is itself mentioned in the actions and can otherwise
    // survive CLOEXEC_DEFAULT. Only its standard-stream destination may stay.
    accept(::posix_spawn_file_actions_addclose(&actions, input.value));
    accept(::posix_spawn_file_actions_addclose(&actions, output.value));
    accept(::posix_spawn_file_actions_addclose(&actions, errors.value));
    accept(spawn_directory(actions, launch.directory.c_str()));
    if (code == 0) code = ::posix_spawn(&child, launch.executable.c_str(), &actions, &attributes,
                                        launch.argv.data(), launch.environment.data());
    (void)::posix_spawn_file_actions_destroy(&actions);
    (void)::posix_spawnattr_destroy(&attributes);
    return code;
}
#endif

struct Root {
    pid_t pid = -1;
    bool reaped = false;
    bool release = false;
    ~Root() {
        if (pid < 0 || release) return;
        (void)::kill(-pid, SIGKILL);
        if (!reaped) {
            (void)::kill(pid, SIGKILL);
            while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        }
    }
};

void retain(ProcessCapture& capture, std::size_t cap, const char* bytes, std::size_t count) {
    const auto kept = std::min(count, cap - capture.bytes.size());
    capture.bytes.append(bytes, kept);
    if (kept != count) capture.truncated = true;
}

int idle_slice(const Clock& clock, std::int64_t progress, std::int64_t budget) {
    const auto elapsed = clock.now_nanos() - progress;
    const auto remaining = budget - std::min(budget, std::max<std::int64_t>(0, elapsed));
    return remaining == 0 ? 0 : static_cast<int>(std::min<std::int64_t>(20, 1 + (remaining - 1) / 1'000'000));
}
} // namespace

core::ProcessRunResult NativeProcessRunner::run(const core::ProcessRunRequest& request) {
    ProcessRunResult result;
    result.input_bytes_total = request.input.size();
    if (!valid_process_request(request)) {
        result.diagnostic = "invalid helper invocation, idle budget or descendant policy";
        return result;
    }
    detail::PosixLaunchData launch;
    detail::ProcessPreparationError preparation;
    if (!detail::prepare_posix_process_launch(request.launch, launch, preparation)) {
        result.state = ProcessRunState::LaunchFailed;
        result.error = preparation.native; result.diagnostic = preparation.diagnostic;
        return result;
    }
    PipeSignalGuard signals;
    if (signals.error != 0) {
        fail(result, ProcessRunState::LaunchFailed, "helper SIGPIPE mask", signals.error);
        return result;
    }
    Descriptor input_read, input_write, output_read, output_write, error_read, error_write, launch_read, launch_write;
    if (!pipe_pair(input_read, input_write) || !pipe_pair(output_read, output_write) ||
        !pipe_pair(error_read, error_write) || !nonblocking(input_write) || !nonblocking(output_read) || !nonblocking(error_read)) {
        fail(result, ProcessRunState::LaunchFailed, "helper private pipe setup", errno);
        return result;
    }
    Root root;
    bool native_exec_verified = false;
#if defined(__APPLE__)
    // Kernel-supported close-by-default is both atomic and independent of the
    // million-descriptor soft limit. Do not snapshot caller fd membership or
    // pay one close syscall per possible descriptor before each helper starts.
    const int spawned = spawn_private_mac(launch, root.pid, input_read, output_write, error_write);
    if (spawned != 0) {
        root.pid = -1; // POSIX leaves the output pid undefined on spawn failure.
        fail(result, ProcessRunState::LaunchFailed, "helper posix_spawn", spawned);
        return result;
    }
    native_exec_verified = true;
#else
    if (!pipe_pair(launch_read, launch_write) || !nonblocking(launch_read)) {
        fail(result, ProcessRunState::LaunchFailed, "helper launch error pipe", errno);
        return result;
    }
    rlimit descriptors{};
    if (::getrlimit(RLIMIT_NOFILE, &descriptors) != 0) {
        fail(result, ProcessRunState::LaunchFailed, "helper descriptor limit", errno);
        return result;
    }
    auto limit = descriptors.rlim_max == RLIM_INFINITY ? descriptors.rlim_cur : descriptors.rlim_max;
    if (limit == RLIM_INFINITY || limit > INT_MAX) {
        fail(result, ProcessRunState::LaunchFailed, "helper unbounded descriptor inventory", EOVERFLOW);
        return result;
    }
    const int maximum_fd = static_cast<int>(limit);
    root.pid = ::fork();
    if (root.pid < 0) {
        fail(result, ProcessRunState::LaunchFailed, "helper fork", errno);
        return result;
    }
    if (root.pid == 0) {
        // Reserve fd3 for the exec-error channel, after stdio has been mapped.
        // All remaining inherited descriptors are closed, not merely our pipes.
        if (::dup2(input_read.value, 0) < 0 || ::dup2(output_write.value, 1) < 0 ||
            ::dup2(error_write.value, 2) < 0) {
            const int code = errno;
            if (::dup2(launch_write.value, 3) >= 0) { errno = code; child_fail(1); }
            ::_exit(127);
        }
        if (::dup2(launch_write.value, 3) < 0) ::_exit(127);
        if (::fcntl(3, F_SETFD, FD_CLOEXEC) < 0) child_fail(1);
        if (::setpgid(0, 0) != 0) child_fail(2);
        close_child_descriptors(maximum_fd);
        detail::reset_process_child_signals();
        if (::chdir(launch.directory.c_str()) != 0) child_fail(3);
        ::execve(launch.executable.c_str(), launch.argv.data(), launch.environment.data());
        child_fail(4);
    }
#endif
    input_read.close(); output_write.close(); error_write.close(); launch_write.close();
    if (request.input.empty()) input_write.close();
    auto last_progress = clock_.now_nanos();
    result.state = ProcessRunState::Completed;
    LaunchError launch_error{};
    std::size_t launch_bytes = 0;
    bool launch_verified = native_exec_verified;
    const auto check_launch = [&] {
        if (launch_read.value < 0) return true;
        const auto count = ::read(launch_read.value, reinterpret_cast<char*>(&launch_error) + launch_bytes,
                                   sizeof(launch_error) - launch_bytes);
        if (count > 0) {
            launch_bytes += static_cast<std::size_t>(count);
            if (launch_bytes == sizeof(launch_error)) {
                fail(result, ProcessRunState::LaunchFailed,
                     launch_error.stage == 3 ? "helper working directory" :
                     launch_error.stage == 4 ? "helper execve" : "helper child setup", launch_error.code);
                return false;
            }
        } else if (count == 0) {
            launch_read.close();
            if (launch_bytes != 0) { fail(result, ProcessRunState::LaunchFailed, "helper incomplete launch error", EIO); return false; }
            launch_verified = true;
        } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            fail(result, ProcessRunState::LaunchFailed, "helper launch error channel", errno);
            return false;
        }
        return true;
    };
    const auto drain = [&](Descriptor& channel, ProcessCapture& capture, std::size_t cap) {
        std::array<char, 16 * 1024> bytes{};
        for (int pass = 0; pass < 16 && channel.value >= 0; ++pass) {
            const auto count = ::read(channel.value, bytes.data(), bytes.size());
            if (count > 0) {
                retain(capture, cap, bytes.data(), static_cast<std::size_t>(count));
                last_progress = clock_.now_nanos();
            } else if (count == 0) channel.close();
            else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) break;
            else { fail(result, ProcessRunState::IoFailed, "helper capture read", errno); return false; }
        }
        return true;
    };
    for (;;) {
        if (!check_launch()) break;
        int status = 0;
        const pid_t observed = ::waitpid(root.pid, &status, WNOHANG);
        if (observed == root.pid) {
            root.reaped = true;
            if (WIFEXITED(status)) result.exit = ProcessExitStatus{ProcessExitKind::Normal, WEXITSTATUS(status)};
            else if (WIFSIGNALED(status)) result.exit = ProcessExitStatus{ProcessExitKind::Signal, WTERMSIG(status)};
            // The child is gone, so the private error channel has either its
            // small record or EOF. Do not mistake an exec failure for exit127.
            (void)check_launch();
            (void)drain(output_read, result.stdout_capture, request.max_stdout_bytes);
            (void)drain(error_read, result.stderr_capture, request.max_stderr_bytes);
            break;
        }
        if (observed < 0 && errno != EINTR) {
            root.reaped = errno == ECHILD;
            fail(result, ProcessRunState::ExitStatusUnavailable, "helper waitpid", errno);
            break;
        }
        const int slice = idle_slice(clock_, last_progress, request.idle_budget_nanos);
        if (slice == 0) {
            result.state = ProcessRunState::IdleTimeout;
            result.diagnostic = "helper made no byte-transfer progress within its idle budget";
            break;
        }
        std::array<pollfd, 4> waiting{{
            {input_write.value, POLLOUT, 0}, {output_read.value, POLLIN, 0},
            {error_read.value, POLLIN, 0}, {launch_read.value, POLLIN, 0}}};
        const int ready = ::poll(waiting.data(), waiting.size(), slice);
        if (ready < 0) {
            if (errno == EINTR) continue;
            fail(result, ProcessRunState::IoFailed, "helper poll", errno); break;
        }
        // Read both channels before writing; separate pipes cannot deadlock
        // a child that emits diagnostics before it begins consuming input.
        if (!drain(output_read, result.stdout_capture, request.max_stdout_bytes) ||
            !drain(error_read, result.stderr_capture, request.max_stderr_bytes)) break;
        if (input_write.value >= 0 && waiting[0].revents != 0) {
            const auto size = std::min<std::size_t>(16 * 1024, request.input.size() - result.input_bytes_written);
            const auto count = ::write(input_write.value, request.input.data() + result.input_bytes_written, size);
            if (count > 0) {
                result.input_bytes_written += static_cast<std::size_t>(count);
                last_progress = clock_.now_nanos();
                if (result.input_bytes_written == request.input.size()) input_write.close();
            } else if (count == 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) {
                const int code = count == 0 ? EIO : errno;
                if (code == EPIPE) signals.consume_epipe();
                fail(result, ProcessRunState::IoFailed, "helper stdin write", code); break;
            }
        }
    }
    // Unknown/failed launch is never a successful release. The root guard kills
    // the owned process group and reaps a live root on every other return path.
    root.release = launch_verified && result.successful() &&
                   request.descendants == ProcessDescendantPolicy::ReleaseOnSuccess;
    return result;
}
} // namespace ckv::term
#endif
