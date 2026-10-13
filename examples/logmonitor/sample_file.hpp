// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "cvision/core/log_source.hpp"
namespace ckv::logmonitor {
// Deterministic range/identity adapter. Exercises the shipped follower without
// writing a generated file or guessing a host log pathname.
class SampleFile final : public LogFileService {
public:
    explicit SampleFile(std::shared_ptr<const LogWake> wake) : wake_(std::move(wake)) {}
    std::unique_ptr<LogFileSession> open_session() override;
    void append(std::string_view bytes);
    void rotate();
    void truncate();
private:
    struct Object { std::array<char, 8192> bytes{}; std::size_t size = 0; std::uint64_t identity = 1, version = 1; };
    class Session;
    std::array<Object, 2> objects_;
    std::size_t current_ = 0;
    std::uint64_t next_identity_ = 2;
    LogFileResult result_;
    bool active_ = false, changed_ = false, ready_ = false;
    std::shared_ptr<const LogWake> wake_;
};
}  // namespace ckv::logmonitor
