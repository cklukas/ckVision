// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/process_stream.hpp"
#include "cvision/testing/cktest.hpp"
using namespace ckv;
using namespace ckv::core;
namespace {
ProcessStreamRequest valid_stream() {
    ProcessStreamRequest request; request.launch = ProcessLaunchSpec::program("/helper");
    request.stdout_source = 1; request.stderr_source = 2; return request;
}
}
CK_TEST(process_stream_requires_distinct_nonzero_source_ids_and_epochs) {
    auto request = valid_stream(); CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Valid);
    request.stderr_source = 1; CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Identity);
    request.stderr_source = 2; request.epoch = 0; CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Identity);
}
CK_TEST(process_stream_limits_launch_work_and_independent_mailbox_capacity) {
    auto request = valid_stream(); request.stdout_queue.batch_bytes = 64;
    CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Limits);
    request.chunk_bytes = 64; CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Valid);
    request.launch.arguments.resize(65); CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Limits);
    request.launch.arguments = {std::string(64 * 1024, 'x')}; CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Limits);
    request.launch.arguments = {std::string("a\0b", 3)}; CK_CHECK(validate_process_stream(request) == ProcessStreamValidation::Launch);
}
CK_TEST(process_stream_default_and_failure_observations_never_invent_success) {
    ProcessStreamStatus status; CK_CHECK(status.state == ProcessStreamState::Preparing && !status.exit);
    status.state = ProcessStreamState::Completed; status.exit = ProcessExitStatus{ProcessExitKind::Normal, 7};
    CK_CHECK(status.exit->code == 7 && status.error.domain == ProcessErrorDomain::None);
}
