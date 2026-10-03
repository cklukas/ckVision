// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Adopter-style header compilation: no project-local CRT suppression macro.
#if defined(_MSC_VER)
#include <cerrno>
#include <cstdlib>
// Forward ordinary copies to the real CRT. Only the explicit error child
// substitutes ENOMEM, so an allocation failure is testable without exhausting
// the host. This wrapper does not suppress the public header's diagnostics.
static errno_t cktest_contract_copy_environment(char** bytes, std::size_t* size, const char* name) {
    char fail[2] = {};
    std::size_t copied = 0;
    if (::getenv_s(&copied, fail, sizeof(fail), "CKTEST_INJECT_COPY_FAILURE") == 0 && fail[0] == '1') {
        *bytes = nullptr;
        *size = 0;
        return ENOMEM;
    }
    return ::_dupenv_s(bytes, size, name);
}
#define _dupenv_s cktest_contract_copy_environment
#endif
#include "cvision/testing/cktest.hpp"
#if defined(_MSC_VER)
#undef _dupenv_s
#endif

CK_TEST(environment_selected_changes_default) {
#if defined(_MSC_VER)
    CK_CHECK(::_putenv_s("CKTEST_FILTER", "selection_that_matches_nothing") == 0);
#else
    CK_CHECK(::setenv("CKTEST_FILTER", "selection_that_matches_nothing", 1) == 0);
#endif
}

CK_TEST(environment_selected_survives_environment_change) {}
CK_TEST(other_selection_case) {}
CK_TEST(cli_override_case) {}

CKTEST_MAIN
