# Copyright (c) 2026 C. Klukas. All rights reserved.
add_executable(cvision_bench bench_smoke.cpp)
target_link_libraries(cvision_bench PRIVATE ckvision::cvision)
ckvision_strict(cvision_bench)

add_custom_target(bench_smoke_run
    COMMAND cvision_bench
    COMMENT "ckVision benchmark smoke")
