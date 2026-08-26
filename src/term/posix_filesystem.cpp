// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/posix_filesystem.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <vector>

namespace ckv::term {
namespace {

FileFingerprint fingerprint_for(const struct stat& st) {
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

std::optional<FileFingerprint> fingerprint_for(const std::string& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return std::nullopt;
    return fingerprint_for(st);
}

std::string parent_directory(const std::string& path) {
    const std::size_t separator = path.find_last_of('/');
    if (separator == std::string::npos) return ".";
    if (separator == 0) return "/";
    return path.substr(0, separator);
}

class Descriptor final {
public:
    explicit Descriptor(int value = -1) noexcept : value_(value) {}
    ~Descriptor() {
        if (value_ >= 0) ::close(value_);
    }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;

    int get() const noexcept { return value_; }
    bool valid() const noexcept { return value_ >= 0; }
    bool close() noexcept {
        if (value_ < 0) return true;
        const int value = value_;
        value_ = -1;
        return ::close(value) == 0;
    }

private:
    int value_ = -1;
};

bool acquire_exclusive_lock(int descriptor) noexcept {
    struct flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    while (::fcntl(descriptor, F_SETLKW, &lock) != 0) {
        if (errno != EINTR) return false;
    }
    return true;
}

bool sync_directory(const std::string& path) noexcept {
    Descriptor directory(::open(path.c_str(), O_RDONLY | O_DIRECTORY));
    return directory.valid() && ::fsync(directory.get()) == 0 && directory.close();
}

bool write_all(int descriptor, std::string_view value) noexcept {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const ssize_t written = ::write(descriptor, value.data() + offset, value.size() - offset);
        if (written < 0 && errno == EINTR) continue;
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

bool PosixFileSystem::create_directories(std::string_view path) {
    const std::string normalized = normalize_path(path);
    if (normalized.empty() || normalized.front() != '/') return false;
    if (normalized == "/") return true;
    std::size_t position = 1;
    while (position <= normalized.size()) {
        const std::size_t separator = normalized.find('/', position);
        const std::string prefix = normalized.substr(0, separator);
        if (::mkdir(prefix.c_str(), 0755) != 0 && errno != EEXIST) return false;
        if (!is_directory(prefix)) return false;
        if (separator == std::string::npos) break;
        position = separator + 1;
    }
    return true;
}

std::optional<FileReadResult> PosixFileSystem::read_file(std::string_view path) const {
    const std::string path_string(path);
    Descriptor descriptor(::open(path_string.c_str(), O_RDONLY));
    if (!descriptor.valid()) return std::nullopt;
    struct stat before{};
    if (::fstat(descriptor.get(), &before) != 0 || !S_ISREG(before.st_mode)) return std::nullopt;
    std::string contents;
    std::array<char, 16 * 1024> buffer{};
    while (true) {
        const ssize_t count = ::read(descriptor.get(), buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            return std::nullopt;
        }
        if (count == 0) break;
        contents.append(buffer.data(), static_cast<std::size_t>(count));
    }
    struct stat after{};
    if (::fstat(descriptor.get(), &after) != 0 || fingerprint_for(before) != fingerprint_for(after) ||
        !descriptor.close())
        return std::nullopt;
    return FileReadResult{std::move(contents), fingerprint_for(after)};
}

FileWriteResult PosixFileSystem::write_file_atomic(std::string_view path, std::string_view contents,
                                                    FileWriteExpectation expectation) {
    const std::string target(path);
    const std::string directory_path = parent_directory(target);
    const std::string lock_path = directory_path + "/.ckvision-write.lock";
    Descriptor lock(::open(lock_path.c_str(), O_CREAT | O_RDWR, 0600));
    if (!lock.valid() || !acquire_exclusive_lock(lock.get())) return FileWriteResult{};

    // The expectation check and publication are one critical section for all
    // cooperating PosixFileSystem instances. The stable directory lock is
    // separate from the replaceable target inode, so rename cannot invalidate
    // the lock protecting it.
    const auto before = fingerprint_for(target);
    if (expectation.kind == FileWriteExpectationKind::MustNotExist && before)
        return FileWriteResult{FileWriteStatus::Conflict, before};
    if (expectation.kind == FileWriteExpectationKind::MatchFingerprint &&
        (!expectation.fingerprint || before != expectation.fingerprint))
        return FileWriteResult{FileWriteStatus::Conflict, before};
    std::string template_path = target + ".ckvision-tmp.XXXXXX";
    std::vector<char> writable(template_path.begin(), template_path.end());
    writable.push_back('\0');
    Descriptor descriptor(::mkstemp(writable.data()));
    if (!descriptor.valid()) return FileWriteResult{};
    const std::string temporary(writable.data());
    bool complete = write_all(descriptor.get(), contents);
    if (complete) complete = ::fsync(descriptor.get()) == 0;
    complete = descriptor.close() && complete;
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
        if (::rename(temporary.c_str(), target.c_str()) != 0) {
            ::unlink(temporary.c_str());
            return FileWriteResult{};
        }
    }
    if (!sync_directory(directory_path)) return FileWriteResult{};
    const auto after = fingerprint_for(target);
    return after ? FileWriteResult{FileWriteStatus::Ok, after} : FileWriteResult{};
}

std::optional<FileFingerprint> PosixFileSystem::fingerprint(std::string_view path) const {
    return fingerprint_for(std::string(path));
}

}  // namespace ckv::term
