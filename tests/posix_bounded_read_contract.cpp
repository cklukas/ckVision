// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/posix_filesystem.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
constexpr std::size_t chunk_bytes = 16 * 1024;
constexpr std::size_t original_bytes = 2 * chunk_bytes;
constexpr std::size_t settings_cap = 1024 * 1024;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class OwnedFile {
public:
    explicit OwnedFile(std::string path) : path_(std::move(path)) {
        descriptor_ = ::open(path_.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
        require(descriptor_ >= 0, "create fresh native fixture file");
    }
    ~OwnedFile() {
        (void)close();
        if (owns_name_) (void)::unlink(path_.c_str());
    }
    OwnedFile(const OwnedFile&) = delete;
    OwnedFile& operator=(const OwnedFile&) = delete;

    int get() const noexcept { return descriptor_; }
    const std::string& path() const noexcept { return path_; }
    bool close() noexcept {
        if (descriptor_ < 0) return true;
        return ::close(std::exchange(descriptor_, -1)) == 0;
    }
    void renamed() noexcept { owns_name_ = false; }

private:
    std::string path_;
    int descriptor_ = -1;
    bool owns_name_ = true;
};

struct ScratchDirectory {
    std::string path;
    ~ScratchDirectory() { (void)::rmdir(path.c_str()); }
};

void write_all(int fd, std::string_view data) {
    while (!data.empty()) {
        const ssize_t count = ::write(fd, data.data(), data.size());
        if (count < 0 && errno == EINTR) continue;
        require(count > 0, "native fixture write");
        data.remove_prefix(static_cast<std::size_t>(count));
    }
}

enum class Mutation { None, Grow, Replace };
struct Observation {
    struct stat original{};
    Mutation mutation = Mutation::None;
    int writer = -1;
    off_t grown_size = 0;
    const char* target = nullptr;
    const char* replacement = nullptr;
    unsigned reads = 0;
    std::size_t consumed = 0, max_request = 0;
    bool same_source = true, actor_succeeded = false;
    int actor_error = 0;
};
thread_local Observation* active = nullptr;

class ObserveReads {
public:
    explicit ObserveReads(Observation& observation) noexcept : previous_(active) { active = &observation; }
    ~ObserveReads() { active = previous_; }
private:
    Observation* previous_;
};
} // namespace

namespace ckv::term::bounded_read_test {
void after_read(int reader, std::size_t request, std::size_t count) noexcept {
    if (!active) return;
    auto& result = *active;
    ++result.reads;
    result.consumed += count;
    result.max_request = std::max(result.max_request, request);
    struct stat held{};
    result.same_source = result.same_source && ::fstat(reader, &held) == 0 &&
        held.st_dev == result.original.st_dev && held.st_ino == result.original.st_ino;
    if (result.reads != 1 || !result.same_source) return;

    switch (result.mutation) {
    case Mutation::None:
        return;
    case Mutation::Grow: {
        struct stat writer{};
        result.actor_succeeded = reader != result.writer && ::fstat(result.writer, &writer) == 0 &&
            writer.st_dev == held.st_dev && writer.st_ino == held.st_ino && writer.st_size == held.st_size &&
            ::ftruncate(result.writer, result.grown_size) == 0 && ::fstat(reader, &held) == 0 &&
            held.st_size == result.grown_size && held.st_dev == writer.st_dev && held.st_ino == writer.st_ino;
        break;
    }
    case Mutation::Replace:
        result.actor_succeeded = ::rename(result.replacement, result.target) == 0;
        break;
    }
    if (!result.actor_succeeded) result.actor_error = errno;
}
} // namespace ckv::term::bounded_read_test

namespace {
void oversized(const std::string& root, const ckv::term::PosixFileSystem& fs) {
    OwnedFile file(root + "/oversized");
    require(::ftruncate(file.get(), static_cast<off_t>(settings_cap + 1)) == 0, "real oversized extent");
    Observation result;
    require(::fstat(file.get(), &result.original) == 0 &&
            result.original.st_size == static_cast<off_t>(settings_cap + 1), "genuine size beyond the settings cap");
    ObserveReads observation(result);
    require(!fs.read_file_bounded(file.path(), settings_cap) && result.reads == 0,
            "pre-existing oversize must fail before the first payload read");
    require(file.close(), "checked oversize fixture close");
}

void growth(const std::string& root, const ckv::term::PosixFileSystem& fs, bool crosses_cap) {
    OwnedFile file(root + (crosses_cap ? "/cross-cap" : "/within-cap"));
    write_all(file.get(), std::string(original_bytes, 'a'));
    Observation result;
    require(::fstat(file.get(), &result.original) == 0 &&
            result.original.st_size == static_cast<off_t>(original_bytes), "genuine original extent");
    result.mutation = Mutation::Grow;
    result.writer = file.get();
    result.grown_size = static_cast<off_t>(original_bytes + 1);
    const auto cap = crosses_cap ? original_bytes : original_bytes + chunk_bytes;
    const auto read = [&] {
        ObserveReads observation(result);
        return fs.read_file_bounded(file.path(), cap);
    }();
    std::printf("POSIX_GROW cap=%zu consumed=%zu reads=%u actor_error=%d\n",
                cap, result.consumed, result.reads, result.actor_error);
    require(result.actor_succeeded && result.same_source && result.reads >= 2,
            "same-object native growth after the first successful read");
    require(!read && result.consumed == original_bytes + 1 && result.max_request <= chunk_bytes,
            "reject growth at the cap sentinel or the final held revision check");
    require(file.close(), "checked growth writer close");
}

void replacement(const std::string& root, const ckv::term::PosixFileSystem& fs) {
    OwnedFile original(root + "/target");
    OwnedFile replacement(root + "/replacement");
    const std::string old_bytes(original_bytes, 'a');
    write_all(original.get(), old_bytes);
    write_all(replacement.get(), "new object");
    Observation result;
    struct stat incoming{};
    require(::fstat(original.get(), &result.original) == 0 && ::fstat(replacement.get(), &incoming) == 0 &&
            (incoming.st_dev != result.original.st_dev || incoming.st_ino != result.original.st_ino),
            "independently created replacement has distinct native identity");
    const bool original_closed = original.close();
    const bool replacement_closed = replacement.close();
    require(original_closed && replacement_closed, "checked actor setup closes");
    const auto old_revision = fs.fingerprint(original.path());
    require(old_revision.has_value(), "old native revision");
    result.mutation = Mutation::Replace;
    result.target = original.path().c_str();
    result.replacement = replacement.path().c_str();
    const auto read = [&] {
        ObserveReads observation(result);
        return fs.read_file_bounded(original.path(), old_bytes.size());
    }();
    if (result.actor_succeeded) replacement.renamed();
    require(result.actor_succeeded && result.same_source && result.reads >= 2 &&
            result.consumed == old_bytes.size(), "native pathname replacement with the actual old reader still held");
    require(read && read->contents == old_bytes && read->fingerprint == *old_revision,
            "returned bytes and revision belong to the held old inode");
    const auto current = fs.read_file_bounded(original.path(), 32);
    require(current && current->contents == "new object" && current->fingerprint != *old_revision,
            "current pathname has the independent replacement revision");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2 && argv[1][0] == '/', "explicit absolute test workspace required");
        std::string path = std::string(argv[1]) + "/bounded-read-XXXXXX";
        require(::mkdtemp(path.data()) != nullptr, "create fresh owned native test directory");
        ScratchDirectory scratch{std::move(path)};
        ckv::term::PosixFileSystem fs;
        oversized(scratch.path, fs);
        growth(scratch.path, fs, true);
        growth(scratch.path, fs, false);
        replacement(scratch.path, fs);
        std::puts("POSIX_BOUNDED_TERMINAL cases=4 failures=0");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "POSIX_BOUNDED_FAIL %s\n", error.what());
        return 1;
    }
}
