// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string_view>

#include "cvision/core/log_source.hpp"

namespace ckv::testing {

// A test controls delivery explicitly, including delayed revisions, missed
// notifications and source identity changes. No filesystem or clock is read.
class ScriptedLogFiles final : public LogFileService {
public:
    ScriptedLogFiles() { request_.path.reserve(32768); }
    std::unique_ptr<LogFileSession> open_session() override {
        if (active_) return {};
        active_ = true;
        return std::make_unique<Session>(*this);
    }
    const LogFileRequest& requested() const noexcept { return request_; }
    std::size_t request_count() const noexcept { return requests_; }
    LogFileResult& publication() noexcept { return result_; }
    void hint(bool lost = false) noexcept { changed_ = true; lost_ = lost_ || lost; }
    void publish(std::uint64_t revision, std::optional<LogFileObservation> file,
                 std::string_view bytes = {}, LogSourceState state = LogSourceState::Following) noexcept {
        result_.reset(); result_.revision = revision; result_.operation = request_.operation;
        result_.offset = request_.offset; result_.file = file; result_.before_read = file;
        result_.state = state; result_.watch = LogWatchMode::Native;
        result_.rescan_required = lost_; lost_ = false;
        const auto count = std::min(bytes.size(), kLogBatchBytes);
        if (count != 0) std::memcpy(result_.writable_bytes().data(), bytes.data(), count);
        result_.set_byte_count(count); ready_ = true;
    }
private:
    class Session final : public LogFileSession {
    public:
        explicit Session(ScriptedLogFiles& files) noexcept : files_(files) {}
        ~Session() override { cancel(); }
        void request(const LogFileRequest& request) override {
            if (retired_) return;
            files_.request_ = request; ++files_.requests_;
            if (files_.request_.operation == LogFileOperation::Inspect) files_.changed_ = false;
        }
        bool take(LogFileResult& result) noexcept override {
            if (retired_ || !files_.ready_) return false;
            result.copy_from(files_.result_); files_.ready_ = false; return true;
        }
        bool changes_pending() const noexcept override { return !retired_ && (files_.changed_ || files_.lost_); }
        void cancel() noexcept override {
            if (retired_) return;
            retired_ = true; files_.active_ = false; files_.ready_ = false;
            files_.changed_ = false; files_.lost_ = false;
        }
    private:
        ScriptedLogFiles& files_;
        bool retired_ = false;
    };
    LogFileRequest request_;
    LogFileResult result_;
    std::size_t requests_ = 0;
    bool active_ = false, ready_ = false, changed_ = false, lost_ = false;
};

}  // namespace ckv::testing
