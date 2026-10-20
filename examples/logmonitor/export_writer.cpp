// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "export_writer.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace ckv::logmonitor {
struct NativeExportWriter::Impl {
    FileSystem& fs;
    std::shared_ptr<const LogWake> wake;
    std::mutex mutex; std::condition_variable ready;
    ExportJob job; ExportResult result;
    bool stopping = false, pending = false, working = false, published = false;
    std::thread thread;
    Impl(FileSystem& filesystem, std::shared_ptr<const LogWake> target) : fs(filesystem), wake(std::move(target)), thread([this] { run(); }) {}
    ~Impl() { { std::lock_guard lock(mutex); stopping = true; } ready.notify_one(); thread.join(); }
    void run() {
        for (;;) {
            ExportJob current;
            { std::unique_lock lock(mutex); ready.wait(lock, [this] { return stopping || pending; });
              if (stopping && !pending) return;
              current = std::move(job); pending = false; working = true; }
            FileWriteStatus outcome = FileWriteStatus::Error;
            try { outcome = fs.write_file_atomic(current.path, current.contents, FileWriteExpectation::must_not_exist()).status; }
            catch (...) {}
            { std::lock_guard lock(mutex); result = {current.revision, outcome, std::move(current.path), std::move(current.scope)};
              published = true; working = false; }
            if (wake) wake->notify();
        }
    }
};
NativeExportWriter::NativeExportWriter(FileSystem& fs, std::shared_ptr<const LogWake> wake) : impl_(std::make_unique<Impl>(fs, std::move(wake))) {}
NativeExportWriter::~NativeExportWriter() = default;
bool NativeExportWriter::submit(ExportJob& job) {
    if (job.path.size() > 32768 || job.contents.size() > 2 * 1024 * 1024 || job.scope.size() > 4096 || !job.revision) return false;
    std::unique_lock lock(impl_->mutex, std::try_to_lock);
    if (!lock.owns_lock() || impl_->stopping || impl_->pending || impl_->working || impl_->published) return false;
    impl_->job = std::move(job); impl_->pending = true; lock.unlock(); impl_->ready.notify_one(); return true;
}
bool NativeExportWriter::take(ExportResult& result) {
    std::unique_lock lock(impl_->mutex, std::try_to_lock);
    if (!lock.owns_lock() || !impl_->published) return false;
    result = std::move(impl_->result); impl_->published = false; return true;
}
bool NativeExportWriter::has_wake() const noexcept { return impl_->wake != nullptr; }
}  // namespace ckv::logmonitor
