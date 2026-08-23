// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/posix_filesystem.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace ckv::term {
namespace {

std::optional<FileFingerprint> fingerprint_for(const std::string& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return std::nullopt;
#if defined(__APPLE__)
    const auto seconds = static_cast<long long>(st.st_mtimespec.tv_sec);
    const auto nanoseconds = static_cast<long long>(st.st_mtimespec.tv_nsec);
#else
    const auto seconds = static_cast<long long>(st.st_mtim.tv_sec);
    const auto nanoseconds = static_cast<long long>(st.st_mtim.tv_nsec);
#endif
    return FileFingerprint{std::to_string(static_cast<unsigned long long>(st.st_dev)) + ":" +
                           std::to_string(static_cast<unsigned long long>(st.st_ino)) + ":" +
                           std::to_string(static_cast<unsigned long long>(st.st_size)) + ":" +
                           std::to_string(seconds) + ":" + std::to_string(nanoseconds)};
}

bool write_all(int descriptor, std::string_view value) noexcept {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const ssize_t written = ::write(descriptor, value.data() + offset, value.size() - offset);
        if (written <= 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

}  // namespace

std::vector<FileEntry> PosixFileSystem::list_directory(std::string_view path) const {
    std::vector<FileEntry> out;
    // POSIX opendir() needs a NUL-terminated C string; `path` is a
    // std::string_view with no such guarantee.
    const std::string path_str(path);
    DIR* dir = ::opendir(path_str.c_str());
    if (dir == nullptr) return out;  // doesn't exist / not a directory / no permission: empty, not an error

    while (const struct dirent* entry = ::readdir(dir)) {
        const std::string_view name = entry->d_name;
        if (name == "." || name == "..") continue;

        FileEntry fe;
        fe.name = std::string(name);
        fe.is_directory = is_directory(join(path, fe.name));
        out.push_back(std::move(fe));
    }
    ::closedir(dir);

    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) return a.is_directory > b.is_directory;  // directories first
        return a.name < b.name;
    });
    return out;
}

bool PosixFileSystem::exists(std::string_view path) const noexcept {
    const std::string path_str(path);
    struct stat st{};
    return ::stat(path_str.c_str(), &st) == 0;
}

bool PosixFileSystem::is_directory(std::string_view path) const noexcept {
    const std::string path_str(path);
    struct stat st{};
    if (::stat(path_str.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

std::optional<FileReadResult> PosixFileSystem::read_file(std::string_view path) const {
    const std::string path_string(path);
    const int descriptor = ::open(path_string.c_str(), O_RDONLY);
    if (descriptor < 0) return std::nullopt;
    std::string contents;
    std::array<char, 16 * 1024> buffer{};
    while (true) {
        const ssize_t count = ::read(descriptor, buffer.data(), buffer.size());
        if (count < 0) {
            ::close(descriptor);
            return std::nullopt;
        }
        if (count == 0) break;
        contents.append(buffer.data(), static_cast<std::size_t>(count));
    }
    ::close(descriptor);
    const auto value = fingerprint_for(path_string);
    if (!value) return std::nullopt;
    return FileReadResult{std::move(contents), *value};
}

FileWriteResult PosixFileSystem::write_file_atomic(std::string_view path, std::string_view contents,
                                                    FileWriteExpectation expectation) {
    const std::string target(path);
    const auto before = fingerprint_for(target);
    if (expectation.kind == FileWriteExpectationKind::MustNotExist && before)
        return FileWriteResult{FileWriteStatus::Conflict, before};
    if (expectation.kind == FileWriteExpectationKind::MatchFingerprint &&
        (!expectation.fingerprint || before != expectation.fingerprint))
        return FileWriteResult{FileWriteStatus::Conflict, before};
    std::string template_path = target + ".ckvision-tmp.XXXXXX";
    std::vector<char> writable(template_path.begin(), template_path.end());
    writable.push_back('\0');
    const int descriptor = ::mkstemp(writable.data());
    if (descriptor < 0) return FileWriteResult{};
    const std::string temporary(writable.data());
    bool complete = write_all(descriptor, contents);
    if (complete) complete = ::fsync(descriptor) == 0;
    const bool close_ok = ::close(descriptor) == 0;
    complete = complete && close_ok;
    if (!complete) {
        ::unlink(temporary.c_str());
        return FileWriteResult{};
    }
    if (expectation.kind == FileWriteExpectationKind::MustNotExist) {
        if (::link(temporary.c_str(), target.c_str()) != 0) {
            ::unlink(temporary.c_str());
            return FileWriteResult{FileWriteStatus::Conflict, fingerprint_for(target)};
        }
        ::unlink(temporary.c_str());
    } else {
        // POSIX rename has no compare-and-swap form. Revalidate directly before
        // replacement so a detected external modification becomes a conflict;
        // platform adapters with stronger primitives may tighten this further.
        if (expectation.kind == FileWriteExpectationKind::MatchFingerprint && before != fingerprint_for(target)) {
            ::unlink(temporary.c_str());
            return FileWriteResult{FileWriteStatus::Conflict, fingerprint_for(target)};
        }
        if (::rename(temporary.c_str(), target.c_str()) != 0) {
            ::unlink(temporary.c_str());
            return FileWriteResult{};
        }
    }
    const auto after = fingerprint_for(target);
    return after ? FileWriteResult{FileWriteStatus::Ok, after} : FileWriteResult{};
}

std::optional<FileFingerprint> PosixFileSystem::fingerprint(std::string_view path) const {
    return fingerprint_for(std::string(path));
}

}  // namespace ckv::term
