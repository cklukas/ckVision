// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "completion_test_fixture.hpp"
#include "cvision/core/text.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

#include <fstream>
#include <sstream>

namespace {
using namespace ckv;
using namespace ckv::widgets;

KeyEvent key(Key value, Modifier modifiers = Modifier::None) {
    return KeyEvent{KeyChord{value, modifiers, ""}};
}

using Fixture = ckv::testing::CompletionFixture;

class DelayedProvider final : public CompletionProvider {
public:
    CompletionRequest query;
    CompletionSnapshot result;
    void request(const CompletionRequest& next) override {
        query = next;
        result = CompletionSnapshot{next.revision, CompletionState::Pending, {}};
    }
    const CompletionSnapshot& snapshot() const noexcept override { return result; }
    void publish(std::uint64_t revision, std::string text) {
        result = CompletionSnapshot{revision, CompletionState::Ready,
                                    {CompletionCandidate{1, 0, query.cursor, std::move(text)}}};
    }
};
}  // namespace

CK_TEST(completion_preview_is_uncommitted_and_right_accepts_one_undoable_edit) {
    Fixture f;
    f.field->set_completions({"report.txt", "report.csv"});
    f.field->set_text("rep");
    int edits = 0;
    f.field->on_edited = [&] { ++edits; };
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(f.field->on_key(key(Key::Right)));
    CK_CHECK(f.field->text() == "report.csv");
    CK_CHECK(edits == 1);
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == "rep");
    CK_CHECK(edits == 2);
}

CK_TEST(completion_never_consumes_tab_or_modified_caret_movement) {
    Fixture f;
    f.field->set_completions({"report.txt"});
    f.field->set_text("rep");
    CK_CHECK(!f.field->on_key(key(Key::Tab)));
    CK_CHECK(!f.field->on_key(key(Key::Tab, Modifier::Shift)));
    CK_CHECK(f.field->on_key(key(Key::Right, Modifier::Ctrl)));
    CK_CHECK(f.field->text() == "rep");
    f.field->set_cursor(1);
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(f.field->on_key(key(Key::Right)));
    CK_CHECK(f.field->cursor() == 2);
    CK_CHECK(f.field->text() == "rep");
}

CK_TEST(completion_tab_reaches_the_next_control_through_application_routing) {
    Fixture f;
    auto owned = std::make_unique<Button>("Next");
    auto* next = owned.get();
    next->set_bounds(Rect{1, 3, 10, 1});
    f.app.root().add_child(std::move(owned));
    f.field->set_completions({"report.txt"});
    f.field->set_text("rep");
    f.app.dispatch(key(Key::Tab));
    CK_CHECK(f.app.focused() == next);
    CK_CHECK(f.field->text() == "rep");
}

CK_TEST(completion_escape_suppresses_late_results_until_the_query_changes) {
    Fixture f;
    auto source = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(source);
    f.field->set_text("rep");
    source->publish(source->query.revision, "report.txt");
    f.field->completion_changed();
    CK_CHECK(f.field->has_completion_preview());
    CK_CHECK(f.field->on_key(key(Key::Escape)));
    f.field->completion_changed();
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(!f.field->on_key(key(Key::Escape)));
    f.field->set_text("repo");
    source->publish(source->query.revision, "report.txt");
    f.field->completion_changed();
    CK_CHECK(f.field->has_completion_preview());
}

CK_TEST(completion_rejects_stale_provider_results_and_clears_old_preview_immediately) {
    Fixture f;
    auto source = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(source);
    f.field->set_text("rep");
    const auto old_revision = source->query.revision;
    source->publish(old_revision, "report.txt");
    f.field->completion_changed();
    CK_CHECK(f.field->has_completion_preview());
    f.field->set_text("notes");
    CK_CHECK(!f.field->has_completion_preview());
    source->publish(old_revision, "report.txt");
    f.field->completion_changed();
    CK_CHECK(f.field->completion_candidate() == nullptr);
    CK_CHECK(f.field->text() == "notes");
}

CK_TEST(completion_admission_is_atomic_and_validation_observes_only_accepted_text) {
    Fixture f;
    f.field->set_completions({"report.txt"});
    f.field->set_text("rep");
    std::string validated;
    f.field->set_validator([&](const std::string& text) { validated = text; return text.ends_with(".txt"); });
    CK_CHECK(validated == "rep");
    f.field->set_grapheme_filter([](std::string_view text) { return text != "."; });
    CK_CHECK(!f.field->accept_completion());
    CK_CHECK(f.field->text() == "rep");
    f.field->set_grapheme_filter({});
    CK_CHECK(f.field->accept_completion());
    CK_CHECK(validated == "report.txt");
    CK_CHECK(f.field->valid());
}

CK_TEST(completion_is_disabled_for_password_mask_selection_and_key_release) {
    Fixture f;
    f.field->set_completions({"report.txt"});
    f.field->set_text("rep");
    f.field->set_password_echo(true);
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(!f.field->accept_completion());
    f.field->set_password_echo(false);
    f.field->set_mask("AAA");
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(!f.field->accept_completion());
    f.field->set_mask("");
    f.field->set_text("rep");
    f.field->on_key(key(Key::Left, Modifier::Shift));
    CK_CHECK(!f.field->has_completion_preview());
    f.field->set_cursor(3);
    auto released = key(Key::Right);
    released.action = KeyAction::Release;
    CK_CHECK(!f.field->on_key(released));
    CK_CHECK(f.field->text() == "rep");
}

CK_TEST(completion_unicode_suffixes_respect_grapheme_boundaries_and_undo) {
    Fixture f;
    const std::string name = "caf\xC3\xA9-\xE7\x8C\xAB.txt";
    f.field->set_completions({name});
    f.field->set_text("caf\xC3\xA9");
    CK_CHECK(f.field->has_completion_preview());
    f.field->on_key(key(Key::Right));
    CK_CHECK(f.field->text() == name);
    CK_CHECK(f.field->cursor() == text::split_graphemes(name).size());
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == "caf\xC3\xA9");
    f.field->set_completions({"e\xCC\x81.txt"});
    f.field->set_text("e");
    CK_CHECK(!f.field->has_completion_preview());
}

CK_TEST(completion_insertion_preview_cannot_change_the_preceding_committed_grapheme) {
    Fixture f;
    auto source = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(source); f.field->set_text("e");
    source->result = {source->query.revision, CompletionState::Ready,
                      {{1, 1, 1, "\xCC\x81x"}}};
    f.field->completion_changed();
    CK_CHECK(!f.field->has_completion_preview());
    f.field->on_key(key(Key::Right));
    CK_CHECK(f.field->text() == "e");
    CK_CHECK(f.field->accept_completion());
    CK_CHECK(f.field->text() == "e\xCC\x81x");
    CK_CHECK(f.field->cursor() == 2);
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == "e");
}

CK_TEST(completion_validates_hostile_ranges_control_text_and_publication_bounds) {
    Fixture f;
    auto source = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(source);
    f.field->set_text("rep");
    source->publish(source->query.revision, "report\n.txt");
    f.field->completion_changed();
    CK_CHECK(f.field->completion_candidate() == nullptr);
    source->publish(source->query.revision, "report.txt");
    source->result.candidates.front().end = 100;
    f.field->completion_changed();
    CK_CHECK(f.field->completion_candidate() == nullptr);
    source->result.candidates.assign(kMaxCompletionCandidates + 1, CompletionCandidate{1, 0, 3, "report.txt"});
    f.field->completion_changed();
    CK_CHECK(f.field->completion_candidate() == nullptr);
}

CK_TEST(completion_static_vocabulary_is_sorted_deduplicated_and_bounded) {
    std::vector<std::string> choices;
    for (int i = 0; i < 1000; ++i) choices.push_back("report-" + std::to_string(i));
    choices.push_back("report-0");
    PrefixCompletionProvider provider(std::move(choices));
    provider.request(CompletionRequest{9, "rep", 3, false, 3, 3});
    const auto& result = provider.snapshot();
    CK_CHECK(result.revision == 9);
    CK_CHECK(result.candidates.size() == kMaxCompletionCandidates);
    CK_CHECK(result.candidates.front().text == "report-0");
}

CK_TEST(completion_explicit_candidate_selection_and_token_replacement_use_the_same_undo_path) {
    Fixture f;
    auto source = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(source);
    f.field->set_text("open rpt now");
    source->result = CompletionSnapshot{source->query.revision, CompletionState::Ready,
        {CompletionCandidate{1, 5, 8, "report.csv"}, CompletionCandidate{2, 5, 8, "report.txt"}}};
    f.field->completion_changed();
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(f.field->select_completion(2));
    CK_CHECK(f.field->accept_completion());
    CK_CHECK(f.field->text() == "open report.txt now");
    CK_CHECK(f.field->cursor() == 15);
    CK_CHECK(f.field->undo());
    CK_CHECK(f.field->text() == "open rpt now");
}

CK_TEST(completion_notifications_can_destroy_the_field_without_followup_access) {
    Fixture f;
    f.field->set_completions({"report.txt"});
    auto* field = f.field;
    field->on_completion_changed = [&] { f.app.root().remove_child(field); f.field = nullptr; };
    field->set_text("rep");
    CK_CHECK(f.field == nullptr);
}

CK_TEST(completion_preview_frame_has_a_subdued_suffix_and_preserves_committed_text) {
    Fixture f;
    f.field->set_completions({"report.txt"});
    f.field->set_text("rep");
    f.app.step(0);
    const auto frame = golden::serialize(scene::capture(f.app.composed_surface(), f.app.current_cursor()));
    std::ifstream input("golden/completion_preview.dump", std::ios::binary);
    std::ostringstream expected;
    expected << input.rdbuf();
    CK_CHECK(input.is_open());
    CK_CHECK(frame == expected.str());
    CK_CHECK(f.field->text() == "rep");
}

CK_TEST(completion_duplicate_nonzero_identities_cannot_offer_or_select_an_ambiguous_edit) {
    Fixture f;
    auto provider = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(provider); f.field->set_text("rep");
    provider->result = {provider->query.revision, CompletionState::Ready,
        {{1, 0, 3, "report.txt"}, {1, 0, 3, "report.csv"}}};
    f.field->completion_changed();
    CK_CHECK(!f.field->has_completion_preview());
    CK_CHECK(!f.field->select_completion(1));
    CK_CHECK(!f.field->accept_completion());
    CK_CHECK(f.field->text() == "rep");
}

CK_TEST(completion_password_and_masked_fields_do_not_send_contents_to_the_provider) {
    Fixture f;
    auto provider = std::make_shared<DelayedProvider>();
    f.field->set_completion_provider(provider);
    const auto before = provider->query;
    f.field->set_password_echo(true);
    f.field->set_text("private-password");
    f.field->refresh_completion();
    CK_CHECK(provider->query.revision == before.revision);
    CK_CHECK(provider->query.text == before.text);
    CK_CHECK(!f.field->has_completion_preview());
    f.field->set_completion_provider(nullptr);
    f.field->set_text("");
    f.field->set_password_echo(false);
    f.field->set_mask("AAAAAAAAAAAAAAAA");
    f.field->set_completion_provider(provider);
    f.field->set_text("private-mask");
    f.field->refresh_completion();
    CK_CHECK(provider->query.revision == before.revision);
    CK_CHECK(provider->query.text == before.text);
    CK_CHECK(!f.field->has_completion_preview());
}
