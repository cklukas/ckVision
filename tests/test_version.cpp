// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/version.hpp"

#include "cktest.hpp"

CK_TEST(version_is_consistent) {
    const ckv::Version v = ckv::version();
    CK_CHECK(v.major == 0);
    CK_CHECK(v.minor == 1);
    CK_CHECK(v.patch == 0);
    CK_CHECK(ckv::version_string() == "0.1.0");
}
