// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal_subsession.hpp"
#include "../examples/native_host.hpp"
#include "../examples/sysinfo/windows_system_probe.hpp"
#include "scratch_directory.hpp"
#include <cstdio>
#include <algorithm>
#include <fstream>

CK_TEST(windows_sysinfo_reports_native_facts_without_relabeling_commit_as_swap) {
    ckv::sysinfo::WindowsSystemProbe probe;
    const auto host = probe.host();
    CK_CHECK(host.host_name && !host.host_name->empty()); CK_CHECK(host.architecture);
    CK_CHECK(host.os_name && host.os_name->starts_with("Microsoft Windows"));
    CK_CHECK(!host.load_average_1m);
    const auto memory = probe.memory();
    CK_CHECK(memory.total_bytes && *memory.total_bytes > 0);
    CK_CHECK(memory.available_bytes && *memory.available_bytes <= *memory.total_bytes);
    CK_CHECK(!memory.swap_total_bytes && !memory.swap_used_bytes);
    const auto cpu = probe.processor();
    CK_CHECK(cpu.physical_cores && *cpu.physical_cores > 0);
    CK_CHECK(cpu.logical_cores && *cpu.logical_cores >= *cpu.physical_cores);
    if (cpu.performance_cores && cpu.efficiency_cores) CK_CHECK(*cpu.performance_cores + *cpu.efficiency_cores == *cpu.physical_cores);
    for (const auto& volume : probe.volumes()) {
        CK_CHECK(!volume.mount_point.empty());
        if (volume.capacity_bytes && volume.free_bytes) CK_CHECK(*volume.free_bytes <= *volume.capacity_bytes);
    }
    const auto power = probe.power();
    if (power.charge_percent) CK_CHECK(*power.charge_percent >= 0 && *power.charge_percent <= 100);
    CK_CHECK(probe.build().pointer_bits == static_cast<int>(sizeof(void*) * 8));
}
CK_TEST(windows_example_arguments_preserve_unicode_text_and_path_separators) {
    wchar_t app[] = L"example", value[] = L"C:\\Données\\日本", board[] = L"Board α";
    wchar_t* argv[]{app, value, board};
    const auto args = ckv::examples::native_arguments(3, argv);
    CK_CHECK(args[1] == "C:\\Données\\日本"); CK_CHECK(args[2] == "Board α");
}
CK_TEST(windows_every_interactive_example_starts_draws_and_exits_through_native_conpty) {
    ckv::testing::ScratchDirectory scratch("windows-examples");
    const auto unicode = scratch.path() / std::filesystem::path(u8"Données-日本");
    std::filesystem::create_directory(unicode);
    const auto text = unicode.generic_u8string(); const std::string root(text.begin(), text.end());
    ckv::term::WindowsClock clock;
    ckv::term::WindowsWaitSet waits(clock);
    for (const auto* name : {"ckvision_completion", "ckvision_progress", "ckvision_gallery", "ckvision_filebrowser", "ckvision_hello", "ckvision_layouts", "ckvision_forms", "ckvision_rootdialog", "ckvision_echo", "ckvision_workbench", "ckvision_graphics", "ckvision_editor", "ckvision_terminal", "ckvision_spin", "ckvision_todo", "ckvision_sysinfo", "ckvision_logmonitor", "ck-sysmon"}) {
        std::fprintf(stderr, "Native example: %s\n", name);
        const auto image = (std::filesystem::path(CKV_EXAMPLE_BINARY_DIR) / (std::string(name) + ".exe")).generic_string();
        auto spec = ckv::term::TerminalLaunchSpec::program(image);
        spec.exit_policy = ckv::core::TerminalExitPolicy::TerminateAfterGrace;
        spec.working_directory = root;
        const auto capture_path = scratch.path() / (std::string(name) + ".vt");
        spec.environment = {{"USERPROFILE", root}, {"HOME", root}, {"APPDATA", root},
            {"CKVISION_OUTPUT_CAPTURE", capture_path.generic_string()},
            {"CKVISION_GRAPHICS_LOG", ""}, {"CKVISION_GRAPHICS_LOG_OWNED", ""},
            {"CKVISION_FORCE_SIXEL", ""}, {"CKVISION_FORCE_CELL", ""}};
        if (std::string_view(name) == "ckvision_completion") spec.arguments = {root, root + "/index"};
        if (std::string_view(name) == "ckvision_todo") spec.arguments = {"--demo"};
        if (std::string_view(name) == "ckvision_filebrowser") spec.arguments = {root};
        spec.profile.cells = {100, 30};
        spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
        auto child = ckv::term::WindowsTerminalSubsession::launch(std::move(spec));
        CK_CHECK(child->state() != ckv::term::TerminalSubsessionState::Failed);
        const auto pump = [&](bool until_exit) {
            const auto deadline = clock.now_nanos() + 12'000'000'000LL;
            while (clock.now_nanos() < deadline) {
                (void)waits.wait(clock.now_nanos() + 20'000'000LL, child->wait_handles());
                (void)child->drain(64 * 1024);
                if (child->state() == ckv::term::TerminalSubsessionState::Exited) return until_exit;
                if (child->state() == ckv::term::TerminalSubsessionState::Failed) return false;
                if (!until_exit) {
                    std::size_t visible = 0;
                    for (const auto& cell : child->snapshot().cell_buffer) if (!cell.is_continuation() && !cell.grapheme().empty() && cell.grapheme() != " ") ++visible;
                    if (visible > 80) return true;
                }
            }
            return false;
        };
        const bool drew = pump(false); CK_CHECK(drew);
        if (drew) {
            if (std::string_view(name) == "ckvision_terminal") {
                // On a plain VT host, keys belong to the embedded shell.
                // Let the probe settle, then use the actual parent File menu.
                const auto settled = clock.now_nanos() + 300'000'000LL;
                while (clock.now_nanos() < settled) {
                    (void)waits.wait(std::min(settled, clock.now_nanos() + 20'000'000LL), child->wait_handles());
                    (void)child->drain(64 * 1024);
                }
                child->send_input("\x1b[<0;3;1M\x1b[<0;3;1m\x1b[F\r");
            } else child->send_input("\x1b" "x");
            CK_CHECK(pump(true));
        }
        CK_CHECK(child->exit_code() == (std::string_view(name) == "ckvision_rootdialog" ? 1 : 0));
        std::ifstream input(capture_path, std::ios::binary);
        const std::string captured((std::istreambuf_iterator<char>(input)), {});
        CK_CHECK(captured.find("\x1b[?1049l") != std::string::npos);
        waits.clear();
    }
}
