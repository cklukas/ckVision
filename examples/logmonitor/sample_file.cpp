// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "sample_file.hpp"
#include <algorithm>
namespace ckv::logmonitor {
class SampleFile::Session final : public LogFileSession {
public:
    explicit Session(SampleFile& owner) : owner_(owner) {}
    ~Session() override { cancel(); }
    void request(const LogFileRequest& request) override {
        if (retired_) return;
        owner_.result_.reset(); auto& result = owner_.result_;
        if (request.operation == LogFileOperation::Inspect) { opened_ = owner_.current_; owner_.changed_ = false; }
        const auto& file = owner_.objects_[opened_];
        result.revision = request.revision; result.operation = request.operation;
        result.state = LogSourceState::Following; result.watch = LogWatchMode::Native;
        result.file = LogFileObservation{{1, file.identity, 0}, file.size, file.version};
        result.before_read = result.file; result.offset = request.offset;
        if (request.operation == LogFileOperation::Read && request.offset < file.size) {
            const auto count = std::min(request.bytes, file.size - static_cast<std::size_t>(request.offset));
            std::copy_n(file.bytes.data() + request.offset, count, result.writable_bytes().data()); result.set_byte_count(count);
        }
        owner_.ready_ = true; if (owner_.wake_) owner_.wake_->notify();
    }
    bool take(LogFileResult& result) noexcept override {
        if (retired_ || !owner_.ready_) return false;
        result.copy_from(owner_.result_); owner_.ready_ = false; return true;
    }
    bool changes_pending() const noexcept override { return !retired_ && owner_.changed_; }
    bool has_wake() const noexcept override { return true; }
    void cancel() noexcept override { if (!retired_) { retired_ = true; owner_.active_ = false; owner_.ready_ = false; } }
private:
    SampleFile& owner_; std::size_t opened_ = 0; bool retired_ = false;
};
std::unique_ptr<LogFileSession> SampleFile::open_session() {
    if (active_) return {};
    active_ = true; return std::make_unique<Session>(*this);
}
void SampleFile::append(std::string_view bytes) {
    auto& file = objects_[current_];
    if (bytes.size() > file.bytes.size() - file.size) truncate();
    auto& target = objects_[current_]; const auto count = std::min(bytes.size(), target.bytes.size() - target.size);
    std::copy_n(bytes.data(), count, target.bytes.data() + target.size); target.size += count; ++target.version;
    changed_ = true; if (wake_) wake_->notify();
}
void SampleFile::rotate() {
    current_ = 1 - current_; auto& file = objects_[current_];
    file.size = 0; file.identity = next_identity_++; ++file.version;
    changed_ = true; if (wake_) wake_->notify();
}
void SampleFile::truncate() { objects_[current_].size = 0; ++objects_[current_].version; changed_ = true; if (wake_) wake_->notify(); }
}  // namespace ckv::logmonitor
