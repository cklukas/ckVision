// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Private compiled filesystem-test seam. Never installed or enabled on cvision.
#pragma once

#if defined(_WIN32)

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <array>
namespace ckv::bounded_test {
// Actual native-operation/cleanup observation points in the compiled test provider.
enum class Event : unsigned { Enter, BeforeOpen, Opened, Metadata, HashOpen, HashCreate, Read,
    Retained, FinalMetadata, HashAppend, HashFinish, HashDestroyed, AlgorithmClosed, BeforeClose, Closed, Exit };
// Native first-error namespace; no diagnostic allocation is needed to retain it.
enum class Domain : unsigned { None, Win32, Cng };
// Fixed native metadata snapshot used to check the held-object revision.
struct Metadata {
    // Genuine queried volume/file identity; alignment follows the Windows ABI.
    alignas(8) FILE_ID_INFO identity{};
    // Queried byte extent and revision timestamps, without string construction.
    LONGLONG size = 0, write_time = 0, change_time = 0;
    // True only after the actual metadata snapshot finished.
    bool complete = false;
};
// Per-call fixed observation state installed by the real bounded-reader entry point.
struct Context {
    // The actual source reader HANDLE, never a metadata-query clone.
    HANDLE source = INVALID_HANDLE_VALUE;
    // Logical cap, bytes consumed/retained, observed capacity and maximum request.
    std::size_t cap = 0, consumed = 0, retained = 0, capacity = 0, max_request = 0;
    // Actual source-open, consumed-close, metadata-query and read cardinalities.
    unsigned opens = 0, close_attempts = 0, metadata_queries = 0, read_calls = 0;
    // Actual native crypto resource acquisition/drain cardinalities.
    unsigned hash_opens = 0, hash_creates = 0, hash_destroys = 0, algorithm_closes = 0;
    // Before/after native observations for the same held source object.
    std::array<Metadata,2> metadata{};
    // Native close consumption/result recorded before failure-only notification.
    bool consumed_close = false, real_close_ok = false;
    // Native namespace of the first genuine or added failure.
    Domain first_domain = Domain::None;
    // Operation that first supplied the retained native error.
    Event first_event = Event::Enter;
    // Full native error bits, recorded before any observer or denied allocation.
    std::uint32_t first_code = 0;
    // Distinguishes a post-success test fault from an actual native failure.
    bool first_injected = false;
};
// Fixture-defined fixed-state observer; it never replaces the native operation.
void observe(Context&, Event, std::uintptr_t, std::size_t, std::size_t, bool, std::uint32_t, bool) noexcept;
// Fixture-defined failure-only decision, considered only after real native success.
bool inject(Context&, Event, std::uintptr_t, std::size_t, std::size_t) noexcept;
// Thread-local observation slot set internally by the real API, not its caller.
inline thread_local Context* active = nullptr;
// Recognizes the still-owned genuine source HANDLE before its consumed close.
inline bool is_source(HANDLE value) noexcept { return active && active->source == value && !active->consumed_close; }
// Reports a fixed observation when an actual API-owned context is active.
inline void note(Event point, std::uintptr_t resource = 0, std::size_t a = 0, std::size_t b = 0) noexcept {
    if (active) observe(*active, point, resource, a, b, true, 0, false);
}
// Retains the first POD error, then observes; a real failure cannot become success.
inline bool checked(Event point, std::uintptr_t resource, bool native_ok, Domain domain,
                    std::uint32_t native_code, std::size_t a = 0, std::size_t b = 0) noexcept {
    if (!active) return native_ok;
    const bool added_failure = native_ok && inject(*active, point, resource, a, b);
    if ((!native_ok || added_failure) && active->first_domain == Domain::None) {
        active->first_domain = domain;
        active->first_event = point;
        active->first_code = added_failure ? (domain == Domain::Win32 ? ERROR_READ_FAULT : 0xC0000185u) : native_code;
        active->first_injected = added_failure;
    }
    // First tuple is POD-latched BEFORE any observer/allocator-denial action.
    observe(*active, point, resource, a, b, native_ok, native_code, added_failure);
    if (added_failure && domain == Domain::Win32) ::SetLastError(ERROR_READ_FAULT);
    else if (!native_ok && domain == Domain::Win32) ::SetLastError(native_code);
    return native_ok && !added_failure; // A real failure can never be changed to success.
}
// Restores nested observation state after all later-created native owners drain.
struct Scope {
    // Stack-owned per-call observation state; no allocation or external scope is needed.
    Context value{};
    // Borrowed outer context restored after this actual API invocation exits.
    Context* previous;
    // Installs this operation state before its first allocation/native open.
    explicit Scope(std::size_t cap) noexcept : previous(active) { value.cap = cap; active = &value; note(Event::Enter); }
    // Reports terminal state after native finalizers and restores the outer slot.
    ~Scope() { note(Event::Exit); active = previous; }
};
}

#endif // defined(_WIN32)
