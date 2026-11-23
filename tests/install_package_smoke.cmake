# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

if(NOT DEFINED CKVISION_BUILD_DIR OR NOT DEFINED CKVISION_SMOKE_DIR OR NOT DEFINED CKVISION_CXX_COMPILER)
    message(FATAL_ERROR "install_package_smoke requires build directory, smoke directory and C++ compiler")
endif()

file(REMOVE_RECURSE "${CKVISION_SMOKE_DIR}")
set(prefix "${CKVISION_SMOKE_DIR}/prefix")
set(consumer "${CKVISION_SMOKE_DIR}/consumer")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${CKVISION_BUILD_DIR}" --config "${CKVISION_CONFIG}" --prefix "${prefix}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "ckVision install failed (${install_result})")
endif()
if(NOT DEFINED CKVISION_INSTALL_LIBDIR OR CKVISION_INSTALL_LIBDIR STREQUAL "")
    if(EXISTS "${prefix}/lib64/cmake/ckvision/ckvisionConfig.cmake")
        set(CKVISION_INSTALL_LIBDIR "lib64")
    else()
        set(CKVISION_INSTALL_LIBDIR "lib")
    endif()
endif()
if(NOT EXISTS "${prefix}/${CKVISION_INSTALL_LIBDIR}/cmake/ckvision/ckvisionConfig.cmake")
    message(FATAL_ERROR "installed CMake package configuration is missing")
endif()
find_program(pkg_config NAMES pkg-config pkgconf)
if(pkg_config)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PKG_CONFIG_PATH="
            "PKG_CONFIG_LIBDIR=${prefix}/${CKVISION_INSTALL_LIBDIR}/pkgconfig"
            "${pkg_config}" --variable=libdir ckvision
        RESULT_VARIABLE pkg_config_result
        OUTPUT_VARIABLE pkg_config_libdir
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    cmake_path(NORMAL_PATH pkg_config_libdir)
    if(NOT pkg_config_result EQUAL 0 OR
            NOT pkg_config_libdir STREQUAL "${prefix}/${CKVISION_INSTALL_LIBDIR}")
        message(FATAL_ERROR "installed ckVision pkg-config metadata points outside the staging prefix")
    endif()
endif()
foreach(module IN ITEMS CkVisionConpty.cmake CkVisionConptyBinary.cmake
        CkVisionConptyCopy.cmake Microsoft.ConPTY.LICENSE.txt)
    if(NOT EXISTS "${prefix}/${CKVISION_INSTALL_LIBDIR}/cmake/ckvision/${module}")
        message(FATAL_ERROR "installed ConPTY deployment support is missing: ${module}")
    endif()
endforeach()
if(CKVISION_EXPECT_TERMINAL AND NOT EXISTS "${prefix}/bin/ckvision_terminal${CKVISION_EXECUTABLE_SUFFIX}")
    message(FATAL_ERROR "installed terminal example is missing")
endif()
if(CKVISION_EXPECT_PROFILE_SAMPLE)
    if(NOT EXISTS "${prefix}/bin/ckvision_editor_profile_sample${CKVISION_EXECUTABLE_SUFFIX}")
        message(FATAL_ERROR "installed non-interactive example is missing")
    endif()
    execute_process(
        COMMAND "${prefix}/bin/ckvision_editor_profile_sample${CKVISION_EXECUTABLE_SUFFIX}"
        RESULT_VARIABLE sample_result)
    if(NOT sample_result EQUAL 0)
        message(FATAL_ERROR "installed profile sample failed (${sample_result})")
    endif()
endif()

file(MAKE_DIRECTORY "${consumer}")
file(WRITE "${consumer}/CMakeLists.txt" [=[
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
project(ckvision_install_consumer LANGUAGES CXX)
foreach(selection IN ITEMS GENERATOR GENERATOR_PLATFORM GENERATOR_TOOLSET GENERATOR_INSTANCE)
    if(DEFINED CKVISION_EXPECT_${selection} AND
            NOT "${CMAKE_${selection}}" STREQUAL "${CKVISION_EXPECT_${selection}}")
        message(FATAL_ERROR "installed-package consumer changed ${selection}: ${CMAKE_${selection}}")
    endif()
endforeach()
string(REPLACE "." ";" version_parts "${CKVISION_EXPECT_VERSION}")
list(GET version_parts 0 expected_major)
list(GET version_parts 1 expected_minor)
if(expected_minor GREATER 0)
    math(EXPR previous_minor "${expected_minor} - 1")
    find_package(ckvision "${expected_major}.${previous_minor}"
        CONFIG QUIET PATHS "${CMAKE_PREFIX_PATH}" NO_DEFAULT_PATH)
    if(ckvision_FOUND)
        message(FATAL_ERROR "installed ckVision accepted a consumer from the previous SDK line")
    endif()
endif()
find_package(ckvision "${CKVISION_EXPECT_VERSION}" EXACT CONFIG REQUIRED
    PATHS "${CMAKE_PREFIX_PATH}" NO_DEFAULT_PATH)
if(NOT COMMAND ckvision_deploy_conpty)
    message(FATAL_ERROR "installed ckVision does not expose ckvision_deploy_conpty")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ckvision::cvision)
get_target_property(sdk_library_type ckvision::cvision TYPE)
if(MSVC AND sdk_library_type STREQUAL "SHARED_LIBRARY")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check-msvc-dll-abi" "true")
endif()
if(DEFINED CKVISION_WRONG_ITERATOR_LEVEL)
    add_executable(incompatible_consumer EXCLUDE_FROM_ALL incompatible.cpp)
    target_compile_definitions(incompatible_consumer PRIVATE
        "_ITERATOR_DEBUG_LEVEL=${CKVISION_WRONG_ITERATOR_LEVEL}")
    target_link_libraries(incompatible_consumer PRIVATE ckvision::cvision)
endif()
if(MSVC)
    target_compile_options(consumer PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
else()
    target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(MSVC AND DEFINED CKVISION_EXPECT_MSVC_RUNTIME_LIBRARY AND
        NOT "${CKVISION_EXPECT_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
    get_target_property(consumer_runtime consumer MSVC_RUNTIME_LIBRARY)
    if(NOT "${consumer_runtime}" STREQUAL "${CKVISION_EXPECT_MSVC_RUNTIME_LIBRARY}")
        message(FATAL_ERROR "installed-package consumer changed the SDK's selected MSVC runtime")
    endif()
endif()
# Record the compiler selected by the actual consumer project. Visual Studio
# generators do not necessarily cache CMAKE_CXX_COMPILER as FILEPATH.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/consumer-compiler.txt" "${CMAKE_CXX_COMPILER}")
]=])
file(WRITE "${consumer}/main.cpp" [=[
// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstdio>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <cvision/core/cell.hpp>
#include <cvision/core/clock.hpp>
#include <cvision/term/headless_terminal.hpp>
#include <cvision/ui/application.hpp>
#include <cvision/widgets/desktop.hpp>
#include <cvision/widgets/menu.hpp>

class ClientView final : public ckv::ui::View {
public:
    explicit ClientView(bool& destroyed) : destroyed_(destroyed) {}
    ~ClientView() override { destroyed_ = true; }

private:
    bool& destroyed_;
};

int main() {
    constexpr std::u8string_view family = u8"\U0001F468\u200D\U0001F469\u200D\U0001F467";
    const std::string_view cluster(reinterpret_cast<const char*>(family.data()), family.size());
    auto original = ckv::Cell::from_grapheme(cluster, {});
    auto copy = original;
    auto moved = std::move(copy);
    original = {};
    if (sizeof(moved) != 32 || moved.grapheme() != cluster)
        return 1;

    ckv::term::HeadlessTerminal terminal({80, 25});
    ckv::ManualClock clock;
    bool destroyed = false;
    {
        ckv::ui::Application app(terminal, clock);
        auto* desktop = app.root().make<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 25});
        app.root().add_child(std::make_unique<ClientView>(destroyed));
        app.step(0);
        if (app.current_frame().size() != ckv::Size{80, 25})
            return 2;

        using ckv::widgets::MenuItem;
        int activated = 0;
        auto items = [&] { return std::vector{MenuItem::action("Open", [&] { ++activated; })}; };
        const ckv::KeyEvent enter{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}};
        if (!ckv::widgets::show_context_menu(items(), {1, 1}, app, *desktop) || !app.dispatch(enter))
            return 3;
        if (!ckv::widgets::show_anchored_menu(items(), {1, 1, 1, 1}, app, *desktop) || !app.dispatch(enter))
            return 4;
        if (activated != 2 || !desktop->popups().empty())
            return 5;
    }
    if (!destroyed)
        return 6;
#if defined(CKVISION_SHARED) && defined(_MSC_VER)
    std::printf("%d\n", _ITERATOR_DEBUG_LEVEL);
#endif
    return 0;
}
]=])
file(WRITE "${consumer}/incompatible.cpp" [=[
// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cvision/core/version.hpp>
int main() { return 0; }
]=])
set(consumer_configuration
    "${CMAKE_COMMAND}" -S "${consumer}" -B "${consumer}/build"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DCKVISION_EXPECT_VERSION=${CKVISION_EXPECT_VERSION}"
    "-DCMAKE_CXX_COMPILER=${CKVISION_CXX_COMPILER}")
if(NOT CKVISION_CONSUMER_GENERATOR MATCHES "Visual Studio|Xcode|Ninja Multi-Config")
    list(APPEND consumer_configuration "-DCMAKE_BUILD_TYPE=${CKVISION_CONFIG}")
endif()
# --config selects a multi-configuration build, but does not configure Ninja's
# compile flags or MSVC runtime. The consumer must compile the installed ABI.
# The generator's selected target is independent of the machine running it.
# Visual Studio can ignore an explicit cl.exe when no platform is supplied,
# silently configuring an ARM64 consumer for an x64 SDK on an ARM64 host.
if(DEFINED CKVISION_CONSUMER_GENERATOR AND NOT CKVISION_CONSUMER_GENERATOR STREQUAL "")
    list(APPEND consumer_configuration -G "${CKVISION_CONSUMER_GENERATOR}"
        "-DCKVISION_EXPECT_GENERATOR=${CKVISION_CONSUMER_GENERATOR}")
endif()
foreach(selection IN ITEMS PLATFORM TOOLSET INSTANCE)
    if(DEFINED CKVISION_CONSUMER_${selection} AND NOT CKVISION_CONSUMER_${selection} STREQUAL "")
        if(selection STREQUAL "PLATFORM")
            list(APPEND consumer_configuration -A "${CKVISION_CONSUMER_PLATFORM}")
        elseif(selection STREQUAL "TOOLSET")
            list(APPEND consumer_configuration -T "${CKVISION_CONSUMER_TOOLSET}")
        else()
            list(APPEND consumer_configuration "-DCMAKE_GENERATOR_INSTANCE=${CKVISION_CONSUMER_INSTANCE}")
        endif()
        list(APPEND consumer_configuration
            "-DCKVISION_EXPECT_GENERATOR_${selection}=${CKVISION_CONSUMER_${selection}}")
    endif()
endforeach()
# Runtime-library selection is part of the installed static library's ABI.
# A fresh consumer must use the same explicit selection, rather than silently
# resetting an MT SDK to CMake's MD default. Preserve generator expressions.
if(DEFINED CKVISION_MSVC_RUNTIME_LIBRARY AND NOT CKVISION_MSVC_RUNTIME_LIBRARY STREQUAL "")
    list(APPEND consumer_configuration
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CKVISION_MSVC_RUNTIME_LIBRARY}"
        "-DCKVISION_EXPECT_MSVC_RUNTIME_LIBRARY=${CKVISION_MSVC_RUNTIME_LIBRARY}")
endif()
execute_process(
    COMMAND ${consumer_configuration}
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer configuration failed (${configure_result})")
endif()
file(READ "${consumer}/build/consumer-compiler.txt" consumer_compiler)
if(NOT consumer_compiler STREQUAL CKVISION_CXX_COMPILER)
    message(FATAL_ERROR "installed-package consumer changed compiler: ${consumer_compiler}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer}/build" --config "${CKVISION_CONFIG}"
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer build failed (${build_result})")
endif()
set(consumer_executable "${consumer}/build/consumer${CKVISION_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${consumer_executable}")
    set(consumer_executable "${consumer}/build/${CKVISION_CONFIG}/consumer${CKVISION_EXECUTABLE_SUFFIX}")
endif()
if(WIN32)
    set(ENV{PATH} "${prefix}/bin;$ENV{PATH}")
endif()
execute_process(
    COMMAND "${consumer_executable}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE consumer_abi
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer run failed (${run_result})")
endif()
if(EXISTS "${consumer}/build/check-msvc-dll-abi")
    if(NOT consumer_abi MATCHES "^[012]$")
        message(FATAL_ERROR "the MSVC DLL consumer did not report its STL iterator debugging level")
    endif()
    if(consumer_abi STREQUAL "1")
        set(wrong_iterator_level 0)
    else()
        set(wrong_iterator_level 1)
    endif()
    execute_process(
        COMMAND ${consumer_configuration} "-DCKVISION_WRONG_ITERATOR_LEVEL=${wrong_iterator_level}"
        RESULT_VARIABLE mismatch_configure_result)
    if(NOT mismatch_configure_result EQUAL 0)
        message(FATAL_ERROR "incompatible consumer configuration failed (${mismatch_configure_result})")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${consumer}/build" --config "${CKVISION_CONFIG}"
            --target incompatible_consumer
        RESULT_VARIABLE mismatch_result
        OUTPUT_VARIABLE mismatch_output
        ERROR_VARIABLE mismatch_error)
    if(mismatch_result EQUAL 0 OR
            NOT "${mismatch_output}${mismatch_error}" MATCHES "ckvision_abi_[a-z]+_iterator_${wrong_iterator_level}")
        message(FATAL_ERROR "incompatible consumer failed to demonstrate the DLL ABI check\n"
                            "${mismatch_output}\n${mismatch_error}")
    endif()
endif()
