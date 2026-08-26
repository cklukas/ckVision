# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
add_executable(cvision_bench bench_editor.cpp bench_scene.cpp bench_smoke.cpp bench_terminal.cpp main.cpp)
target_link_libraries(cvision_bench PRIVATE ckvision::cvision)
ckvision_strict(cvision_bench)

add_custom_target(bench_smoke_run
    COMMAND cvision_bench
    COMMENT "ckVision benchmark smoke")

# Unlike a printed benchmark target, this deterministic scene budget gate is
# a CTest failure when the checked cells-touched limits regress.
add_test(NAME scene_budget_gate COMMAND cvision_bench)
# What this gate asserts is deterministic — cells touched, not time — so it stays
# meaningful under a sanitizer; only its wall-clock cost changes, and ASan/TSan
# make it an order of magnitude slower. A 60s ceiling was enough for an ordinary
# build and not for an instrumented one, so the sanitizer lanes failed on the
# clock while the budget itself was never in question. The ceiling follows the
# instrumentation rather than the gate being skipped, which would drop a real
# regression check from exactly the lanes that run it most carefully.
if(CKVISION_SANITIZE)
    set(ckvision_budget_gate_timeout 600)
else()
    set(ckvision_budget_gate_timeout 60)
endif()
set_tests_properties(scene_budget_gate PROPERTIES LABELS "benchmark;budget"
    TIMEOUT ${ckvision_budget_gate_timeout})

if(TARGET todo_app)
    add_executable(todo_bench bench_todo.cpp)
    target_link_libraries(todo_bench PRIVATE todo_app ckvision::cvision)
    target_include_directories(todo_bench PRIVATE ${PROJECT_SOURCE_DIR}/examples/todo)
    ckvision_strict(todo_bench)

    add_custom_target(todo_bench_run COMMAND todo_bench COMMENT "ckVision TODO benchmarks")
    add_test(NAME todo_benchmark_smoke COMMAND todo_bench)
    set_tests_properties(todo_benchmark_smoke PROPERTIES LABELS "benchmark;budget;example" TIMEOUT 60)
endif()
