// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_search.hpp"
#include "cvision/testing/cktest.hpp"

#include <algorithm>
#include <limits>

using namespace ckv;
using namespace ckv::widgets;
namespace {
LogLimits query_limits(std::size_t records = 16, std::size_t bytes = 128) {
    LogLimits limits; limits.records = records; limits.record_bytes = bytes;
    limits.retained_bytes = kLogMetadataBytes + records * bytes * 4;
    return limits;
}
void settle(LogFilter& filter) {
    std::size_t steps = 0;
    while (filter.step()) { CK_CHECK(++steps < 100000); }
}
void settle(LogSearch& search) {
    std::size_t steps = 0;
    while (search.step()) { CK_CHECK(++steps < 100000); }
}
std::vector<std::string> rows(const LogRecordProvider& provider) {
    std::vector<std::string> result;
    for (std::size_t i = 0; i < provider.size(); ++i) result.emplace_back(provider.at(i)->text);
    return result;
}
struct Custom { std::size_t calls = 0; };
bool custom_fields(std::span<const LogField> fields, const void* context) noexcept {
    auto& state = *const_cast<Custom*>(static_cast<const Custom*>(context)); ++state.calls;
    return !fields.empty() && fields[0].value == "demo";
}
}

CK_TEST(log_literal_yields_between_comparisons_and_finds_overlapping_matches) {
    LogLiteral literal; CK_CHECK(literal.set("aba"));
    LogLiteralCursor cursor;
    std::vector<LogTextRange> matches;
    for (std::size_t iteration = 0; iteration < 20; ++iteration) {
        std::size_t quota = 1; LogTextRange match;
        const auto result = literal.scan("ababa", cursor, quota, match);
        CK_CHECK(quota <= 1);
        if (result == LogLiteralStep::Match) matches.push_back(match);
        if (result == LogLiteralStep::End) break;
    }
    CK_CHECK((matches == std::vector<LogTextRange>{{0, 3}, {2, 5}}));
    CK_CHECK(literal.set("AAAb", false));
    cursor = {}; std::size_t quota = 100; LogTextRange match;
    CK_CHECK(literal.scan("aaaaAAAB", cursor, quota, match) == LogLiteralStep::Match);
    CK_CHECK((match == LogTextRange{4, 8}));
}
CK_TEST(log_literal_rejects_malformed_controls_and_preserves_the_previous_pattern) {
    LogLiteral literal; CK_CHECK(literal.set("é\n中"));
    CK_CHECK(!literal.set("\x80")); CK_CHECK(!literal.set("\t")); CK_CHECK(!literal.set(std::string(1025, 'a')));
    CK_CHECK(literal.pattern() == "é\n中"); CK_CHECK(literal.set("�"));
}
CK_TEST(log_filter_empty_query_is_an_immediate_model_projection) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    model.append_bytes("one\ntwo");
    LogFilter filter(model, {1, 1});
    CK_CHECK(filter.size() == 2 && filter.at(1)->partial);
    CK_CHECK(!filter.step() && filter.status().last_step_comparisons == 0 && filter.status().last_step_records == 0);
    const auto revision = filter.revision();
    model.append_bytes(" more\n"); filter.step();
    CK_CHECK(filter.revision() > revision && filter.at(1)->text == "two more");
}
CK_TEST(log_filter_literal_include_exclude_and_case_are_explicit) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    for (const auto value : {"Warning disk", "warning ignored", "WARNING memory", "other"}) model.append_record(value);
    LogFilter filter(model, {2, 5}); LogFilterQuery query;
    query.include = "warning"; query.exclude = "ignored"; query.case_sensitive = false;
    CK_CHECK(filter.set_query(query)); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"Warning disk", "WARNING memory"}));
    query.case_sensitive = true; CK_CHECK(filter.set_query(query)); settle(filter);
    CK_CHECK(filter.size() == 0);
}
CK_TEST(log_filter_unicode_folding_is_ascii_only_and_multiline_is_literal) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    for (const auto value : {"Är", "är", "prefix\n中 end", "prefix 中 end"}) model.append_record(value);
    LogFilter filter(model, {2, 2}); LogFilterQuery query;
    query.include = "ÄR"; query.case_sensitive = false;
    filter.set_query(query); settle(filter); CK_CHECK((rows(filter) == std::vector<std::string>{"Är"}));
    query.include = "prefix\n中"; filter.set_query(query); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"prefix\n中 end"}));
}
CK_TEST(log_filter_severity_and_source_do_not_guess_metadata_from_text) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    model.append_record("ERROR only text");
    model.append_record("real error", {LogSeverity::Error, {}, {}});
    model.begin_source(2, 1); model.append_record("other error", {LogSeverity::Error, {}, {}});
    LogFilter filter(model); LogFilterQuery query;
    query.severity_mask = static_cast<std::uint16_t>(1U << static_cast<unsigned>(LogSeverity::Error));
    query.sources[0] = 1; query.source_count = 1;
    filter.set_query(query); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"real error"}));
    query.severity_mask = 1; filter.set_query(query); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"ERROR only text"}));
}
CK_TEST(log_filter_named_fields_and_custom_matcher_are_bounded_and_incremental) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    const std::array fields{LogField{"unit", "demo"}, LogField{"process", "WORKER"}};
    model.append_record("a very long message with a needle", {LogSeverity::Info, {}, fields});
    model.append_record("needle without metadata");
    LogFilter filter(model, {1, 1}); LogFilterQuery query;
    query.include = "needle"; query.case_sensitive = false;
    query.field_count = 2; query.fields[0] = {"unit", "demo", LogFieldMatch::Equals};
    query.fields[1] = {"process", "work", LogFieldMatch::Contains};
    Custom context; query.field_matcher = custom_fields; query.field_context = &context;
    filter.set_query(query); settle(filter);
    CK_CHECK(filter.size() == 1 && context.calls == 2);
    query.fields[1] = {"missing", {}, LogFieldMatch::Present};
    filter.set_query(query); settle(filter); CK_CHECK(filter.size() == 0);
}
CK_TEST(log_filter_query_replacement_publishes_only_the_current_revision) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    for (const auto value : {"one", "two", "three", "four"}) model.append_record(value);
    LogFilter filter(model, {1, 2}); filter.set_include("one");
    CK_CHECK(filter.step() && filter.status().query_pending);
    CK_CHECK(filter.size() == 4 && filter.status().published_revision == 1);
    const auto retired = filter.status().requested_revision;
    filter.set_include("three"); CK_CHECK(filter.status().requested_revision > retired);
    settle(filter);
    CK_CHECK(filter.status().published_revision == filter.status().requested_revision);
    CK_CHECK((rows(filter) == std::vector<std::string>{"three"}));
    filter.set_include(""); CK_CHECK(!filter.step()); CK_CHECK(filter.size() == 4);
}
CK_TEST(log_filter_arrivals_and_eviction_join_the_rebuild_without_a_lost_interval) {
    LogModel model(query_limits(3)); model.begin_source(1, 1);
    model.append_bytes("a\nb\nc\n");
    LogFilter filter(model, {1, 1}); filter.set_include("x"); filter.step();
    model.append_record("x first"); filter.step();
    model.append_record("x second"); filter.step();
    model.append_record("unrelated"); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"x first", "x second"}));
    model.append_record("x latest"); settle(filter);
    CK_CHECK((rows(filter) == std::vector<std::string>{"x second", "x latest"}));
}
CK_TEST(log_filter_partial_snapshot_stays_coherent_until_replacement_matching_finishes) {
    LogModel model(query_limits()); model.begin_source(1, 1); model.append_bytes("keep");
    LogFilter filter(model, {1, 2}); LogFilterQuery query;
    query.include = "keep"; query.exclude = "drop";
    filter.set_query(query); settle(filter);
    const auto id = filter.at(0)->id;
    model.append_bytes(" drop"); filter.step();
    CK_CHECK(filter.status().live_pending && filter.at(0)->text == "keep" && filter.at(0)->id == id);
    settle(filter); CK_CHECK(filter.size() == 0);
}
CK_TEST(log_filter_progress_replacement_keeps_id_but_revises_eligibility) {
    LogModel model(query_limits(), LogCarriageReturn::Progress); model.begin_source(1, 1);
    model.append_bytes("yes\r");
    const auto id = model.at(0)->id;
    LogFilter filter(model, {1, 1}); filter.set_include("yes"); settle(filter); CK_CHECK(filter.size() == 1);
    model.append_bytes("no\r"); settle(filter); CK_CHECK(filter.size() == 0);
    model.append_bytes("yes again\r"); settle(filter);
    CK_CHECK(filter.at(0)->id == id && filter.at(0)->text == "yes again");
    model.finish(); settle(filter); CK_CHECK(!filter.at(0)->partial && filter.at(0)->incomplete);
}
CK_TEST(log_filter_oversized_discard_updates_do_not_restart_unchanged_text_matching) {
    LogModel model(query_limits(4, 8)); model.begin_source(1, 1); model.append_bytes("12345678extra");
    const auto text_revision = model.at(0)->text_revision;
    LogFilter filter(model, {1, 1}); filter.set_include("8");
    for (std::size_t i = 0; i < 30 && filter.status().query_pending; ++i) {
        model.append_bytes("discarded"); filter.step();
        CK_CHECK(model.at(0)->text_revision == text_revision);
    }
    CK_CHECK(!filter.status().query_pending && filter.size() == 1);
    model.append_bytes("more"); filter.step();
    CK_CHECK(!filter.status().live_pending && filter.at(0)->discarded_bytes == model.at(0)->discarded_bytes);
}
CK_TEST(log_filter_invalid_queries_leave_valid_queries_and_revisions_intact) {
    LogModel model(query_limits()); model.begin_source(1, 1); model.append_record("kept");
    LogFilter filter(model); filter.set_include("kept"); settle(filter);
    const auto revision = filter.status().requested_revision;
    CK_CHECK(!filter.set_include(std::string(1025, 'x')));
    CK_CHECK(filter.status().error == LogQueryError::TooLong && filter.status().requested_revision == revision);
    LogFilterQuery query; query.severity_mask = 0x100; CK_CHECK(!filter.set_query(query));
    query.severity_mask = kLogAllSeverities; query.sources[0] = 0; query.source_count = 1; CK_CHECK(!filter.set_query(query));
    query.source_count = 0; query.include = "\xFF"; CK_CHECK(!filter.set_query(query));
    CK_CHECK(filter.size() == 1 && filter.published_query().include == "kept");
}
CK_TEST(log_filter_work_quanta_and_incremental_append_are_operation_counted) {
    auto limits = query_limits(10000, 32); LogModel model(limits); model.begin_source(1, 1);
    for (std::size_t i = 0; i < 10000; ++i) model.append_record("line");
    LogFilter filter(model, {3, 7}); filter.set_include("line");
    while (filter.step()) {
        CK_CHECK(filter.status().last_step_records <= 3 && filter.status().last_step_comparisons <= 7);
    }
    const auto evaluated = filter.status().evaluated;
    model.append_record("line new"); settle(filter);
    CK_CHECK(filter.status().evaluated - evaluated == 1);
    CK_CHECK(filter.size() == model.size());
}
CK_TEST(log_filter_detail_and_context_do_not_expand_the_match_index) {
    LogModel model(query_limits(4)); model.begin_source(1, 1); model.append_bytes("before\nneedle\nafter\nlast\n");
    const auto id = model.at(1)->id;
    LogFilter filter(model); filter.set_include("needle"); settle(filter);
    LogContextProvider context(model); CK_CHECK(context.set_anchor(id, 1, 1));
    CK_CHECK((rows(context) == std::vector<std::string>{"before", "needle", "after"}));
    CK_CHECK(filter.size() == 1 && filter.detail(model.at(0)->id)->raw == "before");
    CK_CHECK(context.index_of(id) == 1 && !context.set_anchor(id, 129, 0));
    model.append_bytes("new\nnewer\n"); settle(filter);
    CK_CHECK(!context.anchor_available() && context.size() == 0 && !filter.detail(id));
}
CK_TEST(log_search_next_previous_and_wrap_are_independent_of_filter_visibility) {
    LogModel model(query_limits()); model.begin_source(1, 1);
    model.append_record("x one x"); model.append_record("hide x"); model.append_record("x last");
    LogFilter filter(model); LogFilterQuery query; query.exclude = "hide"; filter.set_query(query); settle(filter);
    LogSearch search(filter, {1, 2}); search.set_query("x");
    search.request(LogSearchDirection::Next); settle(search); auto first = search.take_result();
    CK_CHECK(first && first->id == model.at(0)->id && first->range.begin == 0);
    search.request(LogSearchDirection::Next, LogSearchOrigin{first->id, first->range.end}); settle(search);
    const auto second = search.take_result(); CK_CHECK(second && second->range.begin == 6);
    search.request(LogSearchDirection::Previous); settle(search);
    const auto last = search.take_result(); CK_CHECK(last && last->id == model.at(2)->id);
    search.request(LogSearchDirection::Next, LogSearchOrigin{last->id, last->range.end}); settle(search);
    CK_CHECK(search.status().wrapped && search.take_result()->id == first->id && filter.size() == 2);
    search.request(LogSearchDirection::Previous, LogSearchOrigin{first->id, 0}); settle(search);
    CK_CHECK(search.take_result()->id == last->id && search.status().wrapped);
}
CK_TEST(log_search_changed_query_and_visibility_retire_stale_results) {
    LogModel model(query_limits()); model.begin_source(1, 1); model.append_record("old new");
    LogFilter filter(model); LogSearch search(filter, {1, 1}); search.set_query("old");
    search.request(LogSearchDirection::Next); search.step(); search.set_query("new");
    CK_CHECK(!search.take_result()); search.request(LogSearchDirection::Next); settle(search);
    CK_CHECK(search.take_result()->range.begin == 4);
    search.request(LogSearchDirection::Next); search.step();
    filter.set_include("absent"); settle(filter); settle(search);
    CK_CHECK(search.status().phase == LogSearchPhase::NotFound && !search.take_result());
}
CK_TEST(log_search_partial_content_change_before_delivery_is_stale) {
    LogModel model(query_limits(), LogCarriageReturn::Progress); model.begin_source(1, 1); model.append_bytes("yes\r");
    LogSearch search(model); search.set_query("yes"); search.request(LogSearchDirection::Next); settle(search);
    model.append_bytes("no\r");
    CK_CHECK(!search.take_result() && search.status().phase == LogSearchPhase::Stale);
}
CK_TEST(log_search_admission_bound_terminates_under_continued_arrivals) {
    LogModel model(query_limits()); model.begin_source(1, 1); model.append_record("long record without target");
    LogSearch search(model, {1, 1}); search.set_query("missing"); search.request(LogSearchDirection::Next);
    for (std::size_t i = 0; i < 100 && search.status().phase == LogSearchPhase::Pending; ++i) {
        model.append_record("new missing"); search.step();
        CK_CHECK(search.status().last_step_records <= 1 && search.status().last_step_comparisons <= 1);
    }
    CK_CHECK(search.status().phase == LogSearchPhase::NotFound && !search.take_result());
}
CK_TEST(log_search_highlight_window_handles_overlap_unicode_and_multiline) {
    LogModel model(query_limits()); model.begin_source(1, 1); model.append_record("ababa\n中é tail");
    LogSearch search(model); search.set_query("aba");
    const auto record = model.at(0); std::array<unsigned char, 3> marks{};
    search.mark(*record, 1, 4, marks); CK_CHECK((marks == std::array<unsigned char, 3>{1, 1, 1}));
    search.set_query("aba\n中"); std::array<unsigned char, 4> cross{};
    search.mark(*record, 5, 9, cross); CK_CHECK((cross == std::array<unsigned char, 4>{1, 1, 1, 1}));
    search.set_query("e"); std::array<unsigned char, 3> cluster{};
    search.mark(*record, 9, 12, cluster); CK_CHECK((cluster == std::array<unsigned char, 3>{1, 0, 0}));
}
CK_TEST(log_filter_scripted_mutation_schedule_never_publishes_ineligible_or_duplicate_ids) {
    LogModel model(query_limits(8, 32), LogCarriageReturn::Progress); model.begin_source(1, 1);
    LogFilter filter(model, {2, 3}); std::uint32_t random = 17;
    for (std::size_t turn = 0; turn < 300; ++turn) {
        random = random * 1664525U + 1013904223U;
        switch (random % 7) {
            case 0: filter.set_include("a"); break;
            case 1: filter.set_include("b"); break;
            case 2: filter.set_include(""); break;
            case 3: model.append_bytes("ab"); break;
            case 4: model.append_bytes("x\r"); break;
            case 5: model.append_bytes("b\n"); break;
            case 6: model.append_record("a complete"); break;
        }
        filter.step();
        std::uint64_t previous = 0;
        for (std::size_t i = 0; i < filter.size(); ++i) {
            const auto record = filter.at(i); CK_CHECK(record.has_value());
            CK_CHECK(record->id.sequence > previous); previous = record->id.sequence;
            CK_CHECK(record->text.find(filter.published_query().include) != std::string_view::npos);
        }
    }
    settle(filter);
    std::size_t count = 0;
    for (std::size_t i = 0; i < model.size(); ++i)
        if (model.at(i)->text.find(filter.requested_query().include) != std::string_view::npos) ++count;
    CK_CHECK(filter.size() == count);
}
