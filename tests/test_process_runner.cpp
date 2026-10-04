// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <limits>
#include <type_traits>
#include "cvision/core/process_runner.hpp"
#include "cvision/core/terminal_subsession.hpp"
#include "cvision/testing/cktest.hpp"

using namespace ckv::core;

CK_TEST(process_launch_terminal_uses_the_shared_invocation_without_terminal_policy_leaking) {
    static_assert(std::is_base_of_v<ProcessLaunchSpec, TerminalLaunchSpec>);
    auto terminal = TerminalLaunchSpec::program("/example", {"", "two words"});
    ProcessLaunchSpec& invocation = terminal;
    invocation.argv0 = "-example";
    invocation.environment_policy = ProcessEnvironmentPolicy::ExplicitOnly;
    CK_CHECK(terminal.executable == "/example");
    CK_CHECK(terminal.arguments == std::vector<std::string>({"", "two words"}));
    CK_CHECK(terminal.argv0 == "-example");
    CK_CHECK(terminal.environment_policy == ProcessEnvironmentPolicy::ExplicitOnly);
    CK_CHECK(terminal.exit_policy == TerminalExitPolicy::Unspecified);
    CK_CHECK(validate_process_launch(invocation) == ProcessLaunchValidation::Valid);
}

CK_TEST(process_launch_validation_refuses_native_string_truncation_and_contradictions) {
    const auto valid = ProcessLaunchSpec::program("/example", {"", std::string("\xff", 1)});
    CK_CHECK(validate_process_launch(valid) == ProcessLaunchValidation::Valid);
    for (int field = 0; field < 7; ++field) {
        auto spec = valid;
        const std::string nul("a\0b", 3);
        switch (field) {
            case 0: spec.executable = nul; break;
            case 1: spec.working_directory = nul; break;
            case 2: spec.argv0 = nul; break;
            case 3: spec.arguments = {nul}; break;
            case 4: spec.environment = {{nul, "value"}}; break;
            case 5: spec.environment = {{"NAME", nul}}; break;
            case 6: spec.environment = {{"NAME", "one"}, {"NAME", "two"}}; break;
            default: break;
        }
        CK_CHECK(validate_process_launch(spec) != ProcessLaunchValidation::Valid);
    }
    auto spec = valid;
    spec.environment = {{"Name", "one"}, {"NAME", "two"}};
    // Windows rejects these at its case-insensitive boundary; POSIX allows it.
    CK_CHECK(validate_process_launch(spec) == ProcessLaunchValidation::Valid);
    spec.environment = {{"", "value"}};
    CK_CHECK(validate_process_launch(spec) == ProcessLaunchValidation::InvalidEnvironment);
    spec.environment = {{"A=B", "value"}};
    CK_CHECK(validate_process_launch(spec) == ProcessLaunchValidation::InvalidEnvironment);
    spec.environment.clear();
    spec.environment_policy = static_cast<ProcessEnvironmentPolicy>(99);
    CK_CHECK(validate_process_launch(spec) == ProcessLaunchValidation::InvalidEnvironment);
}

CK_TEST(process_launch_explicit_command_syntax_is_separate_from_argument_data) {
    auto command = ProcessLaunchSpec::windows_command_processor("cmd.exe");
    CK_CHECK(command.windows_command.has_value());
    CK_CHECK(!command.windows_command->command);
    CK_CHECK(validate_process_launch(command) == ProcessLaunchValidation::Valid);
    command = ProcessLaunchSpec::windows_command_processor("cmd.exe", "");
    CK_CHECK(command.windows_command->command.has_value());
    CK_CHECK(command.windows_command->command->empty());
    CK_CHECK(validate_process_launch(command) == ProcessLaunchValidation::Valid);
    command.arguments = {"ordinary data"};
    CK_CHECK(validate_process_launch(command) == ProcessLaunchValidation::ConflictingCommandForms);
    command.arguments.clear();
    command.argv0 = "name";
    CK_CHECK(validate_process_launch(command) == ProcessLaunchValidation::ConflictingCommandForms);
    command.argv0.clear();
    command.windows_command->command = std::string("x\0y", 3);
    CK_CHECK(validate_process_launch(command) == ProcessLaunchValidation::InvalidCommandText);
}

CK_TEST(process_runner_requires_an_explicit_descendant_policy_and_positive_idle_budget) {
    ProcessRunRequest request;
    request.launch = ProcessLaunchSpec::program("/example");
    CK_CHECK(!valid_process_request(request));
    request.descendants = ProcessDescendantPolicy::ReleaseOnSuccess;
    CK_CHECK(valid_process_request(request));
    request.descendants = ProcessDescendantPolicy::TerminateOnCompletion;
    request.max_stdout_bytes = request.max_stderr_bytes = 0;
    request.input = std::string_view("\0\xff", 2);
    CK_CHECK(valid_process_request(request));
    request.idle_budget_nanos = 0;
    CK_CHECK(!valid_process_request(request));
    request.idle_budget_nanos = -1;
    CK_CHECK(!valid_process_request(request));
}

CK_TEST(process_runner_absent_partial_nonzero_and_signaled_exits_are_never_success) {
    ProcessRunResult result;
    CK_CHECK(!result.successful());
    result.state = ProcessRunState::Completed;
    CK_CHECK(!result.successful());
    result.exit = ProcessExitStatus{};
    CK_CHECK(result.successful());
    result.input_bytes_total = 3;
    CK_CHECK(!result.successful());
    result.input_bytes_written = 3;
    CK_CHECK(result.successful());
    result.exit->code = std::numeric_limits<std::uint32_t>::max();
    CK_CHECK(result.exit->code == 4'294'967'295LL);
    CK_CHECK(!result.successful());
    result.exit = ProcessExitStatus{ProcessExitKind::Signal, 0};
    CK_CHECK(!result.successful());
    result.exit = ProcessExitStatus{};
    result.error = {ProcessErrorDomain::Win32, 5};
    CK_CHECK(!result.successful());
    result.error = {};
    result.state = ProcessRunState::ExitStatusUnavailable;
    CK_CHECK(!result.successful());
}

CK_TEST(process_runner_is_injected_and_keeps_binary_stdin_separate_from_launch_arguments) {
    class Fake final : public ProcessRunner {
    public:
        ProcessRunResult run(const ProcessRunRequest& request) override {
            CK_CHECK(valid_process_request(request));
            CK_CHECK(request.launch.arguments == std::vector<std::string>({"unchanged"}));
            ProcessRunResult result;
            result.state = ProcessRunState::Completed;
            result.input_bytes_total = result.input_bytes_written = request.input.size();
            result.stdout_capture.bytes = std::string(request.input);
            result.stderr_capture.bytes = "private diagnostic";
            result.exit = ProcessExitStatus{};
            return result;
        }
    } fake;
    ProcessRunner& injected = fake;
    ProcessRunRequest request;
    request.launch = ProcessLaunchSpec::program("/example", {"unchanged"});
    const std::string bytes("x\0\xffy", 4);
    request.input = bytes;
    request.descendants = ProcessDescendantPolicy::ReleaseOnSuccess;
    const auto result = injected.run(request);
    CK_CHECK(result.successful());
    CK_CHECK(result.stdout_capture.bytes == bytes);
    CK_CHECK(result.stderr_capture.bytes == "private diagnostic");
}
