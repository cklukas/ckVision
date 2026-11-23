// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "cvision/core/log_source.hpp"
#include "cvision/widgets/log_filter.hpp"
#include "cvision/widgets/log_model.hpp"
#include "cvision/widgets/log_search.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::size_t half = input.size() / 2;
    const std::string_view part_a(input.data(), half);
    const std::string_view part_b(input.data() + half, input.size() - half);

    const std::string_view name = part_a.empty() ? "field" : part_a.substr(0, 16);
    const std::array fields{ckv::LogField{name, part_b.substr(0, ckv::kLogMetadataBytes - name.size())}};
    const std::string_view record = part_a.empty() ? "sample log line" : part_a.substr(0, ckv::kLogBatchBytes);
    ckv::widgets::LogModel model({.records = 4, .retained_bytes = 24 * 1024, .record_bytes = 4096});
    ckv::fuzz::require(model.begin_source(1, 1));
    ckv::fuzz::require(model.append_record(record, {ckv::LogSeverity::Warning, {}, fields}));

    ckv::widgets::LogFilter filter(model);
    ckv::widgets::LogFilterQuery query;
    query.include = std::string(part_a);
    if ((size & 4U) != 0U) query.exclude = std::string(part_b);
    query.case_sensitive = (size & 1U) == 0U;
    query.severity_mask = ckv::widgets::kLogAllSeverities;
    if (!part_a.empty()) {
        query.fields[0] = ckv::widgets::LogFieldPredicate{
            std::string(name),
            std::string(part_b),
            ckv::widgets::LogFieldMatch::Contains};
        query.field_count = 1;
    }
    (void)filter.set_query(query);
    for (int i = 0; i < 4; ++i) (void)filter.step();

    ckv::widgets::LogSearch search(filter);
    if (search.set_query((size & 8U) != 0U ? part_b : part_a, (size & 2U) == 0U)) {
        (void)search.request(ckv::widgets::LogSearchDirection::Next);
        for (int i = 0; i < 4; ++i) (void)search.step();
        const auto match = search.take_result();
        if (match.has_value()) {
            ckv::fuzz::require(match->range.begin <= match->range.end);
        }
    }
    return 0;
}
