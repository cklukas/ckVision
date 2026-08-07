# Copyright (c) 2026 C. Klukas. All rights reserved.
#
# Documentation tooling — not part of the cvision library, not
# installed, not part of ckVision's own public API. Only built when
# the gallery example (whose object graph it screenshots) is also
# built.
if(TARGET gallery_app)
    add_library(docgen_frame_svg STATIC frame_svg.cpp)
    target_link_libraries(docgen_frame_svg PUBLIC ckvision::cvision)
    ckvision_strict(docgen_frame_svg)

    add_executable(capture_gallery_screenshots capture_gallery_screenshots.cpp)
    target_link_libraries(capture_gallery_screenshots PRIVATE docgen_frame_svg gallery_app ckvision::cvision)
    target_include_directories(capture_gallery_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/gallery)
    ckvision_strict(capture_gallery_screenshots)
endif()

if(TARGET filebrowser_app)
    if(NOT TARGET docgen_frame_svg)
        add_library(docgen_frame_svg STATIC frame_svg.cpp)
        target_link_libraries(docgen_frame_svg PUBLIC ckvision::cvision)
        ckvision_strict(docgen_frame_svg)
    endif()

    add_executable(capture_filebrowser_screenshots capture_filebrowser_screenshots.cpp)
    target_link_libraries(capture_filebrowser_screenshots PRIVATE docgen_frame_svg filebrowser_app ckvision::cvision)
    target_include_directories(capture_filebrowser_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/filebrowser)
    ckvision_strict(capture_filebrowser_screenshots)
endif()

if(TARGET hello_app)
    add_executable(generate_hello_golden generate_hello_golden.cpp)
    target_link_libraries(generate_hello_golden PRIVATE hello_app ckvision::cvision)
    target_include_directories(generate_hello_golden PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello)
    ckvision_strict(generate_hello_golden)

    if(NOT TARGET docgen_frame_svg)
        add_library(docgen_frame_svg STATIC frame_svg.cpp)
        target_link_libraries(docgen_frame_svg PUBLIC ckvision::cvision)
        ckvision_strict(docgen_frame_svg)
    endif()

    add_executable(capture_hello_screenshots capture_hello_screenshots.cpp)
    target_link_libraries(capture_hello_screenshots PRIVATE docgen_frame_svg hello_app ckvision::cvision)
    target_include_directories(capture_hello_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello)
    ckvision_strict(capture_hello_screenshots)
endif()
