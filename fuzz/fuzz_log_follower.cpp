// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "fuzz_common.hpp"
#include "../tests/log_file_test_fixture.hpp"
#include "cvision/widgets/log_follower.hpp"
#include "cvision/core/utf8.hpp"
#include <algorithm>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ckv; size = std::min<std::size_t>(size, 4096);
    LogLimits limits; limits.records = 16; limits.record_bytes = 128; limits.retained_bytes = 16384;
    widgets::LogModel model(limits); testing::ScriptedLogFiles files;
    widgets::LogFollowOptions options; options.start = LogStartMode::Beginning; options.read_bytes = 64; options.records_per_step = 4; options.drain_bytes = 256; options.reconcile_nanos = 3; options.retry_nanos = 2;
    widgets::LogFollower follower(model, files.open_session(), options); follower.start("/fuzz/log", 1, 0);
    std::int64_t now = 0; std::uint64_t identity = 1, version = 1; std::size_t requests = 0;
    for (std::size_t index = 0; index < std::min<std::size_t>(size, 128); ++index) {
        now += 1 + data[index]; const auto operation = data[index] % 8;
        if (operation == 0) ++identity;
        if (operation == 1) ++version;
        if (operation == 2) files.hint(true);
        if (operation == 3) follower.retry(now);
        if (files.request_count() != requests) {
            requests = files.request_count(); const auto request = files.requested();
            const auto revision = operation == 4 && request.revision > 1 ? request.revision - 1 : request.revision;
            const auto source_size = static_cast<std::uint64_t>(data[index]) * 16;
            const auto count = std::min({size - index, request.bytes, std::size_t{64}});
            const auto bytes = request.operation == LogFileOperation::Read ? std::string_view(reinterpret_cast<const char*>(data + index), count) : std::string_view{};
            const auto state = operation == 5 ? LogSourceState::Missing : operation == 6 ? LogSourceState::Denied : LogSourceState::Following;
            files.publish(revision, state == LogSourceState::Following ? std::optional{LogFileObservation{{1, identity, 0}, source_size, version}} : std::nullopt, bytes, state);
        }
        follower.step(now); fuzz::require(model.size() <= limits.records && model.retained_payload_bytes() <= limits.retained_bytes);
        for (std::size_t record = 0; record < model.size(); ++record) fuzz::require(utf8::is_valid(model.at(record)->text));
    }
    follower.stop(); return 0;
}
