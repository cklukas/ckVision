// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_text.hpp"
#include "cvision/testing/cktest.hpp"
#include <array>

CK_TEST(windows_native_text_matches_literal_unicode_goldens_without_a_code_page) {
    const std::string utf8 = "é 日本 😀 & ^ %";
    const std::wstring wide = L"é 日本 😀 & ^ %";
    CK_CHECK(ckv::term::windows_utf16(utf8) == wide);
    CK_CHECK(ckv::term::windows_utf8(wide) == utf8);
    CK_CHECK(ckv::term::windows_utf16("") == L"");
    CK_CHECK(ckv::term::windows_utf8(L"") == "");
}

CK_TEST(windows_native_text_preserves_explicit_nuls_without_path_interpretation) {
    CK_CHECK(ckv::term::windows_utf16(std::string_view("a\0b", 3)) == std::wstring(L"a\0b", 3));
    CK_CHECK(ckv::term::windows_utf8(std::wstring_view(L"a\0b", 3)) == std::string("a\0b", 3));
}

CK_TEST(windows_native_text_refuses_malformed_utf8_and_unpaired_surrogates) {
    for (const std::string_view malformed : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"})
        CK_CHECK(!ckv::term::windows_utf16(malformed));
    const std::array<wchar_t, 2> pair{static_cast<wchar_t>(0xd83d), static_cast<wchar_t>(0xde00)};
    CK_CHECK(!ckv::term::windows_utf8(std::wstring_view(pair.data(), 1)));
    CK_CHECK(!ckv::term::windows_utf8(std::wstring_view(pair.data() + 1, 1)));
    CK_CHECK(ckv::term::windows_utf8(std::wstring_view(pair.data(), pair.size())) == "😀");
}
