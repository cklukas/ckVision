// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cwchar>

namespace {
std::uint64_t cpu_ticks() {
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!::GetProcessTimes(::GetCurrentProcess(), &creation, &exit, &kernel, &user)) return 0;
    return (static_cast<std::uint64_t>(kernel.dwHighDateTime) << 32) + kernel.dwLowDateTime +
           (static_cast<std::uint64_t>(user.dwHighDateTime) << 32) + user.dwLowDateTime;
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 80;
    const HANDLE ready = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(std::wcstoull(argv[1], nullptr, 10)));
    const HANDLE stop = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(std::wcstoull(argv[2], nullptr, 10)));
    constexpr std::size_t bytes = 4 * 1024 * 1024;
    auto* const allocation = static_cast<volatile unsigned char*>(
        ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (allocation == nullptr) return 81;
    for (std::size_t offset = 0; offset < bytes; offset += 4096) allocation[offset] = 0x5A;
    // Positive CPU is established before acknowledgement, not assumed from
    // a wall-clock sleep. The stop event keeps resident memory live for sampling.
    const std::uint64_t initial = cpu_ticks();
    const ULONGLONG deadline = ::GetTickCount64() + 5000;
    while (cpu_ticks() - initial < 500'000) {
        if (::GetTickCount64() >= deadline) return 82;
        for (std::size_t index = 0; index < 1000; ++index) allocation[index] = static_cast<unsigned char>(index);
    }
    if (!::SetEvent(ready)) return 83;
    const DWORD waited = ::WaitForSingleObject(stop, 15'000);
    (void)::VirtualFree(const_cast<unsigned char*>(allocation), 0, MEM_RELEASE);
    return waited == WAIT_OBJECT_0 ? 0 : 84;
}
