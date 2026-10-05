// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/windows_process_image.hpp"
#include "cvision/term/windows_text.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstring>

namespace ckv::term {
namespace {
struct OwnedFile {
    HANDLE value;
    ~OwnedFile() { (void)::CloseHandle(value); }
};

bool absolute_path(std::wstring_view value) {
    const auto letter = [](wchar_t ch) { return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'); };
    const auto separator = [](wchar_t ch) { return ch == L'\\' || ch == L'/'; };
    if (value.size() >= 3 && letter(value[0]) && value[1] == L':' && separator(value[2])) return true;
    if (!value.starts_with(L"\\\\") || value.starts_with(L"\\\\.\\")) return false;
    if (value.starts_with(L"\\\\?\\")) {
        const auto rest = value.substr(4);
        if (rest.size() >= 3 && letter(rest[0]) && rest[1] == L':' && rest[2] == L'\\') return true;
        if (!rest.starts_with(L"UNC\\")) return false;
        value = rest.substr(4);
    } else value.remove_prefix(2);
    const auto server = value.find(L'\\');
    return server != std::wstring_view::npos && server > 0 && server + 1 < value.size();
}

bool read_at(HANDLE file, LONGLONG offset, void* buffer, DWORD length, DWORD& error) {
    LARGE_INTEGER position{};
    position.QuadPart = offset;
    DWORD read = 0;
    if (!::SetFilePointerEx(file, position, nullptr, FILE_BEGIN) ||
        !::ReadFile(file, buffer, length, &read, nullptr)) {
        error = ::GetLastError();
        return false;
    }
    // Short reads are malformed input, not a native call failure.
    return read == length;
}

WindowsProcessImageState architecture_state(USHORT machine, DWORD& error) {
    // Resolve dynamically: ConPTY's Windows10 baseline predates this Win11 API.
    using Attributes = HRESULT(WINAPI*)(USHORT, DWORD*);
    const auto query = reinterpret_cast<Attributes>(
        ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "GetMachineTypeAttributes"));
    if (query) {
        DWORD attributes = 0;
        const HRESULT status = query(machine, &attributes);
        if (FAILED(status)) {
            error = static_cast<DWORD>(status);
            return WindowsProcessImageState::Unavailable;
        }
        return (attributes & 1u) != 0 ? WindowsProcessImageState::Compatible
                                     : WindowsProcessImageState::UnsupportedArchitecture;
    }
    // Older OS: the native architecture is always supported. The documented
    // WOW64 x86 compatibility covers x86 on AMD64/ARM64. Do not infer x64
    // support on ARM64 from the calling process or GetNativeSystemInfo.
    USHORT process = 0, native = 0;
    if (!::IsWow64Process2(::GetCurrentProcess(), &process, &native)) {
        error = ::GetLastError();
        return WindowsProcessImageState::Unavailable;
    }
    if (machine == native || (machine == IMAGE_FILE_MACHINE_I386 &&
        (native == IMAGE_FILE_MACHINE_AMD64 || native == IMAGE_FILE_MACHINE_ARM64)))
        return WindowsProcessImageState::Compatible;
    return WindowsProcessImageState::UnsupportedArchitecture;
}
} // namespace

WindowsProcessImageInfo inspect_windows_process_image(std::string_view path) {
    WindowsProcessImageInfo result;
    if (path.empty() || path.find('\0') != std::string_view::npos) return result;
    const auto wide = windows_utf16(path);
    if (!wide || !absolute_path(*wide)) return result;
    const HANDLE raw = ::CreateFileW(wide->c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw == INVALID_HANDLE_VALUE) {
        result.state = WindowsProcessImageState::Unavailable;
        result.native_error = ::GetLastError();
        return result;
    }
    const OwnedFile file{raw};
    result.state = WindowsProcessImageState::InvalidImage;
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(raw, &size)) {
        result.state = WindowsProcessImageState::Unavailable;
        result.native_error = ::GetLastError();
        return result;
    }
    IMAGE_DOS_HEADER dos{};
    DWORD error = 0;
    const auto unreadable = [&result, &error]() {
        if (error) { result.state = WindowsProcessImageState::Unavailable; result.native_error = error; }
        return result;
    };
    if (size.QuadPart < static_cast<LONGLONG>(sizeof(dos))) return result;
    if (!read_at(raw, 0, &dos, sizeof(dos), error)) return unreadable();
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos))) return result;
    const LONGLONG nt = dos.e_lfanew;
    DWORD signature = 0;
    IMAGE_FILE_HEADER coff{};
    if (nt > size.QuadPart - static_cast<LONGLONG>(sizeof(signature) + sizeof(coff))) return result;
    if (!read_at(raw, nt, &signature, sizeof(signature), error) ||
        !read_at(raw, nt + sizeof(signature), &coff, sizeof(coff), error)) return unreadable();
    if (signature != IMAGE_NT_SIGNATURE || (coff.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0 ||
        (coff.Characteristics & IMAGE_FILE_DLL) != 0 || coff.NumberOfSections == 0 || coff.NumberOfSections > 96)
        return result;
    // Inspect only fixed fields; the declared optional/section bounds must fit
    // the file. Never allocate or read in proportion to an untrusted offset.
    std::array<std::byte, 112> optional{};
    if (coff.SizeOfOptionalHeader < 96) return result;
    const LONGLONG optional_start = nt + sizeof(signature) + sizeof(coff);
    const LONGLONG table_end = optional_start + coff.SizeOfOptionalHeader +
                              static_cast<LONGLONG>(coff.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (table_end > size.QuadPart) return result;
    const DWORD fixed_size = coff.SizeOfOptionalHeader < optional.size() ? coff.SizeOfOptionalHeader
                                                                      : static_cast<DWORD>(optional.size());
    if (!read_at(raw, optional_start, optional.data(), fixed_size, error)) return unreadable();
    WORD magic = 0, subsystem = 0;
    std::memcpy(&magic, optional.data(), sizeof(magic));
    std::memcpy(&subsystem, optional.data() + 68, sizeof(subsystem));
    const bool pe32 = magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    if (!pe32 && magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return result;
    const std::size_t directories_offset = pe32 ? 96u : 112u;
    if (coff.SizeOfOptionalHeader < directories_offset) return result;
    DWORD directories = 0;
    std::memcpy(&directories, optional.data() + directories_offset - sizeof(directories), sizeof(directories));
    if (directories > (coff.SizeOfOptionalHeader - directories_offset) / sizeof(IMAGE_DATA_DIRECTORY) ||
        (subsystem != IMAGE_SUBSYSTEM_WINDOWS_GUI && subsystem != IMAGE_SUBSYSTEM_WINDOWS_CUI)) return result;
    if ((coff.Machine == IMAGE_FILE_MACHINE_I386 || coff.Machine == IMAGE_FILE_MACHINE_ARMNT) != pe32 &&
        (coff.Machine == IMAGE_FILE_MACHINE_I386 || coff.Machine == IMAGE_FILE_MACHINE_ARMNT ||
         coff.Machine == IMAGE_FILE_MACHINE_AMD64 || coff.Machine == IMAGE_FILE_MACHINE_ARM64)) return result;
    result.machine = coff.Machine;
    result.state = architecture_state(coff.Machine, error);
    result.native_error = error;
    return result;
}
} // namespace ckv::term
