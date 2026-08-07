// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/command.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

void CommandRegistry::register_command(CommandId id, std::string title, std::string category,
                                        std::optional<KeyChord> default_chord) {
    CKV_ASSERT(id >= kFirstApplicationCommandId);
    register_command_internal(id, std::move(title), std::move(category), default_chord);
}

void CommandRegistry::register_command_internal(CommandId id, std::string title,
                                                  std::string category,
                                                  std::optional<KeyChord> default_chord) {
    CKV_ASSERT(id != kInvalidCommand);
    CommandInfo info{id, std::move(title), std::move(category), default_chord};
    commands_[id] = std::move(info);
    if (default_chord) bind_key(*default_chord, id);
}

void CommandRegistry::register_standard_commands() {
    // Titles carry a conventional '&' mnemonic where there's a strong,
    // low-collision-risk default (avoided for kMenu/kHelp, which are
    // invoked by their chord directly, not usually a menu ITEM
    // themselves — an application places them in a menu of its own
    // choosing and can still parse_mnemonic() over whatever text it
    // wants there). kCascade uses 'a' rather than 'c', since it can
    // plausibly share a Window menu with kClose ('c').
    using namespace std_commands;
    register_command_internal(kHelp, "Help", "System", KeyChord::parse("F1"));
    register_command_internal(kMenu, "Menu", "Window", KeyChord::parse("F10"));
    register_command_internal(kNextWindow, "&Next", "Window", KeyChord::parse("F6"));
    register_command_internal(kPreviousWindow, "&Previous", "Window", KeyChord::parse("Shift+F6"));
    register_command_internal(kZoom, "&Zoom", "Window", KeyChord::parse("F5"));
    register_command_internal(kClose, "&Close", "Window", KeyChord::parse("Alt+F3"));
    register_command_internal(kQuit, "&Quit", "System", KeyChord::parse("Alt+X"));
    register_command_internal(kTile, "&Tile", "Window", std::nullopt);
    register_command_internal(kCascade, "C&ascade", "Window", std::nullopt);
    register_command_internal(kWindowList, "&Window List", "Window", std::nullopt);
    register_command_internal(kFocusNext, "Next Field", "Window", KeyChord::parse("Tab"));
    register_command_internal(kFocusPrevious, "Previous Field", "Window",
                               KeyChord::parse("Shift+Tab"));
}

void CommandRegistry::add(CommandDescriptor descriptor) {
    std::optional<KeyChord> chord;
    if (!descriptor.chord.empty()) {
        chord = KeyChord::parse(descriptor.chord);
        CKV_ASSERT(chord.has_value());
    }
    const CommandId id = descriptor.id;
    register_command(id, std::move(descriptor.title), std::move(descriptor.category), chord);
    if (descriptor.handler) set_handler(id, std::move(descriptor.handler));
}

const CommandInfo* CommandRegistry::find(CommandId id) const noexcept {
    auto it = commands_.find(id);
    return it == commands_.end() ? nullptr : &it->second;
}

std::vector<CommandInfo> CommandRegistry::all() const {
    std::vector<CommandInfo> result;
    result.reserve(commands_.size());
    for (const auto& [id, info] : commands_) result.push_back(info);
    std::sort(result.begin(), result.end(),
              [](const CommandInfo& a, const CommandInfo& b) { return a.id < b.id; });
    return result;
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

void CommandRegistry::set_handler(CommandId id, std::function<void()> handler) {
    handlers_[id] = std::move(handler);
}

bool CommandRegistry::has_handler(CommandId id) const {
    auto it = handlers_.find(id);
    return it != handlers_.end() && static_cast<bool>(it->second);
}

bool CommandRegistry::execute(CommandId id) {
    auto it = handlers_.find(id);
    if (it == handlers_.end() || !it->second) return false;
    if (!is_enabled(id)) return false;
    it->second();
    return true;
}

}  // namespace ckv::ui
