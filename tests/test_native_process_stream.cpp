// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/process_stream.hpp"
#include "cvision/widgets/log_stream_receiver.hpp"
#include "cvision/testing/cktest.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <charconv>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "cvision/term/windows_clock.hpp"
using StreamClock = ckv::term::WindowsClock;
#else
#include <csignal>
#include "cvision/term/posix_clock.hpp"
using StreamClock = ckv::term::PosixClock;
#endif
using namespace ckv;
using namespace ckv::core;
namespace {
bool terminal_state(ProcessStreamState state) {
    return state == ProcessStreamState::Completed || state == ProcessStreamState::Cancelled ||
           state == ProcessStreamState::LaunchFailed || state == ProcessStreamState::IoFailed;
}
template<class Predicate> bool wait_for(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    do { if (predicate()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    while (std::chrono::steady_clock::now() < deadline);
    return false;
}
ProcessStreamRequest request_for(std::string mode) {
    ProcessStreamRequest request;
    request.launch = ProcessLaunchSpec::program(CKV_PROCESS_RUNNER_CHILD_PATH, {std::move(mode)});
    request.launch.working_directory = std::filesystem::path(CKV_PROCESS_RUNNER_CHILD_PATH).parent_path().generic_string();
    request.stdout_source = 1; request.stderr_source = 2;
    request.stdout_queue = {2, 16 * 1024, LogOverflowPolicy::Backpressure};
    request.stderr_queue = request.stdout_queue;
    return request;
}
struct Fixture {
    StreamClock clock;
    term::NativeProcessStreamService service{clock, 1};
    std::unique_ptr<ProcessStreamSession> session;
    ProcessStreamStatus status;
    LogMessage message;
    std::string output, errors;
    bool out_end = false, err_end = false;
    bool open(ProcessStreamRequest request) {
        bool opened = false;
        const bool answered = wait_for([&] {
            auto result = service.start(request);
            if (result.state == ProcessStreamOpenState::Busy) return false;
            session = std::move(result.session);
            opened = result.state == ProcessStreamOpenState::Opened && session != nullptr;
            return true;
        });
        return answered && opened;
    }
    void take(LogMailbox& queue, std::string& target, bool& ended, LogSourceId source) {
        for (int turn = 0; turn < 16; ++turn) {
            const auto result = queue.take(message); if (result != LogTakeResult::Taken) break;
            CK_CHECK(message.source == source && message.epoch == 1);
            if (message.kind == LogMessageKind::End) ended = true;
            else target.append(message.bytes().data(), message.bytes().size());
        }
        queue.acknowledge();
    }
    bool drain() {
        return wait_for([&] {
            take(session->stdout_mailbox(), output, out_end, 1);
            take(session->stderr_mailbox(), errors, err_end, 2);
            session->status(status);
            return terminal_state(status.state) && out_end && err_end;
        });
    }
};
bool exited(std::uint64_t identity) {
#if defined(_WIN32)
    const HANDLE process = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(identity));
    if (!process) return true;
    const bool gone = ::WaitForSingleObject(process, 0) == WAIT_OBJECT_0; ::CloseHandle(process); return gone;
#else
    return ::kill(static_cast<pid_t>(identity), 0) != 0;
#endif
}
}
CK_TEST(native_process_stream_delivers_complete_concurrent_stdout_and_stderr) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("stream-dual"))); CK_CHECK(fixture.drain());
    CK_CHECK(fixture.status.state == ProcessStreamState::Completed && fixture.status.exit && fixture.status.exit->code == 0);
    CK_CHECK(fixture.output == std::string(512 * 1024, 'O') && fixture.errors == std::string(512 * 1024, 'E'));
    CK_CHECK(fixture.status.stdout_bytes == fixture.output.size() && fixture.status.stderr_bytes == fixture.errors.size());
}
CK_TEST(native_process_stream_stays_open_across_finite_helper_idle_budget) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("stream-quiet"))); CK_CHECK(fixture.drain());
    CK_CHECK(fixture.status.state == ProcessStreamState::Completed && fixture.status.exit && fixture.status.exit->code == 0);
    CK_CHECK(fixture.output == "before quiet\nafter quiet\n");
}
CK_TEST(native_process_stream_backpressured_stdout_does_not_prevent_stderr_progress) {
    Fixture fixture; auto request = request_for("stream-dual"); request.stdout_queue.batches = 1;
    CK_CHECK(fixture.open(std::move(request)));
    CK_CHECK(wait_for([&] {
        fixture.take(fixture.session->stderr_mailbox(), fixture.errors, fixture.err_end, 2);
        fixture.session->status(fixture.status); return fixture.errors.size() == 512 * 1024;
    }));
    LogMailboxStatus queued; CK_CHECK(fixture.session->stdout_mailbox().status(queued));
    CK_CHECK(queued.queued_batches <= 1 && queued.queued_bytes <= 16 * 1024);
    CK_CHECK(fixture.status.stdout_pending_bytes <= 16 * 1024 && !terminal_state(fixture.status.state));
    CK_CHECK(fixture.drain() && fixture.output.size() == 512 * 1024);
}
CK_TEST(native_process_stream_preserves_argument_data_environment_and_explicit_cwd) {
    Fixture fixture; auto request = request_for("stream-args");
    request.launch.arguments.insert(request.launch.arguments.end(), {"", "a b", "\"quoted\"", "$HOME; $(data)", "中 é"});
    request.launch.environment = {{"CKV_STREAM_DATA", "literal; $(data)"}};
    const auto directory = request.launch.working_directory;
    CK_CHECK(fixture.open(std::move(request)) && fixture.drain());
    CK_CHECK(fixture.output == "\na b\n\"quoted\"\n$HOME; $(data)\n中 é\n");
    CK_CHECK(fixture.errors == directory + "\nliteral; $(data)");
}
CK_TEST(native_process_stream_reports_nonzero_exit_and_launch_failure) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("nonzero")) && fixture.drain());
    CK_CHECK(fixture.status.state == ProcessStreamState::Completed && fixture.status.exit && fixture.status.exit->code == 7);
    CK_CHECK(fixture.errors == "helper failed\n");
    fixture.session.reset();
    auto request = request_for("nonzero"); request.launch.executable += ".missing";
    CK_CHECK(wait_for([&] { return fixture.open(request); })); fixture.out_end = false; fixture.err_end = false;
    CK_CHECK(fixture.drain() && fixture.status.state == ProcessStreamState::LaunchFailed);
    CK_CHECK(fixture.status.error.domain != ProcessErrorDomain::None && !fixture.status.exit);
}
CK_TEST(native_process_stream_closes_each_pipe_independently_of_root_completion) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("stream-stdout-eof")));
    CK_CHECK(wait_for([&] {
        fixture.take(fixture.session->stdout_mailbox(), fixture.output, fixture.out_end, 1);
        fixture.session->status(fixture.status); return fixture.out_end;
    }));
    CK_CHECK(fixture.output == "complete\n" && fixture.status.stdout_eof);
    CK_CHECK(fixture.drain() && fixture.errors == "later\n");
}
CK_TEST(native_process_stream_cancel_wakes_a_quiet_root_and_reaps_its_exact_identity) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("idle")));
    CK_CHECK(wait_for([&] { fixture.session->status(fixture.status); return fixture.status.state == ProcessStreamState::Running; }));
    const auto identity = fixture.status.root_process_id; CK_CHECK(identity != 0);
    fixture.session->cancel(); CK_CHECK(fixture.drain() && fixture.status.state == ProcessStreamState::Cancelled);
    CK_CHECK(exited(identity));
}
CK_TEST(native_process_stream_cancel_unblocks_full_sinks_and_retires_staging) {
    Fixture fixture; auto request = request_for("stream-dual"); request.stdout_queue.batches = 1; request.stderr_queue.batches = 1;
    CK_CHECK(fixture.open(std::move(request)));
    CK_CHECK(wait_for([&] { fixture.session->status(fixture.status); return fixture.status.stdout_pending_bytes > 0; }));
    const auto identity = fixture.status.root_process_id; fixture.session->cancel();
    CK_CHECK(fixture.drain() && exited(identity) && fixture.status.retired_staging_bytes > 0);
    CK_CHECK(fixture.status.stdout_pending_bytes == 0 && fixture.status.stderr_pending_bytes == 0);
}
CK_TEST(native_process_stream_drop_oldest_is_bounded_and_counts_overload) {
    Fixture fixture; auto request = request_for("stream-dual");
    request.stdout_queue = {1, 16 * 1024, LogOverflowPolicy::DropOldest}; request.stderr_queue = request.stdout_queue;
    CK_CHECK(fixture.open(std::move(request)));
    CK_CHECK(wait_for([&] { fixture.session->status(fixture.status); return terminal_state(fixture.status.state); }));
    LogMailboxStatus out, err; CK_CHECK(fixture.session->stdout_mailbox().status(out) && fixture.session->stderr_mailbox().status(err));
    CK_CHECK(out.high_water_batches == 1 && err.high_water_batches == 1 && out.dropped.bytes > 0 && err.dropped.bytes > 0);
    CK_CHECK(out.dropped.records_unknown && err.dropped.records_unknown && fixture.drain());
    CK_CHECK(fixture.output.size() + out.dropped.bytes == 512 * 1024 && fixture.errors.size() + err.dropped.bytes == 512 * 1024);
}
CK_TEST(native_process_stream_root_exit_does_not_wait_for_descendant_held_pipes) {
    Fixture fixture; auto request = request_for("descendant"); request.launch.arguments.push_back("ok");
    const bool opened = fixture.open(std::move(request)); CK_CHECK(opened); if (!opened) return;
    const bool drained = fixture.drain(); CK_CHECK(drained);
    const bool ready = fixture.status.state == ProcessStreamState::Completed && fixture.output.starts_with("READY:") && fixture.output.ends_with('\n');
    CK_CHECK(ready); if (!drained || !ready) return;
    std::uint64_t identity = 0;
    const auto parsed = std::from_chars(fixture.output.data() + 6, fixture.output.data() + fixture.output.size() - 1, identity);
    const bool identified = parsed.ec == std::errc{} && parsed.ptr == fixture.output.data() + fixture.output.size() - 1 && identity != 0;
    CK_CHECK(identified); if (!identified) return;
    // A killed orphan can briefly remain a zombie until the host's reaper runs.
    CK_CHECK(wait_for([&] { return exited(identity); }));
}
CK_TEST(native_process_stream_service_capacity_and_retirement_are_explicit) {
    Fixture fixture; CK_CHECK(fixture.open(request_for("idle")));
    auto second = fixture.service.start(request_for("idle")); CK_CHECK(second.state == ProcessStreamOpenState::Busy && !second.session);
    fixture.session.reset();
    CK_CHECK(wait_for([&] {
        second = fixture.service.start(request_for("nonzero")); return second.state == ProcessStreamOpenState::Opened;
    }));
    second.session->cancel();
}
CK_TEST(native_process_stream_composes_with_shipped_receivers_and_safe_incremental_models) {
    Fixture fixture; auto request = request_for("stream-args");
    request.launch.arguments.push_back("中 é"); request.launch.arguments.push_back("a\x1b" "b");
    request.chunk_bytes = 5;
    CK_CHECK(fixture.open(std::move(request)));
    LogLimits limits; limits.records = 32; limits.record_bytes = 256; limits.retained_bytes = kLogMetadataBytes + 16384;
    widgets::LogModel output(limits), errors(limits);
    widgets::LogStreamReceiver out(output, fixture.session->stdout_mailbox(), {128, 32});
    widgets::LogStreamReceiver err(errors, fixture.session->stderr_mailbox(), {128, 32});
    CK_CHECK(wait_for([&] {
        const auto now = fixture.clock.now_nanos(); out.step(now); err.step(now);
        return out.status().state == LogSourceState::Ended && err.status().state == LogSourceState::Ended;
    }));
    CK_CHECK(output.size() == 2 && output.at(0)->text == "中 é" && output.at(1)->text == "a�b");
    CK_CHECK(output.at(1)->raw == std::string_view("a\x1b" "b") && output.at(0)->id.source == 1);
    CK_CHECK(errors.size() >= 1 && errors.at(0)->id.source == 2);
}
