// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A full in-memory Terminal for tests and headless automation
// (the architecture §4): scripted input, captured output, no real I/O.
#pragma once

#include <string>
#include <vector>

#include "cvision/term/input_decoder.hpp"
#include "cvision/term/terminal.hpp"
#include "cvision/term/virtual_display.hpp"

namespace ckv::term {

// Canonical deterministic profiles for paired graphics/fallback tests
// (D-035). Both use the same truecolor and cell-pixel geometry; only
// the graphics capability differs, so visual diffs isolate that path.
constexpr Capabilities headless_no_graphics_profile() noexcept {
    Capabilities caps = baseline_capabilities();
    caps.color_depth = ColorDepth::TrueColor;
    caps.cell_pixels = PixelSize{9, 18};
    return caps;
}

// headless_no_graphics_profile() with Sixel graphics enabled: the graphics half of the pair.
constexpr Capabilities headless_sixel_profile() noexcept {
    Capabilities caps = headless_no_graphics_profile();
    caps.sixel_graphics = true;
    return caps;
}

// A Terminal whose input is scripted and whose output is captured, with no real I/O and no
// real time. Everything written is appended to written_bytes() and fed to a VirtualDisplay
// sized to the terminal, so a test can inspect the screen a host would show. Its cell pixel
// size follows the effective capabilities, falling back to 9 x 18 when they report none.
class HeadlessTerminal final : public Terminal {
public:
    // `enable_capability_probes` controls whether raw terminal replies fed by
    // inject_bytes() may refine `caps`. It defaults on for an explicit custom
    // capability value, which is useful for deterministic probe scripts.
    // The named-profile overload below defaults it off, matching the POSIX
    // profile constructor: a curated guarantee cannot be upgraded by
    // unsolicited or unrequested terminal traffic.
    explicit HeadlessTerminal(Size size, Capabilities caps = baseline_capabilities(),
                              bool enable_capability_probes = true)
        : size_(size),
          observed_caps_(caps),
          caps_(caps),
          decoder_(caps),
          display_(size, effective_cell_pixels(caps)) {
        decoder_.set_capability_update_policy(enable_capability_probes
                                                  ? CapabilityUpdatePolicy::AcceptProbeRefinements
                                                  : CapabilityUpdatePolicy::Reject);
    }

    // Mirrors PosixTerminal's explicit curated-profile construction. Runtime
    // capability changes remain scriptable through inject_capability_change;
    // only raw probe replies are rejected by default.
    HeadlessTerminal(Size size, TerminalProfile profile)
        : HeadlessTerminal(size, capabilities_for_profile(profile), /*enable_capability_probes=*/false) {}

    Capabilities capabilities() const noexcept override { return caps_; }
    std::size_t frame_acknowledgements() const noexcept override {
        return decoder_.frame_acknowledgements();
    }
    // Mirrors PosixTerminal's live client policy so deterministic tests can
    // exercise the same capability-change contract without a real terminal.
    // Fixed metrics and color-register caps must be positive; otherwise it throws
    // std::invalid_argument and keeps the previous overrides. An effective change also
    // updates the display's cell pixel size and queues a CapabilityChangedEvent.
    void set_capability_overrides(CapabilityOverrides overrides);
    const CapabilityOverrides& capability_overrides() const noexcept { return overrides_; }
    Size size() const noexcept override { return size_; }

    // Headless never blocks on real time — it just drains whatever was
    // queued by inject_bytes()/inject_event() since the last poll().
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;
    void wake() noexcept override {}
    // Discards the diagnostic: a headless session keeps stderr clean.
    void write_diagnostic_after_restore(std::string_view) noexcept override {}

    // Appends to written_bytes() and feeds the display. Output the display refuses is a
    // contract violation: its error() is printed to stderr and the process fails its
    // assertion, in every build type.
    void write(std::string_view bytes) override;
    // Writes the same OSC 0 sequence a live backend sends (osc_title_sequence), so
    // written_bytes() holds exactly what a host would be given and the display checks it.
    void set_title(std::string_view title) override;
    void bell() override { ++bell_count_; }
    // While clipboard_write is effective, writes the OSC 52 export a live POSIX backend sends
    // (osc_clipboard_sequence); otherwise does nothing.
    ClipboardWriteResult write_clipboard(std::string_view text) override;

    // --- Scripting interface (tests / recorded scripts) -------------
    // Decodes raw host bytes (keys, mouse reports, probe replies) as if read at `now_nanos`
    // on the test's clock, and queues the resulting events for the next poll(). Split
    // sequences may span calls.
    void inject_bytes(std::string_view bytes, std::int64_t now_nanos);
    // Lets the decoder act on elapsed time at `now_nanos`, queueing any event that only a
    // timeout completes.
    void inject_timeout_check(std::int64_t now_nanos);  // resolves a pending lone ESC, if due
    // Models a definite terminal-input disconnect. Any partial paste is
    // delivered as recovered text, never reinterpreted as keys.
    void inject_input_disconnect();
    // Queues `event` for the next poll() without decoding. A CapabilityChangedEvent first
    // becomes the observed capability set (as set_capabilities()) and is queued carrying the
    // resulting effective capabilities, so client overrides stay in force.
    void inject_event(TerminalEvent event);
    // set_capabilities(caps) followed by a queued CapabilityChangedEvent carrying the
    // effective result.
    void inject_capability_change(Capabilities caps);
    // Replaces the observed capabilities silently: overrides are reapplied and the decoder
    // and display follow, but no event is queued.
    void set_capabilities(Capabilities caps) noexcept;
    // Changes the grid to `new_size` cells, clears the display, and queues a ResizeEvent.
    void resize(Size new_size) {
        size_ = new_size;
        display_.resize(new_size);
        // GCC 13/14 raises a -Wmaybe-uninitialized from inside libstdc++'s
        // push_back → construct_at when this call is inlined into a consuming
        // TU under optimisation (the std::string member of the pushed
        // TerminalEvent). It is the same post-inlining false positive already
        // audited and suppressed for the library's own files in CMakeLists.txt;
        // here the warning surfaces in every TU that inlines resize(), so the
        // suppression belongs at the trigger. A TerminalEvent cannot hold an
        // uninitialised string — every member runs a constructor — and Clang's
        // uninitialised-warning family reports nothing on the same build.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        pending_events_.push_back(TerminalEvent{ResizeEvent{new_size}});
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    }

    // Everything write() received since construction or the last clear_written(), as one
    // byte string; the view is invalidated by the next write or clear. clear_written() does
    // not touch the display.
    std::string_view written_bytes() const noexcept { return written_; }
    void clear_written() noexcept { written_.clear(); }
    // The window title as the host received it (display().window_title()): the last title
    // set, with every control and malformed byte already removed. Empty until one is set.
    const std::string& title() const noexcept { return display_.window_title(); }
    // How many times bell() has been called.
    int bell_count() const noexcept { return bell_count_; }
    // The last text write_clipboard() exported (only while clipboard_write is effective), as
    // the host decoded it (display().clipboard_text()); empty until one is exported.
    const std::string& clipboard() const noexcept { return display_.clipboard_text(); }
    // The screen model fed by write(). The mutable overload lets a test reset or feed it
    // directly; the terminal keeps using the same display afterwards.
    const VirtualDisplay& display() const noexcept { return display_; }
    VirtualDisplay& display() noexcept { return display_; }

private:
    static constexpr PixelSize effective_cell_pixels(Capabilities caps) noexcept {
        return caps.cell_pixels.width > 0 && caps.cell_pixels.height > 0 ? caps.cell_pixels : PixelSize{9, 18};
    }

    Size size_;
    Capabilities observed_caps_;
    Capabilities caps_;
    CapabilityOverrides overrides_;
    InputDecoder decoder_;
    VirtualDisplay display_;
    std::string written_;
    int bell_count_ = 0;
    std::vector<TerminalEvent> pending_events_;

    void enqueue_decoded(std::vector<TerminalEvent> decoded);
};

}  // namespace ckv::term
