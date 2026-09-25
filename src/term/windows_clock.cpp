// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_clock.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ckv::term {

WindowsClock::WindowsClock() noexcept {
    LARGE_INTEGER frequency{};
    if (::QueryPerformanceFrequency(&frequency)) frequency_ = frequency.QuadPart;
}

std::int64_t WindowsClock::now_nanos() const noexcept {
    LARGE_INTEGER counter{};
    if (frequency_ > 0 && ::QueryPerformanceCounter(&counter)) {
        const std::int64_t seconds = counter.QuadPart / frequency_;
        const std::int64_t remainder = counter.QuadPart % frequency_;
        return seconds * 1'000'000'000LL +
               static_cast<std::int64_t>(static_cast<long double>(remainder) *
                                         1'000'000'000.0L / frequency_);
    }
    return static_cast<std::int64_t>(::GetTickCount64()) * 1'000'000;
}

}  // namespace ckv::term
