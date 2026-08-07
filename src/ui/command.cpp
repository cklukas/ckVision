// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/command.hpp"

#include "cvision/core/assert.hpp"

namespace ckv::ui {

void CommandRegistry::register_command(CommandId id, std::string title, std::string category,
                                        std::optional<KeyChord> default_chord) {
    CKV_ASSERT(id != kInvalidCommand);
    CommandInfo info{id, std::move(title), std::move(category), default_chord};
    commands_[id] = std::move(info);
    if (default_chord) bind_key(*default_chord, id);
}

const CommandInfo* CommandRegistry::find(CommandId id) const noexcept {
    auto it = commands_.find(id);
    return it == commands_.end() ? nullptr : &it->second;
}

void CommandRegistry::set_enabled_predicate(CommandId id, std::function<bool()> predicate) {
    CKV_ASSERT(commands_.find(id) != commands_.end());
    enabled_predicates_[id] = std::move(predicate);
}

bool CommandRegistry::is_enabled(CommandId id) const {
    auto it = enabled_predicates_.find(id);
    if (it == enabled_predicates_.end()) return true;
    return it->second ? it->second() : true;
}

void CommandRegistry::bind_key(KeyChord chord, CommandId id) {
    for (auto& [existing_chord, existing_id] : keymap_) {
        if (existing_chord == chord) {
            existing_id = id;
            return;
        }
    }
    keymap_.emplace_back(chord, id);
}

void CommandRegistry::unbind_key(const KeyChord& chord) {
    for (auto it = keymap_.begin(); it != keymap_.end(); ++it) {
        if (it->first == chord) {
            keymap_.erase(it);
            return;
        }
    }
}

std::optional<CommandId> CommandRegistry::command_for_key(const KeyChord& chord) const {
    for (const auto& [existing_chord, id] : keymap_)
        if (existing_chord == chord) return id;
    return std::nullopt;
}

std::optional<KeyChord> CommandRegistry::chord_for_command(CommandId id) const {
    for (const auto& [chord, existing_id] : keymap_)
        if (existing_id == id) return chord;
    return std::nullopt;
}

}  // namespace ckv::ui
