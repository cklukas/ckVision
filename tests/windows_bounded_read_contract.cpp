// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Source-only draft: all results require matching normal/ASan qualification.
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cvision/term/windows_filesystem.hpp>
#include <cvision/term/windows_text.hpp>
#include "windows_bounded_read_test_hooks.hpp"
#include "windows_bounded_native_fixture.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <utility>
#include <limits>
#include <malloc.h>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace bt = ckv::bounded_test;
namespace nf = ckv::bounded_fixture;
namespace {
struct HeapGate {
    bool deny = false, sampled_first = false;
    unsigned denied = 0;
    std::size_t first_size = 0;
    bt::Context first_context{};
};
thread_local HeapGate heap;
void* allocate(std::size_t count, std::size_t alignment) {
    if (heap.deny) {
        ++heap.denied;
        if (!heap.sampled_first) {
            heap.sampled_first = true;
            heap.first_size = count;
            if (bt::active) heap.first_context = *bt::active; // POD only, once before first denial.
        }
        throw std::bad_alloc(); // Later denials never inspect a library context.
    }
    void* p = alignment ? _aligned_malloc(count ? count : 1, alignment) : std::malloc(count ? count : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
enum class Mode { Exact, Oversize, Zero, Empty, Missing, OpenError, MetadataBefore, MetadataAfter,
    ReadError, ReadAndCloseError, HashOpenError, HashCreateError, HashAppendError, HashFinishError,
    CloseErrorReuse, OomEnter, OomOpen, OomRead, OomFinish, OomClosedReuse, MetadataErrorOom,
    WriterRefused, GrowBeforeOpen, ReplaceDuringRead, ReplaceAfterFinalMetadata };
struct Plan {
    bool active = false, acted = false;
    Mode mode = Mode::Exact;
    unsigned enters = 0, exits = 0, injected = 0, real_errors = 0;
    bt::Context final{};
    alignas(8) FILE_ID_INFO source_identity{}, seed_identity{};
    bool source_identity_ok = false;
    DWORD actor_error = 0;
    bool actor_ok = false, reuse_found = false;
    std::uint64_t size_before = 0, size_after = 0;
    HANDLE old_actor = INVALID_HANDLE_VALUE, new_actor = INVALID_HANDLE_VALUE, growth_actor = INVALID_HANDLE_VALUE;
    std::array<HANDLE, 128> reuse{};
    unsigned reuse_count = 0;
    std::wstring source_path, replacement_path, retired_path, sentinel_path;
    std::vector<std::max_align_t> retire_info, install_info;
};
Plan plan;
void require(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
nf::Proof seed(const std::wstring& path, std::string_view bytes) {
    const auto proof = nf::create_and_prove(path,bytes);
    if (!proof.ok()) std::printf("SETUP_FAIL stage=%u native=%lu written=%llu readback=%llu close_attempts=%u close_ok=%d\n",
        static_cast<unsigned>(proof.first_stage),proof.first_error,static_cast<unsigned long long>(proof.written),
        static_cast<unsigned long long>(proof.readback),proof.close_attempts,proof.close_ok);
    require(proof.ok(),"native CREATE_NEW/write/flush/byte/size/checkedclose/exact-name setup");
    return proof;
}
bool same_id(const FILE_ID_INFO& a, const FILE_ID_INFO& b) noexcept {
    return a.VolumeSerialNumber == b.VolumeSerialNumber && std::memcmp(a.FileId.Identifier, b.FileId.Identifier, 16) == 0;
}
bool close(HANDLE& value) noexcept { if (value == INVALID_HANDLE_VALUE) return true; const HANDLE consumed = std::exchange(value, INVALID_HANDLE_VALUE); return ::CloseHandle(consumed) != FALSE; }
HANDLE attributes(const std::wstring& path, DWORD access = FILE_READ_ATTRIBUTES) {
    return ::CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}
std::vector<std::max_align_t> rename_buffer(const std::wstring& target) {
    const auto bytes = offsetof(FILE_RENAME_INFO, FileName) + (target.size() + 1) * sizeof(wchar_t);
    std::vector<std::max_align_t> result((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    auto* info = ::new(result.data()) FILE_RENAME_INFO{};
    info->ReplaceIfExists = FALSE; info->RootDirectory = nullptr;
    info->FileNameLength = static_cast<DWORD>(target.size() * sizeof(wchar_t));
    std::memcpy(info->FileName, target.c_str(), (target.size()+1)*sizeof(wchar_t));
    return result;
}
bool rename_once(HANDLE h, std::vector<std::max_align_t>& buffer) noexcept {
    auto* info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
    const DWORD bytes = static_cast<DWORD>(offsetof(FILE_RENAME_INFO, FileName) + info->FileNameLength + sizeof(wchar_t));
    return ::SetFileInformationByHandle(h, FileRenameInfo, info, bytes) != FALSE;
}
void replace_actor() noexcept {
    plan.acted = true;
    // External namespace actor only. Two no-replace renames, NOT an atomic
    // SDK publication fallback or a weakened publisher implementation.
    plan.actor_ok = rename_once(plan.old_actor, plan.retire_info) && rename_once(plan.new_actor, plan.install_info);
    if (!plan.actor_ok) plan.actor_error = ::GetLastError();
}
void reuse_actor(std::uintptr_t old) noexcept {
    plan.acted = true;
    for (auto& value : plan.reuse) {
        value = attributes(plan.sentinel_path);
        if (value == INVALID_HANDLE_VALUE) { plan.actor_error = ::GetLastError(); break; }
        ++plan.reuse_count;
        if (reinterpret_cast<std::uintptr_t>(value) == old) {
            alignas(8) FILE_ID_INFO identity{};
            plan.reuse_found = ::GetFileInformationByHandleEx(value, FileIdInfo, &identity, sizeof(identity)) &&
                               plan.source_identity_ok && !same_id(identity, plan.source_identity);
            break;
        }
    }
}
void cleanup_actors() {
    bool okay = close(plan.old_actor);
    okay = close(plan.new_actor) && okay;
    okay = close(plan.growth_actor) && okay;
    for (unsigned i = 0; i < plan.reuse_count; ++i) {
        alignas(8) FILE_ID_INFO identity{};
        // A repeated source close would have consumed a reused sentinel handle.
        okay = (::GetFileInformationByHandleEx(plan.reuse[i], FileIdInfo, &identity, sizeof(identity)) != FALSE) && okay;
        okay = close(plan.reuse[i]) && okay;
    }
    require(okay, "all actor/sentinel handles still genuine and drained once");
}
struct ActorUnwind {
    ~ActorUnwind() {
        (void)close(plan.old_actor); (void)close(plan.new_actor); (void)close(plan.growth_actor);
        for (unsigned i=0;i<plan.reuse_count;++i) (void)close(plan.reuse[i]);
    }
};
}
void* operator new(std::size_t n) { return allocate(n, 0); }
void* operator new[](std::size_t n) { return allocate(n, 0); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) { return allocate(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return allocate(n, static_cast<std::size_t>(a)); }
void operator delete(void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return allocate(n,0); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return allocate(n,0); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return allocate(n,static_cast<std::size_t>(a)); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return allocate(n,static_cast<std::size_t>(a)); } catch (...) { return nullptr; } }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { _aligned_free(p); }
namespace ckv::bounded_test {
bool inject(Context& c, Event e, std::uintptr_t, std::size_t, std::size_t count) noexcept {
    if (!plan.active) return false;
    bool fail = false;
    switch (plan.mode) {
    case Mode::OpenError: fail = e == Event::Opened; break;
    case Mode::MetadataBefore: fail = e == Event::Metadata && c.metadata_queries == 1; break;
    case Mode::MetadataAfter: case Mode::MetadataErrorOom: fail = e == Event::Metadata && c.metadata_queries == 4; break;
    case Mode::ReadError: fail = e == Event::Read && count > 0; break;
    case Mode::ReadAndCloseError: fail = (e == Event::Read && count > 0) || e == Event::Closed; break;
    case Mode::HashOpenError: fail = e == Event::HashOpen; break;
    case Mode::HashCreateError: fail = e == Event::HashCreate; break;
    case Mode::HashAppendError: fail = e == Event::HashAppend; break;
    case Mode::HashFinishError: fail = e == Event::HashFinish; break;
    case Mode::CloseErrorReuse: fail = e == Event::Closed; break;
    default: break;
    }
    if (fail) ++plan.injected;
    return fail;
}
void observe(Context& c, Event e, std::uintptr_t resource, std::size_t, std::size_t count,
             bool native_ok, std::uint32_t, bool added_failure) noexcept {
    if (!plan.active) return;
    if (!native_ok) ++plan.real_errors;
    if (e == Event::Enter) ++plan.enters;
    if (e == Event::Exit) { ++plan.exits; plan.final = c; }
    if (e == Event::Opened && native_ok) plan.source_identity_ok =
        ::GetFileInformationByHandleEx(c.source, FileIdInfo, &plan.source_identity, sizeof(plan.source_identity)) != FALSE;
    if (!plan.acted && e == Event::BeforeOpen && plan.mode == Mode::GrowBeforeOpen) {
        plan.acted = true; LARGE_INTEGER before{}, end{}; end.QuadPart = 65537;
        plan.actor_ok = ::GetFileSizeEx(plan.growth_actor, &before) &&
            ::SetFilePointerEx(plan.growth_actor,end,nullptr,FILE_BEGIN) && ::SetEndOfFile(plan.growth_actor) &&
            ::FlushFileBuffers(plan.growth_actor);
        LARGE_INTEGER after{};
        if (!plan.actor_ok || !::GetFileSizeEx(plan.growth_actor,&after)) plan.actor_error = ::GetLastError();
        plan.size_before = static_cast<std::uint64_t>(before.QuadPart); plan.size_after = static_cast<std::uint64_t>(after.QuadPart);
        if (!close(plan.growth_actor)) { plan.actor_ok = false; plan.actor_error = ::GetLastError(); }
    }
    if (!plan.acted && e == Event::Opened && native_ok && plan.mode == Mode::WriterRefused) {
        plan.acted = true;
        HANDLE writer = attributes(plan.source_path, FILE_APPEND_DATA);
        plan.actor_error = writer == INVALID_HANDLE_VALUE ? ::GetLastError() : ERROR_SUCCESS;
        plan.actor_ok = writer == INVALID_HANDLE_VALUE && plan.actor_error == ERROR_SHARING_VIOLATION;
        if (writer != INVALID_HANDLE_VALUE) (void)close(writer);
    }
    if (!plan.acted && ((plan.mode == Mode::ReplaceDuringRead && e == Event::Read && count > 0) ||
                       (plan.mode == Mode::ReplaceAfterFinalMetadata && e == Event::FinalMetadata))) replace_actor();
    if (!plan.acted && e == Event::Closed && native_ok &&
        (plan.mode == Mode::CloseErrorReuse || plan.mode == Mode::OomClosedReuse)) reuse_actor(resource);
    if ((plan.mode == Mode::OomEnter && e == Event::Enter) ||
        (plan.mode == Mode::OomOpen && e == Event::Opened && native_ok) ||
        (plan.mode == Mode::OomRead && e == Event::Read && count > 0) ||
        (plan.mode == Mode::OomFinish && e == Event::HashFinish) ||
        (plan.mode == Mode::OomClosedReuse && e == Event::Closed) ||
        (plan.mode == Mode::MetadataErrorOom && e == Event::Metadata && added_failure)) heap.deny = true;
}
}
namespace {
void one(ckv::term::WindowsFileSystem& fs, const std::wstring& directory, Mode mode, unsigned serial) {
    heap = {}; plan = {};
    ActorUnwind native_actor_cleanup;
    const auto file = directory + L"\\case-猫-😀-" + std::to_wstring(serial) + L".bin";
    plan.source_path = nf::extended(file); plan.replacement_path = nf::extended(file + L".replacement");
    plan.retired_path = nf::extended(file + L".retired"); plan.sentinel_path = nf::extended(file + L".sentinel");
    std::string payload(mode == Mode::Empty ? 0 : mode == Mode::GrowBeforeOpen ? 64 : 131073, 'o');
    if (mode != Mode::Missing) plan.seed_identity = seed(file,payload).identity;
    const bool replacement = mode == Mode::ReplaceDuringRead || mode == Mode::ReplaceAfterFinalMetadata;
    if (replacement) {
        (void)seed(plan.replacement_path,"replacement");
        plan.retire_info = rename_buffer(plan.retired_path); plan.install_info = rename_buffer(plan.source_path);
        plan.old_actor = attributes(plan.source_path,DELETE | FILE_READ_ATTRIBUTES);
        plan.new_actor = attributes(plan.replacement_path,DELETE | FILE_READ_ATTRIBUTES);
        require(plan.old_actor != INVALID_HANDLE_VALUE && plan.new_actor != INVALID_HANDLE_VALUE,"genuine DELETE-only actors");
    }
    if (mode == Mode::GrowBeforeOpen) {
        plan.growth_actor = attributes(plan.source_path,GENERIC_WRITE | FILE_READ_ATTRIBUTES);
        require(plan.growth_actor != INVALID_HANDLE_VALUE,"pre-open growth actor");
    }
    if (mode == Mode::CloseErrorReuse || mode == Mode::OomClosedReuse)
        (void)seed(plan.sentinel_path,"sentinel");
    const auto input = ckv::term::windows_utf8(file);
    require(input.has_value(),"fixture UTF16 to public valid UTF8");
    std::optional<ckv::FileReadResult> original;
    if (replacement) { original = fs.read_file_bounded(*input,payload.size()); require(original.has_value(),"old snapshot precondition"); }
    const std::size_t cap = mode == Mode::Oversize || mode == Mode::GrowBeforeOpen ? 65536 : mode == Mode::Zero ? 0 : payload.size();
    plan.mode = mode; plan.active = true;
    std::optional<ckv::FileReadResult> result; bool oom = false, unexpected = false;
    try { result = fs.read_file_bounded(*input,cap); }
    catch (const std::bad_alloc&) { oom = true; }
    catch (...) { unexpected = true; }
    heap.deny = false; plan.active = false; // Only now may reporting/checks allocate.
    const auto& c = plan.final;
    bool actors_ok = true;
    try { cleanup_actors(); } catch (...) { actors_ok = false; }
    require(!unexpected && actors_ok,"normal unwinding and genuine actor drain");
    require(plan.real_errors == (mode == Mode::Missing ? 1u : 0u),"real native failures never hidden by added-failure controls");
    require(plan.enters == 1 && plan.exits == 1,"actual API installed seam; no caller scope");
    require(c.opens == (mode == Mode::Missing || mode == Mode::OomEnter ? 0u : 1u),"genuine source opens");
    require(c.close_attempts == c.opens && (c.opens == 0 || c.real_close_ok),"exactly one real source close before API terminal");
    if (mode == Mode::Missing) require(c.first_domain == bt::Domain::Win32 && c.first_event == bt::Event::Opened &&
        !c.first_injected && c.first_code == ERROR_FILE_NOT_FOUND,"real missing-file native tuple retained without injection");
    require(c.opens == 0 || (plan.source_identity_ok && same_id(plan.source_identity,plan.seed_identity)),
            "actual consumed reader handle belongs to independently seeded native object");
    require(c.hash_destroys == c.hash_creates && c.algorithm_closes == c.hash_opens,"native crypto consumers drained");
    require(c.retained <= cap && (cap == std::numeric_limits<std::size_t>::max() || c.consumed <= cap+1),"observed retained/consumed budget");
    require(c.max_request <= 65536,"actual native request bound");
    const bool oom_mode = mode == Mode::OomEnter || mode == Mode::OomOpen || mode == Mode::OomRead ||
        mode == Mode::OomFinish || mode == Mode::OomClosedReuse || mode == Mode::MetadataErrorOom;
    if (oom_mode) require(oom && heap.denied >= 1,"persistent real C++ allocation denial/propagation, not terminate or zero-denial probe");
    else require(!oom,"no unexpected allocation failure");
    if (mode == Mode::ReadAndCloseError) require(plan.injected == 2 && c.first_event == bt::Event::Read,"read error remains first over close error");
    else if (mode >= Mode::OpenError && mode <= Mode::CloseErrorReuse) require(plan.injected == 1,"real-success failure hook executed exactly once");
    if (mode >= Mode::OpenError && mode <= Mode::CloseErrorReuse) {
        const bool crypto = mode >= Mode::HashOpenError && mode <= Mode::HashFinishError;
        require(c.first_injected && c.first_domain == (crypto ? bt::Domain::Cng : bt::Domain::Win32) &&
            c.first_code == (crypto ? 0xC0000185u : ERROR_READ_FAULT),"actual failure-only first tuple retained");
    } else if (mode != Mode::MetadataErrorOom) require(plan.injected == 0,"unfaulted/race/OOM modes cannot pass through an unintended injected rejection");
    if (mode == Mode::MetadataErrorOom) require(plan.injected == 1 && c.first_event == bt::Event::Metadata && c.first_injected &&
        heap.first_context.first_event == c.first_event && heap.first_context.first_code == c.first_code,"native tuple survives first-denied allocation and finalizers");
    if (mode == Mode::CloseErrorReuse || mode == Mode::OomClosedReuse) require(plan.reuse_found,"bounded real HANDLE reuse occurred; no unsupported/pass shortcut");
    if (mode == Mode::WriterRefused) require(plan.actor_ok && plan.actor_error == ERROR_SHARING_VIOLATION,"genuine sharing refusal; not in-flight growth proof");
    if (mode == Mode::GrowBeforeOpen) require(plan.actor_ok && plan.size_before == 64 && plan.size_after == 65537 && c.consumed == cap+1,"genuine pre-open growth and sentinel rejection");
    if (replacement) {
        require(plan.acted && plan.actor_ok,"both real no-replace namespace actor operations completed");
        const auto current = fs.read_file_bounded(*input,32);
        require(current && current->contents == "replacement" && current->fingerprint != original->fingerprint,"replacement current identity/content distinct");
        if (mode == Mode::ReplaceAfterFinalMetadata) require(result && result->contents == payload && result->fingerprint == original->fingerprint,"coherent held-old-object snapshot");
        else {
            const auto& a = c.metadata[0]; const auto& z = c.metadata[1];
            require(a.complete && z.complete,"both actual metadata observations completed");
            const bool equal = same_id(a.identity,z.identity) && a.size == z.size &&
                a.write_time == z.write_time && a.change_time == z.change_time;
            if (equal) require(result && result->contents == payload && result->fingerprint == original->fingerprint,
                               "unchanged observed metadata permits only coherent held-old snapshot");
            else require(!result,"changed observed metadata must reject the snapshot");
        }
        const auto current_revision = fs.fingerprint(*input);
        require(current_revision && *current_revision == current->fingerprint &&
                *current_revision != original->fingerprint,
                "reader and stock fingerprint agree on the replacement; old token is distinct");
    } else if (mode == Mode::Exact || mode == Mode::Empty || mode == Mode::WriterRefused)
        require(result && result->contents == payload,"exact native bounded result");
    else require(!result,"failure/rejection cannot report a snapshot");
    std::printf("CONTROL mode=%u cap=%zu opens=%u closes=%u consumed=%zu retained=%zu capacity=%zu injected=%u denied=%u first_domain=%u first_code=%08X actor_error=%lu\n",
        static_cast<unsigned>(mode),cap,c.opens,c.close_attempts,c.consumed,c.retained,c.capacity,plan.injected,heap.denied,static_cast<unsigned>(c.first_domain),c.first_code,plan.actor_error);
}
}
namespace {
FILE_ID_INFO directory_identity(const std::wstring& directory) {
    const auto path = nf::extended(directory);
    HANDLE held = ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    require(held != INVALID_HANDLE_VALUE, "held native fixture root");
    FILE_ID_INFO id{};
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    const bool identified = ::GetFileInformationByHandleEx(held, FileIdInfo, &id, sizeof(id)) &&
        ::GetFileInformationByHandleEx(held, FileAttributeTagInfo, &attributes, sizeof(attributes)) &&
        (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    const bool closed = close(held);
    require(identified && closed, "genuine native directory identity and checked consumed close");
    return id;
}

std::wstring fresh_invocation_leaf(const std::wstring& dos_parent) {
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        std::array<unsigned char,16> nonce{};
        require(::BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()),
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0, "fresh fixture nonce");
        std::wstring leaf = L"invocation-";
        constexpr wchar_t digits[] = L"0123456789abcdef";
        for (unsigned char byte : nonce) {
            leaf += digits[byte >> 4];
            leaf += digits[byte & 15];
        }
        const auto path = nf::extended(dos_parent + L"\\" + leaf);
        if (::CreateDirectoryW(path.c_str(), nullptr)) return leaf;
        require(::GetLastError() == ERROR_ALREADY_EXISTS, "native fresh fixture directory creation");
    }
    throw std::runtime_error("fresh fixture directory collisions exhausted");
}
} // namespace

int wmain(int argc,wchar_t** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    try {
        require(argc == 2 || argc == 3, "explicit DOS_ROOT and optional UNC_ROOT required");
        const std::wstring dos = argv[1];
        require(dos.size() >= 3 && dos[1] == L':' && (dos[2] == L'\\' || dos[2] == L'/'),
                "configured ordinary absolute DOS root");
        DWORD error = 0;
        require(nf::directories(dos,error), "explicit DOS root exists natively");
        const auto dos_id = directory_identity(dos);
        const bool with_unc = argc == 3;
        if (with_unc) {
            require(nf::directories(argv[2],error), "explicit UNC root exists natively");
            require(same_id(dos_id,directory_identity(argv[2])), "configured DOS/UNC roots have identical native identity");
        }
        const auto invocation = fresh_invocation_leaf(dos);
        if (with_unc)
            require(same_id(directory_identity(dos + L"\\" + invocation),
                            directory_identity(std::wstring(argv[2]) + L"\\" + invocation)),
                    "the fresh invocation resolves to the same native directory via DOS and UNC");
        std::vector<std::wstring> roots{dos, nf::extended(dos)};
        if (with_unc) roots.emplace_back(argv[2]);
        unsigned failures = 0, cases = 0;
        for (unsigned n = 0; n < roots.size(); ++n) {
            const auto prefix = L"\\namespace-" + std::to_wstring(n);
            for (bool is_long : {false,true}) {
                auto suffix = prefix + (is_long ? L"\\long" : L"\\ordinary");
                // Compute the original long path before inserting the fresh
                // invocation component so no original padding is removed.
                if (is_long) while (roots[n].size() + suffix.size() <= 410)
                    suffix += L"\\preserved-long-猫-😀-component-xxxxxxxxxxxxxxxx";
                const auto directory = roots[n] + L"\\" + invocation + suffix;
                require(nf::directories(directory,error), "native control directories");
                ckv::term::WindowsFileSystem fs;
                for (unsigned value = 0; value <= static_cast<unsigned>(Mode::ReplaceAfterFinalMetadata); ++value) {
                    ++cases;
                    try { one(fs,directory,static_cast<Mode>(value),value); }
                    catch (const std::exception& e) {
                        heap.deny = false; plan.active = false; ++failures;
                        std::printf("CONTROL_FAIL namespace=%u long=%d mode=%u reason=%s\n",n,is_long,value,e.what());
                    }
                }
            }
        }
        std::printf("CONTROL_TERMINAL cases=%u failures=%u UNC_profiles=%u; in-flight Windows EOF growth untested\n",
                    cases,failures,with_unc ? 2u : 0u);
        return failures ? 1 : 0;
    } catch (const std::exception& e) {
        heap.deny = false;
        std::printf("CONTROL_FATAL %s\n",e.what());
        return 1;
    }
}
