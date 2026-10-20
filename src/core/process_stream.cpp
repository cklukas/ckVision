// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/process_stream.hpp"

namespace ckv::core {
ProcessStreamValidation validate_process_stream(const ProcessStreamRequest& request) noexcept {
    if (!request.stdout_source || !request.stderr_source || request.stdout_source == request.stderr_source || !request.epoch)
        return ProcessStreamValidation::Identity;
    if (!request.stdout_queue.valid() || !request.stderr_queue.valid() || !request.chunk_bytes ||
        request.chunk_bytes > 16 * 1024 || request.chunk_bytes > request.stdout_queue.batch_bytes ||
        request.chunk_bytes > request.stderr_queue.batch_bytes || request.launch.arguments.size() > 64 ||
        request.launch.environment.size() > 128) return ProcessStreamValidation::Limits;
    std::size_t bytes = 0;
    const auto admit = [&](std::size_t size) {
        if (size >= 64 * 1024 - bytes) return false;
        bytes += size + 1; return true;
    };
    if (!admit(request.launch.executable.size()) || !admit(request.launch.working_directory.size()) ||
        !admit(request.launch.argv0.size())) return ProcessStreamValidation::Limits;
    for (const auto& arg : request.launch.arguments) if (!admit(arg.size())) return ProcessStreamValidation::Limits;
    for (const auto& entry : request.launch.environment)
        if (!admit(entry.first.size()) || !admit(entry.second.size())) return ProcessStreamValidation::Limits;
    if (request.launch.windows_command && request.launch.windows_command->command &&
        !admit(request.launch.windows_command->command->size())) return ProcessStreamValidation::Limits;
    return validate_process_launch(request.launch) == ProcessLaunchValidation::Valid ?
        ProcessStreamValidation::Valid : ProcessStreamValidation::Launch;
}
}  // namespace ckv::core
