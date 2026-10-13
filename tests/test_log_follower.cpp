// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_follower.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/log_file_service.hpp"
#include "log_file_test_fixture.hpp"
#include "scratch_directory.hpp"

namespace {
ckv::LogLimits limits() {
    ckv::LogLimits value; value.records = 64; value.record_bytes = 128;
    value.retained_bytes = ckv::kLogMetadataBytes + 512; return value;
}
ckv::widgets::LogFollowOptions options() {
    ckv::widgets::LogFollowOptions value;
    value.start = ckv::LogStartMode::Beginning; value.read_bytes = 8;
    value.tail_bytes = 64; value.tail_lines = 4; value.drain_bytes = 16;
    value.reconcile_nanos = 100; value.retry_nanos = 50; value.result_poll_nanos = 1;
    return value;
}
struct Harness {
    explicit Harness(ckv::widgets::LogFollowOptions config = options())
        : model(limits()), follower(model, files.open_session(), config) {}
    ckv::testing::ScriptedLogFiles files;
    ckv::widgets::LogModel model;
    ckv::widgets::LogFollower follower;
    std::string contents;
    ckv::LogFileIdentity identity{1, 1, 0};
    std::uint64_t version = 1;
    ckv::LogSourceState state = ckv::LogSourceState::Following;
    std::size_t responded = 0, short_read = ckv::kLogBatchBytes;
    ckv::LogFileObservation observation() const { return {identity, contents.size(), version}; }
    void respond() {
        if (responded == files.request_count()) return;
        responded = files.request_count();
        const auto& request = files.requested();
        std::string_view bytes;
        if (request.operation == ckv::LogFileOperation::Read && request.offset < contents.size()) {
            bytes = std::string_view(contents).substr(static_cast<std::size_t>(request.offset),
                std::min(request.bytes, short_read));
        }
        files.publish(request.revision, state == ckv::LogSourceState::Following
            ? std::optional<ckv::LogFileObservation>(observation()) : std::nullopt, bytes, state);
    }
    void settle(std::int64_t now = 0) {
        for (unsigned i = 0; i < 2000; ++i) {
            respond(); follower.step(now);
            const auto next = follower.next_deadline();
            if (!follower.status().pending && (!next || *next > now)) return;
        }
        CK_CHECK(false);
    }
    void start(std::string bytes) {
        contents = std::move(bytes); CK_CHECK(follower.start("/source", 1, 0)); settle();
    }
};
}

CK_TEST(log_follower_initial_reads_are_bounded_and_reconciled_without_duplicate_probe_bytes) {
    Harness h; h.start("one\ntwo\nthree\n");
    CK_CHECK(h.model.size() == 3); CK_CHECK(h.model.at(0)->raw == "one");
    CK_CHECK(h.model.at(2)->raw == "three");
    CK_CHECK(h.follower.status().offset == h.contents.size());
    CK_CHECK(h.follower.status().bytes_delivered == h.contents.size());
    CK_CHECK(h.follower.status().bytes_read > h.follower.status().bytes_delivered);
    CK_CHECK(h.files.requested().operation == ckv::LogFileOperation::Inspect);
    CK_CHECK(h.follower.next_deadline() == 100);
}

CK_TEST(log_follower_startup_append_between_inspection_and_tail_read_has_no_lost_interval) {
    auto config = options(); config.start = ckv::LogStartMode::RecentTail;
    Harness h(config); h.contents = "first\n";
    CK_CHECK(h.follower.start("/source", 1, 0));
    h.respond(); h.follower.step(0);
    CK_CHECK(h.files.requested().operation == ckv::LogFileOperation::Read);
    h.contents += "second\n"; ++h.version; // Deliberately omit a change hint.
    h.settle();
    CK_CHECK(h.model.size() == 2);
    CK_CHECK(h.model.at(0)->raw == "first"); CK_CHECK(h.model.at(1)->raw == "second");
    CK_CHECK(h.follower.status().bytes_delivered == h.contents.size());
}

CK_TEST(log_follower_file_eof_preserves_partial_identity_until_append_or_real_stop) {
    Harness h; h.start("partial");
    const auto id = h.model.at(0)->id;
    CK_CHECK(h.model.at(0)->partial);
    h.contents += " tail\n"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 1); CK_CHECK(h.model.at(0)->id == id);
    CK_CHECK(h.model.at(0)->raw == "partial tail"); CK_CHECK(!h.model.at(0)->incomplete);
    h.contents += "last"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.at(1)->partial);
    h.follower.stop(); CK_CHECK(h.model.at(1)->incomplete);
    CK_CHECK(!h.follower.next_deadline());
}

CK_TEST(log_follower_recent_tail_selects_last_lines_and_reports_skipped_prefix) {
    auto config = options(); config.start = ckv::LogStartMode::RecentTail; config.tail_lines = 2;
    Harness h(config); h.start("one\ntwo\nthree\nfour\n");
    CK_CHECK(h.model.size() == 2); CK_CHECK(h.model.at(0)->raw == "three");
    CK_CHECK(h.model.at(1)->raw == "four");
    CK_CHECK(h.follower.status().startup_skipped_bytes == 8);
    CK_CHECK(h.follower.status().offset == h.contents.size());
}

CK_TEST(log_follower_tail_window_does_not_materialize_a_large_file_or_emit_cut_line) {
    auto config = options(); config.start = ckv::LogStartMode::RecentTail; config.tail_bytes = 24;
    Harness h(config); h.contents = std::string(100000, 'x') + "\none\ntwo\n";
    CK_CHECK(h.follower.start("/source", 1, 0)); h.respond(); h.follower.step(0);
    CK_CHECK(h.files.requested().offset == h.contents.size() - 24);
    h.settle();
    CK_CHECK(h.model.size() == 2); CK_CHECK(h.model.at(0)->raw == "one");
    CK_CHECK(h.follower.status().bytes_read < 100);
}

CK_TEST(log_follower_unfinished_tail_prefix_is_discarded_through_later_crlf_boundary) {
    auto config = options(); config.start = ckv::LogStartMode::RecentTail; config.tail_bytes = 8;
    Harness h(config); h.start(std::string(30, 'x'));
    CK_CHECK(h.model.size() == 0);
    h.contents += "\r"; ++h.version; h.files.hint(); h.settle();
    h.contents += "\nvisible\n"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 1); CK_CHECK(h.model.at(0)->raw == "visible");
    CK_CHECK(h.follower.status().startup_skipped_bytes == 10);
}

CK_TEST(log_follower_nonzero_start_keeps_a_literal_bom_in_log_content) {
    auto config = options(); config.start = ckv::LogStartMode::Offset; config.offset = 5;
    Harness h(config); h.start("skip\n\xEF\xBB\xBF" "literal\n");
    CK_CHECK(h.model.size() == 1);
    CK_CHECK(h.model.at(0)->raw == "\xEF\xBB\xBF" "literal");
    CK_CHECK(h.model.at(0)->text == "\xEF\xBB\xBF" "literal");
}

CK_TEST(log_follower_short_reads_reassemble_boundary_probes_without_duplication) {
    Harness h; h.short_read = 2; h.start("one\ntwo\nthree\n");
    CK_CHECK(h.model.size() == 3);
    CK_CHECK(h.model.at(1)->raw == "two");
    CK_CHECK(h.follower.status().bytes_delivered == h.contents.size());
    CK_CHECK(h.follower.status().state == ckv::LogSourceState::Following);
}

CK_TEST(log_follower_copy_truncate_finishes_old_partial_and_starts_new_generation) {
    Harness h; h.start("old partial"); const auto old = h.model.at(0)->id;
    h.contents = "new\n"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 2); CK_CHECK(h.model.at(0)->id == old);
    CK_CHECK(h.model.at(0)->incomplete);
    CK_CHECK(h.model.at(1)->raw == "new"); CK_CHECK(h.model.at(1)->id.generation > old.generation);
    CK_CHECK(h.follower.status().uncertain);
}

CK_TEST(log_follower_same_size_rewrite_and_truncate_regrow_are_detected_at_boundary) {
    Harness h; h.start("old\n"); const auto generation = h.model.at(0)->id.generation;
    h.contents = "new\nmore\n"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 3); CK_CHECK(h.model.at(1)->raw == "new");
    CK_CHECK(h.model.at(1)->id.generation > generation);
    h.contents = "NEW\nMORE\n"; ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 5); CK_CHECK(h.model.at(3)->raw == "NEW");
}

CK_TEST(log_follower_metadata_change_preserves_offset_and_retained_ids) {
    Harness h; h.start("unchanged\n"); const auto id = h.model.at(0)->id;
    ++h.version; h.files.hint(); h.settle();
    CK_CHECK(h.model.size() == 1); CK_CHECK(h.model.at(0)->id == id);
    CK_CHECK(h.follower.status().offset == h.contents.size());
    CK_CHECK(h.follower.status().uncertain); // Earlier changes cannot be inferred from an opaque token.
}

CK_TEST(log_follower_missing_denied_retry_preserves_history_and_separates_reconnection) {
    Harness h; h.start("before\n");
    h.state = ckv::LogSourceState::Missing; h.files.hint(); h.settle();
    CK_CHECK(h.follower.status().state == ckv::LogSourceState::Missing);
    CK_CHECK(h.model.size() == 1);
    h.state = ckv::LogSourceState::Denied; h.settle(50);
    CK_CHECK(h.follower.status().state == ckv::LogSourceState::Denied);
    h.state = ckv::LogSourceState::Following; h.identity.object = 2; h.contents = "after\n";
    CK_CHECK(h.follower.retry(51)); h.settle(51);
    CK_CHECK(h.model.size() == 2); CK_CHECK(h.model.at(1)->raw == "after");
    CK_CHECK(h.follower.status().notice == ckv::widgets::LogFollowNotice::Reconnected);
}

CK_TEST(log_follower_periodic_reconciliation_recovers_a_missed_notification) {
    Harness h; h.start("one\n");
    h.contents += "two\n"; ++h.version;
    h.settle(99); CK_CHECK(h.model.size() == 1);
    h.settle(100); CK_CHECK(h.model.size() == 2); CK_CHECK(h.model.at(1)->raw == "two");
}

CK_TEST(log_follower_notification_loss_reports_uncertainty_without_replaying_history) {
    Harness h; h.start("one\n"); const auto id = h.model.at(0)->id;
    h.files.hint(true); h.settle();
    CK_CHECK(h.follower.status().uncertain);
    CK_CHECK(h.follower.status().continuity_warnings > 0);
    CK_CHECK(h.model.size() == 1); CK_CHECK(h.model.at(0)->id == id);
}

CK_TEST(log_follower_source_switch_discards_stale_reads_and_keeps_history_identity) {
    Harness h; h.contents = "first\n";
    CK_CHECK(h.follower.start("/first", 1, 0)); h.respond(); h.follower.step(0);
    const auto old_revision = h.files.requested().revision;
    CK_CHECK(h.follower.start("/second", 2, 1));
    h.files.publish(old_revision, h.observation(), "stale\n"); h.follower.step(1);
    CK_CHECK(h.model.size() == 0); CK_CHECK(h.follower.status().stale_results == 1);
    h.contents = "second\n"; h.responded = 0; h.settle(1);
    CK_CHECK(h.model.size() == 1); CK_CHECK(h.model.at(0)->id.source == 2);
    CK_CHECK(h.model.at(0)->raw == "second");
}

CK_TEST(log_follower_rotation_drains_only_configured_old_generation_quota) {
    auto config = options(); config.drain_bytes = 4;
    Harness h(config); h.start("old\n");
    h.contents += "tail\nmore\n"; ++h.version; h.files.hint();
    h.follower.step(0); h.respond(); h.follower.step(0);
    CK_CHECK(h.files.requested().operation == ckv::LogFileOperation::Inspect);
    h.identity.object = 2; h.contents = "new\n"; ++h.version; h.settle();
    CK_CHECK(h.follower.status().unread_skipped_bytes == 6);
    CK_CHECK(h.follower.status().drain_yields > 0);
    CK_CHECK(h.model.at(h.model.size() - 1)->raw == "new");
}

CK_TEST(log_follower_invalid_publication_and_backwards_time_never_enter_model) {
    Harness h; h.contents = "bad\n"; CK_CHECK(h.follower.start("/source", 1, 0));
    h.respond(); h.files.publication().operation = ckv::LogFileOperation::Read;
    h.follower.step(0); CK_CHECK(h.follower.status().state == ckv::LogSourceState::Failed);
    CK_CHECK(h.model.size() == 0); CK_CHECK(!h.follower.step(-1));
    CK_CHECK(!h.follower.start("relative", 2, 1));
    CK_CHECK(h.follower.path() == "/source");
}

CK_TEST(log_follower_empty_startup_deadlines_saturate_without_wrapping) {
    Harness h; h.start(""); CK_CHECK(h.model.size() == 0);
    const auto end = std::numeric_limits<std::int64_t>::max();
    h.settle(end); CK_CHECK(!h.follower.next_deadline());
}
CK_TEST(log_follower_replacement_advances_from_the_retained_models_generation) {
    ckv::testing::ScriptedLogFiles files;
    ckv::widgets::LogModel model(limits());
    ckv::widgets::LogFollowOptions config; config.start = ckv::LogStartMode::Beginning;
    ckv::widgets::LogFollower follower(model, files.open_session(), config);
    model.begin_source(1, 9); model.append_record("retained");
    CK_CHECK(follower.start("/source", 1, 0) && follower.status().generation == 10);
}

CK_TEST(native_log_follower_appends_rotates_and_keeps_source_owned_outside_dispatch) {
    ckv::testing::ScratchDirectory scratch("log-follower-live");
    const auto path = scratch.path() / "source.txt", old = scratch.path() / "old.txt";
    { std::ofstream output(path); output << "old\n"; }
    ckv::term::NativeLogFileService service(1);
    ckv::widgets::LogModel model(limits());
    ckv::widgets::LogFollower follower(model, service.open_session(), options());
    const auto name = path.u8string();
    CK_CHECK(follower.start({reinterpret_cast<const char*>(name.data()), name.size()}, 7, 0));
    const auto wait_records = [&](std::size_t count, std::int64_t time) {
        while (model.size() < count || follower.status().pending) {
            follower.step(time); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    wait_records(1, 0);
    { std::ofstream output(path, std::ios::app); output << "append\n"; }
    wait_records(2, 100); CK_CHECK(model.at(1)->raw == "append");
    std::filesystem::rename(path, old);
    { std::ofstream output(path); output << "new\n"; }
    wait_records(3, 200); CK_CHECK(model.at(2)->raw == "new");
    CK_CHECK(model.at(2)->id.generation > model.at(0)->id.generation);
}

CK_TEST(native_log_follower_open_file_mode_continues_the_rotated_object) {
    ckv::testing::ScratchDirectory scratch("log-follower-open-file");
    const auto path = scratch.path() / "source.txt", old = scratch.path() / "old.txt";
    { std::ofstream output(path); output << "old\n"; }
    ckv::term::NativeLogFileService service(1);
    ckv::widgets::LogModel model(limits());
    auto config = options(); config.follow = ckv::LogFollowMode::OpenFile;
    ckv::widgets::LogFollower follower(model, service.open_session(), config);
    const auto name = path.u8string();
    CK_CHECK(follower.start({reinterpret_cast<const char*>(name.data()), name.size()}, 7, 0));
    const auto wait = [&](std::size_t records, std::int64_t now) {
        while (model.size() < records || follower.status().pending) {
            follower.step(now); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    wait(1, 0); const auto generation = model.at(0)->id.generation;
    std::filesystem::rename(path, old);
    { std::ofstream output(path); output << "replacement\n"; }
    { std::ofstream output(old, std::ios::app); output << "old tail\n"; }
    wait(2, 100);
    CK_CHECK(model.size() == 2); CK_CHECK(model.at(1)->raw == "old tail");
    CK_CHECK(model.at(1)->id.generation == generation);
}
