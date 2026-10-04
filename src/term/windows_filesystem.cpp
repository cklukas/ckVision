// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/windows_text.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <aclapi.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace ckv::term {
namespace {

bool separator(char ch) noexcept { return ch == '/' || ch == '\\'; }
bool drive_letter(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

std::optional<std::wstring> to_wide(std::string_view text) {
    if (text.empty() || text.find('\0') != std::string_view::npos) return std::nullopt;
    return windows_utf16(text);
}

std::optional<std::string> to_utf8(std::wstring_view text) {
    if (text.empty()) return std::nullopt;
    return windows_utf8(text);
}

std::size_t root_length(std::wstring_view path) noexcept {
    if (path.size() >= 3 && path[1] == L':' && path[2] == L'\\') return 3;
    if (!path.starts_with(L"\\\\")) return 0;
    const auto server = path.find(L'\\', 2);
    if (server == std::wstring_view::npos || server == 2) return 0;
    const auto share = path.find(L'\\', server + 1);
    if (server + 1 == path.size() || share == server + 1) return 0;
    return share == std::wstring_view::npos ? path.size() : share;
}

bool ordinary_name(std::wstring_view path, bool resolved = true) {
    const auto root = resolved ? root_length(path) :
        (path.size() >= 2 && path[1] == L':' ? std::size_t{2} : std::size_t{0});
    if (resolved && root == 0) return false;
    if (std::any_of(path.begin(), path.end(), [](wchar_t ch) { return ch < 32; })) return false;
    // Extended spelling must not turn Win32 device names into surprising files,
    // and this service models a whole file, never an alternate data stream.
    for (std::size_t start = root; start < path.size();) {
        if (path[start] == L'\\') { ++start; continue; }
        const auto end = path.find(L'\\', start);
        const auto segment = path.substr(start, end == std::wstring_view::npos ? path.size() - start : end - start);
        if (!resolved && (segment == L"." || segment == L"..")) {
            if (end == std::wstring_view::npos) break;
            start = end + 1;
            continue;
        }
        if (segment.find_first_of(L":*?\"<>|") != std::wstring_view::npos) return false;
        if (segment.back() == L' ' || segment.back() == L'.') return false;
        std::wstring base(segment.substr(0, segment.find(L'.')));
        for (auto& ch : base) if (ch >= L'a' && ch <= L'z') ch -= L'a' - L'A';
        if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" ||
            (base.size() == 4 && (base.starts_with(L"COM") || base.starts_with(L"LPT")) &&
             ((base[3] >= L'1' && base[3] <= L'9') || base[3] == L'\u00b9' ||
              base[3] == L'\u00b2' || base[3] == L'\u00b3')))
            return false;
        if (end == std::wstring_view::npos) break;
        start = end + 1;
    }
    return true;
}

struct NativePath {
    std::wstring plain;
    std::wstring extended;
};

std::optional<NativePath> native_path(std::string_view text) {
    auto input = to_wide(text.empty() ? std::string_view(".") : text);
    if (!input) return std::nullopt;
    std::replace(input->begin(), input->end(), L'/', L'\\');
    if (input->starts_with(L"\\\\?\\UNC\\")) *input = L"\\\\" + input->substr(8);
    else if (input->starts_with(L"\\\\?\\")) {
        if (input->size() < 7 || (*input)[5] != L':' || (*input)[6] != L'\\' ||
            !(((*input)[4] >= L'A' && (*input)[4] <= L'Z') || ((*input)[4] >= L'a' && (*input)[4] <= L'z')))
            return std::nullopt;
        *input = input->substr(4);
    }
    if (input->starts_with(L"\\\\.\\") || input->starts_with(L"\\??\\")) return std::nullopt;
    // Win32 canonicalization may strip a trailing space/period. Reject the
    // original components first rather than silently redirecting such a name.
    if (!ordinary_name(*input, false)) return std::nullopt;
    const DWORD count = ::GetFullPathNameW(input->c_str(), 0, nullptr, nullptr);
    if (count == 0 || count > 32767) return std::nullopt;
    std::wstring plain(static_cast<std::size_t>(count), L'\0');
    const DWORD written = ::GetFullPathNameW(input->c_str(), count, plain.data(), nullptr);
    if (written == 0 || written >= count) return std::nullopt;
    plain.resize(written);
    if (plain.find_first_of(L"*?\"<>|") != std::wstring::npos ||
        plain.find(L':', plain.starts_with(L"\\\\") ? 0 : 2) != std::wstring::npos)
        return std::nullopt;
    const auto root = root_length(plain);
    while (plain.size() > root && plain.back() == L'\\') plain.pop_back();
    if (!ordinary_name(plain)) return std::nullopt;
    std::wstring extended = plain.starts_with(L"\\\\") ? L"\\\\?\\UNC\\" + plain.substr(2) : L"\\\\?\\" + plain;
    if (extended.size() >= 32767) return std::nullopt;
    return NativePath{std::move(plain), std::move(extended)};
}

class Handle final {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~Handle() { close(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const noexcept { return value_; }
    bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }
    bool close() noexcept {
        if (!valid()) return true;
        const HANDLE value = std::exchange(value_, INVALID_HANDLE_VALUE);
        return ::CloseHandle(value) != 0;
    }
private:
    HANDLE value_;
};

bool directory_handle(HANDLE value) noexcept {
    BY_HANDLE_FILE_INFORMATION info{};
    return ::GetFileInformationByHandle(value, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::optional<FileFingerprint> handle_metadata(HANDLE value) {
    FILE_ID_INFO identity{};
    FILE_STANDARD_INFO standard{};
    FILE_BASIC_INFO basic{};
    if (::GetFileType(value) != FILE_TYPE_DISK ||
        !::GetFileInformationByHandleEx(value, FileIdInfo, &identity, sizeof(identity)) ||
        !::GetFileInformationByHandleEx(value, FileStandardInfo, &standard, sizeof(standard)) ||
        !::GetFileInformationByHandleEx(value, FileBasicInfo, &basic, sizeof(basic)) ||
        standard.Directory || standard.EndOfFile.QuadPart < 0)
        return std::nullopt;
    std::string token = std::to_string(identity.VolumeSerialNumber) + ":";
    constexpr char digits[] = "0123456789abcdef";
    for (const auto byte : identity.FileId.Identifier) {
        token += digits[byte >> 4];
        token += digits[byte & 15];
    }
    token += ":" + std::to_string(standard.EndOfFile.QuadPart) + ":" +
             std::to_string(basic.LastWriteTime.QuadPart) + ":" + std::to_string(basic.ChangeTime.QuadPart);
    return FileFingerprint{std::move(token)};
}

class ContentHash final {
public:
    ContentHash() {
        if (::BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return;
        // Windows 7+ owns the hash object buffer when none is supplied; destroy
        // the hash before its algorithm provider, on every success/failure path.
        if (::BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) != 0) hash_ = nullptr;
    }
    ~ContentHash() {
        if (hash_) ::BCryptDestroyHash(hash_);
        if (algorithm_) ::BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    ContentHash(const ContentHash&) = delete;
    ContentHash& operator=(const ContentHash&) = delete;
    bool valid() const noexcept { return hash_ != nullptr; }
    bool append(unsigned char* bytes, ULONG count) {
        return ::BCryptHashData(hash_, bytes, count, 0) == 0;
    }
    std::optional<std::string> finish() {
        std::array<unsigned char, 32> digest{};
        if (::BCryptFinishHash(hash_, digest.data(), static_cast<ULONG>(digest.size()), 0) != 0)
            return std::nullopt;
        std::string result;
        result.reserve(digest.size() * 2);
        constexpr char digits[] = "0123456789abcdef";
        for (const auto byte : digest) { result += digits[byte >> 4]; result += digits[byte & 15]; }
        return result;
    }
private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
};

// The handle is newly opened at offset zero. Hash exactly the returned byte
// stream, not a later reopening of a possibly replaced path. The same helper
// backs fingerprint-only queries without allocating a whole-file string.
std::optional<FileFingerprint> read_revision(HANDLE value, std::string* contents = nullptr) {
    const auto before = handle_metadata(value);
    ContentHash hash;
    if (!before || !hash.valid()) return std::nullopt;
    std::array<unsigned char, 65536> bytes{};
    for (;;) {
        DWORD count = 0;
        if (!::ReadFile(value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr))
            return std::nullopt;
        if (count == 0) break;
        if (!hash.append(bytes.data(), count)) return std::nullopt;
        if (contents) contents->append(reinterpret_cast<const char*>(bytes.data()), count);
    }
    const auto after = handle_metadata(value);
    const auto digest = hash.finish();
    if (!after || before != after || !digest) return std::nullopt;
    return FileFingerprint{after->value + ":" + *digest};
}

Handle open_reader(const std::wstring& path) {
    return Handle(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
}

std::optional<FileFingerprint> path_fingerprint(const std::wstring& path) {
    const auto reader = open_reader(path);
    return reader.valid() ? read_revision(reader.get()) : std::nullopt;
}

class DirectoryLock final {
public:
    explicit DirectoryLock(const std::wstring& directory)
        : handle_(::CreateFileW((directory + L"\\.ckvision-write.lock").c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_HIDDEN, nullptr)) {
        if (!handle_.valid()) return;
        OVERLAPPED position{};
        locked_ = ::LockFileEx(handle_.get(), LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &position) != 0;
    }
    ~DirectoryLock() {
        if (!locked_) return;
        OVERLAPPED position{};
        ::UnlockFileEx(handle_.get(), 0, 1, 0, &position);
    }
    DirectoryLock(const DirectoryLock&) = delete;
    DirectoryLock& operator=(const DirectoryLock&) = delete;
    bool valid() const noexcept { return locked_; }
private:
    Handle handle_;
    bool locked_ = false;
};

class Security final {
public:
    Security() = default;
    Security(const Security&) = delete;
    Security& operator=(const Security&) = delete;
    ~Security() { if (descriptor_) ::LocalFree(descriptor_); }
    bool load(HANDLE source) {
        return ::GetSecurityInfo(source, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                 nullptr, nullptr, &dacl_, nullptr, &descriptor_) == ERROR_SUCCESS;
    }
    PSECURITY_DESCRIPTOR descriptor() const noexcept { return descriptor_; }
    bool apply(HANDLE target) const {
        SECURITY_DESCRIPTOR_CONTROL control{};
        DWORD revision = 0;
        if (!::GetSecurityDescriptorControl(descriptor_, &control, &revision)) return false;
        const auto flags = DACL_SECURITY_INFORMATION |
            ((control & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION);
        return ::SetSecurityInfo(target, SE_FILE_OBJECT, flags, nullptr, nullptr, dacl_, nullptr) == ERROR_SUCCESS;
    }
private:
    PSECURITY_DESCRIPTOR descriptor_ = nullptr;
    PACL dacl_ = nullptr;
};

class TemporaryFile final {
public:
    TemporaryFile(const std::wstring& directory, PSECURITY_DESCRIPTOR security) {
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), security, FALSE};
        for (int attempt = 0; attempt < 64; ++attempt) {
            std::array<unsigned char, 16> random{};
            if (::BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
                return;
            std::wstring name = directory + L"\\.ckvision-write-";
            constexpr wchar_t digits[] = L"0123456789abcdef";
            for (const auto byte : random) { name += digits[byte >> 4]; name += digits[byte & 15]; }
            name += L".tmp";
            value_ = ::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE | WRITE_DAC,
                                  FILE_SHARE_READ | FILE_SHARE_DELETE, security ? &attributes : nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (valid() || ::GetLastError() != ERROR_FILE_EXISTS) return;
        }
    }
    ~TemporaryFile() {
        if (!valid()) return;
        if (!published_) {
            FILE_DISPOSITION_INFO disposition{TRUE};
            ::SetFileInformationByHandle(value_, FileDispositionInfo, &disposition, sizeof(disposition));
        }
        ::CloseHandle(value_);
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    bool valid() const noexcept { return value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const noexcept { return value_; }
    bool publish(const std::wstring& target, bool replace) {
        const std::size_t size = offsetof(FILE_RENAME_INFO, FileName) + (target.size() + 1) * sizeof(wchar_t);
        if (size > (std::numeric_limits<DWORD>::max)()) return false;
        // Allocate suitably aligned native structure storage, not a byte-vector
        // cast whose object lifetime/alignment is implicit.
        const std::size_t words = (size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t);
        std::vector<std::max_align_t> storage(words);
        auto* const info = ::new (storage.data()) FILE_RENAME_INFO{};
        info->Flags = replace ? FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS : 0;
        info->RootDirectory = nullptr;
        info->FileNameLength = static_cast<DWORD>(target.size() * sizeof(wchar_t));
        std::memcpy(info->FileName, target.c_str(), (target.size() + 1) * sizeof(wchar_t));
        published_ = ::SetFileInformationByHandle(value_, FileRenameInfoEx, info, static_cast<DWORD>(size)) != 0;
        return published_;
    }
    bool close_published() noexcept {
        if (!published_ || !valid()) return false;
        const auto value = std::exchange(value_, INVALID_HANDLE_VALUE);
        return ::CloseHandle(value) != 0;
    }
private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
    bool published_ = false;
};

bool write_all(HANDLE value, std::string_view contents) {
    while (!contents.empty()) {
        const auto chunk = static_cast<DWORD>((std::min)(contents.size(), std::size_t{65536}));
        DWORD written = 0;
        if (!::WriteFile(value, contents.data(), chunk, &written, nullptr) || written == 0) return false;
        contents.remove_prefix(written);
    }
    return ::FlushFileBuffers(value) != 0;
}

}  // namespace

std::string WindowsFileSystem::normalize_path(std::string_view path) const {
    const auto native = native_path(path);
    if (!native) return {};
    auto result = to_utf8(native->plain);
    if (!result) return {};
    std::replace(result->begin(), result->end(), '\\', '/');
    return *result;
}

bool WindowsFileSystem::is_absolute_path(std::string_view path) const noexcept {
    if (path.starts_with("\\\\?\\UNC\\")) {
        path.remove_prefix(8);
        const auto server = path.find_first_of("/\\");
        return server != std::string_view::npos && server > 0 && server + 1 < path.size() &&
               !separator(path[server + 1]);
    }
    if (path.starts_with("\\\\?\\")) path.remove_prefix(4);
    if (path.size() >= 3 && drive_letter(path[0]) && path[1] == ':' && separator(path[2])) return true;
    if (path.size() < 5 || !separator(path[0]) || !separator(path[1]) || separator(path[2])) return false;
    const auto server = path.find_first_of("/\\", 2);
    return server != std::string_view::npos && server + 1 < path.size() && !separator(path[server + 1]);
}

std::string WindowsFileSystem::join(std::string_view directory, std::string_view name) const {
    if (is_absolute_path(name) || (name.size() >= 2 && name[1] == ':')) return {};
    while (!name.empty() && separator(name.front())) name.remove_prefix(1);
    const auto root = normalize_path(directory);
    if (root.empty()) return {};
    return normalize_path(root + "/" + std::string(name));
}

std::string WindowsFileSystem::parent(std::string_view path) const {
    const auto native = native_path(path);
    if (!native) return {};
    const auto root = root_length(native->plain);
    const auto end = native->plain.find_last_of(L'\\');
    const auto length = native->plain.size() <= root || end < root ? root : end;
    auto result = to_utf8(std::wstring_view(native->plain).substr(0, length));
    if (!result) return {};
    std::replace(result->begin(), result->end(), '\\', '/');
    return *result;
}

bool WindowsFileSystem::exists(std::string_view path) const noexcept {
    try {
        const auto native = native_path(path);
        if (!native) return false;
        Handle handle(::CreateFileW(native->extended.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        return handle.valid();
    } catch (...) { return false; }
}

bool WindowsFileSystem::is_directory(std::string_view path) const noexcept {
    try {
        const auto native = native_path(path);
        if (!native) return false;
        Handle handle(::CreateFileW(native->extended.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        return handle.valid() && directory_handle(handle.get());
    } catch (...) { return false; }
}

std::vector<FileEntry> WindowsFileSystem::list_directory(std::string_view path) const {
    const auto native = native_path(path);
    if (!native || !is_directory(path)) return {};
    WIN32_FIND_DATAW data{};
    const auto handle = ::FindFirstFileW((native->extended + L"\\*").c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return {};
    struct CloseFind { HANDLE value; ~CloseFind() { ::FindClose(value); } } closer{handle};
    std::vector<FileEntry> result;
    do {
        const std::wstring_view name(data.cFileName);
        if (name == L"." || name == L"..") continue;
        auto encoded = to_utf8(name);
        if (encoded) result.push_back({std::move(*encoded), (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0});
    } while (::FindNextFileW(handle, &data));
    if (::GetLastError() != ERROR_NO_MORE_FILES) return {};
    std::sort(result.begin(), result.end(), [](const FileEntry& left, const FileEntry& right) {
        return left.is_directory != right.is_directory ? left.is_directory : left.name < right.name;
    });
    return result;
}

bool WindowsFileSystem::create_directories(std::string_view path) {
    const auto native = native_path(path);
    if (!native) return false;
    const auto root = root_length(native->plain);
    for (std::size_t end = root; end <= native->plain.size(); ++end) {
        if (end != native->plain.size() && native->plain[end] != L'\\') continue;
        const auto prefix = native->plain.substr(0, end);
        const auto encoded = to_utf8(prefix);
        if (!encoded) return false;
        if (is_directory(*encoded)) continue;
        const auto component = native_path(*encoded);
        if (!component || !::CreateDirectoryW(component->extended.c_str(), nullptr)) {
            if (!is_directory(*encoded)) return false;
        }
    }
    return true;
}

std::optional<FileReadResult> WindowsFileSystem::read_file(std::string_view path) const {
    const auto native = native_path(path);
    if (!native) return std::nullopt;
    auto reader = open_reader(native->extended);
    if (!reader.valid()) return std::nullopt;
    std::string contents;
    const auto revision = read_revision(reader.get(), &contents);
    if (!revision || !reader.close()) return std::nullopt;
    return FileReadResult{std::move(contents), *revision};
}

std::optional<FileFingerprint> WindowsFileSystem::fingerprint(std::string_view path) const {
    const auto native = native_path(path);
    return native ? path_fingerprint(native->extended) : std::nullopt;
}

FileWriteResult WindowsFileSystem::write_file_atomic(std::string_view path, std::string_view contents,
                                                   FileWriteExpectation expectation) {
    const auto native = native_path(path);
    if (!native) return {};
    if (native->plain.size() <= root_length(native->plain)) return {FileWriteStatus::NotFound, std::nullopt};
    const auto split = native->extended.find_last_of(L'\\');
    const auto directory = native->extended.substr(0, split);
    const auto name = native->extended.substr(split + 1);
    if (::CompareStringOrdinal(name.c_str(), -1, L".ckvision-write.lock", -1, TRUE) == CSTR_EQUAL) return {};
    if (!is_directory(parent(path))) return {FileWriteStatus::NotFound, std::nullopt};
    DirectoryLock lock(directory);
    if (!lock.valid()) return {};
    const DWORD attributes = ::GetFileAttributesW(native->extended.c_str());
    const DWORD attribute_error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : ERROR_SUCCESS;
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return {FileWriteStatus::NotFound, std::nullopt};
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return {};
    if (attributes == INVALID_FILE_ATTRIBUTES && attribute_error != ERROR_FILE_NOT_FOUND)
        return {};
    const bool existed = attributes != INVALID_FILE_ATTRIBUTES;
    auto current = open_reader(native->extended);
    const auto before = current.valid() ? read_revision(current.get()) : std::nullopt;
    if (existed && !before) return {};
    if ((expectation.kind == FileWriteExpectationKind::MustNotExist && existed) ||
        (expectation.kind == FileWriteExpectationKind::MatchFingerprint &&
         (!expectation.fingerprint || expectation.fingerprint != before)))
        return {FileWriteStatus::Conflict, before};
    Security security;
    if (existed && !security.load(current.get())) return {};
    if (!current.close()) return {};
    TemporaryFile temporary(directory, security.descriptor());
    if (!temporary.valid() || (existed && !security.apply(temporary.get())) ||
        !write_all(temporary.get(), contents))
        return {};
    if (!temporary.publish(native->extended, expectation.kind != FileWriteExpectationKind::MustNotExist)) {
        const auto error = ::GetLastError();
        if (expectation.kind == FileWriteExpectationKind::MustNotExist &&
            (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS))
            return {FileWriteStatus::Conflict, path_fingerprint(native->extended)};
        return {};
    }
    if (!temporary.close_published()) return {};
    const auto after = path_fingerprint(native->extended);
    return after ? FileWriteResult{FileWriteStatus::Ok, after} : FileWriteResult{};
}

}  // namespace ckv::term
