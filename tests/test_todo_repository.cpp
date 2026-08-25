// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "memory_todo_repository.hpp"

#include <utility>

#include "cvision/testing/cktest.hpp"

namespace {

using namespace ckv::todo;

AuditStamp repository_stamp() { return {IsoTimestamp{"2026-08-25T12:00:00Z"}, "repository-test"}; }

TodoWorkspace with_task(std::string title = "Persist me") {
    TodoWorkspace workspace = TodoWorkspace::empty();
    TaskDraft task;
    task.title = std::move(title);
    const auto added = workspace.add_task(LaneId{1}, std::move(task), repository_stamp());
    if (!added) return TodoWorkspace::empty();
    return workspace;
}

ArchivedTask archived_record() {
    TodoWorkspace workspace = with_task();
    return workspace.prepare_task_archive(TaskId{1}, repository_stamp()).value->records.front();
}

}  // namespace

CK_TEST(todo_memory_repository_reports_missing_and_empty_revision) {
    MemoryTodoRepository repository;
    const auto loaded = repository.load();
    CK_CHECK(!loaded && loaded.error.code == RepositoryErrorCode::Missing);
    const auto revision = repository.revision();
    CK_CHECK(revision && !revision.value->exists());
}

CK_TEST(todo_memory_repository_creates_reopens_and_keeps_noop_revision) {
    MemoryTodoRepository repository;
    const TodoWorkspace workspace = with_task();
    const auto committed = repository.commit({}, workspace, IsoDate{"2026-08-25"});
    CK_CHECK(committed && committed.value->changed);
    CK_CHECK(committed.value->revision.exists());
    const auto loaded = repository.load();
    CK_CHECK(loaded && loaded.value->workspace.snapshot() == workspace.snapshot());
    CK_CHECK(loaded.value->revision == committed.value->revision);

    const auto no_op = repository.commit(loaded.value->revision, workspace, IsoDate{"2026-08-25"});
    CK_CHECK(no_op && !no_op.value->changed);
    CK_CHECK(no_op.value->revision == committed.value->revision);
    CK_CHECK(repository.backups().empty());
}

CK_TEST(todo_memory_repository_rejects_stale_revision_without_side_effects) {
    MemoryTodoRepository repository(TodoWorkspace::empty());
    const auto before = repository.load();
    const auto rejected = repository.commit(RepositoryRevision{"stale"}, with_task(), IsoDate{"2026-08-25"});
    CK_CHECK(!rejected && rejected.error.code == RepositoryErrorCode::Conflict);
    const auto after = repository.load();
    CK_CHECK(after.value->workspace.snapshot() == before.value->workspace.snapshot());
    CK_CHECK(after.value->revision == before.value->revision);
    CK_CHECK(repository.backups().empty());
}

CK_TEST(todo_memory_repository_writes_previous_workspace_once_per_date) {
    MemoryTodoRepository repository(TodoWorkspace::empty());
    auto loaded = repository.load();
    const WorkspaceSnapshot original = loaded.value->workspace.snapshot();
    auto committed = repository.commit(loaded.value->revision, with_task("first"), IsoDate{"2026-08-25"});
    CK_CHECK(committed && repository.backups().size() == 1);
    CK_CHECK(repository.backups().at("2026-08-25") == original);

    loaded = repository.load();
    const WorkspaceSnapshot first = loaded.value->workspace.snapshot();
    committed = repository.commit(loaded.value->revision, with_task("second"), IsoDate{"2026-08-25"});
    CK_CHECK(committed && repository.backups().size() == 1);
    CK_CHECK(repository.backups().at("2026-08-25") == original);

    loaded = repository.load();
    const WorkspaceSnapshot second = loaded.value->workspace.snapshot();
    committed = repository.commit(loaded.value->revision, with_task("third"), IsoDate{"2026-08-26"});
    CK_CHECK(committed && repository.backups().size() == 2);
    CK_CHECK(repository.backups().at("2026-08-26") == second);
}

CK_TEST(todo_memory_repository_failure_points_preserve_last_primary_workspace) {
    MemoryTodoRepository repository(TodoWorkspace::empty());
    const auto original = repository.load();
    repository.fail_next(MemoryRepositoryFailure::BackupWrite);
    auto result = repository.commit(original.value->revision, with_task(), IsoDate{"2026-08-25"});
    CK_CHECK(!result && result.error.code == RepositoryErrorCode::IoFailure);
    CK_CHECK(repository.load().value->workspace.snapshot() == original.value->workspace.snapshot());
    CK_CHECK(repository.backups().empty());

    repository.fail_next(MemoryRepositoryFailure::PrimaryWrite);
    result = repository.commit(original.value->revision, with_task(), IsoDate{"2026-08-25"});
    CK_CHECK(!result && result.error.code == RepositoryErrorCode::IoFailure);
    CK_CHECK(repository.load().value->workspace.snapshot() == original.value->workspace.snapshot());
    CK_CHECK(repository.backups().size() == 1);
}

CK_TEST(todo_memory_repository_load_and_revision_failures_are_one_shot) {
    MemoryTodoRepository repository(TodoWorkspace::empty());
    repository.fail_next(MemoryRepositoryFailure::Load);
    CK_CHECK(repository.load().error.code == RepositoryErrorCode::IoFailure);
    CK_CHECK(repository.load());
    repository.fail_next(MemoryRepositoryFailure::Revision);
    CK_CHECK(repository.revision().error.code == RepositoryErrorCode::IoFailure);
    CK_CHECK(repository.revision());
}

CK_TEST(todo_memory_repository_archive_is_idempotent_and_conflict_checked) {
    MemoryTodoRepository repository(TodoWorkspace::empty());
    const ArchivedTask record = archived_record();
    auto stored = repository.store_archive(record);
    CK_CHECK(stored && stored.value->changed);
    CK_CHECK(repository.archives().size() == 1);
    stored = repository.store_archive(record);
    CK_CHECK(stored && !stored.value->changed);

    ArchivedTask different = record;
    different.origin_lane_title = "Different";
    stored = repository.store_archive(different);
    CK_CHECK(!stored && stored.error.code == RepositoryErrorCode::ArchiveConflict);
    CK_CHECK(repository.archives().begin()->second == record);
}

CK_TEST(todo_memory_repository_archive_failure_does_not_publish_a_record) {
    MemoryTodoRepository repository;
    repository.fail_next(MemoryRepositoryFailure::ArchiveWrite);
    const auto stored = repository.store_archive(archived_record());
    CK_CHECK(!stored && stored.error.code == RepositoryErrorCode::IoFailure);
    CK_CHECK(repository.archives().empty());
}

CK_TEST(todo_memory_repository_rejects_archive_with_invalid_timestamp) {
    MemoryTodoRepository repository;
    ArchivedTask record = archived_record();
    record.archived.timestamp = IsoTimestamp{"not-a-timestamp"};
    const auto stored = repository.store_archive(record);
    CK_CHECK(!stored && stored.error.code == RepositoryErrorCode::InvalidData);
    CK_CHECK(repository.archives().empty());
}

CK_TEST(todo_memory_repository_rejects_invalid_mutation_date) {
    MemoryTodoRepository repository;
    const auto result = repository.commit({}, TodoWorkspace::empty(), IsoDate{"2026-02-30"});
    CK_CHECK(!result && result.error.code == RepositoryErrorCode::InvalidDate);
    CK_CHECK(!repository.revision().value->exists());
}
