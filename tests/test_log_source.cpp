// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/log_source.hpp"

#include <array>
#include <limits>

#include "cvision/testing/cktest.hpp"

CK_TEST(log_limits_fit_worst_case_record_and_reject_overflow) {
    ckv::LogLimits limits;
    CK_CHECK(limits.valid());
    limits.records = 0;
    CK_CHECK(!limits.valid());
    limits = {};
    limits.record_bytes = std::numeric_limits<std::size_t>::max();
    CK_CHECK(!limits.valid());
    limits = {};
    limits.retained_bytes = limits.record_bytes * 4 + ckv::kLogMetadataBytes - 1;
    CK_CHECK(!limits.valid());
    ++limits.retained_bytes;
    CK_CHECK(limits.valid());
    limits.batch_bytes = ckv::kLogBatchBytes + 1;
    CK_CHECK(!limits.valid());
    limits = {};
    limits.pending_batches = std::numeric_limits<std::size_t>::max();
    CK_CHECK(!limits.valid());
    limits = {};
    limits.frame_interval_nanos = 0;
    CK_CHECK(!limits.valid());
    limits = {};
    limits.retained_bytes = std::numeric_limits<std::size_t>::max();
    CK_CHECK(!limits.valid());
}

CK_TEST(log_metadata_is_bounded_and_names_are_unique) {
    std::array fields{ckv::LogField{"unit", "demo"}, ckv::LogField{"pid", "42"}};
    ckv::LogMetadata metadata;
    metadata.fields = fields;
    CK_CHECK(metadata.valid());
    fields[1].name = "unit";
    CK_CHECK(!metadata.valid());
    fields[1].name = "";
    CK_CHECK(!metadata.valid());
    fields[1].name = "pid";
    const std::string too_long(ckv::kLogMetadataBytes, 'x');
    fields[1].value = too_long;
    CK_CHECK(!metadata.valid());
    metadata.fields = {};
    metadata.severity = static_cast<ckv::LogSeverity>(255);
    CK_CHECK(!metadata.valid());
}

CK_TEST(log_file_requests_use_explicit_paths_revisions_and_bounded_ranges) {
    ckv::LogFileRequest request;
    CK_CHECK(!request.valid());
    request.revision = 1;
    request.path = "/logs/demo";
    CK_CHECK(request.valid());
    request.path = "relative.log";
    CK_CHECK(!request.valid());
    request.path = "C:/logs/demo";
    CK_CHECK(request.valid());
    request.path = std::string("/a\0b", 4);
    CK_CHECK(!request.valid());
    request.path.clear();
    request.operation = ckv::LogFileOperation::Read;
    CK_CHECK(request.valid());
    request.offset = std::numeric_limits<std::uint64_t>::max();
    CK_CHECK(!request.valid());
    request.offset = 0;
    request.bytes = 0;
    CK_CHECK(!request.valid());
}

CK_TEST(log_file_results_transfer_only_valid_payload_and_reset_metadata) {
    ckv::LogFileResult result, copy;
    result.revision = 3; result.operation = ckv::LogFileOperation::Read;
    result.writable_bytes()[0] = 'x';
    CK_CHECK(result.set_byte_count(1));
    CK_CHECK(!result.set_byte_count(ckv::kLogBatchBytes + 1));
    copy.copy_from(result);
    CK_CHECK(copy.revision == 3); CK_CHECK(copy.operation == ckv::LogFileOperation::Read);
    CK_CHECK(copy.bytes().size() == 1); CK_CHECK(copy.bytes()[0] == 'x');
    copy.reset(); CK_CHECK(copy.revision == 0); CK_CHECK(copy.bytes().empty());
    CK_CHECK(copy.state == ckv::LogSourceState::Pending);
    CK_CHECK((ckv::LogFileIdentity{1, 2, 3} != ckv::LogFileIdentity{1, 2, 4}));
}
