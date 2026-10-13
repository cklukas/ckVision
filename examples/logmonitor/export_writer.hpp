// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include "cvision/core/filesystem.hpp"
#include "cvision/core/log_source.hpp"
namespace ckv::logmonitor {
struct ExportJob { std::uint64_t revision = 0; std::string path, contents, scope; };
struct ExportResult { std::uint64_t revision = 0; FileWriteStatus state = FileWriteStatus::Error; std::string path, scope; };
class ExportWriter {
public:
    virtual ~ExportWriter() = default;
    // Success moves the job; Busy leaves it intact. One bounded publication.
    virtual bool submit(ExportJob& job) = 0;
    virtual bool take(ExportResult& result) = 0;
    virtual bool has_wake() const noexcept { return false; }
};
class NativeExportWriter final : public ExportWriter {
public:
    explicit NativeExportWriter(FileSystem& filesystem, std::shared_ptr<const LogWake> wake = {});
    ~NativeExportWriter() override;
    bool submit(ExportJob& job) override;
    bool take(ExportResult& result) override;
    bool has_wake() const noexcept override;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}  // namespace ckv::logmonitor
