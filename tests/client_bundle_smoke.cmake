# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

if(NOT DEFINED CKVISION_SOURCE_DIR OR NOT DEFINED CKVISION_BUILD_DIR OR NOT DEFINED CKVISION_BUNDLE_DIR)
    message(FATAL_ERROR "client_bundle_smoke requires source, build, and bundle directories")
endif()

# The handoff command must refuse an accidental overwrite. This isolated CTest
# fixture owns exactly this deterministic artifact path, so it clears only its
# prior run before asking the command to create a new bundle.
file(REMOVE_RECURSE "${CKVISION_BUNDLE_DIR}")
file(REMOVE "${CKVISION_BUNDLE_DIR}.tar.gz")
execute_process(
    COMMAND "${CKVISION_SOURCE_DIR}/tools/package_client_bundle.sh"
            "${CKVISION_BUILD_DIR}" "${CKVISION_BUNDLE_DIR}"
    RESULT_VARIABLE bundle_result)
if(NOT bundle_result EQUAL 0)
    message(FATAL_ERROR "client bundle generation failed (${bundle_result})")
endif()
if(NOT EXISTS "${CKVISION_BUNDLE_DIR}/sdk/bin/ckvision_terminal")
    message(FATAL_ERROR "client bundle lacks ckvision_terminal")
endif()
if(NOT EXISTS "${CKVISION_BUNDLE_DIR}/docs/generated/screenshots/terminal-menu.svg")
    message(FATAL_ERROR "client bundle lacks generated terminal visual documentation")
endif()
if(NOT EXISTS "${CKVISION_BUNDLE_DIR}.tar.gz")
    message(FATAL_ERROR "client bundle archive is missing")
endif()
