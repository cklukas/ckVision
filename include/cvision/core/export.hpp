// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#if defined(CKVISION_SHARED)
#  if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(CKVISION_BUILDING_LIBRARY)
#      define CVISION_API __declspec(dllexport)
#    else
#      define CVISION_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    define CVISION_API __attribute__((visibility("default")))
#  else
#    define CVISION_API
#  endif
#else
#  define CVISION_API
#endif
