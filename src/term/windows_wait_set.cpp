// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_wait_set.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace ckv::term {
namespace {

using CreatePacket = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
using AssociatePacket = NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, PVOID, PVOID,
                                        NTSTATUS, ULONG_PTR, PBOOLEAN);
using CancelPacket = NTSTATUS(NTAPI*)(HANDLE, BOOLEAN);
using StatusError = ULONG(NTAPI*)(NTSTATUS);

template <typename Function>
Function load_function(HMODULE module, const char* name) {
    const FARPROC address = ::GetProcAddress(module, name);
    if (!address) throw std::system_error(ERROR_PROC_NOT_FOUND, std::system_category(), name);
    static_assert(sizeof(Function) == sizeof(address));
    Function function{};
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

struct NativeApi {
    CreatePacket create;
    AssociatePacket associate;
    CancelPacket cancel;
    StatusError error;

    NativeApi() {
        const HMODULE module = ::GetModuleHandleW(L"ntdll.dll");
        if (!module) throw std::system_error(static_cast<int>(::GetLastError()),
                                            std::system_category(), "GetModuleHandleW ntdll");
        create = load_function<CreatePacket>(module, "NtCreateWaitCompletionPacket");
        associate = load_function<AssociatePacket>(module, "NtAssociateWaitCompletionPacket");
        cancel = load_function<CancelPacket>(module, "NtCancelWaitCompletionPacket");
        error = load_function<StatusError>(module, "RtlNtStatusToDosError");
    }
    void check(NTSTATUS status, const char* operation) const {
        if (status < 0) throw std::system_error(static_cast<int>(error(status)),
                                               std::system_category(), operation);
    }
};

struct OwnedPort {
    HANDLE value = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    OwnedPort() {
        if (!value) throw std::system_error(static_cast<int>(::GetLastError()),
                                           std::system_category(), "CreateIoCompletionPort");
    }
    ~OwnedPort() { ::CloseHandle(value); }
    OwnedPort(const OwnedPort&) = delete;
    OwnedPort& operator=(const OwnedPort&) = delete;
};

struct Registration {
    const NativeApi& api;
    HANDLE source;
    ULONG_PTR key;
    HANDLE packet = nullptr;
    bool armed = false;
    Registration(const NativeApi& native_api, HANDLE source_handle, ULONG_PTR identity)
        : api(native_api), source(source_handle), key(identity) {
        api.check(api.create(&packet, GENERIC_ALL, nullptr), "NtCreateWaitCompletionPacket");
    }
    ~Registration() {
        // STATUS_PENDING forbids immediate reuse; this packet is never reused
        // after cancellation. Any queued numeric key has already been retired.
        (void)api.cancel(packet, TRUE);
        ::CloseHandle(packet);
    }
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
};

DWORD wait_milliseconds(std::int64_t now, std::int64_t deadline) noexcept {
    if (deadline == std::numeric_limits<std::int64_t>::max()) return INFINITE;
    if (deadline <= now) return 0;
    const auto nanos = static_cast<std::uint64_t>(deadline) - static_cast<std::uint64_t>(now);
    const auto millis = 1U + (nanos - 1U) / 1'000'000U;
    return static_cast<DWORD>(std::min<std::uint64_t>(millis, INFINITE - 1U));
}

}  // namespace

struct WindowsWaitSet::Impl {
    const Clock& clock;
    NativeApi api;
    OwnedPort port;
    std::unordered_map<std::uintptr_t, std::unique_ptr<Registration>> registrations;
    std::unordered_map<ULONG_PTR, Registration*> keys;
    std::vector<std::uintptr_t> wanted;
    std::vector<WaitHandle> ready;
    ULONG_PTR next_key = 1;

    explicit Impl(const Clock& source_clock) : clock(source_clock) {}
    ~Impl() { clear(); }

    void clear() noexcept {
        keys.clear();
        registrations.clear();
        wanted.clear();
        ready.clear();
    }

    void synchronize(std::span<const WaitHandle> sources) {
        wanted.clear();
        ready.clear();
        for (const auto source : sources) {
            if (source.kind != WaitHandleKind::WindowsHandle || source.value == 0) continue;
            if (source.value == reinterpret_cast<std::uintptr_t>(INVALID_HANDLE_VALUE))
                throw std::system_error(ERROR_INVALID_HANDLE, std::system_category(), "Windows wait source");
            wanted.push_back(source.value);
        }
        std::sort(wanted.begin(), wanted.end());
        wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
        for (auto it = registrations.begin(); it != registrations.end();) {
            if (std::binary_search(wanted.begin(), wanted.end(), it->first)) { ++it; continue; }
            keys.erase(it->second->key);
            it = registrations.erase(it);
        }
        for (const auto value : wanted) {
            if (registrations.contains(value)) continue;
            // Validate before allocating a packet: allocation could reuse a
            // closed caller handle's numeric value and disguise that error.
            DWORD flags = 0;
            if (!::GetHandleInformation(reinterpret_cast<HANDLE>(value), &flags))
                throw std::system_error(static_cast<int>(::GetLastError()),
                    std::system_category(), "GetHandleInformation Windows wait source");
            if (next_key == std::numeric_limits<ULONG_PTR>::max())
                throw std::system_error(ERROR_ARITHMETIC_OVERFLOW, std::system_category(), "Windows wait identity");
            auto entry = std::make_unique<Registration>(api, reinterpret_cast<HANDLE>(value), next_key++);
            const auto key = entry->key;
            auto* pointer = entry.get();
            registrations.emplace(value, std::move(entry));
            keys.emplace(key, pointer);
        }
        ready.reserve(registrations.size());
        for (auto& [value, entry] : registrations) {
            (void)value;
            if (entry->armed) continue;
            api.check(api.associate(entry->packet, port.value, entry->source,
                reinterpret_cast<PVOID>(entry->key), nullptr, 0, 0, nullptr), "NtAssociateWaitCompletionPacket");
            entry->armed = true;
        }
    }

    std::span<const WaitHandle> wait(std::int64_t deadline, std::span<const WaitHandle> sources) {
        synchronize(sources);
        if (registrations.empty()) return ready;
        while (ready.size() < registrations.size()) {
            DWORD bytes = 0;
            ULONG_PTR key = 0;
            OVERLAPPED* context = nullptr;
            const DWORD timeout = ready.empty() ? wait_milliseconds(clock.now_nanos(), deadline) : 0;
            const BOOL succeeded = ::GetQueuedCompletionStatus(port.value, &bytes, &key, &context, timeout);
            const DWORD error = succeeded ? ERROR_SUCCESS : ::GetLastError();
            if (key == 0) {
                if (!succeeded && error == WAIT_TIMEOUT) break;
                throw std::system_error(static_cast<int>(succeeded ? ERROR_INVALID_DATA : error),
                    std::system_category(), "GetQueuedCompletionStatus");
            }
            const auto found = keys.find(key);
            if (found == keys.end()) continue;  // a canceled/retired generation
            if (!succeeded) throw std::system_error(static_cast<int>(error), std::system_category(), "Windows readiness completion");
            auto& entry = *found->second;
            if (!entry.armed) continue;
            entry.armed = false;
            ready.push_back({WaitHandleKind::WindowsHandle, reinterpret_cast<std::uintptr_t>(entry.source)});
        }
        return ready;
    }
};

WindowsWaitSet::WindowsWaitSet(const Clock& clock) : impl_(std::make_unique<Impl>(clock)) {}
WindowsWaitSet::~WindowsWaitSet() = default;

std::span<const WaitHandle> WindowsWaitSet::wait(std::int64_t deadline_nanos,
                                                std::span<const WaitHandle> sources) {
    try {
        return impl_->wait(deadline_nanos, sources);
    } catch (...) {
        impl_->clear();
        throw;
    }
}

void WindowsWaitSet::clear() noexcept { impl_->clear(); }

}  // namespace ckv::term
