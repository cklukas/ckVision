// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/command.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

void CommandRegistry::register_command(CommandId id, std::string title, std::string category,
                                        std::optional<KeyChord> default_chord,
                                        std::string context) {
    // The reserved range is the library's own, with one deliberate opening:
    // the menu-accelerator ids a MenuBar claims for its Alt+<mnemonic>
    // chords. Those are library-owned too — a widget registering them is
    // the framework populating its own range, not an application reaching
    // into it — but they are allocated per menu at runtime rather than
    // fixed at startup, so they cannot go through
    // register_standard_commands().
    CKV_ASSERT(id >= kFirstApplicationCommandId ||
               (id >= std_commands::kFirstMenuAccelerator &&
                id < std_commands::kMenuAcceleratorLimit));
    register_command_internal(id, std::move(title), std::move(category), default_chord,
                              std::move(context));
}

void CommandRegistry::register_command_internal(CommandId id, std::string title,
                                                  std::string category,
                                                  std::optional<KeyChord> default_chord,
                                                  std::string context) {
    CKV_ASSERT(id != kInvalidCommand);
    CommandInfo info{id, std::move(title), std::move(category), default_chord, std::move(context)};
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
    register_command_internal(kHelp, "Help", "System", KeyChord::parse("F1"), {});
    register_command_internal(kMenu, "Menu", "Window", KeyChord::parse("F10"), {});
    register_command_internal(kNextWindow, "&Next", "Window", KeyChord::parse("F6"), {});
    register_command_internal(kPreviousWindow, "&Previous", "Window", KeyChord::parse("Shift+F6"),
                              {});
    register_command_internal(kZoom, "&Zoom", "Window", KeyChord::parse("F5"), {});
    register_command_internal(kClose, "&Close", "Window", KeyChord::parse("Alt+F3"), {});
    register_command_internal(kQuit, "&Quit", "System", KeyChord::parse("Alt+X"), {});
    register_command_internal(kTile, "&Tile", "Window", std::nullopt, {});
    register_command_internal(kCascade, "C&ascade", "Window", std::nullopt, {});
    register_command_internal(kWindowList, "&Window List", "Window", std::nullopt, {});
    register_command_internal(kFocusNext, "Next Field", "Window", KeyChord::parse("Tab"), {});
    register_command_internal(kFocusPrevious, "Previous Field", "Window",
                               KeyChord::parse("Shift+Tab"), {});
}

void CommandRegistry::add(CommandDescriptor descriptor) {
    std::optional<KeyChord> chord;
    if (!descriptor.chord.empty()) {
        chord = KeyChord::parse(descriptor.chord);
        CKV_ASSERT(chord.has_value());
    }
    const CommandId id = descriptor.id;
    register_command(id, std::move(descriptor.title), std::move(descriptor.category), chord,
                     std::move(descriptor.context));
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

void CommandRegistry::retract_command(CommandId id) {
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
