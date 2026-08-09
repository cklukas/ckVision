# Copyright (c) 2026 C. Klukas. All rights reserved.
add_executable(cvision_bench bench_scene.cpp bench_smoke.cpp main.cpp)
target_link_libraries(cvision_bench PRIVATE ckvision::cvision)
ckvision_strict(cvision_bench)

add_custom_target(bench_smoke_run
    COMMAND cvision_bench
    COMMENT "ckVision benchmark smoke")

# Unlike a printed benchmark target, this deterministic scene budget gate is
# a CTest failure when the checked cells-touched limits regress.
add_test(NAME scene_budget_gate COMMAND cvision_bench)
