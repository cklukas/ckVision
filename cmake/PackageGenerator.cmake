# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
if(CPACK_GENERATOR MATCHES "^(DEB|RPM)$")
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
else()
    set(CPACK_PACKAGING_INSTALL_PREFIX /)
endif()
if(CPACK_CKVISION_WINDOWS AND NOT CPACK_CKVISION_STATIC_CRT)
    message(FATAL_ERROR "Windows ckVision packages require a Release build with -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded")
endif()
if(CPACK_GENERATOR STREQUAL "WIX" AND CMAKE_VERSION VERSION_LESS 4.3)
    message(FATAL_ERROR "The per-user ckVision MSI requires CMake 4.3 or newer")
endif()
