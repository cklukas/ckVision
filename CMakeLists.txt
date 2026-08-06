# Copyright (c) 2026 C. Klukas. All rights reserved.
cmake_minimum_required(VERSION 3.25)
project(ckvision VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CkVisionSettings.cmake)

add_library(cvision
    src/core/golden.cpp
    src/core/version.cpp)
add_library(ckvision::cvision ALIAS cvision)
target_include_directories(cvision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ckvision_strict(cvision)

option(CKVISION_BUILD_TESTING
    "Build ckVision tests and benchmarks" ${PROJECT_IS_TOP_LEVEL})
if(CKVISION_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif()
