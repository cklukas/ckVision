// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

#include "cvision/core/directory_service.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/button.hpp"

namespace ckv::testing {

// Deterministic injected source: only a small scripted MemoryFileSystem is
// consumed here. Native scale/blocked-I/O contracts have separate real workers.
class ScriptedDirectoryService final : public DirectoryService {
public:
    MemoryFileSystem fs;
    DirectoryQuery query;
    std::shared_ptr<const DirectorySnapshot> result;
    bool automatic = true;
    bool closed = false;
    class Session final : public DirectorySession {
    public:
        explicit Session(ScriptedDirectoryService& owner) : owner_(owner) {}
        ~Session() override { owner_.closed = true; }
        void request(DirectoryQuery value) override {
            owner_.query = std::move(value);
            if (owner_.automatic) owner_.deliver();
        }
        std::shared_ptr<const DirectorySnapshot> snapshot() const noexcept override { return owner_.result; }
    private:
        ScriptedDirectoryService& owner_;
    };
    std::unique_ptr<DirectorySession> open_session() override { closed = false; return std::make_unique<Session>(*this); }
    const FileSystem& filesystem() const noexcept override { return fs; }
    void deliver() { deliver(query); }
    void deliver(const DirectoryQuery& value) {
        auto response = std::make_shared<DirectorySnapshot>();
        response->revision = value.revision; response->source_generation = value.source_generation;
        response->state = DirectoryState::Ready;
        if (value.operation == DirectoryOperation::Resolve) {
            const auto path = fs.resolve_directory(value.path);
            response->resolved_directory = path.has_value();
            response->path = path ? *path : fs.normalize_path(value.path);
        } else {
            const auto directory = fs.resolve_directory(value.directory);
            if (!directory) { response->state = DirectoryState::Failed; response->diagnostic = "Directory is unavailable"; }
            else {
                response->directory = *directory;
                for (const auto& entry : fs.list_directory(*directory)) {
                    DirectoryItem item{++response->discovered, entry.name, entry.is_directory};
                    if (directory_item_matches(item, value)) response->entries.push_back(std::move(item));
                }
                std::sort(response->entries.begin(), response->entries.end(), directory_item_less);
                response->matching = response->entries.size();
                if (!response->entries.empty()) { response->first = response->entries.front(); response->last = response->entries.back(); }
            }
        }
        result = std::move(response);
    }
};

template <class T> T* path_dialog_child(ui::View& view) {
    if (auto* typed = dynamic_cast<T*>(&view)) return typed;
    for (const auto& child : view.children()) if (auto* result = path_dialog_child<T>(*child)) return result;
    return nullptr;
}
inline widgets::Button* path_dialog_button(ui::View& view, std::string_view label) {
    if (auto* button = dynamic_cast<widgets::Button*>(&view); button && button->text() == label) return button;
    for (const auto& child : view.children()) if (auto* result = path_dialog_button(*child, label)) return result;
    return nullptr;
}
struct PathDialogFixture {
    ScriptedDirectoryService source;
    term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    widgets::Desktop* desktop = nullptr;
    widgets::Window* window = nullptr;
    widgets::InputLine* field = nullptr;
    widgets::ListView* list = nullptr;
    std::vector<widgets::FileDialogResult> results;
    PathDialogFixture() {
        source.fs.add_directory("/home/user/docs/nested");
        source.fs.add_directory("/home/images");
        source.fs.add_file("/home/user/report.txt");
        source.fs.add_file("/home/user/report.csv");
        source.fs.add_file("/home/user/.secret.txt");
        app.theme() = ui::make_classic_theme(app.roles(), roles);
        auto owned = std::make_unique<widgets::Desktop>(); desktop = owned.get();
        app.root().add_child(std::move(owned));
    }
    void open(widgets::FileDialogOptions options = {}) {
        auto handle = widgets::make_file_dialog(widgets::FileDialogMode::Open, "/home/user", source,
            std::move(options), roles, app, nullptr, [this](widgets::FileDialogResult result) { results.push_back(std::move(result)); });
        attach(std::move(handle));
    }
    void attach(widgets::WindowHandle handle) {
        window = handle.window.get();
        field = path_dialog_child<widgets::InputLine>(*window);
        list = path_dialog_child<widgets::ListView>(*window);
        desktop->present_modeless(std::move(handle), app);
        tick();
    }
    void tick() { clock.advance(20'000'000); app.step(clock.now_nanos()); }
    void type(std::string value) { field->set_text(std::move(value)); tick(); }
    void enter(std::string value) { field->set_text(std::move(value)); app.dispatch(KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}); tick(); }
    std::vector<std::string> names() const {
        std::vector<std::string> output;
        for (std::size_t i = 0; i < list->model()->item_count(); ++i) output.push_back(list->model()->item_at(i).text);
        return output;
    }
};
} // namespace ckv::testing
