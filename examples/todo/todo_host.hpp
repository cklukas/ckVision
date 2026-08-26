// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::todo {

struct TodoLaunchOptions {
    bool demo = false;
    bool show_help = false;
    bool show_version = false;
    std::optional<std::string> data_directory;
    std::optional<std::string> board_name;
    friend bool operator==(const TodoLaunchOptions&, const TodoLaunchOptions&) = default;
};

struct TodoLaunchParseResult {
    std::optional<TodoLaunchOptions> value;
    std::string diagnostic;
    explicit operator bool() const noexcept { return value.has_value(); }
};

TodoLaunchParseResult parse_todo_arguments(const std::vector<std::string_view>& arguments);
std::string todo_usage();

}  // namespace ckv::todo
