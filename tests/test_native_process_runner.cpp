// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <chrono>
#include <charconv>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include "cvision/term/process_runner.hpp"
#include "cvision/testing/cktest.hpp"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "cvision/term/windows_clock.hpp"
using NativeClock = ckv::term::WindowsClock;
#else
#include <cerrno>
#include <fcntl.h>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "cvision/term/posix_clock.hpp"
#if defined(__APPLE__)
#include <libproc.h>
#include <sys/proc.h>
#else
#include <fstream>
#endif
using NativeClock = ckv::term::PosixClock;
#endif

namespace {
using namespace ckv::core;
#if !defined(_WIN32)
// Signal delivery can occur on a sanitizer runtime thread. A lock-free atomic
// keeps this fixture observation signal-safe without a volatile data race.
static_assert(std::atomic<unsigned>::is_always_lock_free);
std::atomic<unsigned> helper_interrupts{0};
void observe_helper_interrupt(int) { helper_interrupts.fetch_add(1, std::memory_order_relaxed); }
#endif
ProcessRunRequest request_for(std::string mode) {
    ProcessRunRequest request;
    request.launch = ProcessLaunchSpec::program(CKV_PROCESS_RUNNER_CHILD_PATH, {std::move(mode)});
#if defined(_WIN32)
    request.launch.working_directory = "C:/";
#endif
    request.descendants = ProcessDescendantPolicy::TerminateOnCompletion;
    return request;
}

std::uint64_t process_identity(const ProcessRunResult& result, std::string_view prefix) {
    const auto& output = result.stdout_capture.bytes;
    if (!output.starts_with(prefix) || !output.ends_with("\n")) return 0;
    std::uint64_t identity = 0;
    const auto parsed = std::from_chars(output.data() + prefix.size(), output.data() + output.size() - 1, identity);
    return parsed.ec == std::errc{} && parsed.ptr == output.data() + output.size() - 1 ? identity : 0;
}

std::uint64_t descendant_identity(const ProcessRunResult& result) { return process_identity(result, "READY:"); }

// Inspect the exact identity printed only after the descendant's ready ack.
// Always clean it if still alive, including after a failing policy assertion.
bool observe_and_clean_descendant(std::uint64_t identity) {
    if (identity == 0) return false;
#if defined(_WIN32)
    const HANDLE child = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                                        FALSE, static_cast<DWORD>(identity));
    if (child == nullptr) {
        CK_CHECK(::GetLastError() == ERROR_INVALID_PARAMETER);
        return false;
    }
    const DWORD state = ::WaitForSingleObject(child, 0);
    CK_CHECK(state == WAIT_OBJECT_0 || state == WAIT_TIMEOUT);
    const bool live = state == WAIT_TIMEOUT;
    if (live) CK_CHECK(::TerminateProcess(child, 1) != FALSE);
    CK_CHECK(::WaitForSingleObject(child, 5000) == WAIT_OBJECT_0);
    CK_CHECK(::CloseHandle(child) != FALSE);
    return live;
#else
    const pid_t child = static_cast<pid_t>(identity);
    bool live = ::kill(child, 0) == 0;
    if (live) {
        // kill(pid, 0) also succeeds for an exited zombie. Inspect actual
        // process state, not existence, before reporting a surviving actor.
#if defined(__APPLE__)
        proc_bsdinfo information{};
        const int bytes = ::proc_pidinfo(child, PROC_PIDTBSDINFO, 0, &information, sizeof(information));
        if (bytes == static_cast<int>(sizeof(information))) live = information.pbi_status != SZOMB;
        else { CK_CHECK(bytes == 0 && errno == ESRCH); live = false; }
#else
        std::ifstream status("/proc/" + std::to_string(identity) + "/stat");
        std::string line;
        if (std::getline(status, line)) {
            const auto closing_name = line.rfind(')');
            CK_CHECK(closing_name != std::string::npos && closing_name + 2 < line.size());
            if (closing_name != std::string::npos && closing_name + 2 < line.size()) live = line[closing_name + 2] != 'Z';
        } else { CK_CHECK(::kill(child, 0) < 0 && errno == ESRCH); live = false; }
#endif
    }
    if (live) CK_CHECK(::kill(child, SIGKILL) == 0 || errno == ESRCH);
    return live;
#endif
}
}

CK_TEST(native_process_runner_preserves_real_binary_stdin_stdout_and_separate_stderr) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("echo");
    std::string bytes;
    for (int count = 0; count < 600'000; ++count) bytes.push_back(static_cast<char>(count % 256));
    request.input = bytes;
    request.max_stdout_bytes = bytes.size();
    const auto result = runner.run(request);
    CK_CHECK(result.successful());
    CK_CHECK(result.input_bytes_written == bytes.size());
    CK_CHECK(result.stdout_capture.bytes == bytes);
    CK_CHECK(!result.stdout_capture.truncated);
    CK_CHECK(result.stderr_capture.bytes == std::string("diag\0\xff\r\n", 8));
    CK_CHECK(!result.stderr_capture.truncated);
}

CK_TEST(native_process_runner_drains_both_floods_past_independent_caps_before_input) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("flood");
    const std::string bytes(700'000, 'x');
    request.input = bytes;
    request.launch.arguments.push_back(std::to_string(bytes.size()));
    request.max_stdout_bytes = 137;
    request.max_stderr_bytes = 53;
    auto result = runner.run(request);
    CK_CHECK(result.successful()); // Child checked its actual complete input.
    CK_CHECK(result.stdout_capture.bytes == std::string(137, 'O'));
    CK_CHECK(result.stderr_capture.bytes == std::string(53, 'E'));
    CK_CHECK(result.stdout_capture.truncated && result.stderr_capture.truncated);
    request.max_stdout_bytes = request.max_stderr_bytes = 0;
    result = runner.run(request);
    CK_CHECK(result.successful());
    CK_CHECK(result.stdout_capture.bytes.empty() && result.stderr_capture.bytes.empty());
    CK_CHECK(result.stdout_capture.truncated && result.stderr_capture.truncated);
}

CK_TEST(native_process_runner_empty_input_is_closed_and_nonzero_diagnostics_are_private) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto result = runner.run(request_for("echo"));
    CK_CHECK(result.successful());
    CK_CHECK(result.input_bytes_written == 0);
    CK_CHECK(result.stdout_capture.bytes.empty());
    CK_CHECK(!result.stderr_capture.bytes.empty());
    result = runner.run(request_for("nonzero"));
    CK_CHECK(result.state == ProcessRunState::Completed);
    CK_CHECK(!result.successful());
    CK_CHECK(result.exit && result.exit->kind == ProcessExitKind::Normal && result.exit->code == 7);
    CK_CHECK(result.stderr_capture.bytes == "helper failed\n");
}

CK_TEST(native_process_runner_timeout_requires_idle_not_merely_slow_total_duration) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("idle");
    request.idle_budget_nanos = 150'000'000;
    auto result = runner.run(request);
    CK_CHECK(result.state == ProcessRunState::IdleTimeout);
    CK_CHECK(!result.successful());
    const auto root = process_identity(result, "ROOT:");
    CK_CHECK(root > 0); // The exact timed-out actor acknowledged actual startup.
    CK_CHECK(!observe_and_clean_descendant(root));
#if !defined(_WIN32)
    if (root > 0) {
        int status = 0;
        errno = 0;
        CK_CHECK(::waitpid(static_cast<pid_t>(root), &status, WNOHANG) == -1 && errno == ECHILD);
    }
#endif
    request = request_for("progress");
    request.idle_budget_nanos = 150'000'000;
    const auto start = clock.now_nanos();
    result = runner.run(request);
    CK_CHECK(result.successful());
    CK_CHECK(result.stdout_capture.bytes == std::string(12, 'p'));
    CK_CHECK(clock.now_nanos() - start > request.idle_budget_nanos);
}

CK_TEST(native_process_runner_refuses_partial_input_and_reports_native_launch_failure) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("early-close");
    const std::string input(2 * 1024 * 1024, 'x');
    request.input = input;
    auto result = runner.run(request);
    CK_CHECK(!result.successful());
    CK_CHECK(result.input_bytes_written < input.size());
    CK_CHECK(result.state == ProcessRunState::IoFailed);
    CK_CHECK(result.error.domain != ProcessErrorDomain::None && result.error.code != 0);
    request = request_for("echo");
    request.launch.executable += ".certainly-missing";
    result = runner.run(request);
    CK_CHECK(result.state == ProcessRunState::LaunchFailed);
    CK_CHECK(!result.successful());
    CK_CHECK(result.error.code != 0);
    request = request_for("echo");
    request.launch.working_directory += "/ckvision-directory-that-does-not-exist";
    result = runner.run(request);
    CK_CHECK(result.state == ProcessRunState::LaunchFailed);
    CK_CHECK(result.error.code != 0);
    request = request_for("echo");
    request.descendants = ProcessDescendantPolicy::Unspecified;
    result = runner.run(request);
    CK_CHECK(result.state == ProcessRunState::InvalidRequest);
    CK_CHECK(!result.exit && result.input_bytes_written == 0);
}

CK_TEST(native_process_runner_does_not_inherit_an_explicitly_inheritable_sentinel) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("sentinel");
#if defined(_WIN32)
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const HANDLE sentinel = ::CreateEventW(&security, TRUE, FALSE, nullptr);
    CK_CHECK(sentinel != nullptr);
    if (sentinel == nullptr) return;
    request.launch.arguments.push_back(std::to_string(reinterpret_cast<std::uintptr_t>(sentinel)));
    const auto result = runner.run(request);
    CK_CHECK(::CloseHandle(sentinel) != FALSE);
#else
    // A high fd avoids confusing the sentinel number with private child stdio
    // or its own loader. It deliberately has no close-on-exec flag.
    const int original = ::open("/dev/null", O_RDONLY);
    CK_CHECK(original >= 0);
    if (original < 0) return;
    const int sentinel = ::fcntl(original, F_DUPFD, 200);
    (void)::close(original);
    CK_CHECK(sentinel >= 200);
    if (sentinel < 0) return;
    request.launch.arguments.push_back(std::to_string(sentinel));
    const auto result = runner.run(request);
    CK_CHECK(::close(sentinel) == 0);
#endif
    CK_CHECK(result.state == ProcessRunState::Completed);
    CK_CHECK(result.exit.has_value());
    CK_CHECK(result.successful());
    CK_CHECK(result.stdout_capture.bytes == "ABSENT");
}

#if !defined(_WIN32)
CK_TEST(native_process_runner_signal_wakeups_do_not_restart_idle_budget) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    struct sigaction action{};
    action.sa_handler = observe_helper_interrupt;
    sigemptyset(&action.sa_mask);
    struct RestoreSignal {
        struct sigaction previous{};
        bool active = false;
        ~RestoreSignal() { if (active) CK_CHECK(::sigaction(SIGUSR1, &previous, nullptr) == 0); }
    } restore;
    restore.active = ::sigaction(SIGUSR1, &action, &restore.previous) == 0;
    CK_CHECK(restore.active);
    if (!restore.active) return;
    helper_interrupts.store(0, std::memory_order_relaxed);
    auto request = request_for("interrupt-idle");
    request.launch.arguments.push_back(std::to_string(::getpid()));
    request.idle_budget_nanos = 150'000'000;
    const auto began = clock.now_nanos();
    const auto result = runner.run(request);
    CK_CHECK(helper_interrupts.load(std::memory_order_relaxed) >= 2);
    CK_CHECK(result.state == ProcessRunState::IdleTimeout && !result.successful());
    CK_CHECK(clock.now_nanos() - began < 2'000'000'000);
    const auto root = process_identity(result, "ROOT:");
    CK_CHECK(root > 0);
    CK_CHECK(!observe_and_clean_descendant(root));
    if (root > 0) {
        int status = 0;
        errno = 0;
        CK_CHECK(::waitpid(static_cast<pid_t>(root), &status, WNOHANG) == -1 && errno == ECHILD);
    }
}

CK_TEST(native_process_runner_reports_real_pipe_resource_failure_without_spawning) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    rlimit original{};
    CK_CHECK(::getrlimit(RLIMIT_NOFILE, &original) == 0);
    if (original.rlim_cur < 3) return;
    struct RestoreLimit {
        rlimit original;
        ~RestoreLimit() { CK_CHECK(::setrlimit(RLIMIT_NOFILE, &original) == 0); }
    } restore{original};
    auto constrained = original;
    constrained.rlim_cur = 3; // No new private pipe can fit above standard I/O.
    CK_CHECK(::setrlimit(RLIMIT_NOFILE, &constrained) == 0);
    const auto result = runner.run(request_for("echo"));
    CK_CHECK(result.state == ProcessRunState::LaunchFailed);
    CK_CHECK(result.error.domain == ProcessErrorDomain::Posix && result.error.code == EMFILE);
    CK_CHECK(!result.exit && result.input_bytes_written == 0 && !result.successful());
    CK_CHECK(result.stdout_capture.bytes.empty() && result.stderr_capture.bytes.empty());
}
#endif

CK_TEST(native_process_runner_root_exit_does_not_wait_for_a_released_holder_pipe_eof) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    auto request = request_for("descendant");
    request.launch.arguments.push_back("success");
    request.descendants = ProcessDescendantPolicy::ReleaseOnSuccess;
    request.idle_budget_nanos = 2'000'000'000;
    const auto began = clock.now_nanos();
    const auto result = runner.run(request);
    CK_CHECK(result.successful());
    const auto identity = descendant_identity(result);
    CK_CHECK(identity > 0);
    CK_CHECK(clock.now_nanos() - began < request.idle_budget_nanos);
    // Start another helper while the first one's pipe ends are still held.
    const auto subsequent = runner.run(request_for("echo"));
    CK_CHECK(subsequent.successful());
    CK_CHECK(observe_and_clean_descendant(identity));
}

CK_TEST(native_process_runner_termination_policy_and_failure_clean_real_descendants) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    for (int path = 0; path < 3; ++path) {
        auto request = request_for(path == 2 ? "descendant-idle" : "descendant");
        request.launch.arguments.push_back(path == 1 ? "fail" : "success");
        request.descendants = path == 0 ? ProcessDescendantPolicy::TerminateOnCompletion :
                                        ProcessDescendantPolicy::ReleaseOnSuccess;
        request.idle_budget_nanos = path == 2 ? 250'000'000 : 2'000'000'000;
        const auto result = runner.run(request);
        const auto identity = descendant_identity(result);
        CK_CHECK(identity > 0); // Ready ack proves a real actor, not failed setup.
        if (path == 0) CK_CHECK(result.successful());
        if (path == 1) CK_CHECK(!result.successful() && result.exit && result.exit->code == 7);
        if (path == 2) CK_CHECK(result.state == ProcessRunState::IdleTimeout);
        // Kernel teardown/reparenting may complete just after the root wait.
        // This is bounded observation, not permission for a live actor to leak.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        CK_CHECK(!observe_and_clean_descendant(identity));
    }
}

CK_TEST(native_process_runner_repeated_execution_keeps_owned_handles_bounded) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    const auto count_handles = [] {
#if defined(_WIN32)
        DWORD handles = 0;
        CK_CHECK(::GetProcessHandleCount(::GetCurrentProcess(), &handles) != FALSE);
        return static_cast<std::size_t>(handles);
#else
        rlimit limit{};
        CK_CHECK(::getrlimit(RLIMIT_NOFILE, &limit) == 0);
        std::size_t count = 0;
        // Owned pipe descriptors are allocated at the lowest free positions.
        // Scan the finite current allocation range, not RLIM_INFINITY.
        for (int descriptor = 0; descriptor < static_cast<int>(std::min<rlim_t>(limit.rlim_cur, 65'536)); ++descriptor)
            if (::fcntl(descriptor, F_GETFD) >= 0) ++count;
        return count;
#endif
    };
    CK_CHECK(runner.run(request_for("echo")).successful()); // Warm runtime first.
    const auto before = count_handles();
    for (int iteration = 0; iteration < 24; ++iteration)
        CK_CHECK(runner.run(request_for("echo")).successful());
    CK_CHECK(count_handles() == before);
}

CK_TEST(native_process_runner_preserves_full_native_exit_and_unavailable_status) {
    NativeClock clock;
    ckv::term::NativeProcessRunner runner(clock);
#if defined(_WIN32)
    const auto result = runner.run(request_for("wide-exit"));
    CK_CHECK(result.state == ProcessRunState::Completed);
    CK_CHECK(result.exit && result.exit->kind == ProcessExitKind::Normal && result.exit->code == 4'294'967'295LL);
    CK_CHECK(!result.successful());
#else
    const auto signaled = runner.run(request_for("signal-exit"));
    CK_CHECK(signaled.state == ProcessRunState::Completed);
    CK_CHECK(signaled.exit && signaled.exit->kind == ProcessExitKind::Signal && signaled.exit->code == SIGTERM);
    CK_CHECK(!signaled.successful());
    struct Restore {
        struct sigaction prior{};
        ~Restore() { (void)::sigaction(SIGCHLD, &prior, nullptr); }
    } restore;
    struct sigaction ignored{};
    ignored.sa_handler = SIG_IGN;
    sigemptyset(&ignored.sa_mask);
    CK_CHECK(::sigaction(SIGCHLD, &ignored, &restore.prior) == 0);
    const auto unknown = runner.run(request_for("echo"));
    CK_CHECK(unknown.state == ProcessRunState::ExitStatusUnavailable);
    CK_CHECK(!unknown.exit && !unknown.successful());
    CK_CHECK(unknown.error.domain == ProcessErrorDomain::Posix && unknown.error.code == ECHILD);
#endif
}
