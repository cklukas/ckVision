# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
add_executable(cvision_bench bench_editor.cpp bench_scene.cpp bench_smoke.cpp bench_terminal.cpp main.cpp)
target_link_libraries(cvision_bench PRIVATE ckvision::cvision)
ckvision_strict(cvision_bench)

add_custom_target(bench_smoke_run
    COMMAND cvision_bench
    COMMENT "ckVision benchmark smoke")

# Unlike printed benchmark targets, the deterministic budget gates below are
# CTest failures when their checked work, size, or redraw limits regress.
add_test(NAME scene_budget_gate COMMAND cvision_bench)
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

if(TARGET todo_app)
    add_executable(todo_bench bench_todo.cpp)
    target_link_libraries(todo_bench PRIVATE todo_app ckvision::cvision)
    target_include_directories(todo_bench PRIVATE ${PROJECT_SOURCE_DIR}/examples/todo)
    ckvision_strict(todo_bench)

    add_custom_target(todo_bench_run COMMAND todo_bench COMMENT "ckVision TODO benchmarks")
    add_test(NAME todo_benchmark_smoke COMMAND todo_bench)
    set_tests_properties(todo_benchmark_smoke PROPERTIES LABELS "benchmark;budget;example"
        TIMEOUT ${ckvision_benchmark_gate_timeout})
endif()
