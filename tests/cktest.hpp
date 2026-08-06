// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// cktest — ckVision's minimal, dependency-free unit-test harness.
#pragma once

#include <cstdio>
#include <vector>

namespace cktest {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> v;
    return v;
}

inline int& failures() {
    static int n = 0;
    return n;
}

inline const char*& current() {
    static const char* c = "";
    return c;
}

inline bool add(const char* name, void (*fn)()) {
    cases().push_back(Case{name, fn});
    return true;
}

inline void report_failure(const char* expr, const char* file, int line) {
    ++failures();
    std::fprintf(stderr, "FAIL %s: %s (%s:%d)\n", current(), expr, file, line);
}

inline int run_all() {
    for (const Case& c : cases()) {
        current() = c.name;
        c.fn();
    }
    if (failures() == 0) {
        std::printf("cktest: %zu tests, all passed\n", cases().size());
        return 0;
    }
    std::fprintf(stderr, "cktest: %d failure(s)\n", failures());
    return 1;
}

}  // namespace cktest

#define CK_TEST(name)                                                                  \
    static void ck_test_##name();                                                      \
    [[maybe_unused]] static const bool ck_reg_##name =                                 \
        ::cktest::add(#name, &ck_test_##name);                                         \
    static void ck_test_##name()

#define CK_CHECK(cond)                                                                 \
    do {                                                                               \
        if (!(cond)) ::cktest::report_failure(#cond, __FILE__, __LINE__);              \
    } while (0)

#define CKTEST_MAIN                                                                    \
    int main() { return ::cktest::run_all(); }
