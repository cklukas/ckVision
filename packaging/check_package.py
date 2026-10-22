#!/usr/bin/env python3
# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Check a complete ckVision package, its example programs and an external SDK consumer."""
from __future__ import annotations
import argparse
import hashlib
import platform
import subprocess
import tarfile
import zipfile
from pathlib import Path

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(*args: str) -> str:
    return subprocess.check_output(args, text=True, timeout=20).strip()

def check(package: Path, scratch: Path, version: str, installed: Path | None) -> None:
    package = package.resolve()
    checksum = package.with_name(package.name + ".sha256").read_text().split()
    if len(checksum) != 2 or checksum[0].lower() != digest(package) or checksum[1].lstrip("*") != package.name:
        raise ValueError("package checksum mismatch")
    scratch.mkdir(parents=True, exist_ok=False)
    root = scratch / "root"
    root.mkdir()
    if package.suffix == ".deb":
        if run("dpkg-deb", "-f", str(package), "Package") != "ckvision":
            raise ValueError("wrong Debian package name")
        if run("dpkg-deb", "-f", str(package), "Version") != version:
            raise ValueError("wrong Debian package version")
        if run("dpkg-deb", "-f", str(package), "Architecture") != run("dpkg", "--print-architecture"):
            raise ValueError("Debian package architecture differs from host")
        subprocess.run(["dpkg-deb", "-x", str(package), str(root)], check=True)
    elif package.suffix == ".rpm":
        if run("rpm", "-qp", "--queryformat", "%{NAME} %{VERSION}", str(package)) != "ckvision " + version:
            raise ValueError("wrong RPM package metadata")
        with subprocess.Popen(["rpm2cpio", str(package)], stdout=subprocess.PIPE) as reader:
            subprocess.run(["cpio", "--extract", "--make-directories", "--no-absolute-filenames"],
                           cwd=root, stdin=reader.stdout, check=True, stdout=subprocess.DEVNULL)
            if reader.stdout:
                reader.stdout.close()
            if reader.wait() != 0:
                raise ValueError("RPM extraction failed")
    elif package.suffix == ".zip":
        with zipfile.ZipFile(package) as archive:
            for name in archive.namelist():
                if Path(name).is_absolute() or ".." in Path(name).parts:
                    raise ValueError("unsafe archive member")
            archive.extractall(root)
    else:
        with tarfile.open(package) as archive:
            for member in archive.getmembers():
                if Path(member.name).is_absolute() or ".." in Path(member.name).parts or member.issym() or member.islnk():
                    raise ValueError("unsafe archive member")
            archive.extractall(root, filter="data")
    # The Gallery is the package's smoke test; it also locates the install prefix.
    executable_name = "ckvision_gallery.exe" if platform.system() == "Windows" else "ckvision_gallery"
    binaries = [path for path in root.rglob(executable_name) if path.is_file()]
    if len(binaries) != 1 or binaries[0].parent.name != "bin":
        raise ValueError("package must contain exactly one Gallery executable")
    prefix = binaries[0].parent.parent
    expected = {"completion", "progress", "gallery", "rootdialog", "editor_profile_sample", "editor", "terminal", "logmonitor"}
    expected |= {"filebrowser", "hello", "layouts", "forms", "echo", "workbench", "graphics", "spin", "sysinfo"}
    suffix = ".exe" if platform.system() == "Windows" else ""
    for name in expected:
        if not (prefix / "bin" / ("ckvision_" + name + suffix)).is_file():
            raise ValueError("missing example executable: " + name)
    if not (prefix / "include/cvision/core/clock.hpp").is_file():
        raise ValueError("missing public headers")
    configs = list(prefix.rglob("ckvisionConfig.cmake"))
    if len(configs) != 1 or not list(prefix.rglob("*cvision*.a")) + list(prefix.rglob("*cvision*.lib")):
        raise ValueError("missing framework library or CMake export")
    if not (prefix / "share/ckvision/examples/log_clients/CMakeLists.txt").is_file():
        raise ValueError("missing SDK client examples")
    # Compile and execute a real external consumer against the extracted package.
    consumer = scratch / "consumer"
    consumer.mkdir()
    (consumer / "CMakeLists.txt").write_text('''# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()
project(package_consumer LANGUAGES CXX)
find_package(ckvision CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ckvision::cvision)
''')
    (consumer / "main.cpp").write_text('''// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cvision/core/clock.hpp>
#include <cvision/core/version.hpp>
#include <cvision/term/headless_terminal.hpp>
#include <cvision/ui/application.hpp>
#include <cstdio>
int main() {
    ckv::term::HeadlessTerminal terminal({80,25});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal,clock);
    app.step(0);
    std::printf("%.*s\\n", static_cast<int>(ckv::version_string().size()), ckv::version_string().data());
    return app.current_frame().size() == ckv::Size{80,25} ? 0 : 1;
}
''')
    configure = ["cmake", "-S", str(consumer), "-B", str(consumer / "build"),
                 "-Dckvision_DIR=" + str(configs[0].parent)]
    if platform.system() != "Windows":
        configure += ["-DCMAKE_BUILD_TYPE=Release"]
    if platform.system() == "Windows":
        configure += ["-A", "ARM64" if platform.machine().lower() in {"arm64", "aarch64"} else "x64"]
    subprocess.run(configure, check=True, timeout=120)
    subprocess.run(["cmake", "--build", str(consumer / "build"), "--config", "Release"], check=True, timeout=120)
    candidates = [consumer / "build" / ("consumer" + suffix), consumer / "build/Release" / ("consumer" + suffix)]
    linked_version = run(str(next(p for p in candidates if p.is_file())))
    if linked_version != version:
        raise ValueError("linked library version differs from package: " + linked_version)
    executable = installed.resolve() if installed else binaries[0]
    if digest(executable) != digest(binaries[0]):
        raise ValueError("installed executable differs from package payload")
    if run(str(executable), "--version") != "ckvision_gallery " + version:
        raise ValueError("installed Gallery version differs from package")
    (scratch / "verification.txt").write_text(
        f"PASS checksum, complete SDK/examples, external linked consumer and library version, Gallery version, installed identity\n"
        f"package={package}\npackage_sha256={digest(package)}\n"
        f"executable={executable}\nexecutable_sha256={digest(executable)}\n", encoding="utf-8")
    print((scratch / "verification.txt").read_text())

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--scratch", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--installed", type=Path)
    args = parser.parse_args()
    check(args.package, args.scratch, args.version, args.installed)

if __name__ == "__main__":
    main()
