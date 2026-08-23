// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/theme.hpp"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {

ckv::Style style_a() {
    return ckv::Style{ckv::Color::rgb(1, 2, 3), ckv::Color::rgb(4, 5, 6), ckv::Attr{}};
}

ckv::Style style_b() {
    return ckv::Style{ckv::Color::rgb(9, 8, 7), ckv::Color::rgb(6, 5, 4), ckv::Attr::Bold};
}

}  // namespace

int main() {
    const pid_t child = ::fork();
    if (child < 0) return 1;

    if (child == 0) {
        ckv::ui::RoleRegistry registry;
        registry.intern("ckv.button.normal", style_a());
        registry.intern("ckv.button.normal", style_b());
        ::_exit(1);
    }

    int status = 0;
    if (::waitpid(child, &status, 0) != child) return 1;
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT ? 0 : 1;
}
