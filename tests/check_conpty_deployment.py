# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
"""Synthetic PE fixtures prove deployment guards/layout, not native execution."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
import platform
import shutil
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zipfile


@contextmanager
def fixture_workspace(scratch):
    root = Path(tempfile.mkdtemp(prefix="deployment-", dir=scratch))
    try:
        yield root
    finally:
        # CMake can extract nested NuGet paths beyond legacy MAX_PATH.
        # Use Windows' explicit extended path for this exact owned directory
        # so cleanup does not leave a partial tree or conceal the test result.
        path = str(root.resolve())
        if os.name == "nt":
            path = "\\\\?\\" + path
        shutil.rmtree(path)


def pe(architecture, dll=False, marker=b"fixture"):
    machine = {"x86": 0x14C, "x64": 0x8664, "arm64": 0xAA64}[architecture]
    data = bytearray(128)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 64)
    data[64:68] = b"PE\0\0"
    struct.pack_into("<H", data, 68, machine)
    struct.pack_into("<H", data, 86, 0x2002 if dll else 2)
    return bytes(data) + marker


def package(path, marker=b"fixture", omit=None, wrong_dll=None, wrong_host=None):
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("Microsoft.Windows.Console.ConPTY.nuspec",
            '<package><metadata><id>Microsoft.Windows.Console.ConPTY</id>'
            '<version>1.0-test</version><license type="expression">MIT</license>'
            '</metadata></package>')
        for architecture in ("x86", "x64", "arm64"):
            archive.writestr(f"runtimes/win-{architecture}/native/conpty.dll",
                pe(wrong_dll or architecture, True, marker))
            if architecture != omit:
                archive.writestr(f"build/native/runtimes/{architecture}/OpenConsole.exe",
                    pe(wrong_host or architecture, marker=marker))
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, env, expect=None):
    started = time.monotonic()
    result = subprocess.run(command, env=env, capture_output=True, text=True)
    output = result.stdout + result.stderr
    if expect is None:
        if result.returncode:
            raise RuntimeError(f"command failed {result.returncode}: {command}\n{output}")
    elif result.returncode == 0 or expect not in output:
        raise RuntimeError(f"expected refusal {expect!r}: {command}\n{output}")
    if "--build" in command:
        if "warning" in output.lower():
            raise RuntimeError(f"fixture build was not warning-free: {command}\n{output}")
        print(f"fixture build: {command}; elapsed={time.monotonic() - started:.3f}s; exit={result.returncode}")
    return output


def project(root, module, architecture, archive, digest, image_architecture=None, destination="bin"):
    root.mkdir()
    source = root / "src"
    source.mkdir()
    (source / "main.cpp").write_text(
        "// Copyright (c) 2026 C. Klukas. All rights reserved.\n"
        "// SPDX-License-Identifier: MIT\nint main() { return 0; }\n", encoding="utf-8")
    image = root / "synthetic-app.bin"
    image.write_bytes(pe(image_architecture or architecture))
    # Deliberately synthetic target-platform/link output: these checks test
    # CMake's real target graph and package files without pretending to run a
    # Windows executable on another OS. Native adopter gates are separate.
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.25)
project(ConptyDeploymentFixture LANGUAGES CXX)
set(WIN32 TRUE)
set(CMAKE_CXX_LINK_EXECUTABLE "\\\"${{CMAKE_COMMAND}}\\\" -E copy \\\"{image.as_posix()}\\\" <TARGET>")
include([=[{module.as_posix()}]=])
add_executable(app main.cpp)
set_target_properties(app PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/out"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${{CMAKE_BINARY_DIR}}/out")
if(MSVC)
    target_compile_options(app PRIVATE /W4 /WX)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
endif()
ckvision_deploy_conpty(TARGET app ARCHIVE [=[{archive.as_posix()}]=]
    SHA256 {digest} ARCHITECTURE {architecture} INSTALL_DESTINATION [=[{destination}]=])
''', encoding="utf-8")
    return source, root / "build"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--scratch", required=True, type=Path)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--platform", default="")
    parser.add_argument("--runtime-archive", type=Path)
    parser.add_argument("--runtime-sha256")
    parser.add_argument("--native-runner", type=Path)
    args = parser.parse_args()
    if not args.scratch.is_absolute():
        raise RuntimeError("deployment tests require explicit absolute scratch")
    args.scratch.mkdir(parents=True, exist_ok=True)
    module = args.source / "cmake/CkVisionConpty.cmake"
    with fixture_workspace(args.scratch) as root:
        process_tmp = root / "process-tmp"
        process_tmp.mkdir()
        env = dict(os.environ, TMPDIR=str(process_tmp), TMP=str(process_tmp), TEMP=str(process_tmp))
        archive = root / "ConPTY.nupkg"
        digest = package(archive)
        configure_options = ["-G", args.generator]
        if args.platform:
            configure_options += ["-A", args.platform]
        native_architecture = (args.platform or platform.machine()).lower()
        native_architecture = {"amd64": "x64", "x86_64": "x64", "aarch64": "arm64",
                               "win32": "x86"}.get(native_architecture, native_architecture)
        cases = 0
        for architecture, hosts in (("x86", {"x86", "x64", "arm64"}),
                                    ("x64", {"x64", "arm64"}), ("arm64", {"arm64"})):
            source, build = project(root / architecture, module, architecture, archive, digest)
            run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env)
            manifest, = (build / "ckvision-conpty").glob("*/*/deployment.cmake")
            output = build / "out"
            output.mkdir(exist_ok=True)
            synthetic_image = output / "synthetic-app.exe"
            synthetic_image.write_bytes(pe(architecture))
            copy = [args.cmake, f"-DCKV_CONPTY_MANIFEST={manifest}",
                    f"-DCKV_CONPTY_IMAGE={synthetic_image}", "-P",
                    str(module.parent / "CkVisionConptyCopy.cmake")]
            run(copy, env)
            expected = {"conpty.dll", "Microsoft.ConPTY.LICENSE.txt", "Microsoft.ConPTY.Runtime.json"}
            expected |= {f"{host}/OpenConsole.exe" for host in hosts}
            actual = {str(path.relative_to(output)).replace("\\", "/")
                      for path in output.rglob("*") if path.is_file() and path.name != "synthetic-app.exe"}
            if actual != expected or (output / "conpty.dll").read_bytes() != pe(architecture, True):
                raise RuntimeError(f"wrong {architecture} runtime layout: {actual}")
            record = json.loads((output / "Microsoft.ConPTY.Runtime.json").read_text())
            if record != {"package": "Microsoft.Windows.Console.ConPTY", "version": "1.0-test",
                          "sha256": digest, "architecture": architecture}:
                raise RuntimeError("package identity record differs")
            prefix = root / f"install-{architecture}"
            run([args.cmake, "--install", str(build), "--config", "Release", "--prefix", str(prefix)], env)
            installed = {str(path.relative_to(prefix / "bin")).replace("\\", "/")
                         for path in (prefix / "bin").rglob("*") if path.is_file()}
            if installed != expected:
                raise RuntimeError(f"wrong installed layout: {installed}")
            for relative in expected:
                if (prefix / "bin" / relative).read_bytes() != (output / relative).read_bytes():
                    raise RuntimeError(f"installed bytes differ: {relative}")
            # Tamper with deployment while the executable is already linked:
            # the ALL target must repair it without needing a new link event.
            (output / "conpty.dll").write_bytes(b"stale")
            run(copy, env)
            if (output / "conpty.dll").read_bytes() != pe(architecture, True):
                raise RuntimeError("deployment did not restore current runtime")
            # The actual build graph is driven too. On Windows use the native
            # fixture target; on other hosts its deliberately synthetic PE link
            # rule lets the same POST_BUILD/ALL guards run without native claims.
            if os.name != "nt" or architecture == native_architecture:
                run([args.cmake, "--build", str(build), "--config", "Release"], env)
                (output / "conpty.dll").write_bytes(b"stale")
                run([args.cmake, "--build", str(build), "--config", "Release"], env)
                if (output / "conpty.dll").read_bytes() != pe(architecture, True):
                    raise RuntimeError("no-op ALL build did not repair current runtime")
            cases += 1
        for name, options, refusal in (
            ("missing-arm64-host", {"omit": "arm64"}, "runtime image is missing"),
            ("wrong-dll", {"wrong_dll": "arm64"}, "architecture mismatch"),
            ("wrong-host", {"wrong_host": "x86"}, "architecture mismatch"),
        ):
            bad = root / f"{name}.nupkg"
            pin = package(bad, **options)
            source, build = project(root / name, module, "x64", bad, pin)
            run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env, refusal)
            cases += 1
        for name, pin, destination, refusal in (
            ("wrong-hash", "0" * 64, "bin", "SHA256 mismatch"),
            ("short-hash", "abc", "bin", "explicit SHA256"),
            ("escaping-install", digest, "../escape", "contained install destination"),
        ):
            source, build = project(root / name, module, "x64", archive, pin, destination=destination)
            run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env, refusal)
            cases += 1
        source, build = project(root / "wrong-application", module, "x64", archive, digest,
                                image_architecture="arm64")
        run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env)
        manifest, = (build / "ckvision-conpty").glob("*/*/deployment.cmake")
        run([args.cmake, f"-DCKV_CONPTY_MANIFEST={manifest}",
             f"-DCKV_CONPTY_IMAGE={root / 'wrong-application/synthetic-app.bin'}", "-P",
             str(module.parent / "CkVisionConptyCopy.cmake")], env, "architecture mismatch")
        if (root / "wrong-application/conpty.dll").exists():
            raise RuntimeError("application ABI refusal copied a runtime anyway")
        cases += 1
        # The same target/archive pathname must not inherit omitted files
        # from an older extraction. Check both a post-config mutation and a
        # changed, explicitly accepted pin at the same target build directory.
        source, build = project(root / "changed-package", module, "x64", archive, digest)
        run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env)
        manifest, = (build / "ckvision-conpty").glob("*/*/deployment.cmake")
        image = root / "changed-package/synthetic-app.bin"
        changed_digest = package(archive, marker=b"changed", omit="arm64")
        run([args.cmake, f"-DCKV_CONPTY_MANIFEST={manifest}",
             f"-DCKV_CONPTY_IMAGE={image}", "-P",
             str(module.parent / "CkVisionConptyCopy.cmake")], env,
            "SHA256 mismatch after configuration")
        text = (source / "CMakeLists.txt").read_text(encoding="utf-8")
        (source / "CMakeLists.txt").write_text(text.replace(digest, changed_digest), encoding="utf-8")
        run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env,
            "runtime image is missing")
        cases += 2
        print(f"ConPTY deployment: {cases} synthetic architecture/layout/refresh/refusal cases passed")
        if args.runtime_archive or args.native_runner:
            if os.name != "nt" or not args.runtime_archive or not args.runtime_sha256 or not args.native_runner:
                raise RuntimeError("native deployment requires Windows, an approved archive/hash and exact built runner")
            source, build = project(root / "native-consumer", module, native_architecture,
                                    args.runtime_archive, args.runtime_sha256)
            run([args.cmake, "-S", str(source), "-B", str(build), *configure_options], env)
            run([args.cmake, "--build", str(build), "--config", "Release"], env)
            output = build / "out"
            runner = output / "cvision_tests.exe"
            shutil.copy2(args.native_runner, runner)
            if hashlib.sha256(runner.read_bytes()).digest() != hashlib.sha256(args.native_runner.read_bytes()).digest():
                raise RuntimeError("native runtime runner differs from its verified build")
            native_env = dict(env, CKVISION_EXPECT_MODERN_CONPTY="1")
            print(run([str(runner), "--suite", "test_windows_terminal_subsession.cpp"], native_env))


if __name__ == "__main__":
    main()
