// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Runs in a separate executable: never duplicates the process-wide allocation
// replacements in test_hot_path_allocations.cpp or the TSan runtime.
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <new>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/log_model.hpp"
#include "cvision/widgets/log_panel.hpp"
#include "cvision/widgets/log_search.hpp"
#include "cvision/widgets/log_follower.hpp"
#include "cvision/widgets/log_controller.hpp"
#include "cvision/term/log_file_service.hpp"
#include "scratch_directory.hpp"
#include "log_file_test_fixture.hpp"

namespace {
std::atomic<bool> measuring{false};
std::atomic<std::size_t> allocations{0};
void* allocate(std::size_t size) {
    if (measuring.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}
void* allocate_nothrow(std::size_t size) noexcept {
    if (measuring.load(std::memory_order_relaxed)) allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate_nothrow(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }

CK_TEST(log_model_first_and_repeated_appends_lookups_evictions_and_clear_allocate_nothing) {
    ckv::LogLimits limits;
    limits.records = 4;
    limits.record_bytes = 32;
    limits.retained_bytes = ckv::kLogMetadataBytes + 128;
    ckv::widgets::LogModel model(limits);
    const std::array fields{ckv::LogField{"unit", "demo"}};
    const ckv::LogMetadata metadata{ckv::LogSeverity::Info, 100, fields};
    bool accepted = true;
    allocations = 0;
    measuring = true;
    accepted = model.begin_source(1, 1) && accepted;
    for (std::size_t i = 0; i < 1000; ++i) {
        accepted = model.append_bytes("a\xE4") && accepted;
        accepted = model.append_bytes("\xB8\xAD\xFF\n") && accepted;
        accepted = model.append_record("one\ntwo", metadata) && accepted;
        const auto entry = model.at(model.size() - 1);
        accepted = entry && model.index_of(entry->id).has_value() && accepted;
        if (i % 13 == 0) model.clear();
    }
    model.finish();
    measuring = false;
    CK_CHECK(accepted);
    CK_CHECK(allocations == 0);
}

CK_TEST(native_log_file_requests_reads_and_transfers_allocate_nothing_after_open) {
    ckv::testing::ScratchDirectory scratch("log-file-allocations");
    const auto path = scratch.path() / "source.txt";
    { std::ofstream output(path, std::ios::binary); output << "text"; }
    ckv::term::NativeLogFileService service(1);
    auto session = service.open_session();
    ckv::LogFileResult result;
    ckv::LogFileRequest open;
    open.revision = 1;
    const auto utf8_path = path.u8string();
    open.path.assign(reinterpret_cast<const char*>(utf8_path.data()), utf8_path.size());
    session->request(std::move(open));
    while (!session->take(result)) std::this_thread::yield();
    CK_CHECK(result.state == ckv::LogSourceState::Following);
    bool correct = true;
    allocations = 0; measuring = true;
    for (std::uint64_t revision = 2; revision < 202; ++revision) {
        ckv::LogFileRequest request;
        request.revision = revision; request.operation = ckv::LogFileOperation::Read; request.bytes = 4;
        session->request(std::move(request));
        while (!session->take(result)) std::this_thread::yield();
        correct = correct && result.state == ckv::LogSourceState::Following && result.bytes().size() == 4;
    }
    session->cancel();
    measuring = false;
    CK_CHECK(correct); CK_CHECK(allocations == 0);
}

CK_TEST(log_follower_start_delivery_and_reconciliation_allocate_nothing) {
    ckv::testing::ScriptedLogFiles files;
    ckv::LogLimits limits;
    limits.records = 16; limits.record_bytes = 32;
    limits.retained_bytes = ckv::kLogMetadataBytes + 128;
    ckv::widgets::LogModel model(limits);
    ckv::widgets::LogFollowOptions options;
    options.start = ckv::LogStartMode::Beginning;
    options.read_bytes = 4; options.reconcile_nanos = 100;
    ckv::widgets::LogFollower follower(model, files.open_session(), options);
    constexpr std::string_view content = "one\ntwo\nthree\n";
    std::size_t responded = 0;
    allocations = 0; measuring = true;
    bool accepted = follower.start("/source", 1, 0);
    for (std::int64_t now = 0; now < 1000; ++now) {
        if (responded != files.request_count()) {
            responded = files.request_count();
            const auto& query = files.requested();
            std::string_view bytes;
            if (query.operation == ckv::LogFileOperation::Read && query.offset < content.size())
                bytes = content.substr(static_cast<std::size_t>(query.offset), query.bytes);
            files.publish(query.revision, ckv::LogFileObservation{{1, 1, 0}, content.size(), 1}, bytes);
        }
        follower.step(now);
    }
    follower.stop(); measuring = false;
    CK_CHECK(accepted); CK_CHECK(model.size() == 3); CK_CHECK(allocations == 0);
}

CK_TEST(log_view_and_panel_refresh_wrap_scroll_and_selection_allocate_nothing) {
    ckv::LogLimits limits;
    limits.records = 16; limits.record_bytes = 128;
    limits.retained_bytes = ckv::kLogMetadataBytes + 16384;
    ckv::widgets::LogModel model(limits);
    model.begin_source(1, 1);
    ckv::widgets::LogPanel panel(model);
    panel.set_bounds({0, 0, 20, 8});
    allocations = 0; measuring = true;
    bool correct = true;
    for (std::size_t i = 0; i < 100; ++i) {
        correct = model.append_record("alpha beta gamma delta\nnext") && correct;
        panel.refresh();
        panel.view().set_wrap_mode(ckv::widgets::WrapMode::Word);
        panel.view().scroll_rows(-1);
        const auto record = model.at(model.size() - 1);
        correct = panel.view().set_selection({{record->id, 0}, {record->id, 5}}) && correct;
        panel.view().resume();
    }
    measuring = false;
    CK_CHECK(correct); CK_CHECK(allocations == 0);
}
CK_TEST(log_filter_query_rebuild_partial_publication_and_search_allocate_nothing) {
    ckv::LogLimits limits; limits.records = 16; limits.record_bytes = 128;
    limits.retained_bytes = ckv::kLogMetadataBytes + 16384;
    ckv::widgets::LogModel model(limits, ckv::widgets::LogCarriageReturn::Progress);
    model.begin_source(1, 1);
    ckv::widgets::LogFilter filter(model, {2, 8});
    ckv::widgets::LogSearch search(filter, {2, 8});
    ckv::widgets::LogPanel panel(filter); panel.set_search(&search);
    panel.set_bounds({0, 0, 20, 8});
    ckv::widgets::LogFilterQuery query; query.include = "keep"; query.exclude = "drop";
    std::array<unsigned char, 4> marks{};
    bool correct = true;
    allocations = 0; measuring = true;
    for (std::size_t i = 0; i < 100; ++i) {
        correct = filter.set_query(query) && correct;
        model.append_bytes("keep one\r");
        while (filter.step()) {}
        correct = search.set_query("one") && correct;
        search.request(ckv::widgets::LogSearchDirection::Next);
        while (search.step()) {}
        const auto result = search.take_result(); correct = result.has_value() && correct;
        const auto record = filter.at(filter.size() - 1);
        search.mark(*record, 0, 4, marks);
        if (result) correct = panel.view().reveal(result->id, result->range) && correct;
        panel.refresh();
        model.append_bytes("drop\r"); while (filter.step()) {}
        model.append_bytes("keep two\n"); while (filter.step()) {}
        filter.set_include("two"); while (filter.step()) {}
        panel.refresh();
    }
    measuring = false;
    CK_CHECK(correct); CK_CHECK(allocations == 0);
}

CK_TEST(log_mailbox_receiver_and_fair_paced_controller_allocate_nothing) {
    ckv::LogLimits limits; limits.records = 16; limits.record_bytes = 128;
    limits.retained_bytes = ckv::kLogMetadataBytes + 16384;
    ckv::widgets::LogModel model(limits);
    ckv::widgets::LogFilter filter(model, {2, 16});
    ckv::widgets::LogPanel panel(filter); panel.set_bounds({0, 0, 40, 8});
    ckv::LogMailbox mailbox(1, 1, {4, 128, ckv::LogOverflowPolicy::DropOldest});
    ckv::widgets::LogStreamReceiver receiver(model, mailbox, {32, 2});
    ckv::widgets::LogController controller({1, 1, 1, 10});
    controller.attach({&model, &filter, nullptr, &panel, nullptr, &receiver});
    ckv::LogMessage message; message.source = 1; message.epoch = 1;
    const std::array fields{ckv::LogField{"unit", "demo"}};
    message.set_record("record with metadata 中", {ckv::LogSeverity::Info, {}, fields});
    bool correct = true;
    allocations = 0; measuring = true;
    for (std::int64_t time = 0; time < 1000; ++time) {
        for (int batch = 0; batch < 5; ++batch)
            correct = (mailbox.offer(message) == ckv::LogOfferResult::Accepted) && correct;
        controller.step(time);
    }
    mailbox.close(ckv::LogSourceState::Ended, 0, 1000);
    for (std::int64_t time = 1000; time < 1100; ++time) controller.step(time);
    measuring = false;
    CK_CHECK(correct && receiver.status().state == ckv::LogSourceState::Ended);
    CK_CHECK(allocations == 0);
}
