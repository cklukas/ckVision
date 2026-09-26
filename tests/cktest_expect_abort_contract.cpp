// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// cktest's own contract for CK_EXPECT_ABORT, checked from outside the harness.
//
// Each expected abort runs in a child that re-runs its case from the start. A
// child that acted on the first CK_EXPECT_ABORT it reached answered every later
// one in the same case with the first one's termination, so a later body that
// did not terminate could never fail. The cases below are run one at a time by
// CTest (tests/CMakeLists.txt), which reads the verdict the harness prints:
// two of them are meant to fail, so they cannot live in cvision_tests.
//
// A body that aborts violates a real library contract, as every other
// CK_EXPECT_ABORT does, rather than calling std::abort: a body the compiler
// can see never returns would make the harness's own failure path unreachable.
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/command.hpp"

namespace {

// Declaring a command with an empty key is a contract violation.
void violate_a_contract() {
    ckv::ui::CommandRegistry registry;
    registry.declare(ckv::ui::CommandDescriptor{.key = "", .title = "Anonymous"});
}

}  // namespace

CK_TEST(two_expected_aborts_that_both_abort) {
    // Passes: each child runs its own body, and each body terminates.
    CK_EXPECT_ABORT({ violate_a_contract(); });
    CK_EXPECT_ABORT({ violate_a_contract(); });
}

CK_TEST(a_second_expected_abort_that_returns) {
    // Exactly one failure, at the second CK_EXPECT_ABORT: its child must run
    // its body, not the first one's.
    CK_EXPECT_ABORT({ violate_a_contract(); });
    CK_EXPECT_ABORT({});
}

CK_TEST(a_first_expected_abort_that_returns) {
    // Exactly one failure, at the first CK_EXPECT_ABORT: the second one's
    // child passes over the first body just as this process does.
    CK_EXPECT_ABORT({});
    CK_EXPECT_ABORT({ violate_a_contract(); });
}

CKTEST_MAIN
