// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Native test setup and independent byte proof. Not a product publication fallback.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
namespace ckv::bounded_fixture {
enum class Stage { None, Address, Directory, Open, Write, Size, Flush, Rewind, Readback, Identity, Close, Find, FindClose, ExactName };
struct Proof {
    Stage first_stage = Stage::None;
    DWORD first_error = 0;
    std::uint64_t written = 0, readback = 0, size = 0;
    unsigned close_attempts = 0;
    bool close_ok = false, bytes_equal = true, exact_name = false;
    alignas(8) FILE_ID_INFO identity{};
    void fail(Stage stage, DWORD error) noexcept { if (first_stage == Stage::None) { first_stage = stage; first_error = error; } }
    bool ok() const noexcept { return first_stage == Stage::None && close_attempts == 1 && close_ok && bytes_equal && exact_name; }
};
inline std::wstring extended(std::wstring_view input) {
    if (input.empty() || input.find(L'\0') != input.npos) return {};
    std::wstring path(input);
    std::replace(path.begin(), path.end(), L'/', L'\\');
    if (path.starts_with(L"\\\\?\\")) {
        if (path.starts_with(L"\\\\?\\UNC\\") ||
            (path.size() >= 7 && path[5] == L':' && path[6] == L'\\')) return path;
        return {};
    }
    if (path.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + path.substr(2);
    if (path.size() >= 3 && path[1] == L':' && path[2] == L'\\') return L"\\\\?\\" + path;
    return {};
}
inline bool directories(std::wstring_view input, DWORD& error) {
    const auto path = extended(input);
    std::size_t root = 0;
    if (path.starts_with(L"\\\\?\\UNC\\")) {
        const auto server = path.find(L'\\', 8);
        if (server == path.npos) { error = ERROR_BAD_PATHNAME; return false; }
        const auto share = path.find(L'\\', server + 1);
        root = share == path.npos ? path.size() : share;
    } else if (path.size() >= 7 && path[5] == L':' && path[6] == L'\\') root = 7;
    else { error = ERROR_BAD_PATHNAME; return false; }
    for (std::size_t i = root; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\') continue;
        const auto prefix = path.substr(0, i);
        if (::CreateDirectoryW(prefix.c_str(), nullptr)) continue;
        const DWORD native = ::GetLastError();
        const DWORD attributes = ::GetFileAttributesW(prefix.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { error = attributes == INVALID_FILE_ATTRIBUTES ? native : ERROR_INVALID_DATA; return false; }
    }
    error = ERROR_SUCCESS;
    return true;
}
inline Proof create_and_prove(std::wstring_view exact_path, std::string_view expected) {
    Proof result;
    const auto path = extended(exact_path); // No component removal, normalization or short alias.
    if (path.empty()) { result.fail(Stage::Address, ERROR_BAD_PATHNAME); return result; }
    const auto separator = path.find_last_of(L'\\');
    if (separator == path.npos || separator + 1 == path.size()) { result.fail(Stage::Address, ERROR_INVALID_NAME); return result; }
    const std::wstring_view name(path.data() + separator + 1, path.size() - separator - 1);
    HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) { result.fail(Stage::Open, ::GetLastError()); return result; }
    // From here to checked close, all operations use this one native object and fixed buffers.
    while (result.written < expected.size()) {
        const auto remaining = expected.size() - static_cast<std::size_t>(result.written);
        const DWORD request = static_cast<DWORD>((std::min)(remaining, std::size_t{65536}));
        DWORD written = 0;
        if (!::WriteFile(handle, expected.data() + result.written, request, &written, nullptr)) { result.fail(Stage::Write, ::GetLastError()); break; }
        if (written == 0 || written > request) { result.fail(Stage::Write, ERROR_WRITE_FAULT); break; }
        result.written += written;
    }
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(handle, &size)) result.fail(Stage::Size, ::GetLastError());
    else { result.size = static_cast<std::uint64_t>(size.QuadPart); if (size.QuadPart < 0 || result.size != expected.size()) result.fail(Stage::Size, ERROR_INVALID_DATA); }
    if (!::FlushFileBuffers(handle)) result.fail(Stage::Flush, ::GetLastError());
    LARGE_INTEGER zero{};
    if (!::SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN)) result.fail(Stage::Rewind, ::GetLastError());
    std::array<char, 65536> bytes{};
    while (result.first_stage == Stage::None) {
        DWORD count = 0;
        if (!::ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr)) { result.fail(Stage::Readback, ::GetLastError()); break; }
        if (count == 0) break;
        const auto offset = static_cast<std::size_t>(result.readback);
        if (offset > expected.size() || count > expected.size() - offset || std::memcmp(bytes.data(), expected.data() + offset, count) != 0) { result.bytes_equal = false; result.fail(Stage::Readback, ERROR_INVALID_DATA); break; }
        result.readback += count;
    }
    if (result.readback != expected.size()) result.fail(Stage::Readback, ERROR_HANDLE_EOF);
    if (!::GetFileInformationByHandleEx(handle, FileIdInfo, &result.identity, sizeof(result.identity))) result.fail(Stage::Identity, ::GetLastError());
    const HANDLE consumed = std::exchange(handle, INVALID_HANDLE_VALUE);
    ++result.close_attempts;
    result.close_ok = ::CloseHandle(consumed) != FALSE;
    if (!result.close_ok) result.fail(Stage::Close, ::GetLastError()); // Consumed once; no retry.
    WIN32_FIND_DATAW data{};
    HANDLE find = ::FindFirstFileW(path.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) result.fail(Stage::Find, ::GetLastError());
    else {
        result.exact_name = std::wstring_view(data.cFileName) == name;
        if (!::FindClose(find)) result.fail(Stage::FindClose, ::GetLastError());
        if (!result.exact_name) result.fail(Stage::ExactName, ERROR_INVALID_NAME);
    }
    return result;
}
}
