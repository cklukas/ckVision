// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "json_todo_repository.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

#include <unistd.h>

#include "cvision/term/posix_filesystem.hpp"
#include "cvision/testing/cktest.hpp"

namespace {

using namespace ckv::todo;

struct ScratchDirectory {
    std::string path;

    ScratchDirectory() {
        char pattern[] = "/tmp/ckvision_todo_repository_XXXXXX";
        const char* created = ::mkdtemp(pattern);
        if (created != nullptr) path = created;
    }

    ~ScratchDirectory() {
        if (path.starts_with("/tmp/ckvision_todo_repository_")) {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    }
};

AuditStamp posix_stamp() { return {IsoTimestamp{"2026-08-25T12:00:00Z"}, "posix-test"}; }

TodoWorkspace posix_workspace() {
    TodoWorkspace workspace = TodoWorkspace::empty();
    TaskDraft task;
    task.title = "Persist on disk";
    task.note = "Archive me after reopen.";
    const auto added = workspace.add_task(LaneId{1}, std::move(task), posix_stamp());
    if (!added) return TodoWorkspace::empty();
    return workspace;
}

}  // namespace

CK_TEST(todo_posix_repository_reopens_backup_and_archive_first_removal) {
    ScratchDirectory scratch;
    CK_CHECK(!scratch.path.empty());
    if (scratch.path.empty()) return;
    ckv::term::PosixFileSystem filesystem;
    const std::string data = scratch.path + "/data";
    JsonTodoRepository repository(filesystem, data);
    auto committed = repository.commit({}, TodoWorkspace::empty(), IsoDate{"2026-08-25"});
    CK_CHECK(committed);
    if (!committed) return;
    auto loaded = repository.load();
    CK_CHECK(loaded);
    if (!loaded) return;
    committed = repository.commit(loaded.value->revision, posix_workspace(), IsoDate{"2026-08-25"});
    CK_CHECK(committed);
    if (!committed) return;
    CK_CHECK(filesystem.exists(data + "/backup/2026-08-25.json"));

    JsonTodoRepository reopened(filesystem, data);
    loaded = reopened.load();
    CK_CHECK(loaded && loaded.value->workspace.find_task(TaskId{1}) != nullptr);
    if (!loaded) return;
    const auto plan = loaded.value->workspace.prepare_task_archive(TaskId{1}, posix_stamp());
    CK_CHECK(plan);
    if (!plan) return;
    CK_CHECK(reopened.store_archive(plan.value->records.front()));
    TodoWorkspace candidate = loaded.value->workspace;
    CK_CHECK(candidate.apply_archive(*plan.value));
    committed = reopened.commit(loaded.value->revision, candidate, IsoDate{"2026-08-25"});
    CK_CHECK(committed);
    if (!committed) return;
    CK_CHECK(filesystem.exists(data + "/archive/2026-08-25T12-00-00--task-1.json"));
    const auto final = reopened.load();
    CK_CHECK(final);
    if (final) CK_CHECK(final.value->workspace.find_task(TaskId{1}) == nullptr);
}

CK_TEST(todo_posix_repository_detects_external_atomic_replacement) {
    ScratchDirectory scratch;
    CK_CHECK(!scratch.path.empty());
    if (scratch.path.empty()) return;
    ckv::term::PosixFileSystem filesystem;
    JsonTodoRepository repository(filesystem, scratch.path + "/data");
    const auto created = repository.commit({}, TodoWorkspace::empty(), IsoDate{"2026-08-25"});
    CK_CHECK(created);
    if (!created) return;
    const auto loaded = repository.load();
    CK_CHECK(loaded);
    if (!loaded) return;
    const auto external = encode_workspace(posix_workspace());
    CK_CHECK(external);
    if (!external) return;
    CK_CHECK(filesystem.write_file_atomic(repository.workspace_path(), *external.value).status ==
             ckv::FileWriteStatus::Ok);
    const auto rejected =
        repository.commit(loaded.value->revision, TodoWorkspace::empty(), IsoDate{"2026-08-25"});
    CK_CHECK(!rejected && rejected.error.code == RepositoryErrorCode::Conflict);
    const auto final = repository.load();
    CK_CHECK(final);
    if (final) CK_CHECK(final.value->workspace.find_task(TaskId{1}) != nullptr);
}
