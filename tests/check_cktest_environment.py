# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Drive the public runner through real process environment/CLI changes."""
import os
import subprocess
import sys


def check(binary, environment_filter, arguments, expected, status=0, copy_failure=False):
    environment = dict(os.environ)
    environment.pop("CKTEST_INJECT_COPY_FAILURE", None)
    if copy_failure:
        environment["CKTEST_INJECT_COPY_FAILURE"] = "1"
    if environment_filter is None:
        environment.pop("CKTEST_FILTER", None)
    else:
        environment["CKTEST_FILTER"] = environment_filter
    result = subprocess.run([binary, *arguments], env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
    actual = [line.rsplit(":", 1)[-1] for line in result.stdout.splitlines() if line.startswith("RUN ")]
    if result.returncode != status or actual != expected:
        raise AssertionError((result.returncode, actual, expected, result.stdout, result.stderr))
    if copy_failure and "cannot read CKTEST_FILTER" not in result.stderr:
        raise AssertionError(result.stderr)


if __name__ == "__main__":
    executable = sys.argv[1]
    check(executable, "environment_selected", [],
          ["environment_selected_changes_default", "environment_selected_survives_environment_change"])
    check(executable, "selection_that_matches_nothing", ["--filter", "cli_override"], ["cli_override_case"])
    check(executable, "", ["--case", "cli_override_case"], ["cli_override_case"])
    all_cases = ["environment_selected_changes_default", "environment_selected_survives_environment_change",
                 "other_selection_case", "cli_override_case"]
    check(executable, "", [], all_cases)
    check(executable, None, [], all_cases)
    check(executable, "selection_that_matches_nothing", [], [], status=2)
    if sys.platform == "win32":
        check(executable, "environment_selected", [], [], status=2, copy_failure=True)
    print("owned environment selection and explicit CLI overrides passed")
