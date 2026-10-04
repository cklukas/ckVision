// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Independent real binary-pipe child, not an in-process recorder.
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
bool write_bytes(int stream, std::string_view text) {
    std::size_t offset = 0;
    while (offset < text.size()) {
#if defined(_WIN32)
        DWORD count = 0;
        const HANDLE handle = ::GetStdHandle(stream == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
        if (!::WriteFile(handle, text.data() + offset, static_cast<DWORD>(text.size() - offset), &count, nullptr) || count == 0) return false;
#else
        const auto count = ::write(stream, text.data() + offset, text.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
#endif
        offset += static_cast<std::size_t>(count);
    }
    return true;
}
int read_bytes(char* bytes, std::size_t size) {
#if defined(_WIN32)
    DWORD count = 0;
    if (!::ReadFile(::GetStdHandle(STD_INPUT_HANDLE), bytes, static_cast<DWORD>(size), &count, nullptr))
        return ::GetLastError() == ERROR_BROKEN_PIPE ? 0 : -1;
    return static_cast<int>(count);
#else
    const auto count = ::read(0, bytes, size);
    if (count < 0 && errno == EINTR) return read_bytes(bytes, size);
    return static_cast<int>(count);
#endif
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 90;
    const std::string_view mode(argv[1]);
#if defined(_WIN32)
    if (mode == "wide-exit") ::ExitProcess(0xffffffffUL);
#else
    if (mode == "signal-exit") { (void)::raise(SIGTERM); return 87; }
#endif
    if (mode == "descendant-hold" && argc == 3) {
#if defined(_WIN32)
        std::uintptr_t ready = 0;
        const auto parsed = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), ready);
        if (parsed.ec != std::errc{}) return 80;
        DWORD written = 0;
        const HANDLE pipe = reinterpret_cast<HANDLE>(ready);
        if (!::WriteFile(pipe, "R", 1, &written, nullptr) || written != 1) return 81;
        (void)::CloseHandle(pipe);
#endif
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    if ((mode == "descendant" || mode == "descendant-idle") && argc == 3) {
        std::uint64_t descendant = 0;
#if defined(_WIN32)
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE ready_read = nullptr, ready_write = nullptr;
        if (!::CreatePipe(&ready_read, &ready_write, &security, 0)) return 82;
        (void)::SetHandleInformation(ready_read, HANDLE_FLAG_INHERIT, 0);
        std::array<wchar_t, 32768> executable{};
        const DWORD count = ::GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (count == 0 || count >= executable.size()) return 83;
        std::wstring command = L"\"" + std::wstring(executable.data()) + L"\" descendant-hold " +
                               std::to_wstring(reinterpret_cast<std::uintptr_t>(ready_write));
        command.push_back(L'\0');
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION process{};
        if (!::CreateProcessW(executable.data(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &startup, &process)) return 84;
        (void)::CloseHandle(ready_write);
        char ready = 0;
        DWORD read = 0;
        const bool live = ::ReadFile(ready_read, &ready, 1, &read, nullptr) && read == 1 && ready == 'R' &&
                          ::WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT;
        (void)::CloseHandle(ready_read);
        descendant = process.dwProcessId;
        (void)::CloseHandle(process.hThread);
        (void)::CloseHandle(process.hProcess);
        if (!live) return 85;
#else
        int readiness[2];
        if (::pipe(readiness) != 0) return 82;
        const pid_t child = ::fork();
        if (child < 0) return 83;
        if (child == 0) {
            (void)::close(readiness[0]);
            const auto wrote = ::write(readiness[1], "R", 1);
            (void)::close(readiness[1]);
            if (wrote != 1) ::_exit(84);
            timespec duration{10, 0};
            while (::nanosleep(&duration, &duration) != 0 && errno == EINTR) {}
            ::_exit(0);
        }
        (void)::close(readiness[1]);
        char ready = 0;
        const auto count = ::read(readiness[0], &ready, 1);
        (void)::close(readiness[0]);
        if (count != 1 || ready != 'R') return 85;
        descendant = static_cast<std::uint64_t>(child);
#endif
        if (!write_bytes(1, "READY:" + std::to_string(descendant) + "\n")) return 86;
        if (mode == "descendant-idle") std::this_thread::sleep_for(std::chrono::seconds(10));
        return std::string_view(argv[2]) == "fail" ? 7 : 0;
    }
    if (mode == "idle" || mode == "interrupt-idle") {
#if defined(_WIN32)
        const auto identity = static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
        const auto identity = static_cast<std::uint64_t>(::getpid());
#endif
        if (!write_bytes(1, "ROOT:" + std::to_string(identity) + "\n")) return 88;
#if !defined(_WIN32)
        if (mode == "interrupt-idle" && argc == 3) {
            int parent = 0;
            const auto parsed = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), parent);
            if (parsed.ec != std::errc{} || parent <= 0) return 89;
            // Wake the parent's native wait without transferring another byte.
            for (int count = 0; count < 200; ++count) {
                if (::kill(static_cast<pid_t>(parent), SIGUSR1) != 0) return 89;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return 0;
        }
#endif
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    if (mode == "nonzero") { return write_bytes(2, "helper failed\n") ? 7 : 91; }
    if (mode == "early-close") {
#if defined(_WIN32)
        (void)::CloseHandle(::GetStdHandle(STD_INPUT_HANDLE));
#else
        (void)::close(0);
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    if (mode == "sentinel" && argc == 3) {
        std::uintptr_t value = 0;
        const auto parsed = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), value);
        if (parsed.ec != std::errc{}) return 92;
#if defined(_WIN32)
        DWORD flags = 0;
        const bool absent = ::GetHandleInformation(reinterpret_cast<HANDLE>(value), &flags) == FALSE;
#else
        const bool absent = ::fcntl(static_cast<int>(value), F_GETFD) < 0 && errno == EBADF;
#endif
        return write_bytes(1, absent ? "ABSENT" : "INHERITED") && absent ? 0 : 93;
    }
    if (mode == "progress") {
        for (int count = 0; count < 12; ++count) {
            if (!write_bytes(1, "p")) return 94;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
    if (mode == "flood") {
        const std::string output(512 * 1024, 'O'), errors(512 * 1024, 'E');
        if (!write_bytes(1, output) || !write_bytes(2, errors)) return 95;
    }
    std::size_t observed = 0;
    std::array<char, 8192> input{};
    for (;;) {
        const int count = read_bytes(input.data(), input.size());
        if (count < 0) return 96;
        if (count == 0) break;
        observed += static_cast<std::size_t>(count);
        if (mode == "echo" && !write_bytes(1, std::string_view(input.data(), static_cast<std::size_t>(count)))) return 97;
    }
    if (mode == "echo") return write_bytes(2, std::string_view("diag\0\xff\r\n", 8)) ? 0 : 98;
    if (mode == "flood" && argc == 3) return std::to_string(observed) == argv[2] ? 0 : 99;
    return 0;
}
