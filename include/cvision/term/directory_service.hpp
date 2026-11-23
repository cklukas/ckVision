// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "cvision/core/export.hpp"

#include <cstddef>
#include <memory>
#include <string>

#include "cvision/core/directory_service.hpp"

namespace ckv::term {

// Explicit worker/index owner. No global pool or implicit temporary directory.
// One worker and disk index per configured slot (1..8). The filesystem must
// permit read operations from these workers, without concurrent mutation.
// Session methods never wait for workers. Shutdown joins workers and therefore
// belongs at the host boundary after UI dispatch, never in a view destructor.
class CVISION_TYPE_API NativeDirectoryService final : public DirectoryService {
public:
    // index_directory must be an exclusive empty directory, supplied by the
    // host. Creates it when absent. Throws on invalid limits/ownership/storage;
    // runtime storage/discovery failures instead produce Failed snapshots.
    CVISION_MEMBER_API NativeDirectoryService(std::shared_ptr<const FileSystem> filesystem,
                           std::string index_directory, std::size_t slots = 2);
    // Cancels and joins all workers. Sessions must already have been destroyed.
    CVISION_MEMBER_API ~NativeDirectoryService() override;
    CVISION_MEMBER_API NativeDirectoryService(const NativeDirectoryService&) = delete;
    CVISION_MEMBER_API NativeDirectoryService& operator=(const NativeDirectoryService&) = delete;
    CVISION_MEMBER_API std::unique_ptr<DirectorySession> open_session() override;
    CVISION_MEMBER_API const FileSystem& filesystem() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ckv::term
