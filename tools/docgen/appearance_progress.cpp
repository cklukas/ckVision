// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "appearance_matrix.hpp"
#include "cvision/widgets/progress_tasks.hpp"

namespace ckv::docgen::appearance {
namespace {
// The specimen owns the same data/view lifetime arrangement as a real client.
class TaskSpecimen : public ui::View {
public:
    widgets::ProgressModel model;
    widgets::ProgressView* display;
    widgets::ProgressTaskId main = 0, pictures = 0, index = 0;
    TaskSpecimen() {
        // ckvision-doc: progressview
        display = make<widgets::ProgressView>(model);
        main = model.add_task({.title = "Import", .total = 5, .unit = "stages"});
        pictures = model.add_task({.title = "Pictures", .parent = main,
                                   .total = 200, .unit = "files", .weight = 3});
        index = model.add_task({.title = "Indexing", .parent = main});
        model.start(main);
        model.start(pictures);
        model.start(index);
        model.set_completed(main, 3);
        model.set_time(1'000'000'000);
        model.set_completed(pictures, 74);
        model.set_message(pictures, "holiday-2026.jpg");
        model.set_message(index, "Discovering files…");
        // ckvision-doc-end: progressview
    }
    void on_resized() override { display->set_bounds(Rect{0, 0, bounds().width, bounds().height}); }
};
}

void add_progress_task_specimens(Catalog& catalog) {
    auto& activity = catalog.element("ActivityIndicator", "include/cvision/widgets/progress_tasks.hpp", Traits{.text = true});
    for (const auto name : {"normal", "dots", "ascii", "custom", "static", "queued", "paused", "completed", "failed", "cancelled", "wide", "narrow", "disabled", "phases"}) {
        state(activity, name, Size{40, 8}, [name](Stage& stage) {
            auto& body = stage.dialog(Rect{1, 1, 36, 6}, "ActivityIndicator");
            // ckvision-doc: activityindicator
            auto* indicator = body.make<widgets::ActivityIndicator>();
            indicator->set_bounds(Rect{2, 1, 12, 1});
            indicator->set_presentation(widgets::ActivityPresentation::Braille);
            indicator->set_phase(3); // explicit replay phase; controller binding animates it
            // ckvision-doc-end: activityindicator
            const std::string_view kind(name);
            if (kind == "dots") indicator->set_presentation(widgets::ActivityPresentation::Dots);
            if (kind == "ascii") indicator->set_presentation(widgets::ActivityPresentation::Ascii);
            if (kind == "custom") indicator->set_frames({"◐", "◓", "◑", "◒"});
            if (kind == "static") indicator->set_motion(widgets::ProgressMotion::Static);
            if (kind == "queued") indicator->set_state(widgets::ProgressTaskState::Queued);
            if (kind == "paused") indicator->set_state(widgets::ProgressTaskState::Paused);
            if (kind == "completed") indicator->set_state(widgets::ProgressTaskState::Completed);
            if (kind == "failed") indicator->set_state(widgets::ProgressTaskState::Failed);
            if (kind == "cancelled") indicator->set_state(widgets::ProgressTaskState::Cancelled);
            if (kind == "wide" || kind == "narrow") indicator->set_frames({std::string(kWideText)});
            if (kind == "narrow") indicator->set_bounds(Rect{2, 1, 1, 1});
            if (kind == "disabled") indicator->set_enabled(false);
            if (kind == "phases") for (int i = 0; i < 8; ++i) {
                auto* phase = body.make<widgets::ActivityIndicator>(); phase->set_bounds(Rect{2 + i * 3, 2, 1, 1}); phase->set_phase(i);
            }
        });
    }
    auto& tasks = catalog.element("ProgressView", "include/cvision/widgets/progress_tasks.hpp", Traits{.text = true});
    for (const auto name : {"normal", "detailed", "solid", "block", "segmented", "bounce", "pulse", "ascii", "static", "columns", "weighted", "failed", "paused", "completed", "collapsed", "wide", "narrow", "disabled", "empty"}) {
        state(tasks, name, Size{84, 14}, [name](Stage& stage) {
            auto& body = stage.dialog(Rect{1, 1, 80, 11}, "ProgressView");
            auto& owner = stage.place(body, Rect{1, 1, 76, 7}, std::make_unique<TaskSpecimen>());
            const std::string_view kind(name);
            if (kind == "detailed") owner.display->set_presentation(widgets::ProgressViewPresentation::Detailed);
            if (kind == "solid") owner.display->set_meter_presentation(widgets::ProgressPresentation::Solid);
            if (kind == "block") owner.display->set_meter_presentation(widgets::ProgressPresentation::Block);
            if (kind == "segmented") owner.display->set_meter_presentation(widgets::ProgressPresentation::Segmented);
            if (kind == "bounce") owner.display->set_activity_style(widgets::ProgressActivityStyle::Bounce);
            if (kind == "pulse") owner.display->set_activity_style(widgets::ProgressActivityStyle::Pulse);
            if (kind == "ascii") owner.display->set_glyphs(widgets::ProgressGlyphs::Ascii);
            if (kind == "static") owner.display->set_motion(widgets::ProgressMotion::Static);
            if (kind == "columns") owner.display->set_columns({true, true, true, true, true, true});
            if (kind == "weighted") owner.model.set_aggregation(owner.main, widgets::ProgressAggregation::WeightedChildren);
            if (kind == "failed") owner.model.fail(owner.pictures);
            if (kind == "paused") owner.model.pause(owner.pictures);
            if (kind == "completed" || kind == "collapsed") owner.model.complete(owner.pictures);
            if (kind == "collapsed") owner.display->set_retain_completed(false);
            if (kind == "wide" || kind == "narrow") owner.model.set_title(owner.pictures, std::string(kWideText));
            if (kind == "narrow") owner.set_bounds(Rect{1, 1, 13, 7});
            if (kind == "disabled") owner.display->set_enabled(false);
            if (kind == "empty") owner.display->set_max_rows(0);
        });
    }
}
} // namespace ckv::docgen::appearance
