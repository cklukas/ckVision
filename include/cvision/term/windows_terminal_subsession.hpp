// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Private ConPTY child-session adapter for the D-042 terminal boundary.
#pragma once

#include "cvision/core/export.hpp"

#if defined(_WIN32)

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "cvision/term/terminal_emulator.hpp"

namespace ckv::term {

// A child process on a private pseudoconsole (ConPTY), its output decoded by an owned
// TerminalEmulator and its process tree held in a job object so no descendant outlives the
// session. Overlapped pipes carry output and input; the wait handles are whichever of the
// read event, the write event, the process and a pending pseudoconsole-close thread can
// currently signal. Model queries forward to the emulator.
class CVISION_TYPE_API WindowsTerminalSubsession final : public TerminalSubsession {
public:
    // Starts `spec`'s program and returns the session; never null. An app-local conpty.dll
    // beside the executable is preferred when present; without it the system ConPTY is used
    // and Sixel is removed from the session's profile. A missing exit policy, an unusable
    // conpty.dll (present but not loadable) or a failed start yields a
    // session in the Failed state with the reason in its diagnostics().
    CVISION_MEMBER_API static std::unique_ptr<WindowsTerminalSubsession> launch(TerminalLaunchSpec spec,
                                                               TerminalSubsessionOptions options = {});
    // Closes the session (see close()), which may wait for the child to exit.
    CVISION_MEMBER_API ~WindowsTerminalSubsession() override;

    // Not copyable: the session owns the child, its job object and the pipe handles.
    CVISION_MEMBER_API WindowsTerminalSubsession(const WindowsTerminalSubsession&) = delete;
    CVISION_MEMBER_API WindowsTerminalSubsession& operator=(const WindowsTerminalSubsession&) = delete;

    TerminalSnapshot snapshot() const override { return emulator_.snapshot(); }
    // The partial snapshot TerminalEmulator::snapshot(options) takes, forwarded so a host
    // holding this session need not reach the emulator.
    TerminalSnapshot snapshot(TerminalSnapshotOptions options) const override { return emulator_.snapshot(options); }
    TerminalStatus status() const override { return emulator_.status(); }
    std::span<const Cell> cells() const noexcept override { return emulator_.cells(); }
    std::span<const Cell> scrollback() const noexcept override { return emulator_.scrollback(); }
    std::span<const TerminalRaster> rasters() const noexcept override { return emulator_.rasters(); }
    std::span<const TerminalDiagnostic> diagnostics() const noexcept override { return emulator_.diagnostics(); }
    std::vector<TerminalPrinterJob> take_printer_jobs() override { return emulator_.take_printer_jobs(); }
    void set_printer_policy(TerminalPrinterPolicy policy) override { emulator_.set_printer_policy(policy); }
    void set_printer_spool_limit(std::size_t bytes) override { emulator_.set_printer_spool_limit(bytes); }
    const TerminalDamage& damage() const noexcept override { return emulator_.damage(); }
    void clear_damage() noexcept override { emulator_.clear_damage(); }
    bool synchronized_output_active() const noexcept override { return emulator_.synchronized_output_active(); }
    const TerminalCapabilityProfile& profile() const noexcept override { return emulator_.profile(); }
    void feed_output(std::string_view bytes) override { emulator_.feed_output(bytes); }
    void set_raster_identity(int identity) noexcept override { emulator_.set_raster_identity(identity); }
    // Forwarded to the emulator that decodes this child's graphics.
    void set_graphics_trace(GraphicsTrace trace) noexcept override { emulator_.set_graphics_trace(trace); }
    CVISION_MEMBER_API void resize(Size cells, PixelSize cell_pixels) override;
    CVISION_MEMBER_API void send_input(std::string_view bytes) override;
    std::string take_pending_input() override { return emulator_.take_pending_input(); }
    TerminalSubsessionState state() const noexcept override { return emulator_.state(); }
    CVISION_MEMBER_API bool drain(std::size_t byte_budget) override;
    // Sends Control-C through private ConPTY once, without waiting for exit.
    CVISION_MEMBER_API void request_termination() noexcept override;
    // Ends the owned job (root and descendants), leaving drain/close to
    // observe exit and release the transport. Never waits for the process.
    CVISION_MEMBER_API void request_kill() noexcept override;
    CVISION_MEMBER_API void close() noexcept override;
    std::span<const WaitHandle> wait_handles() const noexcept override {
        return std::span<const WaitHandle>(wait_handles_.data(), wait_handle_count_);
    }
    core::ProcessId process_id() const noexcept override {
        return process_id_ == 0 ? -1 : static_cast<core::ProcessId>(process_id_);
    }
    // Observes this session's owned native job, including its descendants.
    CVISION_MEMBER_API core::ProcessResources process_resources() const noexcept override;
    // The child's exit status once its exit has been observed; nullopt before that, and for a
    // session that never started.
    std::optional<int> exit_code() const noexcept { return emulator_.exit_code(); }

private:
    struct Native;

    explicit WindowsTerminalSubsession(TerminalLaunchSpec spec, TerminalSubsessionOptions options,
                                     std::unique_ptr<Native> native);
    bool spawn();
    bool open_channels();
    bool begin_read();
    bool collect_read();
    void pump_write();
    void fail_input_write(const char* stage, unsigned long error);
    void observe_exit();
    void refresh_wait_handles() noexcept;
    void release_native() noexcept;
    void fail_at(const char* stage, unsigned long code);

    TerminalLaunchSpec spec_;
    TerminalSubsessionOptions options_;
    std::unique_ptr<Native> native_;
    TerminalEmulator emulator_;
    std::uint32_t process_id_ = 0;
    std::array<char, 4096> read_buffer_{};
    std::size_t read_size_ = 0;
    std::size_t read_offset_ = 0;
    std::string write_queue_;
    std::string active_write_;
    std::string failure_reason_;
    bool read_pending_ = false;
    bool write_pending_ = false;
    bool output_closed_ = false;
    bool closed_ = false;
    bool termination_requested_ = false;
    bool kill_requested_ = false;
    std::array<WaitHandle, 4> wait_handles_{};
    std::size_t wait_handle_count_ = 0;
};

}  // namespace ckv::term

#endif
