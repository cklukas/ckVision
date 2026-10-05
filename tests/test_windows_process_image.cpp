// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_process_image.hpp"
#include "cvision/term/windows_text.hpp"
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/process_runner.hpp"
#include "cvision/testing/cktest.hpp"
#include "scratch_directory.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
using ckv::term::WindowsProcessImageState;
std::string text(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}
std::filesystem::path system_image() {
    std::array<wchar_t, 32768> value{};
    const UINT length = ::GetSystemDirectoryW(value.data(), static_cast<UINT>(value.size()));
    CK_CHECK(length > 0 && length < value.size());
    return std::filesystem::path(std::wstring(value.data(), length)) / L"cmd.exe";
}
std::vector<char> header_fixture(WORD machine = IMAGE_FILE_MACHINE_ARM64) {
    std::vector<char> bytes(512);
    IMAGE_DOS_HEADER dos{};dos.e_magic = IMAGE_DOS_SIGNATURE;dos.e_lfanew = 128;
    const DWORD signature = IMAGE_NT_SIGNATURE;
    IMAGE_FILE_HEADER coff{};
    coff.Machine = machine;coff.NumberOfSections = 1;
    coff.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE;
    coff.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    IMAGE_OPTIONAL_HEADER64 optional{};
    optional.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;optional.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
    std::memcpy(bytes.data(), &dos, sizeof(dos));
    std::memcpy(bytes.data() + 128, &signature, sizeof(signature));
    std::memcpy(bytes.data() + 132, &coff, sizeof(coff));
    std::memcpy(bytes.data() + 152, &optional, sizeof(optional));
    return bytes;
}
ckv::term::WindowsProcessImageInfo inspect_fixture(const std::filesystem::path& path, const std::vector<char>& bytes) {
    { std::ofstream stream(path, std::ios::binary | std::ios::trunc);
      stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));CK_CHECK(stream.good()); }
    return ckv::term::inspect_windows_process_image(text(path));
}
template<class T> void replace(std::vector<char>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
}

CK_TEST(windows_process_image_accepts_actual_system_image_across_caller_architectures_and_runs_it) {
    const auto image = system_image();
    const auto info = ckv::term::inspect_windows_process_image(text(image));
    CK_CHECK(info.compatible());CK_CHECK(info.machine != 0);CK_CHECK(info.native_error == 0);
    ckv::term::WindowsClock clock;
    ckv::term::NativeProcessRunner runner(clock);
    ckv::core::ProcessRunRequest request;
    request.launch = ckv::core::ProcessLaunchSpec::windows_command_processor(text(image), "echo CKV-IMAGE-PREFLIGHT & exit /b 37");
    request.launch.working_directory = text(image.parent_path());
    request.descendants = ckv::core::ProcessDescendantPolicy::TerminateOnCompletion;
    const auto result = runner.run(request);
    CK_CHECK(result.state == ckv::core::ProcessRunState::Completed);
    CK_CHECK(result.exit.has_value());if (result.exit) CK_CHECK(result.exit->code == 37);
    CK_CHECK(result.stdout_capture.bytes.find("CKV-IMAGE-PREFLIGHT") != std::string::npos);
}

CK_TEST(windows_process_image_checks_native_unicode_paths_without_path_search) {
    ckv::testing::ScratchDirectory scratch("process-image");
    const auto copy = scratch.path() / L"Grüße 日本語 & image.exe";
    std::filesystem::copy_file(system_image(), copy);
    CK_CHECK(ckv::term::inspect_windows_process_image(text(copy)).compatible());
    for (const auto& path : {std::string{}, std::string("cmd.exe"), std::string("C:cmd.exe"),
                             std::string("\\cmd.exe"), std::string("\\\\.\\pipe\\image"),
                             text(copy) + std::string("\0other", 6), std::string("\xff", 1)})
        CK_CHECK(ckv::term::inspect_windows_process_image(path).state == WindowsProcessImageState::InvalidPath);
    const auto missing = ckv::term::inspect_windows_process_image(text(scratch.path() / "absent.exe"));
    CK_CHECK(missing.state == WindowsProcessImageState::Unavailable);CK_CHECK(missing.native_error != 0);
    CK_CHECK(!ckv::term::inspect_windows_process_image(text(scratch.path())).compatible());
}

CK_TEST(windows_process_image_rejects_bounded_malformed_headers_and_dlls) {
    ckv::testing::ScratchDirectory scratch("invalid-process-image");
    const auto file = scratch.path() / "fixture.exe";
    for (int mutation = 0; mutation < 13; ++mutation) {
        auto bytes = header_fixture();
        switch (mutation) {
            case 0: bytes.resize(63);break;
            case 1: replace(bytes, 0, WORD{0});break;
            case 2: replace(bytes, 60, LONG{-1});break;
            case 3: replace(bytes, 60, LONG{0x7fffffff});break;
            case 4: replace(bytes, 128, DWORD{0});break;
            case 5: replace(bytes, 134, WORD{97});break;
            case 6: replace(bytes, 150, WORD{IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL});break;
            case 7: replace(bytes, 150, WORD{0});break;
            case 8: replace(bytes, 148, WORD{95});break;
            case 9: replace(bytes, 152, WORD{0});break;
            case 10: replace(bytes, 220, WORD{IMAGE_SUBSYSTEM_NATIVE});break;
            case 11: replace(bytes, 260, DWORD{1000});break;
            case 12: bytes.resize(391);break;
            default: break;
        }
        const auto info = inspect_fixture(file, bytes);
        CK_CHECK(info.state == WindowsProcessImageState::InvalidImage);CK_CHECK(!info.compatible());
    }
}

CK_TEST(windows_process_image_architecture_is_host_support_not_caller_bitness) {
    ckv::testing::ScratchDirectory scratch("process-architecture");
    const auto file = scratch.path() / "header.exe";
    // Synthetic headers prove inspection only, never loader execution.
    auto bytes = header_fixture(0x5032); // RISC-V32, no Windows user-mode backend here.
    CK_CHECK(inspect_fixture(file, bytes).state == WindowsProcessImageState::UnsupportedArchitecture);
    USHORT process = 0, native = 0;
    CK_CHECK(::IsWow64Process2(::GetCurrentProcess(), &process, &native));
    bytes = header_fixture(native);
    if (native == IMAGE_FILE_MACHINE_I386) {
        replace(bytes, 152, WORD{IMAGE_NT_OPTIONAL_HDR32_MAGIC});
    }
    CK_CHECK(inspect_fixture(file, bytes).compatible());
    // A 32-bit header cannot claim a known 64-bit machine.
    bytes = header_fixture(IMAGE_FILE_MACHINE_AMD64);
    replace(bytes, 152, WORD{IMAGE_NT_OPTIONAL_HDR32_MAGIC});
    CK_CHECK(inspect_fixture(file, bytes).state == WindowsProcessImageState::InvalidImage);
}
