// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/history.hpp"

#include "cvision/testing/cktest.hpp"

using ckv::ui::HistoryRegistry;

CK_TEST(entries_for_an_unknown_key_is_empty_not_an_error) {
    HistoryRegistry history;
    CK_CHECK(history.entries("nope").empty());
}

CK_TEST(record_inserts_at_the_front) {
    HistoryRegistry history;
    history.record("path", "a");
    history.record("path", "b");
    CK_CHECK(history.entries("path").size() == 2);
    CK_CHECK(history.entries("path")[0] == "b");
    CK_CHECK(history.entries("path")[1] == "a");
}

CK_TEST(recording_a_value_already_present_moves_it_to_the_front_without_duplicating) {
    HistoryRegistry history;
    history.record("path", "a");
    history.record("path", "b");
    history.record("path", "a");  // re-record an existing value
    CK_CHECK(history.entries("path").size() == 2);
    CK_CHECK(history.entries("path")[0] == "a");
    CK_CHECK(history.entries("path")[1] == "b");
}

CK_TEST(distinct_keys_have_independent_lists) {
    HistoryRegistry history;
    history.record("file", "x");
    history.record("search", "y");
    CK_CHECK(history.entries("file").size() == 1);
    CK_CHECK(history.entries("search").size() == 1);
    CK_CHECK(history.entries("file")[0] == "x");
}

CK_TEST(capacity_is_enforced_dropping_the_oldest_entries) {
    HistoryRegistry history(2);
    history.record("k", "a");
    history.record("k", "b");
    history.record("k", "c");
    CK_CHECK(history.entries("k").size() == 2);
    CK_CHECK(history.entries("k")[0] == "c");
    CK_CHECK(history.entries("k")[1] == "b");
    // "a" was evicted by capacity
}

CK_TEST(zero_capacity_disables_recording_entirely) {
    HistoryRegistry history(0);
    history.record("k", "a");
    CK_CHECK(history.entries("k").empty());
}

CK_TEST(clear_empties_a_keys_list_without_affecting_other_keys) {
    HistoryRegistry history;
    history.record("a", "1");
    history.record("b", "2");
    history.clear("a");
    CK_CHECK(history.entries("a").empty());
    CK_CHECK(history.entries("b").size() == 1);
}

CK_TEST(clear_on_a_never_recorded_key_is_a_harmless_no_op) {
    HistoryRegistry history;
    history.clear("nope");  // must not crash
    CK_CHECK(true);
}

CK_TEST(set_capacity_immediately_truncates_an_oversized_existing_list) {
    HistoryRegistry history(10);
    history.record("k", "a");
    history.record("k", "b");
    history.record("k", "c");
    history.set_capacity("k", 1);
    CK_CHECK(history.entries("k").size() == 1);
    CK_CHECK(history.entries("k")[0] == "c");
}

CK_TEST(per_key_capacity_overrides_the_registrys_default_for_that_key_only) {
    HistoryRegistry history(20);
    history.set_capacity("small", 1);
    history.record("small", "a");
    history.record("small", "b");
    history.record("other", "x");
    history.record("other", "y");
    CK_CHECK(history.entries("small").size() == 1);
    CK_CHECK(history.entries("other").size() == 2);  // still uses the default capacity
}

CK_TEST(capacity_query_for_an_unset_key_reports_the_registrys_default) {
    HistoryRegistry history(7);
    CK_CHECK(history.capacity("never-touched") == 7);
}
