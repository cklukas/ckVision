#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Generate an immutable-source Homebrew formula for the complete ckVision SDK and examples."""
from __future__ import annotations
import argparse
import hashlib
import re
from pathlib import Path
from urllib.parse import urlparse

def formula(version: str, url: str, digest: str) -> str:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("invalid version")
    parsed = urlparse(url)
    if parsed.scheme != "https" or not parsed.netloc or any(c in url for c in '\"\n\r\\'):
        raise ValueError("source must be an absolute HTTPS URL")
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ValueError("invalid SHA-256")
    return f'''# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
# typed: strict
# frozen_string_literal: true
class Ckvision < Formula
  desc "C++ terminal desktop framework, SDK and example applications"
  homepage "https://github.com/cklukas/ckVision"
  url "{url}"
  version "{version}"
  sha256 "{digest}"
  license "MIT"
  depends_on macos: :tahoe
  depends_on "cmake" => [:build, :test]

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DCKVISION_BUILD_TESTING=OFF", "-DCKVISION_BUILD_EXAMPLES=ON",
           *std_cmake_args, "-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0"
    system "cmake", "--build", "build", "--parallel", ENV.make_jobs.to_s
    system "cmake", "--install", "build"
  end

  test do
    assert_equal "ckvision_gallery #{{version}}", shell_output("#{{bin}}/ckvision_gallery --version").strip
    assert_path_exists include/"cvision/core/clock.hpp"
    assert_path_exists lib/"libcvision.a"
    (testpath/"CMakeLists.txt").write <<~CMAKE
      # Copyright (c) 2026 C. Klukas. All rights reserved.
      # SPDX-License-Identifier: MIT
      cmake_minimum_required(VERSION 3.25)
      project(installed_consumer LANGUAGES CXX)
      find_package(ckvision CONFIG REQUIRED)
      add_executable(consumer main.cpp)
      target_link_libraries(consumer PRIVATE ckvision::cvision)
    CMAKE
    (testpath/"main.cpp").write <<~CPP
      // Copyright (c) 2026 C. Klukas. All rights reserved.
      // SPDX-License-Identifier: MIT
      #include <cvision/core/clock.hpp>
      #include <cvision/term/headless_terminal.hpp>
      #include <cvision/ui/application.hpp>
      int main() {{
        ckv::term::HeadlessTerminal terminal({{80, 25}});
        ckv::ManualClock clock;
        ckv::ui::Application app(terminal, clock);
        app.step(0);
        return app.current_frame().size() == ckv::Size{{80, 25}} ? 0 : 1;
      }}
    CPP
    system "cmake", "-S", testpath, "-B", testpath/"build",
           "-Dckvision_DIR=#{{lib}}/cmake/ckvision", "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", testpath/"build", "--config", "Release"
    system testpath/"build/consumer"
  end
end
'''

def self_test() -> None:
    rendered = formula("1.3.0", "https://example.invalid/source.tar.gz", "0" * 64)
    assert '"--component"' not in rendered and '"--target"' not in rendered
    assert "class Ckvision" in rendered
    assert 'shell_output("#{bin}/ckvision_gallery --version")' in rendered
    assert '"--parallel", ENV.make_jobs.to_s' in rendered
    assert 'depends_on macos: :tahoe' in rendered
    assert '"-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0"' in rendered
    for args in [("x", "https://example.invalid/a", "0" * 64),
                 ("1.3.0", "http://example.invalid/a", "0" * 64),
                 ("1.3.0", "https://example.invalid/a", "bad")]:
        try:
            formula(*args)
        except ValueError:
            continue
        raise AssertionError("invalid formula accepted")

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--version")
    parser.add_argument("--source-url")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not all([args.version, args.source_url, args.archive, args.output]):
        parser.error("--version, --source-url, --archive and --output are required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(formula(args.version, args.source_url, hashlib.sha256(args.archive.read_bytes()).hexdigest()), encoding="utf-8")

if __name__ == "__main__":
    main()
