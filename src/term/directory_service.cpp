// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/directory_service.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <filesystem>
#include <fstream>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <utility>

#include "cvision/core/text.hpp"

namespace ckv::term {
namespace {
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
constexpr std::size_t kCancellationBatch = 256;
constexpr std::size_t kPublicationBatch = 4096;
enum class SlotState { Available, Active, Retiring };

// Single-producer/single-consumer triple buffer. The atomic handoff never
// waits or allocates; each actor owns its current slot. Shared ownership only
// preserves bounded immutable values held by the consumer.
template <class T>
class Latest {
public:
    using Pointer = std::shared_ptr<const T>;
    void publish(Pointer value) noexcept {
        values_[write_] = std::move(value);
        write_ = middle_.exchange(static_cast<unsigned>(write_) | kDirty) & kIndex;
    }
    Pointer read() const noexcept {
        if ((middle_.load() & kDirty) != 0) read_ = middle_.exchange(read_) & kIndex;
        return values_[read_];
    }
private:
    static constexpr unsigned kDirty = 4;
    static constexpr unsigned kIndex = 3;
    std::array<Pointer, 3> values_;
    unsigned write_ = 0;
    mutable unsigned read_ = 1;
    mutable std::atomic<unsigned> middle_{2};
};
struct Request {
    DirectoryQuery query;
    bool invalid = false;
};

struct Slot {
    std::atomic<SlotState> state{SlotState::Available};
    std::atomic<bool> stop{false};
    std::atomic<bool> wake_pending{false};
    std::binary_semaphore wake{0};
    Latest<Request> query;
    std::atomic<std::uint64_t> requested_revision{0};
    Latest<DirectorySnapshot> publication;
    std::thread worker;
    void signal() noexcept {
        if (!wake_pending.exchange(true)) wake.release();
    }
};

class Session final : public DirectorySession {
public:
    explicit Session(Slot& slot) noexcept : slot_(slot) {}
    ~Session() override {
        slot_.state.store(SlotState::Retiring);
        slot_.signal();
    }
    void request(DirectoryQuery query) override {
        if (query.revision == 0 || query.revision <= revision_) return;
        revision_ = query.revision;
        const bool invalid = !valid_directory_query(query);
        if (invalid) { query = {}; query.revision = revision_; }
        slot_.requested_revision.store(revision_);
        slot_.query.publish(std::make_shared<Request>(Request{std::move(query), invalid}));
        slot_.signal();
    }
    std::shared_ptr<const DirectorySnapshot> snapshot() const noexcept override {
        return slot_.publication.read();
    }
private:
    Slot& slot_;
    std::uint64_t revision_ = 0;
};

// One bounded ordered page. The disk index, not this vector, holds the source.
class Page {
public:
    explicit Page(const DirectoryQuery& query) : query_(query) { rows_.reserve(kDirectoryPageSize + 1); }
    void add(const DirectoryItem& item) {
        if (!directory_item_matches(item, query_)) return;
        ++matching_;
        if (query_.preferred && item == *query_.preferred) preferred_ = item;
        if (query_.boundary) {
            const bool before = directory_item_less(item, *query_.boundary);
            const bool after = directory_item_less(*query_.boundary, item);
            if (query_.backward ? !before : !after) {
                other_side_ = true;
                return;
            }
        }
        ++qualified_;
        auto at = std::lower_bound(rows_.begin(), rows_.end(), item, directory_item_less);
        // Avoid allocating/copying every later row in a large ordered source.
        if (rows_.size() == kDirectoryPageSize &&
            (query_.backward ? at == rows_.begin() : at == rows_.end())) return;
        rows_.insert(at, item);
        if (rows_.size() > kDirectoryPageSize) {
            if (query_.backward) rows_.erase(rows_.begin());
            else rows_.pop_back();
        }
    }
    void fill(DirectorySnapshot& output) const {
        auto page = rows_;
        // Trimming may remove the preferred row itself. Re-evaluate retention
        // against the actual page at each step and reserve its storage when
        // it becomes an out-of-page pin, including both owning boundary keys.
        const auto needs_pin = [&] {
            return preferred_ && std::find(page.begin(), page.end(), *preferred_) == page.end();
        };
        std::size_t bytes = output.directory.size();
        for (const auto& item : page) bytes += item.name.size() + 1;
        while (!page.empty()) {
            const bool pin = needs_pin();
            const auto extra = (pin ? preferred_->name.size() + 1 : 0) +
                               page.front().name.size() + page.back().name.size();
            if (bytes + extra <= kDirectoryTextBytes &&
                !(pin && page.size() == kDirectoryPageSize)) break;
            const auto index = query_.backward ? 0 : page.size() - 1;
            bytes -= page[index].name.size() + 1;
            page.erase(page.begin() + static_cast<std::ptrdiff_t>(index));
        }
        output.matching = matching_;
        output.has_previous = query_.backward ? qualified_ > page.size() : other_side_;
        output.has_next = query_.backward ? other_side_ : qualified_ > page.size();
        if (!page.empty()) { output.first = page.front(); output.last = page.back(); }
        if (needs_pin())
            page.insert(std::lower_bound(page.begin(), page.end(), *preferred_, directory_item_less), *preferred_);
        output.entries = std::move(page);
    }
private:
    const DirectoryQuery& query_;
    std::vector<DirectoryItem> rows_;
    std::optional<DirectoryItem> preferred_;
    std::uint64_t matching_ = 0;
    std::uint64_t qualified_ = 0;
    bool other_side_ = false;
};

// Process-private scratch format, never an exported/persistent file format.
bool write_item(std::ofstream& stream, const DirectoryItem& item) {
    const auto length = static_cast<std::uint32_t>(item.name.size());
    const std::uint8_t directory = item.is_directory ? 1 : 0;
    stream.write(reinterpret_cast<const char*>(&item.id), sizeof(item.id));
    stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
    stream.write(reinterpret_cast<const char*>(&directory), sizeof(directory));
    stream.write(item.name.data(), static_cast<std::streamsize>(item.name.size()));
    return stream.good();
}
bool read_item(std::ifstream& stream, DirectoryItem& item) {
    std::uint32_t length = 0;
    std::uint8_t directory = 0;
    stream.read(reinterpret_cast<char*>(&item.id), sizeof(item.id));
    stream.read(reinterpret_cast<char*>(&length), sizeof(length));
    stream.read(reinterpret_cast<char*>(&directory), sizeof(directory));
    if (!stream || length == 0 || length > kDirectoryNameBytes || directory > 1) return false;
    item.name.resize(length);
    stream.read(item.name.data(), static_cast<std::streamsize>(length));
    item.is_directory = directory != 0;
    return stream.good();
}

class Discovery {
public:
    Discovery(Slot& slot, const FileSystem& filesystem, std::filesystem::path index)
        : slot_(slot), fs_(filesystem), index_(std::move(index)) {}
    ~Discovery() { clear(); }
    void run() noexcept {
        for (;;) {
            slot_.wake.acquire();
            slot_.wake_pending.store(false);
            if (slot_.stop.load()) return;
            if (slot_.state.load() == SlotState::Retiring) {
                clear();
                slot_.query.read();
                slot_.requested_revision.store(0);
                slot_.publication.publish(nullptr);
                processed_.reset();
                slot_.state.store(SlotState::Available);
                continue;
            }
            const auto request = slot_.query.read();
            if (!request || request == processed_ || !current(request->query)) continue;
            processed_ = request;
            const auto& query = request->query;
            try {
                if (request->invalid) publish(query, DirectoryState::Failed, nullptr, "Directory query exceeds its size limit");
                else process(query);
            } catch (...) {
                // Background failures never unwind through a thread entry or
                // into UI dispatch. A bounded diagnostic deliberately avoids
                // copying an unbounded exception/platform message.
                try { publish(query, DirectoryState::Failed, nullptr, "Directory operation failed"); }
                catch (...) { /* Allocation failure cannot manufacture a result. */ }
                clear();
            }
        }
    }
private:
    bool current(const DirectoryQuery& query) const noexcept {
        return !slot_.stop.load() && slot_.state.load() == SlotState::Active &&
               slot_.requested_revision.load() == query.revision;
    }
    void clear() noexcept {
        cursor_.reset();
        output_.close();
        raw_directory_.clear();
        directory_.clear();
        count_ = 0;
        complete_ = false;
        std::error_code error;
        std::filesystem::remove(index_, error);
    }
    std::shared_ptr<DirectorySnapshot> make_result(const DirectoryQuery& query, DirectoryState state) {
        auto value = std::make_shared<DirectorySnapshot>();
        value->revision = query.revision;
        value->sequence = ++sequence_;
        value->source_generation = query.source_generation;
        value->state = state;
        return value;
    }
    void publish(const DirectoryQuery& query, DirectoryState state, const Page* page,
                 std::string diagnostic = {}) {
        if (!current(query)) return;
        auto value = make_result(query, state);
        value->directory = directory_;
        value->discovered = count_;
        value->diagnostic = std::move(diagnostic);
        if (page) page->fill(*value);
        if (current(query)) slot_.publication.publish(std::move(value));
    }
    void process(const DirectoryQuery& query) {
        if (!fs_.is_absolute_path(query.directory) ||
            (query.operation == DirectoryOperation::Resolve && !fs_.is_absolute_path(query.path))) {
            publish(query, DirectoryState::Failed, nullptr, "Directory queries require absolute paths");
            return;
        }
        if (query.operation == DirectoryOperation::Resolve) {
            auto directory = fs_.resolve_directory(query.path);
            if (!current(query)) return;
            if (directory && query.for_navigation) {
                auto probe = fs_.open_directory(*directory);
                if (!probe) { publish(query, DirectoryState::Failed, nullptr, "Directory cannot be read"); return; }
            } else if (!directory && fs_.is_directory(query.path)) {
                publish(query, DirectoryState::Failed, nullptr, "Directory cannot be resolved");
                return;
            }
            if (!current(query)) return;
            auto value = make_result(query, DirectoryState::Ready);
            value->resolved_directory = directory.has_value();
            value->path = directory ? std::move(*directory) : fs_.normalize_path(query.path);
            if (value->path.size() > kDirectoryTextBytes) {
                value->path.clear(); value->state = DirectoryState::Failed;
                value->diagnostic = "Resolved path exceeds its size limit";
            }
            if (current(query)) slot_.publication.publish(std::move(value));
            return;
        }
        if (raw_directory_ != query.directory || generation_ != query.source_generation) {
            const auto resolved = fs_.resolve_directory(query.directory);
            if (!current(query)) return;
            // One row must fit even at the maximum name size, including both
            // owning pagination keys. Otherwise the next page could have no
            // boundary and the row would become permanently unreachable.
            if (!resolved || resolved->size() > kDirectoryTextBytes - 3 * kDirectoryNameBytes - 1) {
                auto value = make_result(query, DirectoryState::Failed);
                value->diagnostic = "Directory is unavailable";
                if (current(query)) slot_.publication.publish(std::move(value));
                return;
            }
            if (raw_directory_.empty() || directory_ != *resolved || generation_ != query.source_generation) {
                clear();
                directory_ = *resolved;
                cursor_ = fs_.open_directory(directory_);
                if (!current(query)) return;
                if (!cursor_) { publish(query, DirectoryState::Failed, nullptr, "Directory cannot be read"); clear(); return; }
                output_.open(index_, std::ios::binary | std::ios::trunc);
                if (!output_) { publish(query, DirectoryState::Failed, nullptr, "Directory index cannot be created"); clear(); return; }
            }
            raw_directory_ = query.directory;
            generation_ = query.source_generation;
        }
        Page page(query);
        if (count_ != 0) {
            output_.flush();
            if (!output_) { publish(query, DirectoryState::Failed, nullptr, "Directory index cannot be written"); clear(); return; }
            std::ifstream input(index_, std::ios::binary);
            DirectoryItem item;
            for (std::uint64_t index = 0; index < count_; ++index) {
                if (index % kCancellationBatch == 0 && !current(query)) return;
                if (!read_item(input, item)) { publish(query, DirectoryState::Failed, nullptr, "Directory index cannot be read"); clear(); return; }
                page.add(item);
            }
        }
        if (!current(query)) return;
        publish(query, complete_ ? DirectoryState::Ready : DirectoryState::Partial, &page);
        if (complete_) return;
        FileEntry entry;
        while (current(query)) {
            const auto result = cursor_->next(entry);
            if (result != DirectoryRead::Entry) {
                if (result == DirectoryRead::Failed) {
                    publish(query, DirectoryState::Failed, nullptr, "Directory discovery failed"); clear(); return;
                }
                cursor_.reset();
                output_.flush();
                if (!output_) { publish(query, DirectoryState::Failed, nullptr, "Directory index cannot be written"); clear(); return; }
                complete_ = true;
                publish(query, DirectoryState::Ready, &page);
                return;
            }
            if (entry.name.empty() || entry.name == "." || entry.name == "..") continue;
            // Unsupported names fail explicitly; never silently omit an entry
            // whose true path cannot be represented by the bounded contract.
            if (entry.name.size() > kDirectoryNameBytes || !text::is_sanitized_display_text(entry.name) ||
                entry.name.find('/') != std::string::npos || entry.name.find('\\') != std::string::npos) {
                publish(query, DirectoryState::Failed, nullptr, "Directory contains an unsupported filename"); clear(); return;
            }
            DirectoryItem item{count_ + 1, std::move(entry.name), entry.is_directory};
            if (!write_item(output_, item)) { publish(query, DirectoryState::Failed, nullptr, "Directory index cannot be written"); clear(); return; }
            ++count_;
            page.add(item);
            if (count_ % kPublicationBatch == 0) publish(query, DirectoryState::Partial, &page);
        }
    }
    Slot& slot_;
    const FileSystem& fs_;
    std::filesystem::path index_;
    std::ofstream output_;
    std::unique_ptr<DirectoryCursor> cursor_;
    std::string raw_directory_;
    std::string directory_;
    std::uint64_t generation_ = 0;
    std::uint64_t count_ = 0;
    std::uint64_t sequence_ = 0;
    bool complete_ = false;
    std::shared_ptr<const Request> processed_;
};
}  // namespace

struct NativeDirectoryService::Impl {
    std::shared_ptr<const FileSystem> filesystem;
    std::vector<std::unique_ptr<Slot>> slots;
    ~Impl() {
        for (const auto& slot : slots) { slot->stop.store(true); slot->signal(); }
        for (const auto& slot : slots) if (slot->worker.joinable()) slot->worker.join();
    }
};

NativeDirectoryService::NativeDirectoryService(std::shared_ptr<const FileSystem> filesystem,
                                             std::string index_directory, std::size_t slots)
    : impl_(std::make_unique<Impl>()) {
    if (!filesystem || slots == 0 || slots > 8 || index_directory.empty() ||
        index_directory.find('\0') != std::string::npos)
        throw std::invalid_argument("Directory service requires a filesystem, exclusive index directory and 1..8 slots");
    const std::filesystem::path root{std::u8string(index_directory.begin(), index_directory.end())};
    if (!root.is_absolute()) throw std::invalid_argument("Directory index path must be absolute");
    std::filesystem::create_directories(root);
    if (!std::filesystem::is_empty(root)) throw std::invalid_argument("Directory index directory must be empty and exclusive");
    impl_->filesystem = std::move(filesystem);
    impl_->slots.reserve(slots);
    for (std::size_t index = 0; index < slots; ++index) {
        auto slot = std::make_unique<Slot>();
        Slot* pointer = slot.get();
        impl_->slots.push_back(std::move(slot));
        const auto file = root / ("directory-" + std::to_string(index) + ".index");
        pointer->worker = std::thread([pointer, source = impl_->filesystem, file] {
            Discovery discovery(*pointer, *source, file);
            discovery.run();
        });
    }
}
NativeDirectoryService::~NativeDirectoryService() = default;
std::unique_ptr<DirectorySession> NativeDirectoryService::open_session() {
    for (const auto& slot : impl_->slots) {
        auto expected = SlotState::Available;
        if (slot->state.compare_exchange_strong(expected, SlotState::Active)) {
            try { return std::make_unique<Session>(*slot); }
            catch (...) { slot->state.store(SlotState::Available); throw; }
        }
    }
    return nullptr;
}
const FileSystem& NativeDirectoryService::filesystem() const noexcept { return *impl_->filesystem; }

}  // namespace ckv::term
