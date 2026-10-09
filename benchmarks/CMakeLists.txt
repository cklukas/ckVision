# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
add_executable(cvision_bench bench_editor.cpp bench_progress.cpp bench_scene.cpp bench_smoke.cpp bench_terminal.cpp main.cpp)
target_link_libraries(cvision_bench PRIVATE ckvision::cvision)
ckvision_strict(cvision_bench)

add_custom_target(bench_smoke_run
    COMMAND cvision_bench
    COMMENT "ckVision benchmark smoke")

# Unlike printed benchmark targets, the deterministic budget gates below are
# CTest failures when their checked work, size, or redraw limits regress. They
# run in --gate mode, which executes each benchmark only as often as its
# checks need (ckbench::kGateIterations); bench_smoke_run is the timed run.
add_test(NAME scene_budget_gate COMMAND cvision_bench --gate)
# What these gates assert is deterministic rather than wall-clock based, so they
# stay meaningful under a sanitizer; only their runtime changes, and ASan/TSan
# make it an order of magnitude slower. The ceiling follows the instrumentation
# rather than skipping real regression checks from the sanitizer lanes.
if(CKVISION_SANITIZE)
    set(ckvision_benchmark_gate_timeout 600)
else()
    set(ckvision_benchmark_gate_timeout 60)
endif()
set_tests_properties(scene_budget_gate PROPERTIES LABELS "benchmark;budget"
    TIMEOUT ${ckvision_benchmark_gate_timeout})

# The pinned-host p99 harness (docs/performance.md "Pinned-host p99
# procedure", the architecture §8): a real Application on a pseudo-terminal,
# timed from each scripted input write to the final byte of the frame it
# produced. A tool, deliberately never a CTest: shared CI must not gate on
# wall-clock time. Run it from a Release build on the reference host.
if(UNIX)
    add_executable(cvision_pty_latency pty_latency.cpp)
    target_link_libraries(cvision_pty_latency PRIVATE ckvision::cvision)
    # A latency figure from an unoptimized build measures the build, so the
    # result names the configuration it came from.
    target_compile_definitions(cvision_pty_latency PRIVATE CKV_PTY_LATENCY_BUILD_TYPE="$<CONFIG>")
    ckvision_strict(cvision_pty_latency)

    add_custom_target(pty_latency_run
        COMMAND cvision_pty_latency --output ${CMAKE_BINARY_DIR}/pty_latency.json
        COMMENT "ckVision pinned-host PTY latency (event to presented bytes)"
        USES_TERMINAL)

    add_executable(cvision_theme_switch_latency theme_switch_latency.cpp)
    target_link_libraries(cvision_theme_switch_latency PRIVATE ckvision::cvision)
    target_compile_definitions(cvision_theme_switch_latency PRIVATE CKV_THEME_SWITCH_BUILD_TYPE="$<CONFIG>")
    ckvision_strict(cvision_theme_switch_latency)
    # A deterministic workload check runs everywhere. Wall-clock acceptance
    # stays on the idle pinned host, never in shared CI.
    add_test(NAME theme_switch_work_gate COMMAND cvision_theme_switch_latency --verify-only)
    set_tests_properties(theme_switch_work_gate PROPERTIES LABELS "benchmark;budget" TIMEOUT 30)
endif()

if(TARGET todo_app)
    add_executable(todo_bench bench_todo.cpp)
    target_link_libraries(todo_bench PRIVATE todo_app ckvision::cvision)
    target_include_directories(todo_bench PRIVATE ${PROJECT_SOURCE_DIR}/examples/todo)
    ckvision_strict(todo_bench)

    add_custom_target(todo_bench_run COMMAND todo_bench COMMENT "ckVision TODO benchmarks")
    add_test(NAME todo_benchmark_smoke COMMAND todo_bench --gate)
    set_tests_properties(todo_benchmark_smoke PROPERTIES LABELS "benchmark;budget;example"
        TIMEOUT ${ckvision_benchmark_gate_timeout})
endif()
