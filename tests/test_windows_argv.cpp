// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_argv.hpp"
#include "cvision/testing/cktest.hpp"
#include <array>

CK_TEST(windows_argv_literal_goldens_cover_quotes_slashes_empty_and_unicode) {
    using ckv::term::quote_windows_argument;
    CK_CHECK(quote_windows_argument(L"") == L"\"\"");
    CK_CHECK(quote_windows_argument(L"a b\tc") == L"\"a b\tc\"");
    CK_CHECK(quote_windows_argument(L"a\\b") == L"\"a\\b\"");
    CK_CHECK(quote_windows_argument(L"a\"b") == L"\"a\\\"b\"");
    CK_CHECK(quote_windows_argument(L"a\\\"b") == L"\"a\\\\\\\"b\"");
    CK_CHECK(quote_windows_argument(L"a\\") == L"\"a\\\\\"");
    CK_CHECK(quote_windows_argument(L"é 日本 😀 & ^ %") == L"\"é 日本 😀 & ^ %\"");
}

CK_TEST(windows_argv_first_token_uses_filename_not_regular_argument_grammar) {
    const std::array<std::wstring_view, 4> values{L"C:\\program files\\reader\\", L"", L"a\"b", L"tail\\"};
    const auto encoded = ckv::term::windows_argv_command_line(values);
    CK_CHECK(encoded == L"\"C:\\program files\\reader\\\" \"\" \"a\\\"b\" \"tail\\\\\"");
    const std::array<std::wstring_view, 1> invalid{L"bad\"image"};
    CK_CHECK(!ckv::term::windows_argv_command_line(invalid));
}

CK_TEST(windows_argv_rejects_nuls_empty_images_and_excessive_lengths) {
    CK_CHECK(!ckv::term::quote_windows_argument(std::wstring_view(L"a\0b", 3)));
    CK_CHECK(!ckv::term::windows_argv_command_line({}));
    const std::array<std::wstring_view, 1> empty{L""};
    CK_CHECK(!ckv::term::windows_argv_command_line(empty));
    const std::array<std::wstring_view, 2> nul{L"image", std::wstring_view(L"a\0b", 3)};
    CK_CHECK(!ckv::term::windows_argv_command_line(nul));
    const std::wstring largest(32764, L'x');
    const auto encoded = ckv::term::quote_windows_argument(largest);
    CK_CHECK(encoded.has_value());
    if (encoded) CK_CHECK(encoded->size() == 32766);
    const std::wstring excessive(32765, L'x');
    CK_CHECK(!ckv::term::quote_windows_argument(excessive));
    const std::array<std::wstring_view, 2> long_line{L"image", largest};
    CK_CHECK(!ckv::term::windows_argv_command_line(long_line));
}
