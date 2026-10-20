// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ckv {

// Nonzero app-assigned identity, independent of pathname or display position.
using LogSourceId = std::uint64_t;

// Instance-owned notification target. Implementations must remain usable until
// their service has stopped; notify is thread-safe, nonthrowing, and coalesces
// work rather than posting a callback per line. It never mutates a view.
class LogWake {
public:
    // Target retirement encloses users; notify coalesces work without view mutation.
    virtual ~LogWake() = default;
    virtual void notify() const noexcept = 0;
};

// IDs never depend on display position, filtering, or native timestamps.
struct LogRecordId {
    // Source, increasing continuity generation and monotonic admission sequence.
    // Value equality compares identity, never a display-row number.
    LogSourceId source = 0;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    friend bool operator==(const LogRecordId&, const LogRecordId&) = default;
};

// Observed level (Unknown is distinct), lifecycle/notification mode, overflow
// policy, path/open-object following and startup selection.
enum class LogSeverity : std::uint8_t {
    Unknown, Trace, Debug, Info, Notice, Warning, Error, Critical
};
enum class LogSourceState : std::uint8_t {
    Pending, Following, Missing, Denied, Failed, Ended, Cancelled
};
enum class LogWatchMode : std::uint8_t { Native, Polling, Unavailable };
enum class LogOverflowPolicy : std::uint8_t { Backpressure, DropOldest, DropNewest };
enum class LogFollowMode : std::uint8_t { Path, OpenFile };
enum class LogStartMode : std::uint8_t { RecentTail, Beginning, Offset };

// Hard field, metadata-byte and source-publication capacities.
inline constexpr std::size_t kLogMaxFields = 8;
inline constexpr std::size_t kLogMetadataBytes = 4096;
inline constexpr std::size_t kLogBatchBytes = 64 * 1024;

// These are product limits, not measured throughput claims. Fixed storage is
// allocated at model/session construction, outside dispatch; indexes and
// decoder buffers count separately in allocated_bytes().
struct LogLimits {
    // Retained slots/arena, raw record/batch/pending bounds, filter work and
    // positive redraw/reconcile periods. valid rejects impossible capacities.
    std::size_t records = 16384;
    std::size_t retained_bytes = 16 * 1024 * 1024;
    std::size_t record_bytes = 64 * 1024;
    std::size_t batch_bytes = kLogBatchBytes;
    std::size_t pending_batches = 4;
    std::size_t filter_records_per_step = 256;
    std::int64_t frame_interval_nanos = 33'333'334;
    std::int64_t reconcile_interval_nanos = 1'000'000'000;
    bool valid() const noexcept;
};

// Borrowed metadata pair; names must be nonempty and unique per record.
struct LogField {
    // Source-defined name/value bytes borrowed for the metadata call/view life.
    std::string_view name;
    std::string_view value;
};

// Borrowed for the call or until the next model mutation. A timestamp is
// source data, never the model's observation of wall time. Unknown is distinct
// from an observed severity or timestamp. Field names must be unique/nonempty.
struct LogMetadata {
    // Observed level, optional adapter-documented timestamp in ns and bounded
    // borrowed fields. valid checks uniqueness and mandatory metadata capacity.
    LogSeverity severity = LogSeverity::Unknown;
    std::optional<std::int64_t> timestamp_nanos;
    std::span<const LogField> fields;
    bool valid() const noexcept;
};

// Native opened-object identity, independent of pathname/content fingerprint.
struct LogFileIdentity {
    // Native volume/object words and exact identity comparison.
    std::uint64_t volume = 0;
    std::uint64_t object = 0;
    std::uint64_t object_high = 0;
    friend bool operator==(const LogFileIdentity&, const LogFileIdentity&) = default;
};

// Opened-object identity/size and opaque observed modification version.
struct LogFileObservation {
    // Observations do not promise the content remained unchanged between reads.
    LogFileIdentity identity;
    std::uint64_t size = 0;
    // Opaque observed modification token, not used as a wall clock.
    std::uint64_t version = 0;
};

// Inspect selects/reconciles a pathname; Read addresses its session's open object.
enum class LogFileOperation : std::uint8_t { Inspect, Read };

// One request at a time per session. A new revision supersedes old work.
// Inspect selects the pathname; Read addresses that session's opened file.
// All paths are explicit; there is no implicit cwd or default source.
struct LogFileRequest {
    // Nonzero supersession revision, operation/path/follow choice and bounded
    // read offset/length. valid checks structure without filesystem access.
    std::uint64_t revision = 0;
    LogFileOperation operation = LogFileOperation::Inspect;
    std::string path;
    LogFollowMode follow = LogFollowMode::Path;
    std::uint64_t offset = 0;
    std::size_t bytes = kLogBatchBytes;
    bool valid() const noexcept;
};

// Caller-owned fixed-capacity transfer. Construct once outside dispatch;
// take(result) copies one bounded publication, consumed exactly once. The
// byte span borrows this value until its next successful take/reset.
struct LogFileResult {
    // Echoed revision/operation, lifecycle/mode, post/pre-read observations,
    // read offset and reconciliation hint. Missing observations stay unknown.
    std::uint64_t revision = 0;
    LogFileOperation operation = LogFileOperation::Inspect;
    LogSourceState state = LogSourceState::Pending;
    LogWatchMode watch = LogWatchMode::Unavailable;
    std::optional<LogFileObservation> file;
    std::optional<LogFileObservation> before_read;
    std::uint64_t offset = 0;
    bool rescan_required = false;
    // The native error domain is identified by the adapter documentation.
    std::uint32_t native_error = 0;
    std::span<const char> bytes() const noexcept { return {data_.data(), size_}; }
    // Adapter-facing bounded storage; consumers use bytes().
    std::span<char> writable_bytes() noexcept { return data_; }
    // Publish a used prefix; invalid size preserves the previous byte count.
    bool set_byte_count(std::size_t size) noexcept {
        if (size > data_.size()) return false;
        size_ = size;
        return true;
    }
    // Transfer only the used payload; a small read never copies a full batch.
    void copy_from(const LogFileResult& other) noexcept {
        if (&other == this) return;
        revision = other.revision; operation = other.operation;
        state = other.state; watch = other.watch;
        file = other.file; before_read = other.before_read;
        offset = other.offset; rescan_required = other.rescan_required;
        native_error = other.native_error; size_ = other.size_;
        if (size_ != 0) std::memcpy(data_.data(), other.data_.data(), size_);
    }
    // Reset metadata without clearing/copying unused payload capacity.
    void reset() noexcept {
        revision = 0;
        operation = LogFileOperation::Inspect;
        state = LogSourceState::Pending;
        watch = LogWatchMode::Unavailable;
        file.reset(); before_read.reset();
        offset = 0; rescan_required = false; native_error = 0; size_ = 0;
    }
private:
    std::array<char, kLogBatchBytes> data_{};
    std::size_t size_ = 0;
};

// Calls are bounded/nonblocking, on the owning UI thread. Native service
// resources outlive sessions; cancel/destruction only retire publication.
// Service destruction joins execution outside dispatch. No callback is stored
// that could outlive the Application. Consumers poll/take under injected time
// or arrange an application-owned wake bridge at the composition boundary.
class LogFileSession {
public:
    // Retire this session without joining native execution.
    virtual ~LogFileSession() = default;
    // Supersede bounded work; native I/O runs through the enclosing service.
    virtual void request(const LogFileRequest& request) = 0;
    // Try-consume one publication; false leaves the transfer unchanged.
    virtual bool take(LogFileResult& result) noexcept = 0;
    // Observe a coalesced hint without reading source bytes.
    virtual bool changes_pending() const noexcept = 0;
    // A source with an explicit wake target does not need result-poll timers.
    virtual bool has_wake() const noexcept { return false; }
    // Maximum readable bytes per request; cancel retires work without waiting.
    virtual std::size_t read_limit() const noexcept { return kLogBatchBytes; }
    virtual void cancel() noexcept = 0;
};

// App-owned execution resources; all sessions retire before shutdown.
class LogFileService {
public:
    // Shutdown may join workers; destroy outside UI dispatch.
    virtual ~LogFileService() = default;
    // Admit a bounded session, or return null on exhausted service capacity.
    virtual std::unique_ptr<LogFileSession> open_session() = 0;
};

}  // namespace ckv
