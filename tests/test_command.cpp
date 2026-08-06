// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/command.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include "cktest.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::ui::CommandRegistry;
using ckv::ui::kFirstApplicationCommandId;
using ckv::ui::kInvalidCommand;

namespace {
constexpr ckv::ui::CommandId kFileOpen = 1;
constexpr ckv::ui::CommandId kFileSave = 2;
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
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        CommandRegistry reg;
        reg.set_enabled_predicate(kFileOpen, [] { return true; });  // must abort: never registered
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
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

CK_TEST(registering_a_command_with_kinvalidcommand_id_aborts) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        CommandRegistry reg;
        reg.register_command(kInvalidCommand, "Bogus");  // must abort
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}
