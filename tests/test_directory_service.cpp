// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/directory_service.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <semaphore>
#include <thread>

#include "cvision/testing/cktest.hpp"
#include "scratch_directory.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/input_line.hpp"

namespace {
class GeneratedFileSystem final : public ckv::FileSystem {
public:
    explicit GeneratedFileSystem(std::uint64_t count) : count_(count) {}
    std::vector<ckv::FileEntry> list_directory(std::string_view) const override {
        ++whole_lists; return {};
    }
    bool exists(std::string_view path) const noexcept override { return path == "/large"; }
    bool is_directory(std::string_view path) const noexcept override { return path == "/large"; }
    std::optional<std::string> resolve_directory(std::string_view path) const override {
        if (std::this_thread::get_id() == owning_thread_) ++ui_io;
        if (path == "/large" || path == "/large/.") return "/large";
        return std::nullopt;
    }
    class Cursor final : public ckv::DirectoryCursor {
    public:
        explicit Cursor(const GeneratedFileSystem& fs) : fs_(fs) {}
        ckv::DirectoryRead next(ckv::FileEntry& entry) override {
            if (index_ == fs_.block_at.load()) {
                fs_.blocked.store(true);
                fs_.release.acquire();
            }
            if (index_ == fs_.count_) return ckv::DirectoryRead::End;
            ++fs_.reads;
            char digits[24];
            const auto converted = std::to_chars(digits, digits + sizeof(digits), index_);
            const auto length = static_cast<std::size_t>(converted.ptr - digits);
            entry.name = "file" + std::string(7 - length, '0') + std::string(digits, converted.ptr) + ".txt";
            entry.is_directory = index_ % 10000 == 0;
            ++index_;
            return ckv::DirectoryRead::Entry;
        }
    private:
        const GeneratedFileSystem& fs_;
        std::uint64_t index_ = 0;
    };
    std::unique_ptr<ckv::DirectoryCursor> open_directory(std::string_view path) const override {
        if (std::this_thread::get_id() == owning_thread_) ++ui_io;
        if (path != "/large" || deny_open.load()) return nullptr;
        ++opens;
        return std::make_unique<Cursor>(*this);
    }
    mutable std::atomic<unsigned> ui_io{0};
    mutable std::atomic<bool> deny_open{false};
    mutable std::atomic<std::uint64_t> reads{0};
    mutable std::atomic<unsigned> opens{0};
    mutable std::atomic<unsigned> whole_lists{0};
    mutable std::atomic<std::uint64_t> block_at{UINT64_MAX};
    mutable std::atomic<bool> blocked{false};
    mutable std::binary_semaphore release{0};
private:
    std::uint64_t count_;
    const std::thread::id owning_thread_ = std::this_thread::get_id();
};

std::shared_ptr<const ckv::DirectorySnapshot> ready(ckv::DirectorySession& session, std::uint64_t revision) {
    // This is worker-completion correctness, not a latency benchmark. The
    // suite's CTest timeout bounds deadlocks and already accounts for sanitizer
    // instrumentation; an inner timing cutoff can discard a valid publication
    // on a contended host and leave subsequent assertions without their data.
    for (;;) {
        auto result = session.snapshot();
        if (result && result->revision == revision &&
            (result->state == ckv::DirectoryState::Ready || result->state == ckv::DirectoryState::Failed)) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
ckv::DirectoryQuery query(std::uint64_t revision) {
    ckv::DirectoryQuery value; value.revision = revision; value.directory = "/large"; return value;
}
}

CK_TEST(directory_eligibility_keeps_directories_and_exact_prefix_with_insensitive_extensions) {
    auto q = query(1); q.prefix = "Report"; q.extensions = {"txt"};
    CK_CHECK(ckv::directory_item_matches({1, "Report.TXT", false}, q));
    CK_CHECK(!ckv::directory_item_matches({2, "report.txt", false}, q));
    CK_CHECK(!ckv::directory_item_matches({3, "Report.md", false}, q));
    CK_CHECK(ckv::directory_item_matches({4, "Report.md", true}, q));
    q.prefix.clear();
    CK_CHECK(!ckv::directory_item_matches({5, ".hidden.txt", false}, q));
    q.show_hidden = true;
    CK_CHECK(ckv::directory_item_matches({5, ".hidden.txt", false}, q));
    q.directories_only = true;
    CK_CHECK(!ckv::directory_item_matches({5, ".hidden.txt", false}, q));
}

CK_TEST(directory_million_entry_index_reuses_discovery_and_has_bounded_bidirectional_pages) {
    ckv::testing::ScratchDirectory scratch("directory-million");
    auto fs = std::make_shared<GeneratedFileSystem>(1'000'000);
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    CK_CHECK(session != nullptr);
    session->request(query(1));
    auto first = ready(*session, 1);
    CK_CHECK(first->state == ckv::DirectoryState::Ready);
    CK_CHECK(first->discovered == 1'000'000);
    CK_CHECK(first->matching == 1'000'000);
    CK_CHECK(first->entries.size() == 64);
    CK_CHECK(first->has_next && !first->has_previous);
    CK_CHECK(first->entries.front().is_directory);
    std::size_t bytes = first->directory.size();
    for (const auto& row : first->entries) bytes += row.name.size() + 1;
    CK_CHECK(bytes <= ckv::kDirectoryTextBytes);
    auto next = query(2); next.boundary = first->last;
    session->request(next);
    auto second = ready(*session, 2);
    CK_CHECK(second->state == ckv::DirectoryState::Ready);
    CK_CHECK(second->has_previous && second->has_next);
    CK_CHECK(second->entries.front().id != first->entries.front().id);
    auto previous = query(3); previous.boundary = second->first; previous.backward = true;
    session->request(previous);
    auto back = ready(*session, 3);
    CK_CHECK(back->entries == first->entries);
    auto prefix = query(4); prefix.prefix = "file099999"; prefix.extensions = {"TXT"};
    session->request(prefix);
    auto filtered = ready(*session, 4);
    CK_CHECK(filtered->state == ckv::DirectoryState::Ready);
    CK_CHECK(filtered->entries.size() == 10);
    CK_CHECK(filtered->matching == 10);
    CK_CHECK(!filtered->has_next && !filtered->has_previous);
    CK_CHECK(fs->opens.load() == 1);
    CK_CHECK(fs->reads.load() == 1'000'000);
    CK_CHECK(fs->whole_lists.load() == 0);
    CK_CHECK(fs->ui_io.load() == 0);
    auto alias = query(5); alias.directory = "/large/.";
    session->request(alias);
    const auto aliased = ready(*session, 5);
    CK_CHECK(aliased->directory == "/large");
    CK_CHECK(aliased->entries == first->entries);
    CK_CHECK(fs->opens.load() == 1 && fs->reads.load() == 1'000'000);
    auto unavailable = query(6); unavailable.directory = "/missing";
    session->request(unavailable);
    CK_CHECK(ready(*session, 6)->state == ckv::DirectoryState::Failed);
    session->request(query(7));
    CK_CHECK(ready(*session, 7)->entries == first->entries);
    CK_CHECK(fs->opens.load() == 1 && fs->reads.load() == 1'000'000);
    auto refresh = query(8); refresh.source_generation = 1;
    session->request(refresh);
    CK_CHECK(ready(*session, 8)->state == ckv::DirectoryState::Ready);
    CK_CHECK(fs->opens.load() == 2);
    session.reset();
}

CK_TEST(directory_blocked_discovery_does_not_block_requests_publication_or_close_and_slots_stay_bounded) {
    ckv::testing::ScratchDirectory scratch("directory-blocked");
    auto fs = std::make_shared<GeneratedFileSystem>(10000); fs->block_at = 0;
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    session->request(query(1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fs->blocked.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    CK_CHECK(fs->blocked.load());
    for (std::uint64_t revision = 2; revision < 1002; ++revision) {
        auto q = query(revision); q.prefix = "file099999";
        session->request(std::move(q));
        const auto publication = session->snapshot();
        CK_CHECK(!publication || publication->entries.size() <= ckv::kDirectoryPageSize);
    }
    session.reset();
    CK_CHECK(service.open_session() == nullptr);
    // Requests and retirement completed before releasing the blocked read.
    // CTest's timeout detects a wait here; timing noise is not a correctness
    // oracle, particularly under sanitizers or on a loaded host.
    CK_CHECK(fs->reads.load() == 0);
    fs->release.release();
    std::unique_ptr<ckv::DirectorySession> replacement;
    while (!replacement && std::chrono::steady_clock::now() < deadline) {
        replacement = service.open_session(); std::this_thread::yield();
    }
    CK_CHECK(replacement != nullptr);
    fs->block_at = UINT64_MAX;
    replacement->request(query(1));
    CK_CHECK(ready(*replacement, 1)->state == ckv::DirectoryState::Ready);
    replacement.reset();
}

CK_TEST(directory_oversized_queries_supersede_work_without_retaining_oversized_storage) {
    ckv::testing::ScratchDirectory scratch("directory-limits");
    auto fs = std::make_shared<GeneratedFileSystem>(10);
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    auto q = query(1); q.prefix.assign(ckv::kDirectoryTextBytes + 1, 'a');
    session->request(q);
    const auto failure = ready(*session, 1);
    CK_CHECK(failure != nullptr);
    CK_CHECK(failure->revision == 1 && failure->state == ckv::DirectoryState::Failed);
    CK_CHECK(failure->entries.empty());
    session->request(query(2));
    CK_CHECK(ready(*session, 2)->state == ckv::DirectoryState::Ready);
    session.reset();
}

CK_TEST(directory_blocked_worker_leaves_actual_dialog_typing_focus_redraw_and_close_available) {
    ckv::testing::ScratchDirectory scratch("directory-ui-blocked");
    auto fs = std::make_shared<GeneratedFileSystem>(10000); fs->block_at = 0;
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    ckv::term::HeadlessTerminal terminal({80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const auto roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto owned = std::make_unique<ckv::widgets::Desktop>(); auto* desktop = owned.get();
    app.root().add_child(std::move(owned));
    bool cancelled = false;
    auto handle = ckv::widgets::make_file_dialog(ckv::widgets::FileDialogMode::Open, "/large", service, {}, roles,
        app, nullptr, [&](ckv::widgets::FileDialogResult result) { cancelled = !result.accepted; });
    auto* window = handle.window.get();
    auto* field = dynamic_cast<ckv::widgets::InputLine*>(handle.initial_focus);
    CK_CHECK(field != nullptr);
    desktop->present_modeless(std::move(handle), app);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!fs->blocked.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    CK_CHECK(fs->blocked.load());
    for (int index = 0; index < 100; ++index) {
        app.dispatch(ckv::TextEvent{"r"});
        clock.advance(20'000'000); app.step(clock.now_nanos());
    }
    CK_CHECK(field->text() == std::string(100, 'r'));
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Left, ckv::Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Tab, ckv::Modifier::None, ""}});
    CK_CHECK(app.focused() != field);
    window->cancel_request();
    clock.advance(20'000'000); app.step(clock.now_nanos());
    CK_CHECK(cancelled);
    CK_CHECK(fs->ui_io.load() == 0);
    CK_CHECK(desktop->children().empty());
    CK_CHECK(!app.next_timer_deadline_nanos());
    // Every UI operation completed while the platform read was still held.
    // Pinned PTY p99 measurement is a separate performance gate.
    CK_CHECK(fs->reads.load() == 0);
    fs->release.release();
}

CK_TEST(directory_navigation_checks_readability_and_never_accepts_relative_service_paths) {
    ckv::testing::ScratchDirectory scratch("directory-navigation");
    auto fs = std::make_shared<GeneratedFileSystem>(1); fs->deny_open = true;
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    auto navigate = query(1); navigate.operation = ckv::DirectoryOperation::Resolve;
    navigate.path = "/large"; navigate.for_navigation = true;
    session->request(navigate);
    const auto denied = ready(*session, 1);
    CK_CHECK(denied->state == ckv::DirectoryState::Failed);
    CK_CHECK(denied->diagnostic == "Directory cannot be read");
    navigate.revision = 2; navigate.for_navigation = false;
    session->request(navigate);
    CK_CHECK(ready(*session, 2)->resolved_directory);
    navigate.revision = 3; navigate.path = "large";
    session->request(navigate);
    CK_CHECK(ready(*session, 3)->state == ckv::DirectoryState::Failed);
    CK_CHECK(fs->ui_io.load() == 0);
    session.reset();
}

CK_TEST(directory_long_name_pages_bound_all_publication_text_and_keep_every_row_reachable) {
    ckv::testing::ScratchDirectory scratch("directory-long-names");
    auto fs = std::make_shared<ckv::MemoryFileSystem>();
    fs->add_directory("/long");
    for (unsigned i = 0; i < 90; ++i) {
        const auto digits = std::to_string(i);
        fs->add_file("/long/" + std::string(3 - digits.size(), '0') + digits + std::string(3997, 'x'));
    }
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    ckv::DirectoryQuery q; q.revision = 1; q.directory = "/long";
    std::vector<std::uint64_t> seen;
    for (;;) {
        session->request(q);
        const auto page = ready(*session, q.revision);
        CK_CHECK(page->state == ckv::DirectoryState::Ready);
        std::size_t bytes = page->directory.size() + page->path.size() + page->diagnostic.size();
        if (page->first) bytes += page->first->name.size();
        if (page->last) bytes += page->last->name.size();
        for (const auto& row : page->entries) { bytes += row.name.size(); seen.push_back(row.id); }
        CK_CHECK(bytes <= ckv::kDirectoryTextBytes);
        CK_CHECK(!page->entries.empty() && page->entries.size() < ckv::kDirectoryPageSize);
        if (!page->has_next) break;
        CK_CHECK(page->last.has_value()); if (!page->last) break;
        q.boundary = page->last; ++q.revision;
    }
    CK_CHECK(seen.size() == 90);
    std::sort(seen.begin(), seen.end());
    CK_CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
    session.reset();
}

CK_TEST(directory_byte_trimming_retains_the_preferred_row_and_keeps_a_pin_only_page_bounded) {
    ckv::testing::ScratchDirectory scratch("directory-pinned-bytes");
    auto fs = std::make_shared<ckv::MemoryFileSystem>();
    const std::string directory = "/" + std::string(20000, 'd');
    fs->add_directory(directory);
    std::vector<std::string> names;
    for (char first : {'a', 'b', 'c'}) {
        names.push_back(first + std::string(ckv::kDirectoryNameBytes - 1, 'x'));
        fs->add_file(directory + "/" + names.back());
    }
    ckv::term::NativeDirectoryService service(fs, (scratch.path() / "index").string(), 1);
    auto session = service.open_session();
    ckv::DirectoryQuery q; q.revision = 1; q.directory = directory;
    session->request(q);
    const auto first = ready(*session, 1);
    CK_CHECK(first->entries.size() == 1 && first->last.has_value());
    if (first->entries.empty() || !first->last) return;
    q.boundary = first->last;
    q.preferred = ckv::DirectoryItem{3, names.back(), false};
    ++q.revision; session->request(q);
    const auto pinned = ready(*session, q.revision);
    CK_CHECK(pinned->state == ckv::DirectoryState::Ready);
    CK_CHECK(pinned->entries == std::vector<ckv::DirectoryItem>{*q.preferred});
    CK_CHECK(!pinned->first && !pinned->last);
    CK_CHECK(pinned->has_previous && pinned->has_next);
    std::size_t bytes = pinned->directory.size();
    for (const auto& row : pinned->entries) bytes += row.name.size() + 1;
    CK_CHECK(bytes <= ckv::kDirectoryTextBytes);
    q.preferred.reset(); ++q.revision; session->request(q);
    const auto ordinary = ready(*session, q.revision);
    CK_CHECK(ordinary->entries.size() == 1);
    if (!ordinary->entries.empty()) CK_CHECK(ordinary->entries.front().name == names[1]);
    session.reset();
}
