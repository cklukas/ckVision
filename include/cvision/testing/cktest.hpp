// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// cktest — ckVision's minimal, dependency-free unit-test harness.
#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <csignal>
#include <string>
#include <vector>

#if defined(_WIN32)
// windows.h defines min()/max() macros and the legacy `near`/`far` keywords,
// which collide with std::min/std::max and with ordinary identifiers in the
// tests that include this header. NOMINMAX and WIN32_LEAN_AND_MEAN keep those
// out; MSVC's C2589 "illegal token on right side of '::'" on std::max is the
// symptom when they are not.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cktest {

// One registered test: its name, the source file that defines it (__FILE__, whose basename
// is the suite name), and the body. Both strings must have static storage duration.
struct Case {
    // The test's name as written in CK_TEST; --case and --filter match against it.
    const char* name;
    // The defining file as the compiler spelled __FILE__; --suite matches its basename.
    const char* source;
    // The test body. It reports failures through CK_CHECK and returns normally.
    void (*fn)();
};

// Every registered case, in registration order (static initialization order, so file order
// within one translation unit). A function-local static, so registration from any
// translation unit's static initializers is safe.
inline std::vector<Case>& cases() {
    static std::vector<Case> v;
    return v;
}

// How many checks have failed so far in this process, across all cases.
inline int& failures() {
    static int n = 0;
    return n;
}

// The name of the case run_all() is executing; an empty string before the first case
// starts. Failure messages and expected-abort children use it.
inline const char*& current() {
    static const char* c = "";
    return c;
}

// The source file (__FILE__) of the case run_all() is executing; an empty string before the
// first case starts. An expected-abort child is selected by its basename.
inline const char*& current_source() {
    static const char* source = "";
    return source;
}

// The program path run_all() received as argv[0], which an expected-abort child re-runs.
inline const char*& executable() {
    static const char* path = "";
    return path;
}

// True in a child started by --expect-abort-child: the process whose job is to run one
// expected-abort body and die.
inline bool& abort_child() {
    static bool value = false;
    return value;
}

// In such a child, which CK_EXPECT_ABORT of the case it was started for, counted from zero
// in the order the case reaches them.
inline std::size_t& abort_child_target() {
    static std::size_t target = 0;
    return target;
}

// How many CK_EXPECT_ABORTs the running case has reached so far; run_all() resets it before
// each case. Their ordinals are what ties a child to the one it was started for.
inline std::size_t& abort_ordinal() {
    static std::size_t ordinal = 0;
    return ordinal;
}

// Registers a case and returns true, so a CK_TEST can call it from a static initializer.
inline bool add(const char* name, const char* source, void (*fn)()) {
    cases().push_back(Case{name, source, fn});
    return true;
}

// Counts one failure and prints "FAIL <case>: <expr> (<file>:<line>)" to stderr. The case
// keeps running; the failure decides run_all()'s exit status.
inline void report_failure(const char* expr, const char* file, int line) {
    ++failures();
    std::fprintf(stderr, "FAIL %s: %s (%s:%d)\n", current(), expr, file, line);
}

// The part of `source` after its last '/' or '\', pointing into the same string. This is
// the suite name --suite and --list use.
inline const char* source_basename(const char* source) {
    const char* basename = source;
    for (const char* cursor = source; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') basename = cursor + 1;
    }
    return basename;
}

// Reports a CK_EXPECT_ABORT whose body did not terminate, as an ordinary failure at the
// macro's location with `detail` as the message.
inline void report_expected_abort_failure(const char* file, int line, const char* detail) {
    report_failure(detail, file, line);
}

#if defined(_WIN32)

// Widens each byte to one wchar_t. Correct only for ASCII, which is all it is given: case
// names and source basenames.
inline std::wstring widen_ascii(const char* text) {
    std::wstring result;
    while (*text != '\0') result.push_back(static_cast<wchar_t>(*text++));
    return result;
}

// Quotes `argument` for a Windows command line so the standard argument parser reads it back
// unchanged: always wrapped in double quotes, with embedded quotes and the backslashes that
// precede a quote (or the closing quote) escaped.
inline std::wstring quote_windows_argument(const std::wstring& argument) {
    std::wstring quoted;
    quoted.push_back(L'\"');
    std::size_t slash_count = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++slash_count;
            continue;
        }
        if (character == L'\"') quoted.append(slash_count * 2 + 1, L'\\');
        else quoted.append(slash_count, L'\\');
        slash_count = 0;
        quoted.push_back(character);
    }
    quoted.append(slash_count * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

// Re-runs the current case in a child process (this executable, located through
// GetModuleFileNameW, with --suite, --case and --expect-abort-child `ordinal`) and waits for
// it. True when the child ended with a nonzero exit code other than 97, the harness's "body
// returned" sentinel. False when the child cannot be started, including an executable path
// of MAX_PATH characters or more.
inline bool child_aborted(std::size_t ordinal) {
    wchar_t executable_path[MAX_PATH]{};
    const DWORD path_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (path_length == 0 || path_length == MAX_PATH) return false;

    const std::wstring path(executable_path, path_length);
    const std::wstring command = quote_windows_argument(path) + L" --suite " +
                                 quote_windows_argument(widen_ascii(source_basename(current_source()))) +
                                 L" --case " + quote_windows_argument(widen_ascii(current())) +
                                 L" --expect-abort-child " + std::to_wstring(ordinal);
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(path.c_str(), command_buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &startup, &process))
        return false;
    const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0;
    const bool received_status = wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    // 97 is our explicit "body returned" sentinel, so it is not evidence of
    // a contract termination on platforms where abrupt termination is exposed
    // as an implementation-defined process status.
    return received_status && exit_code != 0 && exit_code != 97;
}

#else

// Re-runs the current case in a forked child that executes executable() with --suite,
// --case and --expect-abort-child `ordinal`, and waits for it. True only when the child was
// killed by SIGABRT; any other ending, including a failed fork or exec, is false.
inline bool child_aborted(std::size_t ordinal) {
    // Built before the fork: the child only execs.
    const std::string target = std::to_string(ordinal);
    const pid_t child = ::fork();
    if (child < 0) return false;
    if (child == 0) {
        const char* const arguments[] = {executable(), "--suite", source_basename(current_source()), "--case",
                                         current(), "--expect-abort-child", target.c_str(), nullptr};
        ::execvp(executable(), const_cast<char* const*>(arguments));
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(child, &status, 0) != child) return false;
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

#endif

// The engine behind CK_EXPECT_ABORT. In the parent it runs nothing itself: it asks
// child_aborted() and records a failure at `file`:`line` unless the child terminated.
//
// The child re-runs the whole case from its start and acts only on the CK_EXPECT_ABORT it
// was started for, identified by its ordinal in the case. It passes over the earlier ones
// without running their bodies, exactly as the parent did, so a case may hold several and
// each is judged by its own body. For its own one it calls `function`; should that return,
// it prints a failure and exits with 97. Code before a CK_EXPECT_ABORT runs in both
// processes.
template <typename Function>
inline void expect_abort(Function&& function, const char* file, int line) {
    const std::size_t ordinal = abort_ordinal()++;
    if (abort_child()) {
        if (ordinal != abort_child_target()) return;
        function();
        std::fprintf(stderr, "FAIL %s: expected contract violation did not terminate\n", current());
        std::_Exit(97);
    }
    if (!child_aborted(ordinal))
        report_expected_abort_failure(file, line, "expected contract violation to terminate");
}

// Prints the command-line synopsis to stdout (--help).
inline void print_usage(const char* executable_name) {
    std::printf("usage: %s [--list] [--list-suites] [--filter text] [--suite filename] [--case name] "
                "[--shard index/count]\n",
                executable_name);
}

// Parses a --shard value "index/count" of decimal numbers with 0 <= index < count. On
// success stores both and returns true; otherwise returns false and leaves them untouched.
inline bool parse_shard(const char* argument, std::size_t& index, std::size_t& count) {
    char* separator = nullptr;
    const unsigned long parsed_index = std::strtoul(argument, &separator, 10);
    if (separator == argument || *separator != '/') return false;
    char* end = nullptr;
    const unsigned long parsed_count = std::strtoul(separator + 1, &end, 10);
    if (*end != '\0' || parsed_count == 0 || parsed_index >= parsed_count) return false;
    index = parsed_index;
    count = parsed_count;
    return true;
}

// The test program's main (see CKTEST_MAIN). Selects cases by --filter (a substring of the
// name; the CKTEST_FILTER environment variable supplies a default), --suite (the source
// basename), --case (the exact name) and --shard (every count-th selected case, starting at
// index), prints "RUN suite:name" before each, and runs them in registration order.
// --list and --list-suites print every registered case or suite, ignoring the selection,
// instead of running; --expect-abort-child <ordinal> is internal to CK_EXPECT_ABORT.
// Returns 0 when every selected case passed (or after --help or a listing), 1 when any
// check failed, and 2 for a bad argument or a selection that matched nothing.
inline int run_all(int argc, char** argv) {
    executable() = argc > 0 ? argv[0] : "cvision_tests";
    const char* filter = std::getenv("CKTEST_FILTER");
    const char* suite = nullptr;
    const char* case_name = nullptr;
    std::size_t shard_index = 0;
    std::size_t shard_count = 0;
    bool list_cases = false;
    bool list_suites = false;

    for (int index = 1; index < argc; ++index) {
        const char* const argument = argv[index];
        if (std::strcmp(argument, "--list") == 0) {
            list_cases = true;
        } else if (std::strcmp(argument, "--list-suites") == 0) {
            list_suites = true;
        } else if (std::strcmp(argument, "--filter") == 0 || std::strcmp(argument, "--suite") == 0 ||
                   std::strcmp(argument, "--case") == 0 || std::strcmp(argument, "--shard") == 0 ||
                   std::strcmp(argument, "--expect-abort-child") == 0) {
            if (index + 1 == argc) {
                std::fprintf(stderr, "cktest: %s requires a value\n", argument);
                return 2;
            }
            const char* const value = argv[++index];
            if (std::strcmp(argument, "--filter") == 0) filter = value;
            if (std::strcmp(argument, "--suite") == 0) suite = value;
            if (std::strcmp(argument, "--case") == 0) case_name = value;
            if (std::strcmp(argument, "--shard") == 0 && !parse_shard(value, shard_index, shard_count)) {
                std::fprintf(stderr, "cktest: --shard must be index/count with 0 <= index < count\n");
                return 2;
            }
            if (std::strcmp(argument, "--expect-abort-child") == 0) {
                char* end = nullptr;
                const unsigned long long target = std::strtoull(value, &end, 10);
                if (end == value || *end != '\0') {
                    std::fprintf(stderr, "cktest: --expect-abort-child must be an ordinal\n");
                    return 2;
                }
                abort_child() = true;
                abort_child_target() = static_cast<std::size_t>(target);
            }
        } else if (std::strcmp(argument, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "cktest: unknown argument %s\n", argument);
            return 2;
        }
    }

    if (list_cases) {
        for (const Case& test_case : cases())
            std::printf("%s\t%s\n", source_basename(test_case.source), test_case.name);
        return 0;
    }
    if (list_suites) {
        std::vector<const char*> suites;
        for (const Case& test_case : cases()) {
            const char* const candidate = source_basename(test_case.source);
            bool seen = false;
            for (const char* existing : suites) {
                if (std::strcmp(existing, candidate) == 0) {
                    seen = true;
                    break;
                }
            }
            if (!seen) suites.push_back(candidate);
        }
        for (const char* existing : suites) std::printf("%s\n", existing);
        return 0;
    }

    std::size_t executed = 0;
    std::size_t candidate_index = 0;
    for (const Case& test_case : cases()) {
        if (filter != nullptr && std::strstr(test_case.name, filter) == nullptr) continue;
        if (suite != nullptr && std::strcmp(source_basename(test_case.source), suite) != 0) continue;
        if (case_name != nullptr && std::strcmp(test_case.name, case_name) != 0) continue;
        if (shard_count != 0 && candidate_index++ % shard_count != shard_index) continue;
        current() = test_case.name;
        current_source() = test_case.source;
        abort_ordinal() = 0;
        std::printf("RUN %s:%s\n", source_basename(current_source()), current());
        std::fflush(stdout);
        test_case.fn();
        if (abort_child()) {
            // The case finished without reaching the CK_EXPECT_ABORT this child was started
            // for, so nothing terminated. That is a failure, not a pass.
            std::fprintf(stderr, "FAIL %s: expected contract violation was never reached\n", current());
            std::_Exit(97);
        }
        ++executed;
    }
    if (executed == 0) {
        std::fprintf(stderr, "cktest: no tests matched the requested selection\n");
        return 2;
    }
    if (failures() == 0) {
        std::printf("cktest: %zu tests, all passed\n", executed);
        return 0;
    }
    std::fprintf(stderr, "cktest: %d failure(s)\n", failures());
    return 1;
}

}  // namespace cktest

// Defines and registers a test case; the function body follows the macro. `name` must be a
// valid identifier, unique within its translation unit.
#define CK_TEST(name)                                                                  \
    static void ck_test_##name();                                                      \
    [[maybe_unused]] static const bool ck_reg_##name =                                 \
        ::cktest::add(#name, __FILE__, &ck_test_##name);                               \
    static void ck_test_##name()

// Records a failure with the condition's text and location when `cond` is false, and
// carries on with the case.
#define CK_CHECK(cond)                                                                 \
    do {                                                                               \
        if (!(cond)) ::cktest::report_failure(#cond, __FILE__, __LINE__);              \
    } while (0)

// Asserts that `body`, a braced statement block, terminates the process as a contract
// violation does: by SIGABRT on POSIX, by a nonzero exit code other than 97 on Windows. It
// runs in a child process of its own; see expect_abort() for how a case with several works.
#define CK_EXPECT_ABORT(body)                                                          \
    ::cktest::expect_abort([&] body, __FILE__, __LINE__)

// Defines main() as run_all(); place it in exactly one translation unit of a test program.
#define CKTEST_MAIN                                                                    \
    int main(int argc, char** argv) { return ::cktest::run_all(argc, argv); }
