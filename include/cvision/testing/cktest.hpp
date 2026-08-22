// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// cktest — ckVision's minimal, dependency-free unit-test harness.
#pragma once

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

struct Case {
    const char* name;
    const char* source;
    void (*fn)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> v;
    return v;
}

inline int& failures() {
    static int n = 0;
    return n;
}

inline const char*& current() {
    static const char* c = "";
    return c;
}

inline const char*& current_source() {
    static const char* source = "";
    return source;
}

inline const char*& executable() {
    static const char* path = "";
    return path;
}

inline bool& abort_child() {
    static bool value = false;
    return value;
}

inline bool add(const char* name, const char* source, void (*fn)()) {
    cases().push_back(Case{name, source, fn});
    return true;
}

inline void report_failure(const char* expr, const char* file, int line) {
    ++failures();
    std::fprintf(stderr, "FAIL %s: %s (%s:%d)\n", current(), expr, file, line);
}

inline const char* source_basename(const char* source) {
    const char* basename = source;
    for (const char* cursor = source; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') basename = cursor + 1;
    }
    return basename;
}

inline void report_expected_abort_failure(const char* file, int line, const char* detail) {
    report_failure(detail, file, line);
}

#if defined(_WIN32)

inline std::wstring widen_ascii(const char* text) {
    std::wstring result;
    while (*text != '\0') result.push_back(static_cast<wchar_t>(*text++));
    return result;
}

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

inline bool child_aborted() {
    wchar_t executable_path[MAX_PATH]{};
    const DWORD path_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (path_length == 0 || path_length == MAX_PATH) return false;

    const std::wstring path(executable_path, path_length);
    const std::wstring command = quote_windows_argument(path) + L" --suite " +
                                 quote_windows_argument(widen_ascii(source_basename(current_source()))) +
                                 L" --case " + quote_windows_argument(widen_ascii(current())) +
                                 L" --expect-abort-child";
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

inline bool child_aborted() {
    const pid_t child = ::fork();
    if (child < 0) return false;
    if (child == 0) {
        const char* const arguments[] = {executable(), "--suite", source_basename(current_source()), "--case",
                                         current(), "--expect-abort-child", nullptr};
        ::execvp(executable(), const_cast<char* const*>(arguments));
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(child, &status, 0) != child) return false;
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

#endif

template <typename Function>
inline void expect_abort(Function&& function, const char* file, int line) {
    if (abort_child()) {
        function();
        std::fprintf(stderr, "FAIL %s: expected contract violation did not terminate\n", current());
        std::_Exit(97);
    }
    if (!child_aborted()) report_expected_abort_failure(file, line, "expected contract violation to terminate");
}

inline void print_usage(const char* executable_name) {
    std::printf("usage: %s [--list] [--list-suites] [--filter text] [--suite filename] [--case name] "
                "[--shard index/count]\n",
                executable_name);
}

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
        } else if (std::strcmp(argument, "--expect-abort-child") == 0) {
            abort_child() = true;
        } else if (std::strcmp(argument, "--filter") == 0 || std::strcmp(argument, "--suite") == 0 ||
                   std::strcmp(argument, "--case") == 0 || std::strcmp(argument, "--shard") == 0) {
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
        std::printf("RUN %s:%s\n", source_basename(current_source()), current());
        std::fflush(stdout);
        test_case.fn();
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

#define CK_TEST(name)                                                                  \
    static void ck_test_##name();                                                      \
    [[maybe_unused]] static const bool ck_reg_##name =                                 \
        ::cktest::add(#name, __FILE__, &ck_test_##name);                               \
    static void ck_test_##name()

#define CK_CHECK(cond)                                                                 \
    do {                                                                               \
        if (!(cond)) ::cktest::report_failure(#cond, __FILE__, __LINE__);              \
    } while (0)

#define CK_EXPECT_ABORT(body)                                                          \
    ::cktest::expect_abort([&] body, __FILE__, __LINE__)

#define CKTEST_MAIN                                                                    \
    int main(int argc, char** argv) { return ::cktest::run_all(argc, argv); }
