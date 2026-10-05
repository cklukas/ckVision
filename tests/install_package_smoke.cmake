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
if(NOT EXISTS "${prefix}/lib/cmake/ckvision/ckvisionConfig.cmake")
    message(FATAL_ERROR "installed CMake package configuration is missing")
endif()
foreach(module IN ITEMS CkVisionConpty.cmake CkVisionConptyBinary.cmake
        CkVisionConptyCopy.cmake Microsoft.ConPTY.LICENSE.txt)
    if(NOT EXISTS "${prefix}/lib/cmake/ckvision/${module}")
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
find_package(ckvision CONFIG REQUIRED)
if(NOT COMMAND ckvision_deploy_conpty)
    message(FATAL_ERROR "installed ckVision does not expose ckvision_deploy_conpty")
endif()
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ckvision::cvision)
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
#include <cvision/core/clock.hpp>
#include <cvision/term/headless_terminal.hpp>
#include <cvision/ui/application.hpp>

int main() {
    ckv::term::HeadlessTerminal terminal({80, 25});
    ckv::ManualClock clock;
    ckv::ui::Application application(terminal, clock);
    application.step(0);
    return application.current_frame().size() == ckv::Size{80, 25} ? 0 : 1;
}
]=])
set(consumer_configuration
    "${CMAKE_COMMAND}" -S "${consumer}" -B "${consumer}/build"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DCMAKE_CXX_COMPILER=${CKVISION_CXX_COMPILER}")
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
execute_process(
    COMMAND "${consumer_executable}"
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer run failed (${run_result})")
endif()
