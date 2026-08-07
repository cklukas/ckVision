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

if(TARGET tvhello_app)
    add_executable(generate_tvhello_golden generate_tvhello_golden.cpp)
    target_link_libraries(generate_tvhello_golden PRIVATE tvhello_app ckvision::cvision)
    target_include_directories(generate_tvhello_golden PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/tvhello)
    ckvision_strict(generate_tvhello_golden)

    if(NOT TARGET docgen_frame_svg)
        add_library(docgen_frame_svg STATIC frame_svg.cpp)
        target_link_libraries(docgen_frame_svg PUBLIC ckvision::cvision)
        ckvision_strict(docgen_frame_svg)
    endif()

    add_executable(capture_tvhello_screenshots capture_tvhello_screenshots.cpp)
    target_link_libraries(capture_tvhello_screenshots PRIVATE docgen_frame_svg tvhello_app ckvision::cvision)
    target_include_directories(capture_tvhello_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/tvhello)
    ckvision_strict(capture_tvhello_screenshots)
endif()
