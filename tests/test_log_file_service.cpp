// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/log_file_service.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "scratch_directory.hpp"
#include "log_file_test_fixture.hpp"

#if defined(_WIN32)
#include "cvision/term/windows_text.hpp"
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
std::string path_text(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
void write(const std::filesystem::path& path, std::string_view bytes, bool append = false) {
    std::ofstream output(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    CK_CHECK(!output.fail());
}
ckv::LogFileRequest inspect(std::uint64_t revision, const std::filesystem::path& path,
                           ckv::LogFollowMode follow = ckv::LogFollowMode::Path) {
    ckv::LogFileRequest request;
    request.revision = revision; request.path = path_text(path); request.follow = follow;
    return request;
}
ckv::LogFileRequest read_request(std::uint64_t revision, std::uint64_t offset, std::size_t bytes) {
    ckv::LogFileRequest request;
    request.revision = revision; request.operation = ckv::LogFileOperation::Read;
    request.offset = offset; request.bytes = bytes;
    return request;
}
void await(ckv::LogFileSession& session, std::uint64_t revision, ckv::LogFileResult& result) {
    // Correctness is not a loaded-host latency benchmark. CTest owns the
    // deadline; do not turn a contended scheduler into a missing-data test.
    while (!session.take(result) || result.revision != revision)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void await_hint(ckv::LogFileSession& session) {
    while (!session.changes_pending()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
std::unique_ptr<ckv::LogFileSession> await_session(ckv::term::NativeLogFileService& service) {
    for (;;) {
        auto session = service.open_session();
        if (session) return session;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
std::string_view text(const ckv::LogFileResult& result) {
    const auto bytes = result.bytes(); return {bytes.data(), bytes.size()};
}
class CountedWake final : public ckv::LogWake {
public:
    void notify() const noexcept override { calls.fetch_add(1); }
    mutable std::atomic<unsigned> calls{0};
};
}

CK_TEST(native_log_file_explicit_wake_announces_results_and_coalesces_change_hints) {
    ckv::testing::ScratchDirectory scratch("log-file-wake");
    const auto path = scratch.path() / "source.txt"; write(path, "a");
    auto wake = std::make_shared<CountedWake>();
    ckv::term::NativeLogFileService service(1, ckv::kLogBatchBytes, ckv::term::LogWatchPreference::Native, wake);
    auto session = service.open_session(); ckv::LogFileResult result;
    CK_CHECK(session->has_wake());
    session->request(inspect(1, path)); await(*session, 1, result);
    while (wake->calls.load() == 0) std::this_thread::yield();
    const auto before = wake->calls.load();
    write(path, "b", true); await_hint(*session);
    while (wake->calls.load() == before) std::this_thread::yield();
    const auto notified = wake->calls.load();
    for (unsigned i = 0; i < 10; ++i) write(path, "c", true);
    // The held hint is one pending state, irrespective of event/line count.
    CK_CHECK(wake->calls.load() == notified);
    session->cancel(); CK_CHECK(!session->has_wake());
}

CK_TEST(scripted_log_files_control_identity_changes_loss_and_delayed_publication) {
    ckv::testing::ScriptedLogFiles files;
    auto session = files.open_session();
    CK_CHECK(!files.open_session());
    session->request(inspect(1, "/log"));
    CK_CHECK(files.requested().revision == 1);
    const ckv::LogFileObservation original{{4, 5, 0}, 3, 8};
    files.publish(1, original, "one");
    ckv::LogFileResult result;
    CK_CHECK(session->take(result)); CK_CHECK(text(result) == "one");
    CK_CHECK(!session->take(result));
    files.hint(true); CK_CHECK(session->changes_pending());
    session->request(inspect(2, "/log"));
    files.publish(1, original); // Deliberately inject a delayed old revision.
    CK_CHECK(session->take(result)); CK_CHECK(result.revision == 1);
    const ckv::LogFileObservation replaced{{4, 6, 0}, 0, 9};
    files.hint(true); files.publish(2, replaced);
    CK_CHECK(session->take(result)); CK_CHECK(result.rescan_required);
    CK_CHECK(result.file->identity != original.identity);
    session->cancel(); CK_CHECK(!session->changes_pending());
    CK_CHECK(files.open_session() != nullptr);
}

CK_TEST(native_log_file_reads_bounded_offsets_once_and_reports_real_identity) {
    ckv::testing::ScratchDirectory scratch("log-file-read");
    const auto path = scratch.path() / "source.txt"; write(path, "0123456789");
    ckv::term::NativeLogFileService service(1, 4);
    auto session = service.open_session(); CK_CHECK(session != nullptr);
    ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(result.state == ckv::LogSourceState::Following);
    CK_CHECK(result.file.has_value()); CK_CHECK(result.file->size == 10);
    const auto identity = result.file->identity;
    CK_CHECK(result.bytes().empty());
    session->request(read_request(2, 3, 4)); await(*session, 2, result);
    CK_CHECK(result.operation == ckv::LogFileOperation::Read);
    CK_CHECK(result.offset == 3); CK_CHECK(text(result) == "3456");
    CK_CHECK(result.before_read->identity == identity); CK_CHECK(result.file->identity == identity);
    CK_CHECK(!session->take(result));
    session->request(read_request(3, 100, 4)); await(*session, 3, result);
    CK_CHECK(result.state == ckv::LogSourceState::Following); CK_CHECK(result.bytes().empty());
}

CK_TEST(native_log_file_rejects_invalid_requests_without_opening_relative_paths) {
    ckv::term::NativeLogFileService service(1, 4);
    auto session = service.open_session();
    ckv::LogFileResult result;
    auto request = inspect(1, "relative.txt");
    session->request(std::move(request)); await(*session, 1, result);
    CK_CHECK(result.state == ckv::LogSourceState::Failed); CK_CHECK(result.native_error != 0);
    session->request(read_request(2, 0, 5)); await(*session, 2, result);
    CK_CHECK(result.state == ckv::LogSourceState::Failed); CK_CHECK(result.bytes().empty());
}

CK_TEST(native_log_file_superseded_revisions_publish_only_the_latest_request) {
    ckv::testing::ScratchDirectory scratch("log-file-revision");
    const auto a = scratch.path() / "a.txt", b = scratch.path() / "b.txt";
    write(a, "AAAA"); write(b, "BBBB");
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session();
    ckv::LogFileResult result;
    for (std::uint64_t revision = 1; revision < 50; ++revision) session->request(inspect(revision, a));
    session->request(inspect(50, b)); await(*session, 50, result);
    CK_CHECK(result.file->size == 4);
    session->request(read_request(51, 0, 4)); await(*session, 51, result);
    CK_CHECK(text(result) == "BBBB");
}

CK_TEST(native_log_file_notification_append_and_truncate_reconcile_current_bytes) {
    ckv::testing::ScratchDirectory scratch("log-file-change");
    const auto path = scratch.path() / "source.txt"; write(path, "one");
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session(); ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(result.watch == ckv::LogWatchMode::Native);
    const auto identity = result.file->identity;
    write(path, "two", true); await_hint(*session);
    session->request(inspect(2, path)); await(*session, 2, result);
    CK_CHECK(result.file->identity == identity); CK_CHECK(result.file->size == 6);
    session->request(read_request(3, 3, 3)); await(*session, 3, result); CK_CHECK(text(result) == "two");
    write(path, "x"); await_hint(*session);
    session->request(inspect(4, path)); await(*session, 4, result);
    CK_CHECK(result.file->identity == identity); CK_CHECK(result.file->size == 1);
    session->request(read_request(5, 0, 1)); await(*session, 5, result); CK_CHECK(text(result) == "x");
}

CK_TEST(native_log_file_path_rotation_and_open_file_following_are_distinct) {
    ckv::testing::ScratchDirectory scratch("log-file-rotation");
    const auto path = scratch.path() / "source.txt", rotated = scratch.path() / "old.txt";
    write(path, "old");
    ckv::term::NativeLogFileService service(2);
    auto by_path = service.open_session(), by_file = service.open_session();
    ckv::LogFileResult result, open_result;
    by_path->request(inspect(1, path)); await(*by_path, 1, result);
    by_file->request(inspect(1, path, ckv::LogFollowMode::OpenFile)); await(*by_file, 1, open_result);
    const auto old = result.file->identity;
    std::filesystem::rename(path, rotated); write(path, "new"); await_hint(*by_path);
    by_file->request(inspect(2, path, ckv::LogFollowMode::OpenFile)); await(*by_file, 2, open_result);
    CK_CHECK(open_result.file->identity == old);
    by_file->request(read_request(3, 0, 3)); await(*by_file, 3, open_result); CK_CHECK(text(open_result) == "old");
    by_path->request(inspect(2, path)); await(*by_path, 2, result);
    CK_CHECK(result.file->identity != old);
    by_path->request(read_request(3, 0, 3)); await(*by_path, 3, result); CK_CHECK(text(result) == "new");
}

CK_TEST(native_log_file_missing_recreate_and_nonregular_sources_report_states) {
    ckv::testing::ScratchDirectory scratch("log-file-missing");
    const auto path = scratch.path() / "source.txt";
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session(); ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(result.state == ckv::LogSourceState::Missing);
    CK_CHECK(result.watch == ckv::LogWatchMode::Native);
    write(path, "created"); await_hint(*session);
    session->request(inspect(2, path)); await(*session, 2, result);
    CK_CHECK(result.state == ckv::LogSourceState::Following);
    std::filesystem::remove(path); await_hint(*session);
    session->request(inspect(3, path)); await(*session, 3, result);
    CK_CHECK(result.state == ckv::LogSourceState::Missing);
    session->request(inspect(4, scratch.path())); await(*session, 4, result);
    CK_CHECK(result.state != ckv::LogSourceState::Following);
}

CK_TEST(native_log_file_explicit_polling_has_no_hidden_event_hint_dependency) {
    ckv::testing::ScratchDirectory scratch("log-file-polling");
    const auto path = scratch.path() / "source.txt"; write(path, "a");
    ckv::term::NativeLogFileService service(1, ckv::kLogBatchBytes, ckv::term::LogWatchPreference::Polling);
    auto session = service.open_session(); ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(result.watch == ckv::LogWatchMode::Polling);
    write(path, "b", true);
    CK_CHECK(!session->changes_pending());
    session->request(inspect(2, path)); await(*session, 2, result);
    CK_CHECK(result.file->size == 2);
}

CK_TEST(native_log_file_session_retirement_reuses_capacity_without_stale_data) {
    ckv::testing::ScratchDirectory scratch("log-file-retirement");
    const auto path = scratch.path() / "source.txt"; write(path, "current");
    ckv::term::NativeLogFileService service(1);
    for (unsigned i = 0; i < 30; ++i) {
        auto session = await_session(service);
        CK_CHECK(!service.open_session());
        session->request(inspect(1, path));
        session->cancel();
        ckv::LogFileResult result;
        CK_CHECK(!session->take(result)); CK_CHECK(!session->changes_pending());
    }
    auto session = await_session(service); ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    session->request(read_request(2, 0, 7)); await(*session, 2, result);
    CK_CHECK(text(result) == "current");
}

CK_TEST(native_log_file_unicode_path_and_same_size_rewrite_preserve_explicit_bytes) {
    ckv::testing::ScratchDirectory scratch("log-file-unicode");
    const auto path = scratch.path() / std::filesystem::path(std::u8string(u8"Grüße.txt"));
    write(path, "before");
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session(); ckv::LogFileResult result;
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(result.state == ckv::LogSourceState::Following);
    write(path, "after!"); await_hint(*session);
    session->request(read_request(2, 0, 6)); await(*session, 2, result);
    CK_CHECK(text(result) == "after!"); CK_CHECK(result.file->size == 6);
}

CK_TEST(native_log_file_access_denial_and_recovery_are_observable) {
    ckv::testing::ScratchDirectory scratch("log-file-denied");
    const auto path = scratch.path() / "source.txt"; write(path, "accessible");
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session(); ckv::LogFileResult result;
#if defined(_WIN32)
    const auto wide = ckv::term::windows_utf16(path_text(path)); CK_CHECK(wide.has_value());
    HANDLE exclusive = CreateFileW(wide->c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    CK_CHECK(exclusive != INVALID_HANDLE_VALUE);
    session->request(inspect(1, path)); await(*session, 1, result);
    CloseHandle(exclusive);
#else
    if (::geteuid() == 0) return; // A privileged user bypasses this permission fixture.
    CK_CHECK(::chmod(path.c_str(), 0) == 0);
    session->request(inspect(1, path)); await(*session, 1, result);
    CK_CHECK(::chmod(path.c_str(), 0600) == 0);
#endif
    CK_CHECK(result.state == ckv::LogSourceState::Denied); CK_CHECK(result.native_error != 0);
    session->request(inspect(2, path)); await(*session, 2, result);
    CK_CHECK(result.state == ckv::LogSourceState::Following);
}
