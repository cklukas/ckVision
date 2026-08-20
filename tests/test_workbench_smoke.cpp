// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "workbench_app.hpp"

using ckv::ManualClock;
using ckv::ui::Application;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::workbench::WorkbenchApp workbench{app};
};
}  // namespace

CK_TEST(workbench_example_renders_text_page_and_chrome) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("Workbench") != std::string::npos);
    CK_CHECK(bytes.find("Text") != std::string::npos);
    CK_CHECK(bytes.find("Quit") != std::string::npos);
}

CK_TEST(workbench_text_page_exposes_memo_history_and_links) {
    Fixture f;
    CK_CHECK(f.workbench.memo()->wrap_mode() == ckv::widgets::WrapMode::Word);
    CK_CHECK(f.workbench.command_input()->text() == "test");
    CK_CHECK(f.workbench.text_view()->link_count() == 1);

    CK_CHECK(f.workbench.text_view()->activate_current_link());
    CK_CHECK(f.workbench.last_link() == std::string{"https://example.invalid/osc8"});
    CK_CHECK(f.workbench.text_view()->osc8_text().find("https://example.invalid/osc8") != std::string::npos);
    CK_CHECK(f.workbench.flow_view()->link_count() == 1);
    CK_CHECK(f.workbench.flow_view()->activate_current_link());
    CK_CHECK(f.workbench.last_link() == std::string{"https://example.invalid/flow"});
}

CK_TEST(workbench_data_page_contains_table_tree_list_combo_and_progress) {
    Fixture f;
    f.workbench.tabs()->set_active_index(1);
    f.app.step(0);

    CK_CHECK(f.workbench.tabs()->active_index() == 1);
    CK_CHECK(f.workbench.tree()->selected()->label == "Project");
    CK_CHECK(f.workbench.list()->selected_indices().size() == 1);
    CK_CHECK(f.workbench.combo()->selected_index() == std::optional<std::size_t>{1});
    CK_CHECK(f.workbench.progress()->fraction() > 0.6);
    CK_CHECK(f.workbench.search_box()->query() == "alpha");
    CK_CHECK(f.workbench.breadcrumb()->segments().size() == 3);

    f.workbench.table()->sort_by(1, true);
    CK_CHECK(f.workbench.table()->sort_column() == 1);
}

CK_TEST(workbench_help_page_contains_command_and_utility_components) {
    Fixture f;
    f.workbench.tabs()->set_active_index(2);
    f.app.step(0);

    // The palette lists what declares itself browsable, so the workbench's
    // own command is what it highlights -- named by key, never by number.
    CK_CHECK(f.workbench.command_palette()->highlighted_command() ==
             f.app.commands().id_for("workbench.build-project"));
    CK_CHECK(f.workbench.property_inspector()->items().size() == 2);
    CK_CHECK(f.workbench.notifications()->notifications().size() == 1);
    CK_CHECK(f.workbench.tooltip()->shown());
}
