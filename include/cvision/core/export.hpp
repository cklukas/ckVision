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

// Preserve exported type information on ELF and Mach-O. Windows DLL entry
// points use member annotations so class storage can stay in the SDK headers.
#if defined(CKVISION_SHARED) && (defined(_WIN32) || defined(__CYGWIN__))
#  define CVISION_TYPE_API
#  define CVISION_MEMBER_API CVISION_API
#else
#  define CVISION_TYPE_API CVISION_API
#  define CVISION_MEMBER_API
#endif

#if defined(CKVISION_SHARED) && defined(_MSC_VER)
// The MSVC iterator header loads and validates the STL configuration.
#  include <iterator>
#  if !defined(_DLL)
#    error "The ckVision DLL requires the dynamic Microsoft CRT (/MD or /MDd)."
#  endif
#  if !defined(_ITERATOR_DEBUG_LEVEL)
#    error "The ckVision DLL requires the Microsoft C++ STL iterator configuration."
#  endif
#  if defined(_DEBUG)
#    define CKVISION_DETAIL_CRT mdd
#  else
#    define CKVISION_DETAIL_CRT md
#  endif
#  define CKVISION_DETAIL_ABI_NAME_IMPL(crt, level) ckvision_abi_ ## crt ## _iterator_ ## level
#  define CKVISION_DETAIL_ABI_NAME(crt, level) CKVISION_DETAIL_ABI_NAME_IMPL(crt, level)
#  define CKVISION_DETAIL_MSVC_ABI CKVISION_DETAIL_ABI_NAME(CKVISION_DETAIL_CRT, _ITERATOR_DEBUG_LEVEL)
// Internal linker dependency for checking DLL and consumer ABI settings.
extern "C" CVISION_API void CKVISION_DETAIL_MSVC_ABI() noexcept;
#  if !defined(CKVISION_BUILDING_LIBRARY)
#    define CKVISION_DETAIL_STRING_IMPL(token) #token
#    define CKVISION_DETAIL_STRING(token) CKVISION_DETAIL_STRING_IMPL(token)
#    if defined(_M_IX86)
#      pragma comment(linker, "/include:__imp__" CKVISION_DETAIL_STRING(CKVISION_DETAIL_MSVC_ABI))
#    else
#      pragma comment(linker, "/include:__imp_" CKVISION_DETAIL_STRING(CKVISION_DETAIL_MSVC_ABI))
#    endif
#    undef CKVISION_DETAIL_STRING
#    undef CKVISION_DETAIL_STRING_IMPL
#  endif
#endif
