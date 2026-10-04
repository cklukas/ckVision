// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include <optional>

CK_TEST(constant_true) {
    CK_CHECK(true);
    constexpr int declared = 7;
    CK_CHECK(declared == 7);
}

CK_TEST(constant_false) {
    CK_CHECK(false);
}

CK_TEST(condition_evaluates_once_and_preserves_contextual_negation) {
    int evaluations = 0;
    CK_CHECK(++evaluations == 1);
    CK_CHECK(evaluations == 1);
    const std::optional<int> present{7};
    CK_CHECK(present);
    struct Negated {
        int& calls;
        bool operator!() const { ++calls; return false; }
    };
    int negations = 0;
    CK_CHECK(Negated{negations});
    CK_CHECK(negations == 1);
}

CKTEST_MAIN
