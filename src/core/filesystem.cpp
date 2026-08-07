// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/core/filesystem.hpp"

namespace ckv {

namespace {
std::string normalize(std::string_view path) {
    std::string p(path);
    if (p.empty() || p.front() != '/') p = "/" + p;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}
}  // namespace

std::string FileSystem::join(std::string_view directory, std::string_view name) const {
    std::string dir(directory);
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    std::string n(name);
    while (!n.empty() && n.front() == '/') n.erase(n.begin());
    if (dir == "/") return "/" + n;
    return dir + "/" + n;
}

std::string FileSystem::parent(std::string_view path) const {
    const std::string p = normalize(path);
    if (p == "/") return "/";
    const std::size_t pos = p.find_last_of('/');
    if (pos == std::string::npos || pos == 0) return "/";
    return p.substr(0, pos);
}

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
        if (find(prefix) == nullptr) nodes_.emplace_back(prefix, Node{true});
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    if (Node* existing = find(norm)) existing->is_directory = true;
}

void MemoryFileSystem::add_file(std::string_view path) {
    const std::string norm = normalize(path);
    add_directory(parent(norm));
    if (Node* existing = find(norm))
        existing->is_directory = false;
    else
        nodes_.emplace_back(norm, Node{false});
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

}  // namespace ckv
