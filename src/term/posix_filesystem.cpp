// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/posix_filesystem.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstring>

namespace ckv::term {

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

}  // namespace ckv::term
