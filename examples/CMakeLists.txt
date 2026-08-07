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

# File Browser: a master-detail example (TreeView drives a ListView) —
# same split as gallery_app above: the object graph is a plain static
# library, always built and always headlessly testable; only the
# interactive binary (which needs PosixFileSystem to browse the real
# disk) is POSIX-only.
add_library(filebrowser_app STATIC filebrowser/filebrowser_app.cpp)
target_link_libraries(filebrowser_app PUBLIC ckvision::cvision)
target_include_directories(filebrowser_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/filebrowser)
ckvision_strict(filebrowser_app)

if(UNIX)
    add_executable(ckvision_filebrowser filebrowser/main.cpp)
    target_link_libraries(ckvision_filebrowser PRIVATE filebrowser_app ckvision::cvision)
    ckvision_strict(ckvision_filebrowser)
endif()

# ckVision Hello: a faithful port of Turbo Vision's own canonical minimal
# example, for a golden regression test (tests/test_hello_golden.cpp)
# and docs/turbo-vision-conversion.md's worked comparison.
add_library(hello_app STATIC hello/hello_app.cpp)
target_link_libraries(hello_app PUBLIC ckvision::cvision)
target_include_directories(hello_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hello)
ckvision_strict(hello_app)

if(UNIX)
    add_executable(ckvision_hello hello/main.cpp)
    target_link_libraries(ckvision_hello PRIVATE hello_app ckvision::cvision)
    ckvision_strict(ckvision_hello)
endif()
