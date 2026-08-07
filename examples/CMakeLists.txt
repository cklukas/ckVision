# Copyright (c) 2026 C. Klukas. All rights reserved.
#
# The interactive gallery binary only builds where a real terminal
# backend exists (POSIX today; the Windows/ConPTY backend is M7 scope
# per ROADMAP.md) — but the object graph it wires up (gallery_app.cpp)
# is plain library code, so it is always built as a static library and
# always exercised headlessly by tests/test_gallery_smoke.cpp even on
# a platform without an interactive backend yet.
add_library(gallery_app STATIC gallery/gallery_app.cpp)
target_link_libraries(gallery_app PUBLIC ckvision::cvision)
target_include_directories(gallery_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gallery)
ckvision_strict(gallery_app)

# CKVISION_HAS_POSIX_TERMINAL is a preprocessor definition on the
# cvision target, not visible here as a CMake variable — UNIX is the
# same condition CMakeLists.txt itself uses to add the POSIX backend.
if(UNIX)
    add_executable(ckvision_gallery gallery/main.cpp)
    target_link_libraries(ckvision_gallery PRIVATE gallery_app ckvision::cvision)
    ckvision_strict(ckvision_gallery)
endif()
