#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Scratch-path contracts; no reference executable or external source needed."""
import os
import pathlib
import shlex
import subprocess
import tempfile
import unittest
from unittest import mock

import conform


class ScratchPaths(unittest.TestCase):
    def test_missing_or_relative_root_fails_before_creating_or_spawning(self):
        for root in ("", "relative"):
            with mock.patch.dict(os.environ, {"TMPDIR": root}), \
                    mock.patch.object(conform.tempfile, "NamedTemporaryFile") as create, \
                    mock.patch.object(conform.subprocess, "run") as run:
                with self.assertRaises(conform.HarnessError):
                    conform.reference_capture("hello", 10, 2, "fixture")
                create.assert_not_called()
                run.assert_not_called()

    def test_script_and_reference_socket_use_the_selected_root(self):
        selected = pathlib.Path(os.environ["TMPDIR"])
        self.assertTrue(selected.is_absolute() and selected.is_dir())
        with tempfile.TemporaryDirectory(dir=selected, prefix="conform-'space ") as directory:
            calls = []

            def run(argv, **kwargs):
                calls.append((argv, kwargs))
                if "display-message" in argv:
                    output = b"sleep\n"
                elif "capture-pane" in argv:
                    output = b"hello\n"
                else:
                    output = b""
                return subprocess.CompletedProcess(argv, 0, output, b"")

            factory = tempfile.NamedTemporaryFile
            with mock.patch.dict(os.environ, {"TMPDIR": directory}), \
                    mock.patch.object(conform.tempfile, "NamedTemporaryFile", wraps=factory) as create, \
                    mock.patch.object(conform.subprocess, "run", side_effect=run):
                self.assertEqual(conform.reference_capture("hello", 10, 2, "fixture"), ["hello", ""])
                self.assertEqual(create.call_args.kwargs["dir"], pathlib.Path(directory))
            for _, kwargs in calls:
                self.assertEqual(kwargs["env"]["TMUX_TMPDIR"], directory)
            command = next(argv[-1] for argv, _ in calls if "new-session" in argv)
            script_path = shlex.split(command)[1][:-1]  # shell command separator
            self.assertEqual(pathlib.Path(script_path).parent, pathlib.Path(directory))
            self.assertFalse(pathlib.Path(script_path).exists())
            self.assertEqual(list(pathlib.Path(directory).iterdir()), [])


if __name__ == "__main__":
    unittest.main()
