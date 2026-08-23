# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

if(NOT DEFINED CKVISION_BUILD_DIR OR NOT DEFINED CKVISION_SMOKE_DIR)
    message(FATAL_ERROR "install_package_smoke requires CKVISION_BUILD_DIR and CKVISION_SMOKE_DIR")
endif()

file(REMOVE_RECURSE "${CKVISION_SMOKE_DIR}")
set(prefix "${CKVISION_SMOKE_DIR}/prefix")
set(consumer "${CKVISION_SMOKE_DIR}/consumer")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${CKVISION_BUILD_DIR}" --prefix "${prefix}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "ckVision install failed (${install_result})")
endif()
if(NOT EXISTS "${prefix}/lib/cmake/ckvision/ckvisionConfig.cmake")
    message(FATAL_ERROR "installed CMake package configuration is missing")
endif()
if(CKVISION_EXPECT_TERMINAL AND NOT EXISTS "${prefix}/bin/ckvision_terminal")
    message(FATAL_ERROR "installed terminal example is missing")
endif()
if(CKVISION_EXPECT_PROFILE_SAMPLE)
    if(NOT EXISTS "${prefix}/bin/ckvision_editor_profile_sample")
        message(FATAL_ERROR "installed non-interactive example is missing")
    endif()
    execute_process(
        COMMAND "${prefix}/bin/ckvision_editor_profile_sample"
        RESULT_VARIABLE sample_result)
    if(NOT sample_result EQUAL 0)
        message(FATAL_ERROR "installed profile sample failed (${sample_result})")
    endif()
endif()

file(MAKE_DIRECTORY "${consumer}")
file(WRITE "${consumer}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25)
project(ckvision_install_consumer LANGUAGES CXX)
find_package(ckvision CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ckvision::cvision)
]=])
file(WRITE "${consumer}/main.cpp" [=[
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
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${consumer}" -B "${consumer}/build"
            "-DCMAKE_PREFIX_PATH=${prefix}"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer configuration failed (${configure_result})")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer}/build"
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer build failed (${build_result})")
endif()
execute_process(
    COMMAND "${consumer}/build/consumer"
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "installed-package consumer run failed (${run_result})")
endif()
