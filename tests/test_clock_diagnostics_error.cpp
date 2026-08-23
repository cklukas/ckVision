// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/clock.hpp"
#include "cvision/core/diagnostics.hpp"
#include "cvision/core/error.hpp"

#include "cvision/testing/cktest.hpp"

CK_TEST(manual_clock_starts_at_given_time_and_advances) {
    ckv::ManualClock clock(1000);
    CK_CHECK(clock.now_nanos() == 1000);
    clock.advance(500);
    CK_CHECK(clock.now_nanos() == 1500);
    clock.set(42);
    CK_CHECK(clock.now_nanos() == 42);
}

CK_TEST(manual_clock_used_polymorphically) {
    ckv::ManualClock manual;
    ckv::Clock& clock = manual;
    CK_CHECK(clock.now_nanos() == 0);
}

CK_TEST(buffered_diagnostics_holds_entries_in_memory_only) {
    // core performs no I/O (the architecture §1): this sink buffers and
    // nothing else — no way to write anywhere is exposed at this layer.
    ckv::BufferedDiagnostics sink;
    CK_CHECK(sink.entries().empty());
    sink.log(ckv::LogLevel::Warning, "something odd");
    sink.log(ckv::LogLevel::Error, "something broke");
    CK_CHECK(sink.entries().size() == 2);
    CK_CHECK(sink.entries()[0].level == ckv::LogLevel::Warning);
    CK_CHECK(sink.entries()[0].text == "something odd");
    CK_CHECK(sink.entries()[1].level == ckv::LogLevel::Error);
    sink.clear();
    CK_CHECK(sink.entries().empty());
}

CK_TEST(buffered_diagnostics_used_polymorphically) {
    ckv::BufferedDiagnostics sink;
    ckv::DiagnosticsSink& base = sink;
    base.log(ckv::LogLevel::Info, "via base");
    CK_CHECK(sink.entries().size() == 1);
}

CK_TEST(error_carries_code_and_message) {
    const ckv::Error err(ckv::ErrorCode::TerminalLost, "tty closed");
    CK_CHECK(err.code() == ckv::ErrorCode::TerminalLost);
    CK_CHECK(err.message() == "tty closed");
}

