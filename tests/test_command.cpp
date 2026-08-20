// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/command.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::ui::CommandDescriptor;
using ckv::ui::CommandId;
using ckv::ui::CommandRegistry;
using ckv::ui::CommandVisibility;
using ckv::ui::kInvalidCommand;

namespace {
// Nothing in these tests writes a command id. Every id comes back from
// declare(), which is the whole point of the mechanism: the only thing
// a caller chooses is the key.
constexpr std::string_view kFileOpenKey = "test.file.open";
constexpr std::string_view kFileSaveKey = "test.file.save";

CommandId declare_open(CommandRegistry& reg, std::string title = "Open",
                       std::string category = {}, std::string chord = {}) {
    return reg.declare(CommandDescriptor{.key = std::string(kFileOpenKey),
                                         .title = std::move(title),
                                         .category = std::move(category),
                                         .chord = std::move(chord)});
}

CommandId declare_save(CommandRegistry& reg, std::string title = "Save") {
    return reg.declare(
        CommandDescriptor{.key = std::string(kFileSaveKey), .title = std::move(title)});
}

// An id the registry once assigned and no longer knows — the only
// honest way to obtain one, now that ids cannot be invented.
CommandId withdrawn_command(CommandRegistry& reg) {
    const CommandId id = reg.declare(CommandDescriptor{.key = "test.gone", .title = "Gone"});
    reg.withdraw(id);
    return id;
}

KeyChord ctrl_o() { return KeyChord{Key::Char, Modifier::Ctrl, "o"}; }
KeyChord ctrl_s() { return KeyChord{Key::Char, Modifier::Ctrl, "s"}; }
} // namespace

// --- Identity: the key names the command, the registry names the id ------

CK_TEST(find_returns_null_for_a_never_declared_command) {
    CommandRegistry reg;
    CK_CHECK(reg.find(withdrawn_command(reg)) == nullptr);
}

CK_TEST(declare_makes_a_command_findable_with_the_supplied_metadata) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File");
    const auto* info = reg.find(open);
    CK_CHECK(info != nullptr);
    CK_CHECK(info->key == kFileOpenKey);
    CK_CHECK(info->title == "Open");
    CK_CHECK(info->category == "File");
    CK_CHECK(!info->default_chord.has_value());
}

CK_TEST(declare_never_assigns_the_invalid_command_id) {
    CommandRegistry reg;
    CK_CHECK(declare_open(reg) != kInvalidCommand);
}

CK_TEST(two_different_keys_receive_two_different_ids) {
    CommandRegistry reg;
    CK_CHECK(declare_open(reg) != declare_save(reg));
}

CK_TEST(declaring_the_same_key_twice_returns_the_same_id_and_replaces_the_metadata) {
    CommandRegistry reg;
    const CommandId first = declare_open(reg, "Open");
    const CommandId second = declare_open(reg, "Open File...", "File");
    CK_CHECK(first == second);
    const auto* info = reg.find(first);
    CK_CHECK(info->title == "Open File...");
    CK_CHECK(info->category == "File");
}

CK_TEST(a_key_keeps_its_id_across_withdraw_and_redeclaration) {
    // What lets a surface that rebuilds its commands — a MenuBar's menu
    // accelerators — hand out ids that can never come to mean something
    // else.
    CommandRegistry reg;
    const CommandId first = declare_open(reg);
    reg.withdraw(first);
    reg.declare(CommandDescriptor{.key = "test.other", .title = "Other"});
    CK_CHECK(declare_open(reg) == first);
}

CK_TEST(a_withdrawn_id_is_never_handed_to_another_key) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    reg.withdraw(open);
    CK_CHECK(declare_save(reg) != open);
}

CK_TEST(declaring_with_an_empty_key_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        reg.declare(CommandDescriptor{.key = "", .title = "Anonymous"}); // must abort
    });
}

CK_TEST(id_for_and_key_for_round_trip_a_declared_command) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    CK_CHECK(reg.id_for(kFileOpenKey) == open);
    CK_CHECK(reg.key_for(open) == kFileOpenKey);
}

CK_TEST(id_for_is_nullopt_for_an_unknown_or_withdrawn_key) {
    CommandRegistry reg;
    CK_CHECK(!reg.id_for("test.never.declared").has_value());
    reg.withdraw(declare_open(reg));
    CK_CHECK(!reg.id_for(kFileOpenKey).has_value());
}

CK_TEST(key_for_is_empty_for_an_unknown_id) {
    CommandRegistry reg;
    CK_CHECK(reg.key_for(kInvalidCommand).empty());
    CK_CHECK(reg.key_for(withdrawn_command(reg)).empty());
}

// --- Declaring a chord ----------------------------------------------------

CK_TEST(declaring_with_a_default_chord_also_binds_that_chord) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File", "Ctrl+O");
    const auto bound = reg.command_for_key(ctrl_o());
    CK_CHECK(bound.has_value());
    CK_CHECK(*bound == open);
}

CK_TEST(declaring_an_empty_chord_string_declares_no_default_chord) {
    CommandRegistry reg;
    CK_CHECK(!reg.find(declare_open(reg))->default_chord.has_value());
}

CK_TEST(declaring_an_unparseable_chord_string_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        // must abort
        reg.declare(CommandDescriptor{
            .key = std::string(kFileOpenKey), .title = "Open", .chord = "NotAChord"});
    });
}

CK_TEST(redeclaring_drops_the_chord_the_previous_declaration_asked_for) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File", "Ctrl+O");
    declare_open(reg, "Open", "File", "Ctrl+S");
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
    CK_CHECK(reg.command_for_key(ctrl_s()) == open);
}

CK_TEST(redeclaring_leaves_a_chord_something_else_has_since_claimed) {
    CommandRegistry reg;
    declare_open(reg, "Open", "File", "Ctrl+O");
    const CommandId save = declare_save(reg);
    reg.bind_key(ctrl_o(), save); // a runtime rebind, not the declaration's doing
    declare_open(reg, "Open", "File");
    CK_CHECK(reg.command_for_key(ctrl_o()) == save);
}

// --- Handlers across a re-declaration -------------------------------------

CK_TEST(declaring_installs_the_descriptor_handler) {
    CommandRegistry reg;
    bool ran = false;
    const CommandId open = reg.declare(CommandDescriptor{.key = std::string(kFileOpenKey),
                                                         .title = "&Open...",
                                                         .category = "File",
                                                         .handler = [&] { ran = true; }});
    const auto* info = reg.find(open);
    CK_CHECK(info != nullptr);
    CK_CHECK(info->title == "&Open...");
    CK_CHECK(info->category == "File");
    CK_CHECK(reg.execute(open));
    CK_CHECK(ran);
}

CK_TEST(redeclaring_without_a_handler_keeps_the_one_already_installed) {
    // Handlers are routinely attached separately from the metadata — by
    // set_handler, or by the framework's own default installers — so a
    // re-declaration that says nothing about behavior must not unhook it.
    CommandRegistry reg;
    bool ran = false;
    const CommandId open = declare_open(reg);
    reg.set_handler(open, [&] { ran = true; });
    declare_open(reg, "Open File...");
    CK_CHECK(reg.execute(open));
    CK_CHECK(ran);
}

CK_TEST(redeclaring_with_a_handler_replaces_the_one_already_installed) {
    CommandRegistry reg;
    int first_ran = 0;
    int second_ran = 0;
    const CommandId open = reg.declare(CommandDescriptor{
        .key = std::string(kFileOpenKey), .title = "Open", .handler = [&] { ++first_ran; }});
    reg.declare(CommandDescriptor{
        .key = std::string(kFileOpenKey), .title = "Open", .handler = [&] { ++second_ran; }});
    CK_CHECK(reg.execute(open));
    CK_CHECK(first_ran == 0);
    CK_CHECK(second_ran == 1);
}

// --- Visibility is metadata, not arithmetic -------------------------------

CK_TEST(a_declared_command_is_palette_visible_by_default) {
    CommandRegistry reg;
    CK_CHECK(reg.find(declare_open(reg))->visibility == CommandVisibility::Palette);
}

CK_TEST(a_command_may_declare_itself_hidden) {
    CommandRegistry reg;
    const CommandId open = reg.declare(CommandDescriptor{.key = std::string(kFileOpenKey),
                                                         .title = "Open",
                                                         .visibility = CommandVisibility::Hidden});
    CK_CHECK(reg.find(open)->visibility == CommandVisibility::Hidden);
}

CK_TEST(the_standard_set_is_hidden_and_can_be_opted_into_the_palette) {
    CommandRegistry reg;
    CK_CHECK(reg.find(reg.standard().quit)->visibility == CommandVisibility::Hidden);
    reg.set_visibility(reg.standard().quit, CommandVisibility::Palette);
    CK_CHECK(reg.find(reg.standard().quit)->visibility == CommandVisibility::Palette);
}

CK_TEST(set_visibility_on_an_undeclared_command_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        // must abort
        reg.set_visibility(withdrawn_command(reg), CommandVisibility::Hidden);
    });
}

// --- Withdrawal -----------------------------------------------------------

CK_TEST(withdraw_removes_metadata_handler_predicate_and_bindings_together) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File", "Ctrl+O");
    reg.set_enabled_predicate(open, [] { return false; });
    reg.set_handler(open, [] {});
    reg.withdraw(open);
    CK_CHECK(reg.find(open) == nullptr);
    CK_CHECK(!reg.has_handler(open));
    CK_CHECK(reg.is_enabled(open)); // no predicate left to say otherwise
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
    CK_CHECK(!reg.chord_for_command(open).has_value());
}

CK_TEST(withdrawing_a_command_removes_it_from_all) {
    CommandRegistry reg;
    const std::size_t before = reg.all().size();
    const CommandId open = declare_open(reg);
    CK_CHECK(reg.all().size() == before + 1);
    reg.withdraw(open);
    CK_CHECK(reg.all().size() == before);
}

// --- Enablement --------------------------------------------------------

CK_TEST(a_command_with_no_predicate_is_enabled_by_default) {
    CommandRegistry reg;
    CK_CHECK(reg.is_enabled(declare_open(reg)));
}

CK_TEST(a_registered_predicate_can_report_disabled) {
    CommandRegistry reg;
    const CommandId save = declare_save(reg);
    bool dirty = false;
    reg.set_enabled_predicate(save, [&dirty] { return dirty; });
    CK_CHECK(!reg.is_enabled(save));
    dirty = true;
    CK_CHECK(reg.is_enabled(save)); // re-evaluated each call, not cached
}

CK_TEST(is_enabled_for_an_undeclared_command_is_true_absent_a_predicate) {
    // No CKV_ASSERT contract is documented for is_enabled() on an
    // unknown id (unlike set_enabled_predicate, which requires prior
    // declaration) — it degrades to "no predicate found" rather than
    // crashing a caller that merely queries.
    CommandRegistry reg;
    CK_CHECK(reg.is_enabled(withdrawn_command(reg)));
}

CK_TEST(set_enabled_predicate_on_an_undeclared_command_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        // must abort: never declared
        reg.set_enabled_predicate(withdrawn_command(reg), [] { return true; });
    });
}

// --- Keymap --------------------------------------------------------------

CK_TEST(command_for_key_returns_nullopt_for_an_unbound_chord) {
    CommandRegistry reg;
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(bind_key_then_command_for_key_round_trips) {
    CommandRegistry reg;
    const CommandId save = declare_save(reg);
    reg.bind_key(ctrl_s(), save);
    const auto bound = reg.command_for_key(ctrl_s());
    CK_CHECK(bound.has_value() && *bound == save);
}

CK_TEST(rebinding_an_already_bound_chord_replaces_the_command_not_duplicates_it) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    const CommandId save = declare_save(reg);
    reg.bind_key(ctrl_o(), open);
    reg.bind_key(ctrl_o(), save); // same chord, different command
    const auto bound = reg.command_for_key(ctrl_o());
    CK_CHECK(bound.has_value() && *bound == save);
}

CK_TEST(unbind_key_removes_the_binding) {
    CommandRegistry reg;
    reg.bind_key(ctrl_o(), declare_open(reg));
    reg.unbind_key(ctrl_o());
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(unbind_key_on_a_chord_that_was_never_bound_is_a_harmless_no_op) {
    CommandRegistry reg;
    reg.unbind_key(ctrl_o()); // must not crash or throw
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(two_distinct_chords_can_bind_to_the_same_command) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    const KeyChord f2{Key::F2, Modifier::None, ""};
    reg.bind_key(ctrl_o(), open);
    reg.bind_key(f2, open);
    CK_CHECK(*reg.command_for_key(ctrl_o()) == open);
    CK_CHECK(*reg.command_for_key(f2) == open);
}

CK_TEST(chords_differing_only_by_modifier_are_distinct_bindings) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    const CommandId save = declare_save(reg);
    reg.bind_key(KeyChord{Key::Char, Modifier::None, "o"}, open);
    reg.bind_key(KeyChord{Key::Char, Modifier::Ctrl, "o"}, save);
    CK_CHECK(*reg.command_for_key(KeyChord{Key::Char, Modifier::None, "o"}) == open);
    CK_CHECK(*reg.command_for_key(KeyChord{Key::Char, Modifier::Ctrl, "o"}) == save);
}

// --- chord_for_command (the reverse lookup, M9/WP-11) ---------------------

CK_TEST(chord_for_command_returns_nullopt_when_nothing_is_bound) {
    CommandRegistry reg;
    CK_CHECK(!reg.chord_for_command(declare_open(reg)).has_value());
}

CK_TEST(chord_for_command_finds_the_chord_a_declared_default_bound) {
    CommandRegistry reg;
    const auto chord = reg.chord_for_command(declare_open(reg, "Open", "File", "Ctrl+O"));
    CK_CHECK(chord.has_value());
    CK_CHECK(*chord == ctrl_o());
}

CK_TEST(chord_for_command_reflects_a_runtime_rebind_not_just_the_original_default) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File", "Ctrl+O");
    const KeyChord f2{Key::F2, Modifier::None, ""};
    reg.unbind_key(ctrl_o());
    reg.bind_key(f2, open);
    const auto chord = reg.chord_for_command(open);
    CK_CHECK(chord.has_value());
    CK_CHECK(*chord == f2);
}

CK_TEST(chord_for_command_returns_nullopt_after_its_only_chord_is_unbound) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg, "Open", "File", "Ctrl+O");
    reg.unbind_key(ctrl_o());
    CK_CHECK(!reg.chord_for_command(open).has_value());
}

// --- The standard set (M9/WP-12) -------------------------------------------

CK_TEST(every_standard_command_is_declared_with_a_title_by_the_constructor) {
    CommandRegistry reg;
    const ckv::ui::StandardCommands& std_cmd = reg.standard();
    const CommandId ids[] = {
        std_cmd.quit,       std_cmd.close,   std_cmd.zoom,        std_cmd.next_window,
        std_cmd.previous_window, std_cmd.tile, std_cmd.cascade,   std_cmd.menu,
        std_cmd.window_list, std_cmd.help,   std_cmd.focus_next,  std_cmd.focus_previous,
        std_cmd.tile_horizontally, std_cmd.tile_vertically, std_cmd.tile_grid,
    };
    for (auto id : ids) {
        const auto* info = reg.find(id);
        CK_CHECK(info != nullptr);
        CK_CHECK(!info->title.empty());
        CK_CHECK(!info->key.empty());
    }
}

CK_TEST(the_standard_set_is_reachable_by_its_documented_keys) {
    CommandRegistry reg;
    namespace keys = ckv::ui::std_command_keys;
    CK_CHECK(reg.id_for(keys::kQuit) == reg.standard().quit);
    CK_CHECK(reg.id_for(keys::kHelp) == reg.standard().help);
    CK_CHECK(reg.id_for(keys::kMenu) == reg.standard().menu);
    CK_CHECK(reg.id_for(keys::kClose) == reg.standard().close);
    CK_CHECK(reg.id_for(keys::kZoom) == reg.standard().zoom);
    CK_CHECK(reg.id_for(keys::kNextWindow) == reg.standard().next_window);
    CK_CHECK(reg.id_for(keys::kPreviousWindow) == reg.standard().previous_window);
    CK_CHECK(reg.id_for(keys::kTile) == reg.standard().tile);
    CK_CHECK(reg.id_for(keys::kTileHorizontally) == reg.standard().tile_horizontally);
    CK_CHECK(reg.id_for(keys::kTileVertically) == reg.standard().tile_vertically);
    CK_CHECK(reg.id_for(keys::kTileGrid) == reg.standard().tile_grid);
    CK_CHECK(reg.id_for(keys::kCascade) == reg.standard().cascade);
    CK_CHECK(reg.id_for(keys::kWindowList) == reg.standard().window_list);
    CK_CHECK(reg.id_for(keys::kFocusNext) == reg.standard().focus_next);
    CK_CHECK(reg.id_for(keys::kFocusPrevious) == reg.standard().focus_previous);
}

CK_TEST(the_standard_set_binds_the_documented_default_chords) {
    CommandRegistry reg;
    const ckv::ui::StandardCommands& std_cmd = reg.standard();
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F1")) == std_cmd.help);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F10")) == std_cmd.menu);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F6")) == std_cmd.next_window);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Shift+F6")) == std_cmd.previous_window);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F5")) == std_cmd.zoom);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Alt+F3")) == std_cmd.close);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Alt+X")) == std_cmd.quit);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Tab")) == std_cmd.focus_next);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Shift+Tab")) == std_cmd.focus_previous);
}

CK_TEST(the_standard_set_leaves_tile_cascade_and_window_list_with_no_default_chord) {
    CommandRegistry reg;
    CK_CHECK(!reg.find(reg.standard().tile)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().cascade)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().window_list)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().terminal_report)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().tile_horizontally)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().tile_vertically)->default_chord.has_value());
    CK_CHECK(!reg.find(reg.standard().tile_grid)->default_chord.has_value());
}

CK_TEST(two_registries_assign_the_same_ids_to_the_same_declaration_order) {
    // Ids are per-registry handles, not global constants: what makes two
    // registries agree is the sequence of keys declared into them, and
    // nothing else may be assumed about the numbers themselves.
    CommandRegistry first;
    CommandRegistry second;
    CK_CHECK(declare_open(first) == declare_open(second));
    CK_CHECK(first.standard().quit == second.standard().quit);
}

// --- all() (M9/WP-12) -------------------------------------------------------

CK_TEST(all_holds_exactly_the_standard_set_for_a_freshly_constructed_registry) {
    CommandRegistry reg;
    const auto all = reg.all();
    CK_CHECK(all.size() == 17);
    CK_CHECK(all.front().id == reg.standard().quit);
    // A new standard command is declared at the END of the constructor, so
    // every command declared before it keeps the id it has always been
    // assigned — which is what lets two registries built the same way still
    // agree, and what keeps a persisted key resolving to the same command.
    // kMinimize (U4-j) is the most recent one to arrive that way.
    CK_CHECK(all.back().id == reg.standard().minimize);
    CK_CHECK(all[all.size() - 2U].id == reg.standard().tile_grid);
}

CK_TEST(all_returns_every_declared_command_in_declaration_order) {
    CommandRegistry reg;
    const std::size_t standard_count = reg.all().size();
    const CommandId save = declare_save(reg);
    const CommandId open = declare_open(reg);
    const auto all = reg.all();
    CK_CHECK(all.size() == standard_count + 2);
    CK_CHECK(all[standard_count].id == save);
    CK_CHECK(all[standard_count + 1].id == open);
}

// --- set_handler / execute (M9/WP-10, moved here from Application) --------

CK_TEST(execute_runs_the_handler_and_reports_it_ran) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    bool ran = false;
    reg.set_handler(open, [&] { ran = true; });
    CK_CHECK(reg.execute(open));
    CK_CHECK(ran);
}

CK_TEST(execute_on_an_id_with_no_handler_is_a_harmless_no_op) {
    CommandRegistry reg;
    CK_CHECK(!reg.execute(declare_open(reg)));
}

CK_TEST(execute_on_a_disabled_command_does_not_run_its_handler) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    reg.set_enabled_predicate(open, [] { return false; });
    bool ran = false;
    reg.set_handler(open, [&] { ran = true; });
    CK_CHECK(!reg.execute(open));
    CK_CHECK(!ran);
}

CK_TEST(a_handler_may_be_attached_before_the_command_is_ever_declared) {
    CommandRegistry reg;
    bool ran = false;
    // The id has to come from somewhere: declare, withdraw, and attach a
    // handler to the id the key keeps, then declare it again.
    const CommandId open = declare_open(reg);
    reg.withdraw(open);
    reg.set_handler(open, [&] { ran = true; });
    CK_CHECK(declare_open(reg) == open);
    CK_CHECK(reg.execute(open));
    CK_CHECK(ran);
}

// --- has_handler (M9/WP-13, the guard a default-handler installer
// like MenuBar checks before installing itself) ----------------------

CK_TEST(has_handler_is_false_for_an_id_with_no_handler_installed) {
    CommandRegistry reg;
    CK_CHECK(!reg.has_handler(declare_open(reg)));
}

CK_TEST(has_handler_is_true_once_a_real_handler_is_set) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    reg.set_handler(open, [] {});
    CK_CHECK(reg.has_handler(open));
}

CK_TEST(has_handler_is_false_after_the_handler_is_cleared_with_an_empty_function) {
    CommandRegistry reg;
    const CommandId open = declare_open(reg);
    reg.set_handler(open, [] {});
    reg.set_handler(open, nullptr);
    CK_CHECK(!reg.has_handler(open));
}
