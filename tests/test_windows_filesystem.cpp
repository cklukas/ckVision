// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "windows_filesystem_test_support.hpp"
#include "scratch_directory.hpp"
#include <aclapi.h>
#include <atomic>
#include <cstddef>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/file_editor_controller.hpp"

using ckv::FileWriteStatus;
using ckv::FileWriteExpectation;
using ckv::term::WindowsFileSystem;
using ckv::testing::ScratchDirectory;
using ckv::testing::FileTestHandle;
using ckv::testing::filesystem_utf8;
using ckv::testing::filesystem_wide;

namespace {

std::string root_path(const ScratchDirectory& directory) { return filesystem_utf8(directory.path().native()); }

void no_temporary_siblings(const WindowsFileSystem& fs, const std::string& directory) {
    for (const auto& entry : fs.list_directory(directory))
        CK_CHECK(!entry.name.starts_with(".ckvision-write-") || !entry.name.ends_with(".tmp"));
}

class WriterChild final {
public:
    WriterChild(const std::wstring& file, const std::wstring& mode, const std::wstring& fingerprint,
                const std::wstring& event, const std::wstring& content) {
        const auto binary = filesystem_wide(CKV_WINDOWS_FILESYSTEM_CHILD_PATH);
        std::wstring command;
        for (const auto& value : {binary, file, mode, fingerprint, event, content}) {
            // This fixture deliberately accepts only its own non-quote,
            // non-trailing-backslash arguments; it is not a shell/CLI adapter.
            if (value.find(L'"') != std::wstring::npos || value.empty() || value.back() == L'\\')
                throw std::runtime_error("unexpected writer fixture argument");
            if (!command.empty()) command += L' ';
            command += L'"' + value + L'"';
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!::CreateProcessW(binary.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                              nullptr, nullptr, &startup, &process))
            throw std::runtime_error("cannot launch native writer fixture");
        process_ = process.hProcess;
        ::CloseHandle(process.hThread);
    }
    ~WriterChild() {
        if (::WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
            ::TerminateProcess(process_, 3);
            ::WaitForSingleObject(process_, 5000);
        }
        ::CloseHandle(process_);
    }
    WriterChild(const WriterChild&) = delete;
    WriterChild& operator=(const WriterChild&) = delete;
    DWORD result() const {
        if (::WaitForSingleObject(process_, 15000) != WAIT_OBJECT_0) return 3;
        DWORD code = 3;
        ::GetExitCodeProcess(process_, &code);
        return code;
    }
private:
    HANDLE process_ = INVALID_HANDLE_VALUE;
};

std::vector<unsigned char> security_bytes(const std::wstring& file) {
    FileTestHandle handle(::CreateFileW(file.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, 0, nullptr));
    if (!handle.valid()) return {};
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PACL dacl = nullptr;
    if (::GetSecurityInfo(handle.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                          nullptr, nullptr, &dacl, nullptr, &descriptor) != ERROR_SUCCESS) return {};
    struct Free { PSECURITY_DESCRIPTOR value; ~Free() { ::LocalFree(value); } } owner{descriptor};
    if (!dacl) return {};
    const auto* const begin = reinterpret_cast<const unsigned char*>(dacl);
    return {begin, begin + dacl->AclSize};
}

void racing_writers(bool create_only) {
    ScratchDirectory scratch("native-writers");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "shared.txt");
    std::wstring fingerprint = L"unused";
    if (!create_only) {
        const auto first = fs.write_file_atomic(file, "before");
        CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
        if (!first.fingerprint) return;
        fingerprint = filesystem_wide(first.fingerprint->value);
    }
    const auto event_name = L"Local\\ckvision-filesystem-" + std::to_wstring(::GetCurrentProcessId()) +
        L"-" + std::to_wstring(std::random_device{}());
    FileTestHandle event(::CreateEventW(nullptr, TRUE, FALSE, event_name.c_str()));
    const DWORD error = ::GetLastError();
    CK_CHECK(event.valid() && error != ERROR_ALREADY_EXISTS);
    if (!event.valid() || error == ERROR_ALREADY_EXISTS) return;
    const std::wstring mode = create_only ? L"create" : L"match";
    WriterChild first(filesystem_wide(file), mode, fingerprint, event_name, L"first");
    WriterChild second(filesystem_wide(file), mode, fingerprint, event_name, L"second");
    CK_CHECK(::SetEvent(event.get()) != 0);
    const auto left = first.result();
    const auto right = second.result();
    CK_CHECK((left == 0 && right == 2) || (left == 2 && right == 0));
    const auto contents = fs.read_file(file);
    CK_CHECK(contents && (contents->contents == "first" || contents->contents == "second"));
    no_temporary_siblings(fs, root_path(scratch));
}

}  // namespace

CK_TEST(native_filesystem_preserves_drive_and_unc_roots) {
    WindowsFileSystem fs;
    CK_CHECK(fs.normalize_path("C:\\one\\..\\two\\") == "C:/two");
    CK_CHECK(fs.normalize_path("\\\\host\\share\\one\\..\\two") == "//host/share/two");
    CK_CHECK(fs.parent("C:/one") == "C:/");
    CK_CHECK(fs.parent("C:/") == "C:/");
    CK_CHECK(fs.parent("//host/share/one") == "//host/share");
    CK_CHECK(fs.parent("//host/share") == "//host/share");
    CK_CHECK(fs.join("//host/share", "one") == "//host/share/one");
    CK_CHECK(fs.normalize_path("\\\\?\\C:\\one") == "C:/one");
    CK_CHECK(fs.normalize_path("\\\\?\\UNC\\host\\share\\one") == "//host/share/one");
    CK_CHECK(fs.is_absolute_path("C:/one"));
    CK_CHECK(fs.is_absolute_path("//host/share/one"));
    CK_CHECK(fs.is_absolute_path("\\\\?\\UNC\\host\\share\\one"));
    CK_CHECK(!fs.is_absolute_path("/one"));
    CK_CHECK(!fs.is_absolute_path("C:one"));
    CK_CHECK(!fs.is_absolute_path("//host/"));
    CK_CHECK(fs.join("C:/one", "D:/elsewhere").empty());
}

CK_TEST(native_filesystem_unicode_listing_and_recursive_directories) {
    ScratchDirectory scratch("native-list");
    WindowsFileSystem fs;
    const auto root = root_path(scratch);
    const std::string directory = fs.join(root, "\xE4\xB8\xAD-\xF0\x9F\x98\x80/sub");
    CK_CHECK(fs.create_directories(directory));
    CK_CHECK(fs.create_directories(directory));
    CK_CHECK(fs.is_directory(directory));
    CK_CHECK(fs.exists(directory));
    CK_CHECK(fs.write_file_atomic(fs.join(directory, "\xCE\xA9.txt"), "hello").status == FileWriteStatus::Ok);
    CK_CHECK(fs.create_directories(fs.join(directory, "zzz")));
    const auto entries = fs.list_directory(directory);
    CK_CHECK(entries.size() == 3); // directory, persistent lock, Unicode file
    CK_CHECK((!entries.empty() && entries.front() == ckv::FileEntry{"zzz", true}));
    bool unicode_file = false;
    for (const auto& entry : entries) {
        CK_CHECK(entry.name != "." && entry.name != "..");
        if (entry.name == "\xCE\xA9.txt") unicode_file = !entry.is_directory;
    }
    CK_CHECK(unicode_file);
    CK_CHECK(fs.list_directory(fs.join(directory, "missing")).empty());
    CK_CHECK(!fs.create_directories(fs.join(directory, "\xCE\xA9.txt/child")));
}

CK_TEST(native_filesystem_rejects_invalid_device_stream_and_wildcard_paths) {
    ScratchDirectory scratch("native-invalid");
    WindowsFileSystem fs;
    const auto root = root_path(scratch);
    for (const auto& name : {std::string("bad\0name", 8), std::string("\xFF"), std::string("data:stream"),
                             std::string("*"), std::string("CON"), std::string("NUL.txt"),
                             std::string("bad\x01name"), std::string("trailing "), std::string("trailing."),
                             std::string("COM\xC2\xB9.txt"), std::string("LPT\xC2\xB3")}) {
        const auto invalid = root + "/" + name;
        CK_CHECK(fs.normalize_path(invalid).empty());
        CK_CHECK(!fs.exists(invalid));
        CK_CHECK(!fs.is_directory(invalid));
        CK_CHECK(!fs.create_directories(invalid));
        CK_CHECK(!fs.read_file(invalid));
        CK_CHECK(!fs.fingerprint(invalid));
        CK_CHECK(fs.list_directory(invalid).empty());
        CK_CHECK(fs.write_file_atomic(invalid, "bad").status == FileWriteStatus::Error);
    }
    CK_CHECK(fs.normalize_path("\\\\.\\NUL").empty());
    CK_CHECK(fs.normalize_path("\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1").empty());
    CK_CHECK(fs.write_file_atomic(root + "/.ckvision-write.lock", "bad").status == FileWriteStatus::Error);
    CK_CHECK(fs.list_directory(root).empty());
}

CK_TEST(native_filesystem_reads_and_saves_empty_and_binary_bytes_without_conversion) {
    ScratchDirectory scratch("native-bytes");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "bytes.bin");
    const std::string bytes("\0\xFF\r\n\xE4\xB8\xAD", 7);
    const auto first = fs.write_file_atomic(file, bytes);
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    const auto read = fs.read_file(file);
    CK_CHECK(read && read->contents == bytes && read->fingerprint == first.fingerprint);
    if (!read) return;
    CK_CHECK(fs.fingerprint(file) == first.fingerprint);
    const auto empty = fs.write_file_atomic(file, {}, FileWriteExpectation::matching(read->fingerprint));
    CK_CHECK(empty.status == FileWriteStatus::Ok && empty.fingerprint != first.fingerprint);
    CK_CHECK(fs.read_file(file) && fs.read_file(file)->contents.empty());
    CK_CHECK(!fs.read_file(root_path(scratch)));
    CK_CHECK(fs.write_file_atomic(root_path(scratch), "not a directory").status == FileWriteStatus::NotFound);
    CK_CHECK(fs.write_file_atomic(fs.join(root_path(scratch), "missing/child"), "no parent").status == FileWriteStatus::NotFound);
    no_temporary_siblings(fs, root_path(scratch));
}

CK_TEST(native_filesystem_write_expectations_preserve_the_current_version_on_conflict) {
    ScratchDirectory scratch("native-expectation");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "document.txt");
    const auto absent = fs.write_file_atomic(file, "bad", FileWriteExpectation::matching({"missing"}));
    CK_CHECK(absent.status == FileWriteStatus::Conflict && !absent.fingerprint && !fs.exists(file));
    const auto first = fs.write_file_atomic(file, "first", FileWriteExpectation::must_not_exist());
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    if (!first.fingerprint) return;
    const auto create = fs.write_file_atomic(file, "bad", FileWriteExpectation::must_not_exist());
    CK_CHECK(create.status == FileWriteStatus::Conflict && create.fingerprint == first.fingerprint);
    const auto changed = fs.write_file_atomic(file, "other");
    CK_CHECK(changed.status == FileWriteStatus::Ok && changed.fingerprint != first.fingerprint);
    const auto stale = fs.write_file_atomic(file, "bad", FileWriteExpectation::matching(*first.fingerprint));
    CK_CHECK(stale.status == FileWriteStatus::Conflict && stale.fingerprint == changed.fingerprint);
    const auto remaining = fs.read_file(file);
    CK_CHECK(remaining && remaining->contents == "other");
    no_temporary_siblings(fs, root_path(scratch));
}

CK_TEST(native_filesystem_native_same_size_write_changes_the_revision) {
    ScratchDirectory scratch("native-revision");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "version.txt");
    const auto first = fs.write_file_atomic(file, "first");
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    if (!first.fingerprint) return;
    FileTestHandle writer(::CreateFileW(filesystem_wide(file).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, 0, nullptr));
    CK_CHECK(writer.valid());
    if (!writer.valid()) return;
    DWORD written = 0;
    CK_CHECK(::WriteFile(writer.get(), "other", 5, &written, nullptr) && written == 5);
    CK_CHECK(!fs.read_file(file)); // excludes an outstanding ordinary write handle
    writer.close();
    const auto next = fs.read_file(file);
    CK_CHECK(next && next->contents == "other" && next->fingerprint != first.fingerprint);
    CK_CHECK(fs.write_file_atomic(file, "bad", FileWriteExpectation::matching(*first.fingerprint)).status == FileWriteStatus::Conflict);
}

CK_TEST(native_filesystem_equal_native_timestamps_do_not_hide_changed_bytes) {
    ScratchDirectory scratch("native-same-time");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "version.txt");
    const auto first = fs.write_file_atomic(file, "first");
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    if (!first.fingerprint) return;
    FileTestHandle writer(::CreateFileW(filesystem_wide(file).c_str(), GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
    CK_CHECK(writer.valid());
    if (!writer.valid()) return;
    FILE_BASIC_INFO before{};
    CK_CHECK(::GetFileInformationByHandleEx(writer.get(), FileBasicInfo, &before, sizeof(before)) != 0);
    FILE_BASIC_INFO frozen{};
    frozen.LastWriteTime.QuadPart = -1;
    frozen.ChangeTime.QuadPart = -1;
    CK_CHECK(::SetFileInformationByHandle(writer.get(), FileBasicInfo, &frozen, sizeof(frozen)) != 0);
    DWORD written = 0;
    CK_CHECK(::WriteFile(writer.get(), "other", 5, &written, nullptr) && written == 5);
    writer.close();
    FileTestHandle observer(::CreateFileW(filesystem_wide(file).c_str(), FILE_READ_ATTRIBUTES,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr, OPEN_EXISTING, 0, nullptr));
    FILE_BASIC_INFO after{};
    CK_CHECK(observer.valid() && ::GetFileInformationByHandleEx(observer.get(), FileBasicInfo, &after, sizeof(after)));
    CK_CHECK(before.LastWriteTime.QuadPart == after.LastWriteTime.QuadPart);
    CK_CHECK(before.ChangeTime.QuadPart == after.ChangeTime.QuadPart);
    const auto changed = fs.read_file(file);
    CK_CHECK(changed && changed->contents == "other" && changed->fingerprint != first.fingerprint);
    CK_CHECK(changed && fs.fingerprint(file) == changed->fingerprint);
    CK_CHECK(fs.write_file_atomic(file, "bad", FileWriteExpectation::matching(*first.fingerprint)).status == FileWriteStatus::Conflict);
    const auto remaining = fs.read_file(file);
    CK_CHECK(remaining && remaining->contents == "other");
}

CK_TEST(native_filesystem_sharing_denial_preserves_content_and_recovers) {
    ScratchDirectory scratch("native-sharing");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "shared.txt");
    const auto first = fs.write_file_atomic(file, "before");
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    if (!first.fingerprint) return;
    FileTestHandle blocker(::CreateFileW(filesystem_wide(file).c_str(), GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, 0, nullptr));
    CK_CHECK(blocker.valid());
    CK_CHECK(fs.write_file_atomic(file, "bad").status == FileWriteStatus::Error);
    no_temporary_siblings(fs, root_path(scratch));
    blocker.close();
    const auto remaining = fs.read_file(file);
    CK_CHECK(remaining && remaining->contents == "before");
    CK_CHECK(fs.write_file_atomic(file, "after", FileWriteExpectation::matching(*first.fingerprint)).status == FileWriteStatus::Ok);
}

CK_TEST(native_filesystem_read_only_denial_preserves_content_and_recovers) {
    ScratchDirectory scratch("native-readonly");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "readonly.txt");
    CK_CHECK(fs.write_file_atomic(file, "before").status == FileWriteStatus::Ok);
    const auto native = filesystem_wide(file);
    const DWORD attributes = ::GetFileAttributesW(native.c_str());
    CK_CHECK(attributes != INVALID_FILE_ATTRIBUTES);
    if (attributes == INVALID_FILE_ATTRIBUTES) return;
    // Always restore attributes before the fixture's directory cleanup, even
    // when a regression assertion fails or a later operation throws.
    struct RestoreAttributes {
        std::wstring path;
        DWORD value;
        ~RestoreAttributes() { ::SetFileAttributesW(path.c_str(), value); }
    } restore{native, attributes};
    CK_CHECK(::SetFileAttributesW(native.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != 0);
    const auto before = fs.read_file(file);
    CK_CHECK(before && before->contents == "before");
    if (!before) return;
    CK_CHECK(fs.write_file_atomic(file, "bad", FileWriteExpectation::matching(before->fingerprint)).status == FileWriteStatus::Error);
    const auto unchanged = fs.read_file(file);
    CK_CHECK(unchanged && unchanged->contents == "before" && unchanged->fingerprint == before->fingerprint);
    CK_CHECK((::GetFileAttributesW(native.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
    no_temporary_siblings(fs, root_path(scratch));
    CK_CHECK(::SetFileAttributesW(native.c_str(), attributes) != 0);
    const auto writable = fs.fingerprint(file);
    CK_CHECK(writable.has_value());
    if (!writable) return;
    CK_CHECK(fs.write_file_atomic(file, "after", FileWriteExpectation::matching(*writable)).status == FileWriteStatus::Ok);
    const auto saved = fs.read_file(file);
    CK_CHECK(saved && saved->contents == "after");
    no_temporary_siblings(fs, root_path(scratch));
}

CK_TEST(native_filesystem_hard_links_share_a_revision_but_replacement_changes_only_one_name) {
    ScratchDirectory scratch("native-hardlink");
    WindowsFileSystem fs;
    const auto root = root_path(scratch);
    const auto original = fs.join(root, "original.txt");
    const auto alias = fs.join(root, "alias.txt");
    CK_CHECK(fs.write_file_atomic(original, "before").status == FileWriteStatus::Ok);
    CK_CHECK(::CreateHardLinkW(filesystem_wide(alias).c_str(), filesystem_wide(original).c_str(), nullptr) != 0);
    const auto before = fs.read_file(original);
    const auto linked = fs.read_file(alias);
    CK_CHECK(before && linked && before->contents == linked->contents && before->fingerprint == linked->fingerprint);
    if (!linked) return;
    CK_CHECK(fs.write_file_atomic(alias, "after", FileWriteExpectation::matching(linked->fingerprint)).status == FileWriteStatus::Ok);
    const auto unchanged = fs.read_file(original);
    const auto saved = fs.read_file(alias);
    CK_CHECK(unchanged && unchanged->contents == "before");
    CK_CHECK(saved && saved->contents == "after");
    CK_CHECK(unchanged && saved && unchanged->fingerprint != saved->fingerprint);
    CK_CHECK(fs.write_file_atomic(alias, "bad", FileWriteExpectation::matching(linked->fingerprint)).status == FileWriteStatus::Conflict);
    no_temporary_siblings(fs, root);
}

CK_TEST(native_filesystem_preserves_an_existing_protected_dacl) {
    ScratchDirectory scratch("native-security");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "protected.txt");
    CK_CHECK(fs.write_file_atomic(file, "before").status == FileWriteStatus::Ok);
    const auto native = filesystem_wide(file);
    FileTestHandle target(::CreateFileW(native.c_str(), READ_CONTROL | WRITE_DAC,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, 0, nullptr));
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD fetched = ::GetSecurityInfo(target.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                             nullptr, nullptr, &dacl, nullptr, &descriptor);
    CK_CHECK(fetched == ERROR_SUCCESS && dacl);
    if (fetched != ERROR_SUCCESS || !dacl) { if (descriptor) ::LocalFree(descriptor); return; }
    HANDLE raw_token = nullptr;
    CK_CHECK(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token));
    FileTestHandle token(raw_token);
    DWORD needed = 0;
    ::GetTokenInformation(token.get(), TokenUser, nullptr, 0, &needed);
    std::vector<std::max_align_t> token_bytes((needed + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    const bool user_fetched = ::GetTokenInformation(token.get(), TokenUser, token_bytes.data(), needed, &needed) != 0;
    CK_CHECK(user_fetched);
    if (!user_fetched) { ::LocalFree(descriptor); return; }
    const auto* const user = reinterpret_cast<const TOKEN_USER*>(token_bytes.data());
    EXPLICIT_ACCESSW deny{};
    deny.grfAccessPermissions = FILE_EXECUTE;
    deny.grfAccessMode = DENY_ACCESS;
    deny.grfInheritance = NO_INHERITANCE;
    deny.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    deny.Trustee.TrusteeType = TRUSTEE_IS_USER;
    deny.Trustee.ptstrName = reinterpret_cast<LPWSTR>(user->User.Sid);
    PACL restricted = nullptr;
    const auto restricted_result = ::SetEntriesInAclW(1, &deny, dacl, &restricted);
    CK_CHECK(restricted_result == ERROR_SUCCESS);
    if (restricted_result == ERROR_SUCCESS) {
        CK_CHECK(::SetSecurityInfo(target.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                   nullptr, nullptr, restricted, nullptr) == ERROR_SUCCESS);
        ::LocalFree(restricted);
    }
    ::LocalFree(descriptor);
    target.close();
    const auto before = security_bytes(native);
    CK_CHECK(!before.empty());
    CK_CHECK(fs.write_file_atomic(file, "after").status == FileWriteStatus::Ok);
    CK_CHECK(security_bytes(native) == before);
    const auto saved = fs.read_file(file);
    CK_CHECK(saved && saved->contents == "after");
    no_temporary_siblings(fs, root_path(scratch));
}

CK_TEST(native_filesystem_long_unicode_paths_work_without_a_host_manifest) {
    ScratchDirectory scratch("native-long");
    WindowsFileSystem fs;
    std::string directory = root_path(scratch);
    for (int i = 0; i < 12; ++i) directory += "/\xE4\xB8\xAD-abcdefghijklmnopqrst";
    CK_CHECK(directory.size() > 300);
    CK_CHECK(fs.create_directories(directory));
    const auto file = fs.join(directory, "\xF0\x9F\x98\x80.txt");
    CK_CHECK(fs.write_file_atomic(file, "long path").status == FileWriteStatus::Ok);
    CK_CHECK(fs.read_file(file) && fs.read_file(file)->contents == "long path");
    CK_CHECK(fs.is_directory(fs.parent(file)));
    CK_CHECK(fs.exists(file));
    no_temporary_siblings(fs, directory);
}

CK_TEST(native_filesystem_create_only_serializes_real_competing_processes) { racing_writers(true); }
CK_TEST(native_filesystem_matching_revision_serializes_real_competing_processes) { racing_writers(false); }

CK_TEST(native_filesystem_readers_get_whole_versions_during_atomic_replacement) {
    ScratchDirectory scratch("native-read-race");
    WindowsFileSystem fs;
    const auto file = fs.join(root_path(scratch), "racing.bin");
    const std::string original(262144, 'A');
    const auto first = fs.write_file_atomic(file, original);
    CK_CHECK(first.status == FileWriteStatus::Ok && first.fingerprint);
    if (!first.fingerprint) return;
    std::atomic<bool> done = false;
    std::atomic<bool> failed = false;
    std::map<std::string, std::string> published;
    published.emplace(first.fingerprint->value, original);
    std::vector<ckv::FileReadResult> observed;
    std::thread writer([&] {
        WindowsFileSystem independent;
        for (int revision = 0; revision < 32; ++revision) {
            std::string contents(262144, static_cast<char>('B' + revision % 20));
            contents.replace(0, 8, std::to_string(revision));
            const auto saved = independent.write_file_atomic(file, contents);
            if (saved.status != FileWriteStatus::Ok || !saved.fingerprint) { failed = true; break; }
            published.emplace(saved.fingerprint->value, std::move(contents));
        }
        done = true;
    });
    while (!done) {
        auto result = fs.read_file(file);
        if (result) observed.push_back(std::move(*result));
    }
    writer.join();
    CK_CHECK(!failed);
    CK_CHECK(!observed.empty());
    // Fingerprints may also change on link metadata updates; what must never
    // happen is one token describing two different byte versions.
    for (const auto& version : observed) {
        const auto [entry, inserted] = published.emplace(version.fingerprint.value, version.contents);
        CK_CHECK(inserted || entry->second == version.contents);
        bool complete = version.contents == original;
        for (int revision = 0; !complete && revision < 32; ++revision) {
            std::string expected(262144, static_cast<char>('B' + revision % 20));
            expected.replace(0, 8, std::to_string(revision));
            complete = version.contents == expected;
        }
        CK_CHECK(complete);
    }
    no_temporary_siblings(fs, root_path(scratch));
}

CK_TEST(native_filesystem_is_consumed_by_the_real_file_dialog_and_editor_controller) {
    ScratchDirectory scratch("native-editor");
    WindowsFileSystem fs;
    const auto root = root_path(scratch);
    const auto file = fs.join(root, "\xE4\xB8\xAD.txt");
    CK_CHECK(fs.write_file_atomic(file, "before\r\n").status == FileWriteStatus::Ok);
    ckv::term::HeadlessTerminal terminal({80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::ui::RoleRegistry registry;
    const auto roles = ckv::ui::intern_standard_roles(registry);
    std::optional<ckv::widgets::FileDialogResult> chosen;
    auto dialog = ckv::widgets::make_file_dialog(ckv::widgets::FileDialogMode::Open, root, fs, roles, app,
                                                nullptr, [&](auto result) { chosen = std::move(result); });
    app.root().add_child(std::move(dialog.window));
    app.set_focus(dialog.initial_focus);
    // Select the Unicode file through actual list events (dotdot, lock, file).
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::End, ckv::Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    CK_CHECK(!chosen);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    CK_CHECK(chosen.has_value());
    if (!chosen) return;
    CK_CHECK(chosen->path == fs.normalize_path(file));
    auto document = std::make_shared<ckv::widgets::EditorDocument>();
    ckv::widgets::FileEditorController editor(document, fs);
    CK_CHECK(editor.open(chosen->path) == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(document->text() == "before\n");
    const auto end = document->position_at_byte(6);
    CK_CHECK(end.has_value());
    if (!end) return;
    CK_CHECK(document->replace({document->begin(), *end}, "after"));
    CK_CHECK(editor.save() == ckv::widgets::EditorFileStatus::Ok);
    const auto saved = fs.read_file(file);
    CK_CHECK(saved && saved->contents == "after\r\n");
    CK_CHECK(fs.write_file_atomic(file, "external").status == FileWriteStatus::Ok);
    CK_CHECK(editor.externally_changed());
    CK_CHECK(editor.save() == ckv::widgets::EditorFileStatus::Conflict);
    const auto external = fs.read_file(file);
    CK_CHECK(external && external->contents == "external");
}
