// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/command.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

CommandRegistry::CommandRegistry() {
    // Titles carry a conventional '&' mnemonic where there's a strong,
    // low-collision-risk default (avoided for menu/help, which are
    // invoked by their chord directly, not usually a menu ITEM
    // themselves — an application places them in a menu of its own
    // choosing and can still parse_mnemonic() over whatever text it
    // wants there). cascade uses 'a' rather than 'c', since it can
    // plausibly share a Window menu with close ('c').
    //
    // Declaration order is the order docs/standard-commands.md lists
    // them in and the order all() reports them in, so it is chosen for
    // the reader of that table: the two application-wide commands,
    // then window management, then focus traversal.
    using namespace std_command_keys;
    const auto declare_standard = [this](std::string_view key, std::string title,
                                         std::string category, std::string chord) {
        return declare(CommandDescriptor{.key = std::string(key),
                                         .title = std::move(title),
                                         .category = std::move(category),
                                         .chord = std::move(chord),
                                         .visibility = CommandVisibility::Hidden});
    };
    standard_.quit = declare_standard(kQuit, "&Quit", "System", "Alt+X");
    standard_.help = declare_standard(kHelp, "Help", "System", "F1");
    standard_.menu = declare_standard(kMenu, "Menu", "Window", "F10");
    standard_.close = declare_standard(kClose, "&Close", "Window", "Alt+F3");
    standard_.zoom = declare_standard(kZoom, "&Zoom", "Window", "F5");
    standard_.next_window = declare_standard(kNextWindow, "&Next", "Window", "F6");
    standard_.previous_window =
        declare_standard(kPreviousWindow, "&Previous", "Window", "Shift+F6");
    standard_.tile = declare_standard(kTile, "&Tile", "Window", "");
    standard_.cascade = declare_standard(kCascade, "C&ascade", "Window", "");
    standard_.window_list = declare_standard(kWindowList, "&Window List", "Window", "");
    standard_.focus_next = declare_standard(kFocusNext, "Next Field", "Window", "Tab");
    standard_.focus_previous =
        declare_standard(kFocusPrevious, "Previous Field", "Window", "Shift+Tab");
    // Declared after the original set so those commands keep the ids they
    // have always been assigned in declaration order.
    standard_.terminal_report =
        declare_standard(kTerminalReport, "&Terminal Report", "System", "");
    // Mnemonics on the distinguishing word rather than on "Tile": a Window
    // menu that carries all of these would otherwise have four items
    // competing for 'T', and the axis (or "Grid") is the word the reader is
    // actually choosing between.
    standard_.tile_horizontally =
        declare_standard(kTileHorizontally, "Tile &Horizontally", "Window", "");
    standard_.tile_vertically =
        declare_standard(kTileVertically, "Tile &Vertically", "Window", "");
    standard_.tile_grid = declare_standard(kTileGrid, "Tile &Grid", "Window", "");
    // Also after the original set, and for the same reason. The mnemonic is
    // 'n' rather than 'M': a Window menu carrying this one carries "&Next"
    // too, and two items competing for a letter is a menu where one of them
    // cannot be typed.
    standard_.minimize = declare_standard(kMinimize, "Mi&nimize", "Window", "");
}

CommandId CommandRegistry::id_for_key(std::string_view key) {
    const auto existing = ids_.find(std::string(key));
    if (existing != ids_.end()) return existing->second;
    const CommandId id{next_command_id_++};
    ids_.emplace(std::string(key), id);
    return id;
}

CommandId CommandRegistry::declare(CommandDescriptor descriptor) {
    // A command with no key has no identity: it could never be found,
    // re-declared, withdrawn or persisted, and two of them would be
    // the same command by the rule above.
    CKV_ASSERT(!descriptor.key.empty());
    std::optional<KeyChord> chord;
    if (!descriptor.chord.empty()) {
        chord = KeyChord::parse(descriptor.chord);
        // A malformed chord literal in source is a programmer error to
        // fix, not a condition to silently degrade from.
        CKV_ASSERT(chord.has_value());
    }

    const CommandId id = id_for_key(descriptor.key);
    // A re-declaration replaces the definition, so the chord the
    // PREVIOUS definition asked for stops applying — unless the keymap
    // has since been rebound and that chord now belongs to something
    // else, which is a runtime decision this must not undo.
    const auto previous = commands_.find(id);
    if (previous != commands_.end() && previous->second.default_chord &&
        command_for_key(*previous->second.default_chord) == id)
        unbind_key(*previous->second.default_chord);

    commands_[id] = CommandInfo{id,
                                std::move(descriptor.key),
                                std::move(descriptor.title),
                                std::move(descriptor.category),
                                chord,
                                std::move(descriptor.context),
                                descriptor.visibility};
    if (chord) bind_key(*chord, id);
    if (descriptor.handler) set_handler(id, std::move(descriptor.handler));
    return id;
}

const CommandInfo* CommandRegistry::find(CommandId id) const noexcept {
    auto it = commands_.find(id);
    return it == commands_.end() ? nullptr : &it->second;
}

std::optional<CommandId> CommandRegistry::id_for(std::string_view key) const {
    const auto assigned = ids_.find(std::string(key));
    if (assigned == ids_.end() || !commands_.contains(assigned->second)) return std::nullopt;
    return assigned->second;
}

std::string_view CommandRegistry::key_for(CommandId id) const noexcept {
    const CommandInfo* info = find(id);
    return info == nullptr ? std::string_view{} : std::string_view{info->key};
}

std::vector<CommandInfo> CommandRegistry::all() const {
    std::vector<CommandInfo> result;
    result.reserve(commands_.size());
    for (const auto& [id, info] : commands_) result.push_back(info);
    // Ids are assigned in declaration order, so ordering by id is
    // ordering by when each command entered the registry.
    std::sort(result.begin(), result.end(),
              [](const CommandInfo& a, const CommandInfo& b) { return a.id < b.id; });
    return result;
}

void CommandRegistry::withdraw(CommandId id) {
    commands_.erase(id);
    enabled_predicates_.erase(id);
    handlers_.erase(id);
    keymap_.erase(std::remove_if(keymap_.begin(), keymap_.end(),
                                 [id](const auto& entry) { return entry.second == id; }),
                  keymap_.end());
}

void CommandRegistry::set_command_context(CommandId id, std::string context) {
    auto it = commands_.find(id);
    CKV_ASSERT(it != commands_.end());
    it->second.context = std::move(context);
}

CommandRegistry::ContextScopeId CommandRegistry::push_context(std::string context) {
    CKV_ASSERT(!context.empty());
    const ContextScopeId id = next_context_scope_id_++;
    active_contexts_.emplace_back(id, std::move(context));
    return id;
}

bool CommandRegistry::pop_context(ContextScopeId id) {
    if (active_contexts_.empty()) return false;
    if (active_contexts_.back().first != id) return false;
    active_contexts_.pop_back();
    return true;
}

bool CommandRegistry::context_active(std::string_view context) const noexcept {
    if (context.empty()) return true;
    for (const auto& [id, active] : active_contexts_) {
        (void)id;
        if (active == context) return true;
    }
    return false;
}

void CommandRegistry::set_visibility(CommandId id, CommandVisibility visibility) {
    auto it = commands_.find(id);
    CKV_ASSERT(it != commands_.end());
    it->second.visibility = visibility;
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

bool CommandRegistry::is_available(CommandId id,
                                   const std::vector<std::string>& focus_contexts) const {
    if (!is_enabled(id)) return false;
    const auto info = commands_.find(id);
    if (info == commands_.end() || info->second.context.empty()) return true;
    if (context_active(info->second.context)) return true;
    return std::find(focus_contexts.begin(), focus_contexts.end(), info->second.context) !=
           focus_contexts.end();
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

void CommandRegistry::set_chord_formatter(std::function<std::string(const KeyChord&)> formatter) {
    chord_formatter_ = std::move(formatter);
}

std::string CommandRegistry::format_chord(const KeyChord& chord) const {
    return chord_formatter_ ? chord_formatter_(chord) : format(chord);
}

void CommandRegistry::set_handler(CommandId id, std::function<void()> handler) {
    handlers_[id] = std::move(handler);
}

bool CommandRegistry::has_handler(CommandId id) const {
    auto it = handlers_.find(id);
    return it != handlers_.end() && static_cast<bool>(it->second);
}

bool CommandRegistry::execute(CommandId id, const std::vector<std::string>& focus_contexts) {
    auto it = handlers_.find(id);
    if (it == handlers_.end() || !it->second) return false;
    if (!is_available(id, focus_contexts)) return false;
    it->second();
    return true;
}

}  // namespace ckv::ui
