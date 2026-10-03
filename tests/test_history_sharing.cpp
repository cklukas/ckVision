// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Histories live in the application-scoped registry, shared by key across
// widgets and dialogs (the widget catalog, Controls): every history-bearing
// control names a key, and the Application it is attached to holds the list.
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;

namespace {

struct SharingFixture {
    ckv::term::HeadlessTerminal term{ckv::Size{70, 22}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::Desktop* desktop = nullptr;

    SharingFixture() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 70, 22}));
    }

    template <class T>
    static T* find_descendant(ckv::ui::View& view) {
        if (auto* typed = dynamic_cast<T*>(&view)) return typed;
        for (const auto& child : view.children())
            if (T* found = find_descendant<T>(*child)) return found;
        return nullptr;
    }

    bool press(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        const bool handled = app.dispatch(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        app.step(0);
        return handled;
    }
    void type(std::string_view text) {
        for (const char c : text) press(Key::Char, Modifier::None, std::string(1, c));
    }
};

}  // namespace

CK_TEST(an_input_line_a_combo_box_and_a_search_box_naming_one_key_share_its_entries) {
    SharingFixture f;
    auto window = std::make_unique<ckv::widgets::Window>("Find");
    window->set_bounds(ckv::Rect{1, 1, 50, 8});
    auto* placed = f.desktop->add_window(std::move(window));
    ckv::ui::View& pane = placed->content_pane();
    auto* input = pane.add(std::make_unique<ckv::widgets::InputLine>());
    input->set_bounds(ckv::Rect{0, 0, 30, 1});
    auto* search = pane.add(std::make_unique<ckv::widgets::SearchBox>());
    search->set_bounds(ckv::Rect{0, 2, 30, 1});
    auto* combo = pane.add(std::make_unique<ckv::widgets::ComboBox>(ckv::widgets::ComboBoxMode::Editable));
    combo->set_bounds(ckv::Rect{0, 4, 30, 1});
    input->set_history_key("find");
    search->set_history_key("find");
    combo->set_history_key("find");
    std::vector<std::string> queries;
    search->on_change = [&](const std::string& query) { queries.push_back(query); };

    // The input line's owner commits what was typed there...
    f.app.set_focus(input);
    f.type("alpha");
    input->commit_to_history();
    // ...and the search box, a different widget, recalls it: one list, held
    // by the application, not a copy each widget keeps.
    f.app.set_focus(&search->field());
    CK_CHECK(f.press(Key::Up));
    CK_CHECK(search->query() == "alpha");
    CK_CHECK(queries.back() == "alpha");  // a recall filters as typing does
    CK_CHECK(f.press(Key::Down));
    CK_CHECK(search->query().empty());
    // Enter records the search box's own query in the same list, and leaves
    // the key for whatever else answers it.
    f.type("beta");
    CK_CHECK(!f.press(Key::Enter));
    CK_CHECK((f.app.history().entries("find") == std::vector<std::string>{"beta", "alpha"}));

    // The combo box walks the same list, newest first.
    f.app.set_focus(&combo->focus_target());
    CK_CHECK(f.press(Key::Down));
    CK_CHECK(combo->text() == "beta");
    CK_CHECK(f.press(Key::Down));
    CK_CHECK(combo->text() == "alpha");
    // ...and the input line sees what the others added after it.
    f.app.set_focus(input);
    CK_CHECK(f.press(Key::Up));
    CK_CHECK(input->text() == "beta");
}

CK_TEST(a_descriptor_dialog_field_recalls_and_records_a_shared_history_key) {
    SharingFixture f;
    f.app.history().record("find", "older");
    ckv::widgets::FieldDescriptor field{"&Find:", "", nullptr};
    field.history_key = "find";
    ckv::widgets::DialogDescriptor descriptor{"Find", {field}, {{"OK", ckv::widgets::ButtonRole::Accept, nullptr}}};
    auto presentation = ckv::widgets::present_modal_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.app.step(0);
    auto* field_input = dynamic_cast<ckv::widgets::InputLine*>(f.app.focused());
    CK_CHECK(field_input != nullptr);
    if (field_input == nullptr) return;
    CK_CHECK(f.press(Key::Up));
    CK_CHECK(field_input->text() == "older");
    CK_CHECK(f.press(Key::Down));
    f.type("newer");
    CK_CHECK(f.press(Key::Enter));  // accepts the dialog
    CK_CHECK(presentation.completed());
    CK_CHECK((f.app.history().entries("find") == std::vector<std::string>{"newer", "older"}));
}

CK_TEST(file_dialogs_naming_one_recent_locations_key_share_it_through_the_application) {
    SharingFixture f;
    ckv::MemoryFileSystem fs;
    fs.add_directory("/home/docs");
    fs.add_file("/home/docs/a.txt");
    ckv::widgets::FileDialogOptions options;
    options.recent_locations_key = "files";

    // Accepting one dialog, from its path field, records the directory it
    // was showing in the application's list under the key...
    auto first = ckv::widgets::present_modal_file_dialog(ckv::widgets::FileDialogMode::Open, "/home/docs", fs, options,
                                                         f.app, *f.desktop, f.roles);
    f.app.step(0);
    ckv::widgets::InputLine* const path = SharingFixture::find_descendant<ckv::widgets::InputLine>(f.app.root());
    CK_CHECK(path != nullptr);
    if (path == nullptr) return;
    f.app.set_focus(path);
    CK_CHECK(f.press(Key::Enter));
    CK_CHECK(first.completed());
    CK_CHECK((f.app.history().entries("files") == std::vector<std::string>{"/home/docs"}));

    // ...which another dialog, built later and elsewhere, lists at the top.
    auto second = ckv::widgets::make_file_dialog(ckv::widgets::FileDialogMode::Save, "/home", fs, options, f.roles,
                                                 f.app, nullptr, nullptr);
    auto* list = dynamic_cast<ckv::widgets::ListView*>(second.initial_focus);
    CK_CHECK(list != nullptr);
    if (list != nullptr) CK_CHECK(list->items().front() == "Recent: /home/docs");
}
