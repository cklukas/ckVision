// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/filesystem.hpp"

#include <cstdint>

namespace ckv {

namespace {
bool drive_absolute(std::string_view path) noexcept {
    if (path.size() < 3U) return false;
    const bool drive = (path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z');
    return path.size() >= 3 && drive && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
}

std::string normalize(std::string_view path) {
    std::string p;
    p.reserve(path.size());
    bool previous_slash = false;
    for (char ch : path) {
        const bool slash = ch == '/' || ch == '\\';
        if (slash) {
            if (!previous_slash) p.push_back('/');
            previous_slash = true;
        } else {
            p.push_back(ch);
            previous_slash = false;
        }
    }
    if (p.empty()) p = "/";
    if (p.front() != '/' && !drive_absolute(p)) p = "/" + p;
    while (p.size() > 1 && !(p.size() == 3 && p[1] == ':' && p[2] == '/') && p.back() == '/') p.pop_back();
    return p;
}
}  // namespace

std::string FileSystem::normalize_path(std::string_view path) const { return normalize(path); }

bool FileSystem::create_directories(std::string_view) { return false; }

bool FileSystem::is_absolute_path(std::string_view path) const noexcept {
    return !path.empty() && (path.front() == '/' || path.front() == '\\' || drive_absolute(path));
}

std::string FileSystem::join(std::string_view directory, std::string_view name) const {
    std::string dir = normalize_path(directory);
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    std::string n(name);
    for (char& ch : n)
        if (ch == '\\') ch = '/';
    while (!n.empty() && n.front() == '/') n.erase(n.begin());
    if (dir == "/") return "/" + n;
    return normalize_path(dir + "/" + n);
}

std::string FileSystem::parent(std::string_view path) const {
    const std::string p = normalize_path(path);
    if (p == "/") return "/";
    const std::size_t pos = p.find_last_of('/');
    if (p.size() == 3 && p[1] == ':' && p[2] == '/') return p;
    if (pos == 2 && p[1] == ':') return p.substr(0, 3);
    if (pos == std::string::npos || pos == 0) return "/";
    return p.substr(0, pos);
}

std::optional<FileReadResult> FileSystem::read_file(std::string_view) const { return std::nullopt; }

FileWriteResult FileSystem::write_file_atomic(std::string_view, std::string_view, FileWriteExpectation) {
    return FileWriteResult{};
}

std::optional<FileFingerprint> FileSystem::fingerprint(std::string_view) const { return std::nullopt; }

MemoryFileSystem::Node* MemoryFileSystem::find(std::string_view path) noexcept {
    const std::string norm = normalize(path);
    for (auto& [p, node] : nodes_)
        if (p == norm) return &node;
    return nullptr;
}

const MemoryFileSystem::Node* MemoryFileSystem::find(std::string_view path) const noexcept {
    const std::string norm = normalize(path);
    for (const auto& [p, node] : nodes_)
        if (p == norm) return &node;
    return nullptr;
}

void MemoryFileSystem::add_directory(std::string_view path) {
    const std::string norm = normalize(path);
    std::size_t pos = 1;
    while (pos <= norm.size()) {
        const std::size_t next = norm.find('/', pos);
        const std::string prefix = norm.substr(0, next == std::string::npos ? norm.size() : next);
        if (find(prefix) == nullptr) nodes_.emplace_back(prefix, Node{true, {}, 0});
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    if (Node* existing = find(norm)) existing->is_directory = true;
}

void MemoryFileSystem::add_file(std::string_view path) {
    add_file(path, {});
}

void MemoryFileSystem::add_file(std::string_view path, std::string contents) {
    const std::string norm = normalize(path);
    add_directory(parent(norm));
    if (Node* existing = find(norm)) {
        existing->is_directory = false;
        existing->contents = std::move(contents);
        ++existing->revision;
    } else {
        nodes_.emplace_back(norm, Node{false, std::move(contents), 1});
    }
}

std::vector<FileEntry> MemoryFileSystem::list_directory(std::string_view path) const {
    const std::string norm = normalize(path);
    std::vector<FileEntry> out;
    const Node* self = find(norm);
    if (self == nullptr || !self->is_directory) return out;
    for (const auto& [p, node] : nodes_) {
        if (p == norm) continue;
        if (parent(p) != norm) continue;
        const std::size_t pos = p.find_last_of('/');
        out.push_back(FileEntry{p.substr(pos + 1), node.is_directory});
    }
    return out;
}

bool MemoryFileSystem::exists(std::string_view path) const noexcept { return find(path) != nullptr; }

bool MemoryFileSystem::is_directory(std::string_view path) const noexcept {
    const Node* n = find(path);
    return n != nullptr && n->is_directory;
}

bool MemoryFileSystem::create_directories(std::string_view path) {
    const std::string normalized = normalize_path(path);
    const Node* existing = find(normalized);
    if (existing != nullptr && !existing->is_directory) return false;
    add_directory(normalized);
    return true;
}

std::optional<FileReadResult> MemoryFileSystem::read_file(std::string_view path) const {
    const Node* node = find(path);
    if (node == nullptr || node->is_directory) return std::nullopt;
    return FileReadResult{node->contents, FileFingerprint{std::to_string(node->revision)}};
}

FileWriteResult MemoryFileSystem::write_file_atomic(std::string_view path, std::string_view contents,
                                                     FileWriteExpectation expectation) {
    const std::string normalized = normalize(path);
    Node* node = find(normalized);
    if (node != nullptr && node->is_directory) return FileWriteResult{FileWriteStatus::NotFound, std::nullopt};
    if (expectation.kind == FileWriteExpectationKind::MustNotExist && node != nullptr)
        return FileWriteResult{FileWriteStatus::Conflict, fingerprint(normalized)};
    if (expectation.kind == FileWriteExpectationKind::MatchFingerprint &&
        (!expectation.fingerprint || !node || *expectation.fingerprint != FileFingerprint{std::to_string(node->revision)}))
        return FileWriteResult{FileWriteStatus::Conflict, fingerprint(normalized)};
    if (node == nullptr) {
        add_directory(parent(normalized));
        nodes_.emplace_back(normalized, Node{false, std::string(contents), 1});
        return FileWriteResult{FileWriteStatus::Ok, FileFingerprint{"1"}};
    }
    node->contents.assign(contents);
    ++node->revision;
    return FileWriteResult{FileWriteStatus::Ok, FileFingerprint{std::to_string(node->revision)}};
}

std::optional<FileFingerprint> MemoryFileSystem::fingerprint(std::string_view path) const {
    const Node* node = find(path);
    if (node == nullptr || node->is_directory) return std::nullopt;
    return FileFingerprint{std::to_string(node->revision)};
}

}  // namespace ckv
