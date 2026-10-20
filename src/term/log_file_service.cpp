// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/log_file_service.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "cvision/core/assert.hpp"
#include "cvision/term/terminal.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "cvision/term/windows_text.hpp"
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/event.h>
#else
#include <sys/inotify.h>
#endif
#endif

namespace ckv::term {
namespace {
constexpr std::size_t kPathBytes = 32768;
enum class SlotState { Available, Active, Retiring, Dead };
enum class Publication { Empty, Writing, Ready, Reading };
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

struct Request {
    std::uint64_t revision = 0;
    LogFileOperation operation = LogFileOperation::Inspect;
    LogFollowMode follow = LogFollowMode::Path;
    std::uint64_t offset = 0;
    std::size_t bytes = 0, path_size = 0;
    bool valid = false;
    std::array<char, kPathBytes + 1> path{};
};

// SPSC ownership exchange: writer/read actor always owns a distinct buffer.
// Only request revisions may supersede one another; reads never use Latest.
class Requests {
public:
    Request& writable() noexcept { return values_[write_]; }
    void publish() noexcept { write_ = middle_.exchange(write_ | kDirty) & kIndex; }
    bool pending() const noexcept { return (middle_.load() & kDirty) != 0; }
    const Request& read() noexcept {
        if (pending()) read_ = middle_.exchange(read_) & kIndex;
        return values_[read_];
    }
private:
    static constexpr unsigned kDirty = 4, kIndex = 3;
    std::array<Request, 3> values_;
    unsigned write_ = 0, read_ = 1;
    std::atomic<unsigned> middle_{2};
};

struct WaitResult { bool wake = false, changed = false, rescan = false; std::uint32_t error = 0; };

#if defined(_WIN32)
constexpr std::uint32_t invalid_request_error = ERROR_INVALID_PARAMETER;
constexpr std::uint32_t allocation_error = ERROR_NOT_ENOUGH_MEMORY;
LogSourceState error_state(std::uint32_t error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return LogSourceState::Missing;
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) return LogSourceState::Denied;
    return LogSourceState::Failed;
}

class NativeFile {
public:
    explicit NativeFile(bool notifications) : notifications_(notifications) {
        path_.reserve(kPathBytes); parent_.reserve(kPathBytes);
        wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!wake_) throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!event_) {
            const auto error = GetLastError(); CloseHandle(wake_);
            throw std::system_error(static_cast<int>(error), std::system_category());
        }
    }
    ~NativeFile() { close_source(); CloseHandle(event_); CloseHandle(wake_); }
    void wake() noexcept { SetEvent(wake_); }
    LogWatchMode mode() const noexcept { return watching_ ? LogWatchMode::Native : LogWatchMode::Polling; }
    void close_source() noexcept {
        close_watch();
        if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; }
        path_.clear(); parent_.clear();
    }
    void inspect(const Request& request, LogFileResult& result) {
        const std::string_view path(request.path.data(), request.path_size);
        const bool changed_path = path != path_;
        if (changed_path) { close_source(); path_.assign(path); }
        ensure_watch(); // Parent observation is registered before opening.
        if (file_ == INVALID_HANDLE_VALUE || request.follow == LogFollowMode::Path) {
            const auto wide = windows_utf16(path_);
            if (!wide) { fail(result, ERROR_NO_UNICODE_TRANSLATION); return; }
            HANDLE candidate = CreateFileW(wide->c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (candidate == INVALID_HANDLE_VALUE) {
                const auto error = GetLastError();
                if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; }
                fail(result, error); return;
            }
            const auto observed = observe(candidate, result);
            if (!observed) {
                CloseHandle(candidate);
                if (file_ != INVALID_HANDLE_VALUE) { CloseHandle(file_); file_ = INVALID_HANDLE_VALUE; }
                return;
            }
            if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
            file_ = candidate;
            result.file = observed;
        } else result.file = observe(file_, result);
        if (result.file) result.state = LogSourceState::Following;
        result.watch = mode();
    }
    void read(const Request& request, LogFileResult& result) noexcept {
        if (file_ == INVALID_HANDLE_VALUE) { fail(result, ERROR_INVALID_HANDLE); return; }
        result.before_read = observe(file_, result);
        if (!result.before_read) return;
        if (request.offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max())) {
            fail(result, ERROR_ARITHMETIC_OVERFLOW); return;
        }
        LARGE_INTEGER offset; offset.QuadPart = static_cast<LONGLONG>(request.offset);
        if (!SetFilePointerEx(file_, offset, nullptr, FILE_BEGIN)) { fail(result, GetLastError()); return; }
        DWORD count = 0;
        if (!ReadFile(file_, result.writable_bytes().data(), static_cast<DWORD>(request.bytes), &count, nullptr)) {
            const auto error = GetLastError();
            if (error != ERROR_HANDLE_EOF) { fail(result, error); return; }
        }
        result.set_byte_count(count);
        result.file = observe(file_, result);
        if (result.file) result.state = LogSourceState::Following;
        result.watch = mode();
    }
    WaitResult wait() noexcept {
        std::array<HANDLE, 2> handles{wake_, event_};
        const auto status = WaitForMultipleObjects(watching_ ? 2U : 1U, handles.data(), FALSE, INFINITE);
        if (status == WAIT_OBJECT_0) return {true, false, false, 0};
        if (status != WAIT_OBJECT_0 + 1) {
            const auto error = GetLastError();
            return {false, false, false, error == 0 ? ERROR_GEN_FAILURE : error};
        }
        DWORD count = 0;
        const bool completed = GetOverlappedResult(directory_, &overlapped_, &count, FALSE) != FALSE;
        outstanding_ = false;
        const bool rescan = !completed || count == 0;
        if (!arm_watch()) close_watch();
        return {false, true, rescan, 0};
    }
private:
    void fail(LogFileResult& result, DWORD error) const noexcept {
        result.native_error = error; result.state = error_state(error); result.watch = mode();
    }
    static std::optional<LogFileObservation> observe(HANDLE file, LogFileResult& result) noexcept {
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file, &info)) {
            result.native_error = GetLastError(); result.state = error_state(result.native_error); return {};
        }
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || GetFileType(file) != FILE_TYPE_DISK) {
            result.native_error = ERROR_INVALID_DATA; result.state = LogSourceState::Failed; return {};
        }
        LogFileObservation observation;
        observation.identity = {info.dwVolumeSerialNumber,
            (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow, 0};
        FILE_ID_INFO id{};
        if (GetFileInformationByHandleEx(file, FileIdInfo, &id, static_cast<DWORD>(sizeof(id)))) {
            observation.identity.volume = id.VolumeSerialNumber;
            std::memcpy(&observation.identity.object, id.FileId.Identifier, 8);
            std::memcpy(&observation.identity.object_high, id.FileId.Identifier + 8, 8);
        }
        observation.size = (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        observation.version = (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) |
            info.ftLastWriteTime.dwLowDateTime;
        return observation;
    }
    bool arm_watch() noexcept {
        overlapped_ = {}; overlapped_.hEvent = event_; ResetEvent(event_);
        const bool armed = ReadDirectoryChangesW(directory_, events_.data(), static_cast<DWORD>(events_.size()),
            FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES, nullptr, &overlapped_, nullptr) != FALSE;
        outstanding_ = armed; watching_ = armed; return armed;
    }
    void close_watch() noexcept {
        if (directory_ != INVALID_HANDLE_VALUE) {
            if (outstanding_) {
                CancelIoEx(directory_, &overlapped_);
                DWORD ignored = 0; GetOverlappedResult(directory_, &overlapped_, &ignored, TRUE);
            }
            CloseHandle(directory_); directory_ = INVALID_HANDLE_VALUE;
        }
        outstanding_ = false; watching_ = false;
    }
    void ensure_watch() {
        if (!notifications_ || watching_) return;
        const auto slash = path_.find_last_of("/\\");
        if (slash == std::string::npos) return;
        parent_.assign(path_.data(), slash == 0 ? 1 : slash);
        if (parent_.size() == 2 && parent_[1] == ':') parent_.push_back('\\');
        const auto wide = windows_utf16(parent_);
        if (!wide) return;
        directory_ = CreateFileW(wide->c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (directory_ != INVALID_HANDLE_VALUE && !arm_watch()) close_watch();
    }
    bool notifications_, watching_ = false, outstanding_ = false;
    HANDLE wake_ = nullptr, event_ = nullptr, file_ = INVALID_HANDLE_VALUE, directory_ = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped_{};
    alignas(DWORD) std::array<char, 4096> events_{};
    std::string path_, parent_;
};
#else
constexpr std::uint32_t invalid_request_error = EINVAL;
constexpr std::uint32_t allocation_error = ENOMEM;
LogSourceState error_state(std::uint32_t error) noexcept {
    if (error == ENOENT || error == ENOTDIR) return LogSourceState::Missing;
    if (error == EACCES || error == EPERM) return LogSourceState::Denied;
    return LogSourceState::Failed;
}
void close_fd(int& fd) noexcept { if (fd >= 0) { ::close(fd); fd = -1; } }

class NativeFile {
public:
    explicit NativeFile(bool notifications) : notifications_(notifications) {
        path_.reserve(kPathBytes); parent_.reserve(kPathBytes);
        std::array<int, 2> pipes{};
        if (::pipe(pipes.data()) != 0) throw std::system_error(errno, std::generic_category());
        wake_read_ = pipes[0]; wake_write_ = pipes[1];
        for (const int fd : pipes) {
            if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 || ::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
                const int error = errno; close_fd(wake_read_); close_fd(wake_write_);
                throw std::system_error(error, std::generic_category());
            }
        }
        if (notifications_) {
#if defined(__APPLE__)
            watch_ = ::kqueue();
            if (watch_ >= 0 && ::fcntl(watch_, F_SETFD, FD_CLOEXEC) < 0) close_fd(watch_);
#else
            watch_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#endif
        }
    }
    ~NativeFile() { close_source(); close_fd(watch_); close_fd(wake_read_); close_fd(wake_write_); }
    void wake() noexcept {
        const char byte = 1;
        // A single coalesced wake cannot fill the pipe. Retry interrupted
        // kernel transfers; dropping the only wake would strand a request.
        ssize_t count;
        do { count = ::write(wake_write_, &byte, 1); } while (count < 0 && errno == EINTR);
    }
    LogWatchMode mode() const noexcept {
#if defined(__APPLE__)
        return watch_ >= 0 && directory_ >= 0 && directory_watched_ && (file_ < 0 || file_watched_)
            ? LogWatchMode::Native : LogWatchMode::Polling;
#else
        return watch_ >= 0 && directory_watch_ >= 0 && (file_ < 0 || file_watch_ >= 0)
            ? LogWatchMode::Native : LogWatchMode::Polling;
#endif
    }
    void close_source() noexcept {
#if defined(__APPLE__)
        close_fd(directory_); directory_watched_ = false;
#else
        remove_watch(file_watch_); remove_watch(directory_watch_);
#endif
        close_file(); path_.clear(); parent_.clear();
    }
    void inspect(const Request& request, LogFileResult& result) {
        const std::string_view path(request.path.data(), request.path_size);
        if (path != path_) { close_source(); path_.assign(path); }
        ensure_directory_watch();
        if (file_ < 0 || request.follow == LogFollowMode::Path) {
            int candidate = ::open(path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (candidate < 0) {
                const int error = errno; close_file(); fail(result, error); return;
            }
            const auto observed = observe(candidate, result);
            if (!observed) { close_fd(candidate); close_file(); return; }
            bool same = false;
            if (file_ >= 0) {
                int ignored = 0;
                const auto current = observe_raw(file_, ignored);
                same = current && current->identity == observed->identity;
            }
            if (same) close_fd(candidate);
            else { close_file(); file_ = candidate; }
            result.file = observed;
        } else result.file = observe(file_, result);
        ensure_file_watch();
        if (result.file) result.state = LogSourceState::Following;
        result.watch = mode();
    }
    void read(const Request& request, LogFileResult& result) noexcept {
        if (file_ < 0) { fail(result, EBADF); return; }
        result.before_read = observe(file_, result);
        if (!result.before_read) return;
        if (request.offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
            fail(result, EOVERFLOW); return;
        }
        ssize_t count = -1;
        for (unsigned attempt = 0; attempt < 4; ++attempt) {
            count = ::pread(file_, result.writable_bytes().data(), request.bytes, static_cast<off_t>(request.offset));
            if (count >= 0 || errno != EINTR) break;
        }
        if (count < 0) { fail(result, errno); return; }
        result.set_byte_count(static_cast<std::size_t>(count));
        result.file = observe(file_, result);
        if (result.file) result.state = LogSourceState::Following;
        result.watch = mode();
    }
    WaitResult wait() noexcept {
        std::array<pollfd, 2> descriptors{{{wake_read_, POLLIN, 0}, {watch_, POLLIN, 0}}};
        const int count = ::poll(descriptors.data(), descriptors.size(), -1);
        if (count < 0) return errno == EINTR ? WaitResult{} : WaitResult{false, false, false, static_cast<std::uint32_t>(errno)};
        WaitResult result;
        if ((descriptors[0].revents & POLLIN) != 0) {
            std::array<char, 64> bytes{};
            while (::read(wake_read_, bytes.data(), bytes.size()) > 0) {}
            result.wake = true;
        }
        if ((descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) result.error = EIO;
        if ((descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            close_fd(watch_); result.changed = true; result.rescan = true;
        } else if ((descriptors[1].revents & POLLIN) != 0) collect_changes(result);
        return result;
    }
private:
    void fail(LogFileResult& result, int error) const noexcept {
        result.native_error = static_cast<std::uint32_t>(error);
        result.state = error_state(result.native_error); result.watch = mode();
    }
    static std::optional<LogFileObservation> observe(int file, LogFileResult& result) noexcept {
        int error = 0;
        const auto observed = observe_raw(file, error);
        if (!observed) {
            result.native_error = static_cast<std::uint32_t>(error);
            result.state = error_state(result.native_error);
        }
        return observed;
    }
    static std::optional<LogFileObservation> observe_raw(int file, int& error) noexcept {
        struct stat info{};
        if (::fstat(file, &info) != 0) { error = errno; return {}; }
        if (!S_ISREG(info.st_mode) || info.st_size < 0) {
            error = EINVAL; return {};
        }
        LogFileObservation observation;
        observation.identity = {static_cast<std::uint64_t>(info.st_dev), static_cast<std::uint64_t>(info.st_ino), 0};
        observation.size = static_cast<std::uint64_t>(info.st_size);
#if defined(__APPLE__)
        const auto modified = info.st_mtimespec, changed = info.st_ctimespec;
#else
        const auto modified = info.st_mtim, changed = info.st_ctim;
#endif
        observation.version = static_cast<std::uint64_t>(modified.tv_sec) ^
            (static_cast<std::uint64_t>(modified.tv_nsec) << 32) ^
            (static_cast<std::uint64_t>(changed.tv_sec) << 17) ^ static_cast<std::uint64_t>(changed.tv_nsec);
        return observation;
    }
    void close_file() noexcept {
#if defined(__APPLE__)
        file_watched_ = false;
#else
        remove_watch(file_watch_);
#endif
        close_fd(file_);
    }
    void ensure_directory_watch() {
        if (watch_ < 0) return;
        const auto slash = path_.find_last_of('/');
        if (slash == std::string::npos) return;
        parent_.assign(path_.data(), slash == 0 ? 1 : slash);
#if defined(__APPLE__)
        if (directory_ < 0) directory_ = ::open(parent_.c_str(), O_EVTONLY | O_CLOEXEC);
        if (directory_ >= 0 && !directory_watched_) directory_watched_ = add_vnode(directory_);
#else
        if (directory_watch_ < 0) directory_watch_ = ::inotify_add_watch(watch_, parent_.c_str(),
            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY | IN_ATTRIB |
            IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR);
#endif
    }
    void ensure_file_watch() noexcept {
        if (watch_ < 0 || file_ < 0) return;
#if defined(__APPLE__)
        if (!file_watched_) file_watched_ = add_vnode(file_);
#else
        if (file_watch_ < 0) file_watch_ = ::inotify_add_watch(watch_, path_.c_str(),
            IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF);
#endif
    }
#if defined(__APPLE__)
    bool add_vnode(int fd) noexcept {
        struct kevent event{};
        EV_SET(&event, static_cast<uintptr_t>(fd), EVFILT_VNODE, EV_ADD | EV_CLEAR,
            NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_DELETE | NOTE_REVOKE, 0, nullptr);
        return ::kevent(watch_, &event, 1, nullptr, 0, nullptr) == 0;
    }
    void collect_changes(WaitResult& result) noexcept {
        std::array<struct kevent, 16> events{};
        const timespec immediate{};
        const int count = ::kevent(watch_, nullptr, 0, events.data(), static_cast<int>(events.size()), &immediate);
        if (count < 0) { result.changed = true; result.rescan = true; return; }
        for (int i = 0; i < count; ++i) {
            const auto& event = events[static_cast<std::size_t>(i)];
            result.changed = true;
            if ((event.flags & EV_ERROR) != 0 || (event.fflags & NOTE_REVOKE) != 0) result.rescan = true;
            if (event.ident == static_cast<uintptr_t>(directory_) &&
                (event.fflags & (NOTE_DELETE | NOTE_RENAME | NOTE_REVOKE)) != 0) {
                close_fd(directory_); directory_watched_ = false; result.rescan = true;
            }
        }
    }
#else
    void remove_watch(int& descriptor) noexcept {
        if (descriptor >= 0 && watch_ >= 0) ::inotify_rm_watch(watch_, descriptor);
        descriptor = -1;
    }
    void collect_changes(WaitResult& result) noexcept {
        alignas(inotify_event) std::array<char, 4096> bytes{};
        const auto count = ::read(watch_, bytes.data(), bytes.size());
        if (count < 0) {
            if (errno != EAGAIN && errno != EINTR) { result.changed = true; result.rescan = true; }
            return;
        }
        if (count == 0) { close_fd(watch_); result.changed = true; result.rescan = true; return; }
        std::size_t position = 0;
        const auto size = static_cast<std::size_t>(count);
        while (size - position >= sizeof(inotify_event)) {
            const auto* event = reinterpret_cast<const inotify_event*>(bytes.data() + position);
            if (event->len > size - position - sizeof(inotify_event)) { result.rescan = true; break; }
            if ((event->mask & IN_Q_OVERFLOW) != 0) { result.changed = true; result.rescan = true; }
            if (event->wd == file_watch_ || event->wd == directory_watch_) result.changed = true;
            const bool relevant = event->wd == file_watch_ || event->wd == directory_watch_;
            if (relevant && (event->mask & (IN_IGNORED | IN_UNMOUNT | IN_DELETE_SELF)) != 0) {
                if (event->wd == file_watch_) file_watch_ = -1;
                if (event->wd == directory_watch_) directory_watch_ = -1;
                result.rescan = true;
            }
            if (event->wd == directory_watch_ && (event->mask & IN_MOVE_SELF) != 0) {
                remove_watch(directory_watch_); result.rescan = true;
            }
            position += sizeof(inotify_event) + event->len;
        }
    }
#endif
    bool notifications_;
    int wake_read_ = -1, wake_write_ = -1, watch_ = -1, file_ = -1;
#if defined(__APPLE__)
    int directory_ = -1;
    bool directory_watched_ = false, file_watched_ = false;
#else
    int directory_watch_ = -1, file_watch_ = -1;
#endif
    std::string path_, parent_;
};
#endif

struct Slot {
    Slot(bool notifications, std::shared_ptr<const LogWake> wake) : native(notifications), wake_target(std::move(wake)) {}
    NativeFile native;
    const std::shared_ptr<const LogWake> wake_target;
    Requests requests;
    LogFileResult result;
    std::atomic<SlotState> state{SlotState::Available};
    std::atomic<Publication> publication{Publication::Empty};
    std::atomic<std::uint64_t> requested_revision{0};
    std::atomic<bool> stop{false}, wake_pending{false}, changed{false}, rescan{false};
    std::atomic<std::uint32_t> fatal_error{0};
    std::thread worker;
    void signal() noexcept { if (!wake_pending.exchange(true)) native.wake(); }
    void notify() const noexcept { if (wake_target) wake_target->notify(); }
};

void run(Slot& slot) noexcept {
    while (!slot.stop.load()) {
        if (slot.state.load() == SlotState::Retiring) {
            slot.native.close_source();
            if (slot.requests.pending()) slot.requests.read();
            slot.publication.store(Publication::Empty);
            slot.requested_revision.store(0);
            slot.changed.store(false); slot.rescan.store(false);
            slot.state.store(SlotState::Available);
        }
        if (slot.state.load() == SlotState::Active && slot.requests.pending()) {
            auto phase = slot.publication.load();
            if ((phase == Publication::Empty || phase == Publication::Ready) &&
                slot.publication.compare_exchange_strong(phase, Publication::Writing)) {
                const auto& request = slot.requests.read();
                auto& result = slot.result;
                result.reset(); result.revision = request.revision;
                result.operation = request.operation; result.offset = request.offset;
                try {
                    if (!request.valid) {
                        result.state = LogSourceState::Failed; result.native_error = invalid_request_error;
                    } else if (request.operation == LogFileOperation::Inspect) {
                        result.rescan_required = slot.rescan.exchange(false);
                        slot.changed.store(false);
                        slot.native.inspect(request, result);
                    } else slot.native.read(request, result);
                } catch (...) {
                    result.state = LogSourceState::Failed; result.native_error = allocation_error;
                }
                const bool current = slot.state.load() == SlotState::Active &&
                    slot.requested_revision.load() == result.revision;
                slot.publication.store(current ? Publication::Ready : Publication::Empty);
                if (current) slot.notify();
                // Requests arriving during I/O must be handled before sleeping.
                if (slot.requests.pending()) continue;
            }
        }
        if (slot.stop.load() || slot.state.load() == SlotState::Retiring) continue;
        const auto waited = slot.native.wait();
        if (waited.wake) slot.wake_pending.store(false);
        bool notify = false;
        if (waited.changed && slot.state.load() == SlotState::Active) notify = !slot.changed.exchange(true);
        if (waited.rescan && slot.state.load() == SlotState::Active) notify = !slot.rescan.exchange(true) || notify;
        if (notify) slot.notify();
        if (waited.error != 0) {
            slot.fatal_error.store(waited.error); slot.state.store(SlotState::Dead); slot.notify(); break;
        }
    }
    slot.native.close_source();
}

class Session final : public LogFileSession {
public:
    Session(Slot& slot, std::size_t bytes) noexcept : slot_(slot), bytes_(bytes) {}
    ~Session() override { cancel(); }
    void request(const LogFileRequest& request) override {
        if (retired_ || slot_.state.load() != SlotState::Active || request.revision <= revision_) return;
        revision_ = request.revision;
        auto& data = slot_.requests.writable();
        data.revision = revision_; data.operation = request.operation;
        data.follow = request.follow; data.offset = request.offset; data.bytes = request.bytes;
        data.valid = request.valid() &&
            (request.operation == LogFileOperation::Inspect || request.bytes <= bytes_);
        data.path_size = data.valid ? request.path.size() : 0;
        if (data.path_size != 0) std::memcpy(data.path.data(), request.path.data(), data.path_size);
        data.path[data.path_size] = '\0';
        slot_.requested_revision.store(revision_);
        slot_.requests.publish(); slot_.signal();
    }
    bool take(LogFileResult& result) noexcept override {
        if (retired_) return false;
        auto ready = Publication::Ready;
        if (slot_.publication.compare_exchange_strong(ready, Publication::Reading)) {
            const bool current = slot_.result.revision == revision_;
            if (current) result.copy_from(slot_.result);
            slot_.publication.store(Publication::Empty);
            if (slot_.requests.pending()) slot_.signal();
            return current;
        }
        const auto fatal = slot_.fatal_error.load();
        if (fatal != 0 && !fatal_delivered_) {
            fatal_delivered_ = true; result.reset(); result.revision = revision_;
            result.state = LogSourceState::Failed; result.native_error = fatal;
            return true;
        }
        return false;
    }
    bool changes_pending() const noexcept override {
        return !retired_ && (slot_.changed.load() || slot_.rescan.load() || slot_.fatal_error.load() != 0);
    }
    bool has_wake() const noexcept override { return !retired_ && static_cast<bool>(slot_.wake_target); }
    std::size_t read_limit() const noexcept override { return bytes_; }
    void cancel() noexcept override {
        if (retired_) return;
        retired_ = true;
        if (slot_.state.load() != SlotState::Dead) slot_.state.store(SlotState::Retiring);
        slot_.signal();
    }
private:
    Slot& slot_;
    std::size_t bytes_;
    std::uint64_t revision_ = 0;
    bool retired_ = false, fatal_delivered_ = false;
};
}

struct NativeLogFileService::Impl {
    Impl(std::size_t count, std::size_t bytes, bool notifications, const std::shared_ptr<const LogWake>& wake) : bytes_(bytes) {
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) slots.push_back(std::make_unique<Slot>(notifications, wake));
        try {
            for (auto& slot : slots) slot->worker = std::thread([ptr = slot.get()] { run(*ptr); });
        } catch (...) { stop(); throw; }
    }
    ~Impl() { stop(); }
    void stop() noexcept {
        for (auto& slot : slots) { slot->stop.store(true); slot->signal(); }
        for (auto& slot : slots) if (slot->worker.joinable()) slot->worker.join();
    }
    std::size_t bytes_;
    std::vector<std::unique_ptr<Slot>> slots;
};

NativeLogFileService::NativeLogFileService(std::size_t slots, std::size_t read_bytes, LogWatchPreference watch,
                                         std::shared_ptr<const LogWake> wake) {
    if (slots == 0 || slots > 8 || read_bytes == 0 || read_bytes > kLogBatchBytes ||
        (watch != LogWatchPreference::Native && watch != LogWatchPreference::Polling))
        throw std::invalid_argument("Invalid native log file service limits");
    impl_ = std::make_unique<Impl>(slots, read_bytes, watch == LogWatchPreference::Native, wake);
}
NativeLogFileService::~NativeLogFileService() {
    for (const auto& slot : impl_->slots) CKV_ASSERT(slot->state.load() != SlotState::Active);
}
std::unique_ptr<LogFileSession> NativeLogFileService::open_session() {
    for (auto& slot : impl_->slots) {
        auto available = SlotState::Available;
        if (slot->state.compare_exchange_strong(available, SlotState::Active)) {
            try { return std::make_unique<Session>(*slot, impl_->bytes_); }
            catch (...) { slot->state.store(SlotState::Retiring); slot->signal(); throw; }
        }
    }
    return {};
}

void TerminalLogWake::notify() const noexcept { terminal_.wake(); }

}  // namespace ckv::term
