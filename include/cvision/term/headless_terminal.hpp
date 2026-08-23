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
    caps.cell_pixels = Size{9, 18};
    return caps;
}

constexpr Capabilities headless_sixel_profile() noexcept {
    Capabilities caps = headless_no_graphics_profile();
    caps.sixel_graphics = true;
    return caps;
}

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
    // Fixed metrics and color-register caps must be positive.
    void set_capability_overrides(CapabilityOverrides overrides);
    const CapabilityOverrides& capability_overrides() const noexcept { return overrides_; }
    Size size() const noexcept override { return size_; }

    // Headless never blocks on real time — it just drains whatever was
    // queued by inject_bytes()/inject_event() since the last poll().
    std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) override;
    void wake() noexcept override {}
    void write_diagnostic_after_restore(std::string_view) noexcept override {}

    void write(std::string_view bytes) override;
    void set_title(std::string_view title) override { title_ = std::string(title); }
    void bell() override { ++bell_count_; }
    void write_clipboard(std::string_view text) override {
        if (caps_.clipboard_write) clipboard_ = std::string(text);
    }

    // --- Scripting interface (tests / recorded scripts) -------------
    void inject_bytes(std::string_view bytes, std::int64_t now_nanos);
    void inject_timeout_check(std::int64_t now_nanos);  // resolves a pending lone ESC, if due
    // Models a definite terminal-input disconnect. Any partial paste is
    // delivered as recovered text, never reinterpreted as keys.
    void inject_input_disconnect();
    void inject_event(TerminalEvent event);
    void inject_capability_change(Capabilities caps);
    void set_capabilities(Capabilities caps) noexcept;
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

    std::string_view written_bytes() const noexcept { return written_; }
    void clear_written() noexcept { written_.clear(); }
    const std::string& title() const noexcept { return title_; }
    int bell_count() const noexcept { return bell_count_; }
    const std::string& clipboard() const noexcept { return clipboard_; }
    const VirtualDisplay& display() const noexcept { return display_; }
    VirtualDisplay& display() noexcept { return display_; }

private:
    static constexpr Size effective_cell_pixels(Capabilities caps) noexcept {
        return caps.cell_pixels.width > 0 && caps.cell_pixels.height > 0 ? caps.cell_pixels : Size{9, 18};
    }

    Size size_;
    Capabilities observed_caps_;
    Capabilities caps_;
    CapabilityOverrides overrides_;
    InputDecoder decoder_;
    VirtualDisplay display_;
    std::string written_;
    std::string title_;
    int bell_count_ = 0;
    std::string clipboard_;
    std::vector<TerminalEvent> pending_events_;

    void enqueue_decoded(std::vector<TerminalEvent> decoded);
};

}  // namespace ckv::term
