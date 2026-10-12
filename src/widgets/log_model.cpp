// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_model.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "cvision/core/utf8.hpp"

namespace ckv::widgets {
namespace {
void add_counter(std::uint64_t& value, std::uint64_t amount = 1) noexcept {
    value += std::min(amount, std::numeric_limits<std::uint64_t>::max() - value);
}

std::size_t scalar_bytes(unsigned char first) noexcept {
    if (first >= 0xC2 && first <= 0xDF) return 2;
    if (first >= 0xE0 && first <= 0xEF) return 3;
    if (first >= 0xF0 && first <= 0xF4) return 4;
    return 1;
}

bool valid_prefix(std::span<const char> bytes) noexcept {
    for (std::size_t i = 1; i < bytes.size(); ++i) {
        const auto byte = static_cast<unsigned char>(bytes[i]);
        if (byte < 0x80 || byte > 0xBF) return false;
    }
    if (bytes.size() < 2) return true;
    const auto first = static_cast<unsigned char>(bytes[0]);
    const auto second = static_cast<unsigned char>(bytes[1]);
    return !((first == 0xE0 && second < 0xA0) || (first == 0xED && second > 0x9F) ||
             (first == 0xF0 && second < 0x90) || (first == 0xF4 && second > 0x8F));
}
}

LogModel::LogModel(LogLimits limits, LogCarriageReturn carriage_return)
    : limits_(limits), carriage_return_(carriage_return) {
    static_assert(sizeof(Slot) <= 1024);
    static_assert(sizeof(LogModel) <= 16384);
    if (!limits.valid()) throw std::invalid_argument("Invalid log storage limits");
    if (carriage_return != LogCarriageReturn::LineBreak && carriage_return != LogCarriageReturn::Progress)
        throw std::invalid_argument("Invalid log carriage return policy");
    arena_.resize(limits_.retained_bytes);
    slots_.resize(limits_.records);
    raw_.reserve(limits_.record_bytes);
    display_.reserve(limits_.record_bytes * 3);
    encoded_.reserve(4);
}

std::size_t LogModel::allocated_bytes() const noexcept {
    return sizeof(*this) + arena_.capacity() + slots_.capacity() * sizeof(Slot) +
        raw_.capacity() + display_.capacity() + encoded_.capacity();
}

LogRecord LogModel::partial_record() const noexcept {
    LogRecord record;
    record.id = *partial_id_;
    record.raw = {raw_.data(), raw_.size()};
    record.text = {display_.data(), display_.size()};
    record.severity = severity_;
    record.timestamp_nanos = timestamp_;
    record.partial = true;
    record.discarded_bytes = partial_discarded_;
    record.field_count = field_count_;
    for (std::size_t i = 0; i < field_count_; ++i) {
        const auto& field = partial_fields_[i];
        record.fields[i] = {{metadata_bytes_.data() + field.name, field.name_size},
                            {metadata_bytes_.data() + field.value, field.value_size}};
    }
    return record;
}

std::optional<LogRecord> LogModel::at(std::size_t index) const noexcept {
    if (index >= size()) return std::nullopt;
    if (index == count_) return partial_record();
    const auto& slot = slots_[(head_ + index) % slots_.size()];
    LogRecord record;
    record.id = slot.id;
    record.raw = {arena_.data() + slot.offset, slot.raw_size};
    record.text = {arena_.data() + slot.offset + slot.raw_size, slot.text_size};
    record.severity = slot.severity;
    record.timestamp_nanos = slot.timestamp;
    record.incomplete = slot.incomplete;
    record.discarded_bytes = slot.discarded;
    record.field_count = slot.field_count;
    for (std::size_t i = 0; i < slot.field_count; ++i) {
        const auto& field = slot.fields[i];
        record.fields[i] = {{arena_.data() + field.name, field.name_size},
                            {arena_.data() + field.value, field.value_size}};
    }
    return record;
}

std::optional<std::size_t> LogModel::index_of(LogRecordId id) const noexcept {
    if (partial_id_ && *partial_id_ == id) return count_;
    std::size_t first = 0, last = count_;
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto& candidate = slots_[(head_ + middle) % slots_.size()].id;
        if (candidate.sequence < id.sequence) first = middle + 1;
        else last = middle;
    }
    if (first < count_ && slots_[(head_ + first) % slots_.size()].id == id) return first;
    return std::nullopt;
}

bool LogModel::start_partial() noexcept {
    if (partial_id_) return true;
    if (source_ == 0 || next_sequence_ == std::numeric_limits<std::uint64_t>::max()) return false;
    // The partial record is an entry too: reserve one slot by evicting now.
    if (count_ == slots_.size()) evict_oldest();
    partial_id_ = LogRecordId{source_, generation_, next_sequence_++};
    return true;
}

void LogModel::reset_partial() noexcept {
    partial_id_.reset();
    raw_.clear();
    display_.clear();
    utf8_size_ = 0;
    metadata_size_ = 0;
    field_count_ = 0;
    severity_ = LogSeverity::Unknown;
    timestamp_.reset();
    partial_discarded_ = 0;
    multiline_ = false;
}

void LogModel::evict_oldest() noexcept {
    if (count_ == 0) return;
    arena_used_ -= slots_[head_].allocation;
    head_ = (head_ + 1) % slots_.size();
    --count_;
    add_counter(stats_.evicted);
}

void LogModel::decode_pending(bool final) noexcept {
    while (utf8_size_ != 0) {
        const std::span<const char> bytes(utf8_bytes_.data(), utf8_size_);
        const auto required = scalar_bytes(static_cast<unsigned char>(bytes[0]));
        if (!final && utf8_size_ < required && valid_prefix(bytes)) return;
        std::size_t consumed = 0;
        auto scalar = utf8::decode({bytes.data(), bytes.size()}, consumed);
        if ((scalar < 0x20 || (scalar >= 0x7F && scalar <= 0x9F)) &&
            !(multiline_ && scalar == U'\n')) scalar = utf8::replacement_char;
        encoded_.clear();
        utf8::encode(scalar, encoded_);
        display_.insert(display_.end(), encoded_.begin(), encoded_.end());
        utf8_size_ -= consumed;
        for (std::size_t i = 0; i < utf8_size_; ++i) utf8_bytes_[i] = utf8_bytes_[i + consumed];
    }
}

void LogModel::content_byte(char byte) noexcept {
    if (!start_partial()) return;
    if (raw_.size() == limits_.record_bytes) {
        add_counter(partial_discarded_);
        add_counter(stats_.discarded_bytes);
        return;
    }
    raw_.push_back(byte);
    const auto value = static_cast<unsigned char>(byte);
    if (utf8_size_ == 0 && ((value >= 0x20 && value < 0x7F) || (multiline_ && byte == '\n'))) {
        display_.push_back(byte);
        return;
    }
    utf8_bytes_[utf8_size_++] = byte;
    decode_pending(false);
}

void LogModel::commit_partial(bool incomplete) noexcept {
    if (!partial_id_) return;
    decode_pending(true);
    const auto payload = raw_.size() + display_.size() + metadata_size_;
    std::size_t padding = 0;
    for (;;) {
        if (count_ == 0) { arena_tail_ = 0; arena_used_ = 0; }
        padding = payload > arena_.size() - arena_tail_ ? arena_.size() - arena_tail_ : 0;
        if (count_ < slots_.size() && payload + padding <= arena_.size() - arena_used_) break;
        evict_oldest();
    }
    const auto offset = padding == 0 ? arena_tail_ : 0;
    auto& slot = slots_[(head_ + count_) % slots_.size()];
    slot = {};
    slot.id = *partial_id_;
    slot.offset = offset;
    slot.raw_size = raw_.size();
    slot.text_size = display_.size();
    slot.allocation = payload + padding;
    slot.severity = severity_;
    slot.timestamp = timestamp_;
    slot.incomplete = incomplete;
    slot.discarded = partial_discarded_;
    slot.field_count = field_count_;
    if (!raw_.empty()) std::memcpy(arena_.data() + offset, raw_.data(), raw_.size());
    if (!display_.empty()) std::memcpy(arena_.data() + offset + raw_.size(), display_.data(), display_.size());
    const auto metadata_offset = offset + raw_.size() + display_.size();
    if (metadata_size_ != 0) std::memcpy(arena_.data() + metadata_offset, metadata_bytes_.data(), metadata_size_);
    for (std::size_t i = 0; i < field_count_; ++i) {
        slot.fields[i] = partial_fields_[i];
        slot.fields[i].name += metadata_offset;
        slot.fields[i].value += metadata_offset;
    }
    ++count_;
    arena_used_ += slot.allocation;
    arena_tail_ = (offset + payload) % arena_.size();
    add_counter(stats_.committed);
    reset_partial();
}

void LogModel::line_byte(char byte) noexcept {
    if (after_cr_) {
        after_cr_ = false;
        if (byte == '\n') {
            if (carriage_return_ == LogCarriageReturn::Progress) commit_partial(false);
            return;
        }
        if (carriage_return_ == LogCarriageReturn::Progress) {
            // Preserve identity while replacing one progress line, including
            // metadata and the already-accounted oversized-byte diagnostic.
            raw_.clear();
            display_.clear();
            utf8_size_ = 0;
        }
    }
    if (byte == '\r') {
        if (start_partial()) {
            if (carriage_return_ == LogCarriageReturn::LineBreak) commit_partial(false);
            else decode_pending(true);
        }
        after_cr_ = true;
    } else if (byte == '\n') {
        if (start_partial()) commit_partial(false);
    } else content_byte(byte);
}

void LogModel::flush_bom() noexcept {
    bom_pending_ = false;
    for (std::size_t i = 0; i < bom_size_; ++i) line_byte(bom_bytes_[i]);
    bom_size_ = 0;
}

bool LogModel::begin_source(LogSourceId source, std::uint64_t generation) noexcept {
    if (source == 0 || generation == 0) return false;
    if (source == source_ && generation == generation_) return true;
    if (source == source_ && generation < generation_) return false;
    finish();
    source_ = source;
    generation_ = generation;
    bom_pending_ = true;
    bom_size_ = 0;
    after_cr_ = false;
    add_counter(stats_.revision);
    return true;
}

bool LogModel::append_bytes(std::string_view bytes) noexcept {
    if (source_ == 0 || bytes.size() > limits_.batch_bytes ||
        next_sequence_ > std::numeric_limits<std::uint64_t>::max() - bytes.size()) return false;
    constexpr std::array<unsigned char, 3> bom{0xEF, 0xBB, 0xBF};
    for (const auto byte : bytes) {
        if (bom_pending_) {
            bom_bytes_[bom_size_++] = byte;
            if (static_cast<unsigned char>(byte) != bom[bom_size_ - 1]) flush_bom();
            else if (bom_size_ == bom.size()) { bom_pending_ = false; bom_size_ = 0; }
        } else line_byte(byte);
    }
    add_counter(stats_.processed_bytes, bytes.size());
    if (!bytes.empty()) add_counter(stats_.revision);
    return true;
}

bool LogModel::append_record(std::string_view raw, LogMetadata metadata) noexcept {
    if (source_ == 0 || raw.size() > limits_.batch_bytes || !metadata.valid() ||
        next_sequence_ >= std::numeric_limits<std::uint64_t>::max() - 1) return false;
    finish();
    // An explicit record bypasses BOM and line separation rules.
    bom_pending_ = false;
    after_cr_ = false;
    if (!start_partial()) return false;
    multiline_ = true;
    severity_ = metadata.severity;
    timestamp_ = metadata.timestamp_nanos;
    field_count_ = metadata.fields.size();
    for (std::size_t i = 0; i < field_count_; ++i) {
        const auto& field = metadata.fields[i];
        partial_fields_[i] = {metadata_size_, field.name.size(), metadata_size_ + field.name.size(), field.value.size()};
        std::memcpy(metadata_bytes_.data() + metadata_size_, field.name.data(), field.name.size());
        metadata_size_ += field.name.size();
        if (!field.value.empty()) std::memcpy(metadata_bytes_.data() + metadata_size_, field.value.data(), field.value.size());
        metadata_size_ += field.value.size();
    }
    for (const auto byte : raw) content_byte(byte);
    commit_partial(false);
    add_counter(stats_.processed_bytes, raw.size());
    add_counter(stats_.revision);
    return true;
}

void LogModel::finish() noexcept {
    if (bom_pending_) flush_bom();
    if (partial_id_) { commit_partial(true); add_counter(stats_.revision); }
    after_cr_ = false;
}

void LogModel::clear() noexcept {
    add_counter(stats_.evicted, size());
    head_ = 0;
    count_ = 0;
    arena_tail_ = 0;
    arena_used_ = 0;
    reset_partial();
    // Clear is a view/retention action, not a new byte-stream generation.
    bom_size_ = 0;
    bom_pending_ = false;
    after_cr_ = false;
    add_counter(stats_.revision);
}

}  // namespace ckv::widgets
