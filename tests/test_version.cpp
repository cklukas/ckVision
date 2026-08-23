// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/version.hpp"

#include <string>

#include "cvision/testing/cktest.hpp"

// The literals below are deliberately literal, and they are the whole point of
// this suite now that version() derives from PROJECT_VERSION: they are the
// human declaration of which release this is, standing against the build
// system's. Bumping `project(ckvision VERSION ...)` without bumping these — or
// these without that — turns this red, which is the moment to notice that half
// a release has been prepared. Update both, in one commit, on purpose.
CK_TEST(version_is_consistent) {
    const ckv::Version v = ckv::version();
    CK_CHECK(v.major == 0);
    CK_CHECK(v.minor == 1);
    CK_CHECK(v.patch == 1);
    CK_CHECK(ckv::version_string() == "0.1.1");
}

CK_TEST(the_version_string_spells_the_same_numbers_as_the_triple) {
    // Both come from PROJECT_VERSION, but not from the same expansion of it:
    // the triple is three separate CMake variables and the string is the whole
    // value. A four-component project version, or a stray suffix, agrees with
    // neither — and would be published as the package's version regardless.
    const ckv::Version v = ckv::version();
    const std::string expected =
        std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
    CK_CHECK(std::string(ckv::version_string()) == expected);
}
