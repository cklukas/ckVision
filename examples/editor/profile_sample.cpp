// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Compile-backed public-API example: application-owned INI profile.
#include "cvision/widgets/syntax_profile.hpp"

#include <string>
#include <string_view>

int main() {
    ckv::widgets::SyntaxProfileRegistry profiles;
    ckv::widgets::register_standard_syntax_profiles(profiles);
    const bool registered = profiles.register_profile(ckv::widgets::LanguageProfile{
        "ini", "INI",
        [](const ckv::widgets::LanguageDetectionInput& input) {
            return input.file_name.ends_with(".ini") ? ckv::widgets::LanguageDetection{90, "file suffix"}
                                                    : ckv::widgets::LanguageDetection{};
        },
        [](std::string_view line, std::string_view state) {
            ckv::widgets::SyntaxLineResult result;
            result.next_state = std::string(state);
            const std::size_t equals = line.find('=');
            if (equals != std::string_view::npos) {
                result.spans.push_back({0, equals, ckv::widgets::SyntaxTokenKind::Property});
                result.spans.push_back({equals, equals + 1U, ckv::widgets::SyntaxTokenKind::Operator});
            }
            return result;
        }});
    if (!registered) return 1;
    const auto& profile = profiles.detect({std::nullopt, "settings.ini", {}, {}});
    return profile.id == "ini" ? 0 : 1;
}
