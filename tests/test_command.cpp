// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/command.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::ui::CommandDescriptor;
using ckv::ui::CommandRegistry;
using ckv::ui::kFirstApplicationCommandId;
using ckv::ui::kInvalidCommand;

namespace {
// Application-space ids (M9/WP-12: register_command()/add() now
// CKV_ASSERT that id is at or above kFirstApplicationCommandId).
constexpr ckv::ui::CommandId kFileOpen = kFirstApplicationCommandId;
constexpr ckv::ui::CommandId kFileSave = kFirstApplicationCommandId + 1;
constexpr ckv::ui::CommandId kAppCustom = kFirstApplicationCommandId;

KeyChord ctrl_o() { return KeyChord{Key::Char, Modifier::Ctrl, "o"}; }
KeyChord ctrl_s() { return KeyChord{Key::Char, Modifier::Ctrl, "s"}; }
}  // namespace

CK_TEST(reserved_and_application_command_ranges_are_disjoint_by_construction) {
    CK_CHECK(kFirstApplicationCommandId == 4096);
    CK_CHECK(kAppCustom >= kFirstApplicationCommandId);
}

CK_TEST(find_returns_null_for_a_never_registered_command) {
    CommandRegistry reg;
    CK_CHECK(reg.find(kFileOpen) == nullptr);
}

CK_TEST(register_command_makes_it_findable_with_the_supplied_metadata) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open", "File");
    const auto* info = reg.find(kFileOpen);
    CK_CHECK(info != nullptr);
    CK_CHECK(info->title == "Open");
    CK_CHECK(info->category == "File");
    CK_CHECK(!info->default_chord.has_value());
}

CK_TEST(re_registering_the_same_id_replaces_its_metadata) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open");
    reg.register_command(kFileOpen, "Open File...", "File");
    const auto* info = reg.find(kFileOpen);
    CK_CHECK(info->title == "Open File...");
    CK_CHECK(info->category == "File");
}

CK_TEST(registering_with_a_default_chord_also_binds_that_chord) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open", "File", ctrl_o());
    const auto bound = reg.command_for_key(ctrl_o());
    CK_CHECK(bound.has_value());
    CK_CHECK(*bound == kFileOpen);
}

// --- Enablement --------------------------------------------------------

CK_TEST(a_command_with_no_predicate_is_enabled_by_default) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open");
    CK_CHECK(reg.is_enabled(kFileOpen));
}

CK_TEST(a_registered_predicate_can_report_disabled) {
    CommandRegistry reg;
    reg.register_command(kFileSave, "Save");
    bool dirty = false;
    reg.set_enabled_predicate(kFileSave, [&dirty] { return dirty; });
    CK_CHECK(!reg.is_enabled(kFileSave));
    dirty = true;
    CK_CHECK(reg.is_enabled(kFileSave));  // re-evaluated each call, not cached
}

CK_TEST(is_enabled_for_an_unregistered_command_is_true_absent_a_predicate) {
    // No CKV_ASSERT contract is documented for is_enabled() on an
    // unknown id (unlike set_enabled_predicate, which requires prior
    // registration) — it degrades to "no predicate found" rather than
    // crashing a caller that merely queries before registering.
    CommandRegistry reg;
    CK_CHECK(reg.is_enabled(999));
}

CK_TEST(set_enabled_predicate_on_an_unregistered_command_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        reg.set_enabled_predicate(kFileOpen, [] { return true; });  // must abort: never registered
    });
}

// --- Keymap --------------------------------------------------------------

CK_TEST(command_for_key_returns_nullopt_for_an_unbound_chord) {
    CommandRegistry reg;
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(bind_key_then_command_for_key_round_trips) {
    CommandRegistry reg;
    reg.bind_key(ctrl_s(), kFileSave);
    const auto bound = reg.command_for_key(ctrl_s());
    CK_CHECK(bound.has_value() && *bound == kFileSave);
}

CK_TEST(rebinding_an_already_bound_chord_replaces_the_command_not_duplicates_it) {
    CommandRegistry reg;
    reg.bind_key(ctrl_o(), kFileOpen);
    reg.bind_key(ctrl_o(), kFileSave);  // same chord, different command
    const auto bound = reg.command_for_key(ctrl_o());
    CK_CHECK(bound.has_value() && *bound == kFileSave);
}

CK_TEST(unbind_key_removes_the_binding) {
    CommandRegistry reg;
    reg.bind_key(ctrl_o(), kFileOpen);
    reg.unbind_key(ctrl_o());
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(unbind_key_on_a_chord_that_was_never_bound_is_a_harmless_no_op) {
    CommandRegistry reg;
    reg.unbind_key(ctrl_o());  // must not crash or throw
    CK_CHECK(!reg.command_for_key(ctrl_o()).has_value());
}

CK_TEST(two_distinct_chords_can_bind_to_the_same_command) {
    CommandRegistry reg;
    reg.bind_key(ctrl_o(), kFileOpen);
    reg.bind_key(KeyChord{Key::F1, Modifier::None, ""}, kFileOpen);
    CK_CHECK(*reg.command_for_key(ctrl_o()) == kFileOpen);
    CK_CHECK(*reg.command_for_key(KeyChord{Key::F1, Modifier::None, ""}) == kFileOpen);
}

CK_TEST(chords_differing_only_by_modifier_are_distinct_bindings) {
    CommandRegistry reg;
    reg.bind_key(KeyChord{Key::Char, Modifier::None, "o"}, kFileOpen);
    reg.bind_key(KeyChord{Key::Char, Modifier::Ctrl, "o"}, kFileSave);
    CK_CHECK(*reg.command_for_key(KeyChord{Key::Char, Modifier::None, "o"}) == kFileOpen);
    CK_CHECK(*reg.command_for_key(KeyChord{Key::Char, Modifier::Ctrl, "o"}) == kFileSave);
}

// --- chord_for_command (the reverse lookup, M9/WP-11) ---------------------

CK_TEST(chord_for_command_returns_nullopt_when_nothing_is_bound) {
    CommandRegistry reg;
    CK_CHECK(!reg.chord_for_command(kFileOpen).has_value());
}

CK_TEST(chord_for_command_finds_the_chord_a_registered_default_bound) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open", "File", ctrl_o());
    const auto chord = reg.chord_for_command(kFileOpen);
    CK_CHECK(chord.has_value());
    CK_CHECK(*chord == ctrl_o());
}

CK_TEST(chord_for_command_reflects_a_runtime_rebind_not_just_the_original_default) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open", "File", ctrl_o());
    reg.unbind_key(ctrl_o());
    reg.bind_key(KeyChord{Key::F1, Modifier::None, ""}, kFileOpen);
    const auto chord = reg.chord_for_command(kFileOpen);
    CK_CHECK(chord.has_value());
    CK_CHECK(*chord == (KeyChord{Key::F1, Modifier::None, ""}));
}

CK_TEST(chord_for_command_returns_nullopt_after_its_only_chord_is_unbound) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open", "File", ctrl_o());
    reg.unbind_key(ctrl_o());
    CK_CHECK(!reg.chord_for_command(kFileOpen).has_value());
}

CK_TEST(registering_a_command_with_kinvalidcommand_id_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        reg.register_command(kInvalidCommand, "Bogus");  // must abort
    });
}

// --- Standard command set (M9/WP-12) ---------------------------------------

CK_TEST(register_command_below_the_reserved_range_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        reg.register_command(1, "Sneaks into the reserved range");  // must abort
    });
}

CK_TEST(add_below_the_reserved_range_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        // must abort
        reg.add(CommandDescriptor{.id = 1, .title = "Sneaks into the reserved range"});
    });
}

CK_TEST(register_command_at_exactly_the_first_application_id_is_allowed) {
    CommandRegistry reg;
    reg.register_command(kFirstApplicationCommandId, "Fine");
    CK_CHECK(reg.find(kFirstApplicationCommandId) != nullptr);
}

CK_TEST(register_standard_commands_populates_every_std_commands_id_with_a_title) {
    using namespace ckv::ui::std_commands;
    CommandRegistry reg;
    reg.register_standard_commands();
    const ckv::ui::CommandId ids[] = {
        kQuit,    kClose, kZoom,       kNextWindow, kPreviousWindow, kTile,
        kCascade, kMenu,  kWindowList, kHelp,       kFocusNext,      kFocusPrevious,
    };
    for (auto id : ids) {
        const auto* info = reg.find(id);
        CK_CHECK(info != nullptr);
        CK_CHECK(!info->title.empty());
    }
}

CK_TEST(register_standard_commands_binds_the_documented_default_chords) {
    using namespace ckv::ui::std_commands;
    CommandRegistry reg;
    reg.register_standard_commands();
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F1")) == kHelp);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F10")) == kMenu);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F6")) == kNextWindow);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Shift+F6")) == kPreviousWindow);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("F5")) == kZoom);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Alt+F3")) == kClose);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Alt+X")) == kQuit);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Tab")) == kFocusNext);
    CK_CHECK(reg.command_for_key(*KeyChord::parse("Shift+Tab")) == kFocusPrevious);
}

CK_TEST(register_standard_commands_leaves_tile_cascade_and_window_list_with_no_default_chord) {
    using namespace ckv::ui::std_commands;
    CommandRegistry reg;
    reg.register_standard_commands();
    CK_CHECK(!reg.find(kTile)->default_chord.has_value());
    CK_CHECK(!reg.find(kCascade)->default_chord.has_value());
    CK_CHECK(!reg.find(kWindowList)->default_chord.has_value());
}

// --- all() (M9/WP-12) -------------------------------------------------------

CK_TEST(all_is_empty_for_a_freshly_constructed_registry) {
    CommandRegistry reg;
    CK_CHECK(reg.all().empty());
}

CK_TEST(all_returns_every_registered_command_sorted_by_id) {
    CommandRegistry reg;
    reg.register_command(kFileSave, "Save");
    reg.register_command(kFileOpen, "Open");  // registered second, sorts first (lower id)
    const auto all = reg.all();
    CK_CHECK(all.size() == 2);
    CK_CHECK(all[0].id == kFileOpen);
    CK_CHECK(all[1].id == kFileSave);
}

// --- set_handler / execute (M9/WP-10, moved here from Application) --------

CK_TEST(execute_runs_the_handler_and_reports_it_ran) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open");
    bool ran = false;
    reg.set_handler(kFileOpen, [&] { ran = true; });
    CK_CHECK(reg.execute(kFileOpen));
    CK_CHECK(ran);
}

CK_TEST(execute_on_an_id_with_no_handler_is_a_harmless_no_op) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open");
    CK_CHECK(!reg.execute(kFileOpen));
}

CK_TEST(execute_on_a_disabled_command_does_not_run_its_handler) {
    CommandRegistry reg;
    reg.register_command(kFileOpen, "Open");
    reg.set_enabled_predicate(kFileOpen, [] { return false; });
    bool ran = false;
    reg.set_handler(kFileOpen, [&] { ran = true; });
    CK_CHECK(!reg.execute(kFileOpen));
    CK_CHECK(!ran);
}

CK_TEST(a_handler_may_be_attached_before_the_command_is_ever_registered) {
    CommandRegistry reg;
    bool ran = false;
    reg.set_handler(kFileOpen, [&] { ran = true; });
    reg.register_command(kFileOpen, "Open");
    CK_CHECK(reg.execute(kFileOpen));
    CK_CHECK(ran);
}

// --- has_handler (M9/WP-13, the guard a default-handler installer
// like MenuBar checks before installing itself) ----------------------

CK_TEST(has_handler_is_false_for_an_id_with_no_handler_installed) {
    CommandRegistry reg;
    CK_CHECK(!reg.has_handler(kFileOpen));
}

CK_TEST(has_handler_is_true_once_a_real_handler_is_set) {
    CommandRegistry reg;
    reg.set_handler(kFileOpen, [] {});
    CK_CHECK(reg.has_handler(kFileOpen));
}

CK_TEST(has_handler_is_false_after_the_handler_is_cleared_with_an_empty_function) {
    CommandRegistry reg;
    reg.set_handler(kFileOpen, [] {});
    reg.set_handler(kFileOpen, nullptr);
    CK_CHECK(!reg.has_handler(kFileOpen));
}

// --- add() (M9/WP-10 one-call registration) --------------------------------

CK_TEST(add_registers_metadata_and_installs_the_handler_from_one_descriptor) {
    CommandRegistry reg;
    bool ran = false;
    reg.add(CommandDescriptor{
        .id = kFileOpen, .title = "&Open...", .category = "File", .handler = [&] { ran = true; }});
    const auto* info = reg.find(kFileOpen);
    CK_CHECK(info != nullptr);
    CK_CHECK(info->title == "&Open...");
    CK_CHECK(info->category == "File");
    CK_CHECK(reg.execute(kFileOpen));
    CK_CHECK(ran);
}

CK_TEST(add_parses_a_nonempty_chord_string_and_binds_it) {
    CommandRegistry reg;
    reg.add(CommandDescriptor{.id = kFileOpen, .title = "Open", .chord = "Ctrl+O"});
    const auto* info = reg.find(kFileOpen);
    CK_CHECK(info->default_chord.has_value());
    CK_CHECK(*info->default_chord == ctrl_o());
    CK_CHECK(reg.command_for_key(ctrl_o()) == kFileOpen);
}

CK_TEST(add_with_an_empty_chord_string_registers_no_default_chord) {
    CommandRegistry reg;
    reg.add(CommandDescriptor{.id = kFileOpen, .title = "Open"});
    CK_CHECK(!reg.find(kFileOpen)->default_chord.has_value());
}

CK_TEST(add_with_no_handler_leaves_execute_a_no_op) {
    CommandRegistry reg;
    reg.add(CommandDescriptor{.id = kFileOpen, .title = "Open"});
    CK_CHECK(!reg.execute(kFileOpen));
}

CK_TEST(add_with_an_unparseable_chord_string_aborts) {
    CK_EXPECT_ABORT({
        CommandRegistry reg;
        // must abort
        reg.add(CommandDescriptor{.id = kFileOpen, .title = "Open", .chord = "NotAChord"});
    });
}
