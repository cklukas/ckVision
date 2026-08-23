// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "layouts_app.hpp"

#include <memory>
#include <vector>

#include "cvision/ui/anchor_pane.hpp"
#include "cvision/ui/dock.hpp"
#include "cvision/ui/grid.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/overlay.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::layouts {

LayoutsApp::LayoutsApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_chrome();
    build_window();

    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_focus(splitter_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Layouts example",
                                "Layout containers and a splitter, shown responding to resize.");
}

void LayoutsApp::build_chrome() {
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit}));

    widgets::MenuBarItem window_menu{"&Window", {}};
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().tile}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().cascade}));

    desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file_menu), std::move(window_menu)}));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                       widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().quit}}});
    desktop_->dock_bottom(std::move(status));
}

void LayoutsApp::build_window() {
    auto window = std::make_unique<widgets::Window>("Layouts");
    window->set_bounds(Rect{2, 2, 72, 19});
    window->set_min_size(Size{28, 10});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    window->add_frame_overlay(std::make_unique<widgets::Label>("frame:start"),
                              widgets::FrameSlot{widgets::Edge::Bottom, ui::Alignment::Start, 1});
    window->add_frame_overlay(std::make_unique<widgets::Label>("frame:end"),
                              widgets::FrameSlot{widgets::Edge::Bottom, ui::Alignment::End, -1});

    ui::AnchorPane& pane = window->content_pane();

    auto row = std::make_unique<ui::Row>(Rect{1, 1, 30, 3});
    row->set_spacing(1);
    row_ = row.get();
    row->add_item(std::make_unique<widgets::Label>("Row"), ui::LayoutSpec{ui::SizePolicy::Fixed});
    row->add_item(std::make_unique<widgets::StaticText>("expands with margins"),
                  ui::LayoutSpec{ui::SizePolicy::Expanding, 1, ui::Alignment::Fill, 0, 0});
    pane.add_item(std::move(row), ui::Anchors{true, true, true, false});

    auto column = std::make_unique<ui::Column>(Rect{33, 1, 18, 7});
    column->set_spacing(1);
    column_ = column.get();
    column->add_item(std::make_unique<widgets::Label>("Column"), ui::LayoutSpec{ui::SizePolicy::Fixed});
    column->add_item(std::make_unique<widgets::StaticText>("wrapped static text participates in height-for-width"),
                     ui::LayoutSpec{ui::SizePolicy::Expanding});
    pane.add_item(std::move(column), ui::Anchors{false, true, true, false});

    auto grid = std::make_unique<ui::Grid>(Rect{1, 5, 30, 5}, 2, 3);
    grid->set_spacing(1);
    grid_ = grid.get();
    grid->add_item(std::make_unique<widgets::Label>("Grid"), ui::GridSpec{0, 0, 1, 2});
    grid->add_item(std::make_unique<widgets::Label>("A"), ui::GridSpec{0, 2, 1, 1, ui::Alignment::Center});
    grid->add_item(std::make_unique<widgets::Label>("span"), ui::GridSpec{1, 0, 1, 3, ui::Alignment::Center});
    pane.add_item(std::move(grid), ui::Anchors{true, true, true, false});

    auto dock = std::make_unique<ui::Dock>(Rect{1, 11, 30, 5});
    dock_ = dock.get();
    dock->add_item(std::make_unique<widgets::Label>("Dock top"), ui::DockEdge::Top);
    dock->add_item(std::make_unique<widgets::Label>("left"), ui::DockEdge::Left);
    dock->add_item(std::make_unique<widgets::StaticText>("center fill"), ui::DockEdge::Center);
    pane.add_item(std::move(dock), ui::Anchors{true, false, true, true});

    auto overlay = std::make_unique<ui::Overlay>(Rect{33, 9, 18, 5});
    overlay_ = overlay.get();
    overlay->add_item(std::make_unique<widgets::StaticText>("Overlay base"), ui::OverlayMode::Fill);
    auto badge = std::make_unique<widgets::Label>("badge");
    badge->set_bounds(Rect{11, 1, 5, 1});
    overlay->add_item(std::move(badge), ui::OverlayMode::Manual);
    pane.add_item(std::move(overlay), ui::Anchors{false, false, true, true});

    auto first = std::make_unique<widgets::StaticText>("Splitter left pane");
    auto second = std::make_unique<widgets::StaticText>("Splitter right pane");
    auto splitter = std::make_unique<widgets::Splitter>(Rect{52, 1, 17, 13}, std::move(first), std::move(second));
    splitter_ = splitter.get();
    pane.add_item(std::move(splitter), ui::Anchors{false, true, true, true});

    auto anchored = std::make_unique<widgets::Label>("Anchored");
    anchored->set_bounds(Rect{58, 15, 8, 1});
    anchored_label_ = anchored.get();
    pane.add_item(std::move(anchored), ui::Anchors{false, false, true, true});

    window_ = desktop_->add_window(std::move(window));
}

}  // namespace ckv::layouts
