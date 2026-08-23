// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// What the compiler knows about the binary it is producing. Every value
// here is a preprocessor or language fact, so this file is portable and
// every probe shares it — a platform probe should be about the platform.
#include <bit>
#include <string>

#include "system_probe.hpp"

namespace ckv::sysinfo {
namespace {

#define CKV_SYSINFO_STRINGIFY_INNER(value) #value
#define CKV_SYSINFO_STRINGIFY(value) CKV_SYSINFO_STRINGIFY_INNER(value)

std::string compiler_name() {
#if defined(__apple_build_version__)
    return "Apple Clang " CKV_SYSINFO_STRINGIFY(__clang_major__) "." CKV_SYSINFO_STRINGIFY(__clang_minor__) "." CKV_SYSINFO_STRINGIFY(__clang_patchlevel__);
#elif defined(__clang__)
    return "Clang " CKV_SYSINFO_STRINGIFY(__clang_major__) "." CKV_SYSINFO_STRINGIFY(__clang_minor__) "." CKV_SYSINFO_STRINGIFY(__clang_patchlevel__);
#elif defined(__GNUC__)
    return "GCC " CKV_SYSINFO_STRINGIFY(__GNUC__) "." CKV_SYSINFO_STRINGIFY(__GNUC_MINOR__) "." CKV_SYSINFO_STRINGIFY(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return "MSVC " CKV_SYSINFO_STRINGIFY(_MSC_VER);
#else
    return "unknown compiler";
#endif
}

std::string standard_library_name() {
#if defined(_LIBCPP_VERSION)
    return "libc++ " CKV_SYSINFO_STRINGIFY(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
    return "libstdc++ " CKV_SYSINFO_STRINGIFY(__GLIBCXX__);
#elif defined(_MSVC_STL_VERSION)
    return "MSVC STL " CKV_SYSINFO_STRINGIFY(_MSVC_STL_VERSION);
#else
    return "unknown standard library";
#endif
}

std::string cxx_standard_name() {
    // The language level this translation unit was actually compiled at,
    // which is not necessarily the one the build system asked for.
    if (__cplusplus > 202302L) return "C++26 or later";
    if (__cplusplus >= 202302L) return "C++23";
    if (__cplusplus >= 202002L) return "C++20";
    if (__cplusplus >= 201703L) return "C++17";
    return "C++14 or earlier";
}

#undef CKV_SYSINFO_STRINGIFY
#undef CKV_SYSINFO_STRINGIFY_INNER

}  // namespace

bool build_is_optimized(const BuildReport& build) noexcept {
    return build.build_type == "Release" || build.build_type == "RelWithDebInfo" ||
           build.build_type == "MinSizeRel";
}

BuildReport current_build_report() {
    BuildReport report;
    report.compiler = compiler_name();
    report.standard_library = standard_library_name();
    report.cxx_standard = cxx_standard_name();
#if defined(CKV_SYSINFO_BUILD_TYPE)
    report.build_type = CKV_SYSINFO_BUILD_TYPE;
#endif
    report.pointer_bits = static_cast<int>(sizeof(void*) * 8);
    report.little_endian = std::endian::native == std::endian::little;
    return report;
}

}  // namespace ckv::sysinfo
