// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/view.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/ui/context.hpp"

using ckv::Rect;
using ckv::ui::Context;
using ckv::ui::FocusPolicy;
using ckv::ui::InvalidationKind;
using ckv::ui::RoleRegistry;
using ckv::ui::Theme;
using ckv::ui::View;

namespace {

class RecordingView : public View {
public:
    explicit RecordingView(Rect bounds = {}) : View(bounds) {}
    int draws = 0;
    int attach_count = 0;
    void draw(ckv::scene::Painter&) override { ++draws; }
    void on_attached() override { ++attach_count; }
};

class InvalidationSpyView final : public View {
public:
    int descendant_invalidations = 0;
    InvalidationKind last_kind = InvalidationKind::Content;
    const View* last_source = nullptr;

protected:
    void on_descendant_invalidated(const View& source, Rect, InvalidationKind kind) override {
        ++descendant_invalidations;
        last_source = &source;
        last_kind = kind;
    }
};

}  // namespace

// --- Tree ownership --------------------------------------------------------

CK_TEST(add_child_transfers_ownership_and_returns_an_observer_pointer) {
    View root;
    auto child = std::make_unique<RecordingView>();
    RecordingView* observer = static_cast<RecordingView*>(root.add_child(std::move(child)));
    CK_CHECK(observer != nullptr);
    CK_CHECK(root.children().size() == 1);
    CK_CHECK(root.children().front().get() == observer);
    CK_CHECK(observer->parent() == &root);
}

CK_TEST(remove_child_returns_ownership_and_detaches_the_parent_link) {
    View root;
    View* observer = root.add_child(std::make_unique<View>());
    std::unique_ptr<View> owned = root.remove_child(observer);
    CK_CHECK(owned.get() == observer);
    CK_CHECK(owned->parent() == nullptr);
    CK_CHECK(root.children().empty());
}

CK_TEST(remove_child_for_a_view_that_is_not_a_child_returns_null_and_changes_nothing) {
    View root;
    View* real_child = root.add_child(std::make_unique<View>());
    View stray;
    CK_CHECK(root.remove_child(&stray) == nullptr);
    CK_CHECK(root.children().size() == 1);
    CK_CHECK(root.children().front().get() == real_child);
}

CK_TEST(adding_a_view_that_already_has_a_parent_aborts) {
    CK_EXPECT_ABORT({
        View root1;
        View root2;
        View* observer = root1.add_child(std::make_unique<View>());
        // Re-parenting via a raw, non-owning pointer without going
        // through remove_child first must abort — a view cannot have
        // two owners.
        auto second_owner = std::unique_ptr<View>(observer);
        root2.add_child(std::move(second_owner));
    });
}

CK_TEST(destroying_a_parent_destroys_its_children_deterministically) {
    static int destroyed = 0;
    struct CountingView : View {
        ~CountingView() override { ++destroyed; }
    };
    {
        View root;
        root.add_child(std::make_unique<CountingView>());
        root.add_child(std::make_unique<CountingView>());
    }
    CK_CHECK(destroyed == 2);
}

CK_TEST(a_views_lifetime_token_expires_with_that_specific_view_instance) {
    std::weak_ptr<void> token;
    {
        auto view = std::make_unique<View>();
        token = view->lifetime_token();
        CK_CHECK(!token.expired());
    }
    CK_CHECK(token.expired());
}

// --- Geometry ----------------------------------------------------------

CK_TEST(absolute_bounds_of_a_root_view_equals_its_own_bounds) {
    View root(Rect{5, 5, 20, 10});
    CK_CHECK(root.absolute_bounds() == (Rect{5, 5, 20, 10}));
}

CK_TEST(absolute_bounds_accumulates_offsets_through_every_ancestor) {
    View root(Rect{2, 3, 40, 20});
    View* mid = root.add_child(std::make_unique<View>(Rect{1, 1, 30, 15}));
    View* leaf = mid->add_child(std::make_unique<View>(Rect{4, 2, 5, 5}));
    CK_CHECK(leaf->absolute_bounds() == (Rect{2 + 1 + 4, 3 + 1 + 2, 5, 5}));
}

CK_TEST(set_bounds_to_the_same_rect_is_a_no_op_that_does_not_invalidate) {
    View root;
    int dirty_calls = 0;
    root.set_dirty_rect_sink([&](Rect) { ++dirty_calls; });
    root.set_bounds(root.bounds());
    CK_CHECK(dirty_calls == 0);
}

CK_TEST(set_bounds_invalidates_both_the_new_and_the_vacated_old_rect) {
    View root(Rect{0, 0, 80, 24});
    View* child = root.add_child(std::make_unique<View>(Rect{2, 2, 5, 3}));
    std::vector<Rect> dirtied;
    root.set_dirty_rect_sink([&](Rect r) { dirtied.push_back(r); });
    child->set_bounds(Rect{10, 10, 5, 3});
    CK_CHECK(dirtied.size() == 2);
    CK_CHECK(dirtied[0] == (Rect{10, 10, 5, 3}));  // new bounds, damaged first
    CK_CHECK(dirtied[1] == (Rect{2, 2, 5, 3}));    // old bounds, vacated
}

// --- Visibility / enabled ------------------------------------------------

CK_TEST(set_visible_to_the_current_value_does_not_invalidate) {
    View v;
    int dirty_calls = 0;
    v.set_dirty_rect_sink([&](Rect) { ++dirty_calls; });
    v.set_visible(true);  // already true
    CK_CHECK(dirty_calls == 0);
}

CK_TEST(set_visible_false_then_true_invalidates_both_times) {
    View v(Rect{0, 0, 10, 4});
    int dirty_calls = 0;
    v.set_dirty_rect_sink([&](Rect) { ++dirty_calls; });
    v.set_visible(false);
    v.set_visible(true);
    CK_CHECK(dirty_calls == 2);
}

CK_TEST(a_disabled_or_hidden_view_is_never_focusable_even_with_tabstop_policy) {
    View v;
    v.set_focus_policy(FocusPolicy::TabStop);
    CK_CHECK(v.focusable());
    v.set_enabled(false);
    CK_CHECK(!v.focusable());
    v.set_enabled(true);
    v.set_visible(false);
    CK_CHECK(!v.focusable());
}

CK_TEST(default_focus_policy_is_none_and_not_focusable) {
    View v;
    CK_CHECK(v.focus_policy() == FocusPolicy::None);
    CK_CHECK(!v.focusable());
}

CK_TEST(custom_view_can_declare_tabstop_focus_at_construction) {
    View v({}, FocusPolicy::TabStop);
    CK_CHECK(v.focus_policy() == FocusPolicy::TabStop);
    CK_CHECK(v.focusable());
}

// --- Invalidation propagation to the dirty-rect sink -----------------------

CK_TEST(invalidate_before_any_sink_is_installed_is_a_harmless_no_op) {
    View v(Rect{0, 0, 10, 4});
    v.invalidate();  // must not crash
    CK_CHECK(true);
}

CK_TEST(invalidate_whole_view_reports_the_full_absolute_bounds) {
    View root(Rect{3, 3, 80, 24});
    View* child = root.add_child(std::make_unique<View>(Rect{1, 1, 10, 5}));
    std::optional<Rect> reported;
    root.set_dirty_rect_sink([&](Rect r) { reported = r; });
    child->invalidate();
    CK_CHECK(reported.has_value());
    CK_CHECK(*reported == (Rect{4, 4, 10, 5}));
}

CK_TEST(invalidate_local_rect_is_translated_into_absolute_space) {
    View root(Rect{10, 0, 80, 24});
    View* child = root.add_child(std::make_unique<View>(Rect{5, 0, 20, 10}));
    std::optional<Rect> reported;
    root.set_dirty_rect_sink([&](Rect r) { reported = r; });
    child->invalidate(Rect{2, 3, 4, 1});
    CK_CHECK(*reported == (Rect{10 + 5 + 2, 0 + 0 + 3, 4, 1}));
}

CK_TEST(retained_owners_can_distinguish_content_from_geometry_invalidation) {
    InvalidationSpyView parent;
    View* child = parent.add_child(std::make_unique<View>(Rect{1, 2, 3, 4}));

    child->invalidate();
    CK_CHECK(parent.descendant_invalidations == 1);
    CK_CHECK(parent.last_source == child);
    CK_CHECK(parent.last_kind == InvalidationKind::Content);

    child->set_bounds(Rect{5, 6, 3, 4});
    CK_CHECK(parent.descendant_invalidations == 2);
    CK_CHECK(parent.last_source == child);
    CK_CHECK(parent.last_kind == InvalidationKind::Geometry);
}

CK_TEST(set_dirty_rect_sink_on_a_parent_propagates_to_children_added_before_it) {
    View root;
    View* child = root.add_child(std::make_unique<View>());
    View* grandchild = child->add_child(std::make_unique<View>());
    int calls = 0;
    root.set_dirty_rect_sink([&](Rect) { ++calls; });
    grandchild->invalidate();
    CK_CHECK(calls == 1);
}

CK_TEST(a_newly_added_child_inherits_the_parents_already_installed_sink) {
    View root;
    int calls = 0;
    root.set_dirty_rect_sink([&](Rect) { ++calls; });
    View* child = root.add_child(std::make_unique<View>());
    child->invalidate();
    CK_CHECK(calls == 1);
}

CK_TEST(removing_a_child_detaches_it_from_the_sink_so_further_invalidation_is_silent) {
    View root;
    root.set_dirty_rect_sink([](Rect) {});
    View* child_ptr = root.add_child(std::make_unique<View>());
    std::unique_ptr<View> detached = root.remove_child(child_ptr);
    int calls = 0;
    detached->set_dirty_rect_sink([&](Rect) { ++calls; });  // re-attach our own probe
    // The detach itself must have cleared the old sink first (verified
    // indirectly: no crash / no phantom call to the parent's now-freed
    // capture context when we exercise the detached subtree below).
    detached->invalidate();
    CK_CHECK(calls == 1);
}

// --- Events default to unhandled -----------------------------------------

CK_TEST(default_event_handlers_report_unhandled) {
    View v;
    CK_CHECK(!v.on_key(ckv::KeyEvent{}));
    CK_CHECK(!v.on_text(ckv::TextEvent{}));
    CK_CHECK(!v.on_mouse(ckv::MouseEvent{}));
}

// --- Size-hint-change propagation (M9/WP-16, E10) --------------------------

namespace {
class HintNotifyingView : public View {
public:
    void notify() { size_hint_changed(); }
};

class HintSpyView : public View {
public:
    int notifications = 0;
    View* last_child = nullptr;
    void on_child_size_hint_changed(View& child) override {
        ++notifications;
        last_child = &child;
    }
};
}  // namespace

CK_TEST(size_hint_changed_before_attachment_is_a_harmless_no_op) {
    HintNotifyingView v;
    v.notify();  // no parent yet — must not crash
    CK_CHECK(v.parent() == nullptr);
}

CK_TEST(size_hint_changed_notifies_only_the_immediate_parent) {
    HintSpyView grandparent;
    auto parent_owned = std::make_unique<HintSpyView>();
    auto* parent = static_cast<HintSpyView*>(grandparent.add_child(std::move(parent_owned)));
    auto child_owned = std::make_unique<HintNotifyingView>();
    auto* child = static_cast<HintNotifyingView*>(parent->add_child(std::move(child_owned)));

    child->notify();

    CK_CHECK(parent->notifications == 1);
    CK_CHECK(parent->last_child == child);
    // single-hop by design — no automatic bubbling further up
    CK_CHECK(grandparent.notifications == 0);
}

CK_TEST(the_default_on_child_size_hint_changed_does_nothing) {
    View parent;
    HintNotifyingView* child =
        static_cast<HintNotifyingView*>(parent.add_child(std::make_unique<HintNotifyingView>()));
    // plain View's default override — must not crash
    child->notify();
    CK_CHECK(true);
}

// --- Help context ----------------------------------------------------------

CK_TEST(resolve_help_context_key_returns_null_when_no_ancestor_has_one) {
    View root;
    View* child = root.add_child(std::make_unique<View>());
    CK_CHECK(child->resolve_help_context_key() == nullptr);
}

CK_TEST(resolve_help_context_key_prefers_the_views_own_key_over_an_ancestors) {
    View root;
    root.set_help_context_key("root.topic");
    View* child = root.add_child(std::make_unique<View>());
    child->set_help_context_key("child.topic");
    CK_CHECK(*child->resolve_help_context_key() == "child.topic");
}

CK_TEST(resolve_help_context_key_walks_up_to_the_nearest_ancestor_that_has_one) {
    View root;
    root.set_help_context_key("root.topic");
    View* mid = root.add_child(std::make_unique<View>());
    View* leaf = mid->add_child(std::make_unique<View>());  // no key of its own
    CK_CHECK(*leaf->resolve_help_context_key() == "root.topic");
}

// --- Context propagation and on_attached() (M9 WP-7, D-028) ----------------

namespace {
struct ContextFixture {
    RoleRegistry registry;
    Theme theme{registry};
    Context ctx{&theme, &registry, nullptr};
};
}  // namespace

CK_TEST(a_fresh_view_has_no_valid_context) {
    View v;
    CK_CHECK(!v.context().valid());
}

CK_TEST(set_context_directly_makes_it_valid_and_fires_on_attached_once) {
    ContextFixture f;
    RecordingView v;
    v.set_context(f.ctx);
    CK_CHECK(v.context().valid());
    CK_CHECK(v.context().theme == &f.theme);
    CK_CHECK(v.context().roles == &f.registry);
    CK_CHECK(v.attach_count == 1);
}

CK_TEST(add_child_propagates_the_parents_existing_context_to_the_new_child) {
    ContextFixture f;
    View root;
    root.set_context(f.ctx);
    auto* child = static_cast<RecordingView*>(root.add_child(std::make_unique<RecordingView>()));
    CK_CHECK(child->context().valid());
    CK_CHECK(child->attach_count == 1);
}

CK_TEST(on_attached_may_detach_and_destroy_the_new_child_without_leaving_a_dangling_observer) {
    struct SelfDestroyingOnAttach final : View {
        void on_attached() override {
            std::unique_ptr<View> detached = parent()->remove_child(this);
            CK_CHECK(detached.get() == this);
            detached.reset();
        }
    };

    ContextFixture f;
    View root;
    root.set_context(f.ctx);

    CK_CHECK(root.add_child(std::make_unique<SelfDestroyingOnAttach>()) == nullptr);
    CK_CHECK(root.children().empty());
}

CK_TEST(on_attached_may_destroy_its_own_parent_without_add_child_reusing_the_dead_tree) {
    struct DestroyingParentOnAttach final : View {
        void on_attached() override {
            View* const owner = parent();
            View* const root = owner->parent();
            std::unique_ptr<View> detached = root->remove_child(owner);
            CK_CHECK(detached.get() == owner);
            detached.reset();
        }
    };

    ContextFixture f;
    View root;
    root.set_context(f.ctx);
    View* const owner = root.add_child(std::make_unique<View>());

    // The child destroys `owner` while owner->add_child() is in its context
    // propagation path. A safe attachment operation reports no observer and
    // leaves root with no departed subtree.
    CK_CHECK(owner->add_child(std::make_unique<DestroyingParentOnAttach>()) == nullptr);
    CK_CHECK(root.children().empty());
}

CK_TEST(add_child_before_the_parent_has_a_context_leaves_the_child_unattached) {
    View root;  // no context installed
    auto* child = static_cast<RecordingView*>(root.add_child(std::make_unique<RecordingView>()));
    CK_CHECK(!child->context().valid());
    CK_CHECK(child->attach_count == 0);
}

CK_TEST(a_subtree_built_while_detached_resolves_entirely_when_attached_in_one_shot) {
    ContextFixture f;
    // Build a small tree with NO context anywhere yet — the common
    // pattern: construct content, THEN attach it under a live parent.
    auto mid = std::make_unique<RecordingView>();
    auto* mid_raw = mid.get();
    auto* leaf_raw = static_cast<RecordingView*>(mid->add_child(std::make_unique<RecordingView>()));
    CK_CHECK(!mid_raw->context().valid());
    CK_CHECK(!leaf_raw->context().valid());

    View root;
    root.set_context(f.ctx);
    root.add_child(std::move(mid));  // one shot: both mid and leaf resolve now

    CK_CHECK(mid_raw->context().valid());
    CK_CHECK(mid_raw->attach_count == 1);
    CK_CHECK(leaf_raw->context().valid());
    CK_CHECK(leaf_raw->attach_count == 1);
}

CK_TEST(re_setting_context_updates_it_but_does_not_refire_on_attached) {
    ContextFixture f;
    RecordingView v;
    v.set_context(f.ctx);
    CK_CHECK(v.attach_count == 1);

    RoleRegistry other_registry;
    Theme other_theme{other_registry};
    v.set_context(Context{&other_theme, &other_registry, nullptr});  // e.g. per-window theme override

    CK_CHECK(v.context().theme == &other_theme);  // the new context DOES take effect
    CK_CHECK(v.attach_count == 1);                // but on_attached fires only on the first transition
}
