// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// cvision_pty_latency — the scripted PTY harness behind the wall-clock half
// of the performance charter (the architecture §8: input event to presented
// bytes, p99 < 2 ms), run by the procedure in docs/performance.md
// ("Pinned-host p99 procedure"). It is a tool, never a CTest gate: shared CI
// must not judge wall-clock time.
//
// The harness plays the terminal. It opens a pseudo-terminal sized to the
// charter's 200x60 reference configuration and forks a child that runs a
// real ckVision Application on the slave side through PosixTerminal: the
// reference scene below (menu bar, status line, an editor window with three
// input lines, a peer window). The parent writes each scripted operation's
// input bytes into the master and timestamps the steady clock immediately
// before the write and again when the read that delivers the final byte of
// the frame the operation produced returns.
//
// The frame boundary is the Presenter's own: with synchronized output (DEC
// mode 2026) in the session's capabilities, every non-empty frame is one
// write() bracketed by CSI ? 2026 h ... CSI ? 2026 l, so the frame's final
// byte is the `l` of the closing bracket. Nothing else the session writes
// (cursor blink, pointer shapes) carries that bracket.
//
// Before anything is timed, one cycle of the script is rehearsed headlessly
// on the same scene and capability profile: every operation must present
// exactly one frame, and the cycle must leave the scene exactly as it found
// it, so each of the 10,000 timed operations measures the same work.
//
// Note: benchmarks are tooling and may read the steady clock and the host's
// load average; library code may not (the architecture §10).
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/core/version.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window.hpp"

#ifndef CKV_PTY_LATENCY_BUILD_TYPE
#define CKV_PTY_LATENCY_BUILD_TYPE "unknown"
#endif

namespace {

using SteadyClock = std::chrono::steady_clock;

// The architecture §8: the reference configuration and the latency limit.
constexpr ckv::Size kGrid{200, 60};
constexpr std::int64_t kLimitP99Nanos = 2'000'000;
// docs/performance.md: a later run fails when its p99 exceeds the checked-in
// baseline by more than this fraction.
constexpr double kBaselineTolerance = 0.05;

constexpr std::string_view kFrameBegin = "\x1B[?2026h";
constexpr std::string_view kFrameEnd = "\x1B[?2026l";
// Alt+X, the standard quit chord the reference scene answers.
constexpr std::string_view kQuitBytes = "\x1Bx";

// How long one operation may take to produce its frame before the run is
// declared broken rather than slow, and how long the child may take to
// present its first frame and to exit.
constexpr int kFrameTimeoutMillis = 5'000;
constexpr int kExitTimeoutMillis = 5'000;

// The operation classes of the documented mix, in reporting order.
enum class OperationClass : std::size_t {
    Echo,
    Focus,
    MenuOpen,
    MenuNavigate,
    MenuClose,
    WindowMove,
    WindowResize,
};
constexpr std::size_t kClassCount = 7;
constexpr std::array<const char*, kClassCount> kClassNames = {
    "echo", "focus", "menu_open", "menu_navigate", "menu_close", "window_move", "window_resize"};

const char* class_name(OperationClass kind) { return kClassNames[static_cast<std::size_t>(kind)]; }

// One scripted operation: the bytes a host would send for it, written with
// one write(), which the session reads, dispatches and presents as one frame.
struct Operation {
    OperationClass kind;
    std::string bytes;
};

// The session's capability profile: the ModernVt baseline (legacy keyboard,
// SGR mouse, focus reports, bracketed paste, pointer shapes), with 24-bit
// colour and synchronized output, and probing off -- the harness answers no
// probes, and a stated profile is the one being measured.
ckv::term::Capabilities reference_capabilities() {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.color_depth = ckv::term::ColorDepth::TrueColor;
    caps.synchronized_output = true;
    return caps;
}

constexpr const char* kCapabilityProfile =
    "ModernVt baseline + TrueColor + synchronized output (DEC 2026); probes off";

// The application under measurement: ordinary chrome over a Desktop, an
// Editor window with three input lines (the echo and focus targets) on top
// of a Notes window whose retained content a move or resize must not
// repaint. Built identically in the measured child and in the headless
// rehearsal, which is also where the script reads its geometry from.
class ReferenceScene {
public:
    explicit ReferenceScene(ckv::ui::Application& app) : roles_(ckv::ui::intern_standard_roles(app.roles())) {
        const ckv::ui::StandardCommands& standard = app.commands().standard();
        std::vector<ckv::widgets::MenuItem> file_items;
        file_items.push_back(ckv::widgets::MenuItem::action("&New", [] {}));
        file_items.push_back(ckv::widgets::MenuItem::action("&Open", [] {}));
        file_items.push_back(ckv::widgets::MenuItem::action("&Save", [] {}));
        file_items.push_back(ckv::widgets::MenuItem::separator());
        file_items.push_back(ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.quit, "E&xit"}));
        std::vector<ckv::widgets::MenuItem> window_items;
        window_items.push_back(ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.next_window}));
        window_items.push_back(ckv::widgets::MenuItem::command(ckv::widgets::CommandPresentation{standard.zoom}));
        ckv::widgets::ApplicationShell shell(
            app, {.theme = ckv::ui::make_classic_theme(app.roles(), roles_),
                  .menus = {{"&File", std::move(file_items)}, {"&Window", std::move(window_items)}},
                  .status_items = {ckv::widgets::StatusLineItem{ckv::widgets::CommandPresentation{standard.quit}},
                                   ckv::widgets::StatusLineItem{ckv::widgets::CommandPresentation{standard.menu}}}});
        ckv::widgets::Desktop& desktop = shell.desktop();
        app.commands().set_handler(standard.quit, [&app] { app.request_quit(); });

        auto notes = std::make_unique<ckv::widgets::Window>("Notes");
        notes->set_bounds(ckv::Rect{96, 12, 90, 34});
        notes->set_content(std::make_unique<ckv::widgets::StaticText>(
            "The peer window. It stays behind the Editor while the Editor is typed into, walked with "
            "Tab, moved and resized in front of it."));
        desktop.add_window(std::move(notes));

        auto column = std::make_unique<ckv::ui::Column>();
        column->set_spacing(1);
        std::array<ckv::widgets::InputLine*, 3> fields{};
        for (ckv::widgets::InputLine*& field : fields) {
            auto input = std::make_unique<ckv::widgets::InputLine>();
            field = input.get();
            column->add_item(std::move(input));
        }
        auto editor = std::make_unique<ckv::widgets::Window>("Editor");
        editor->set_bounds(ckv::Rect{4, 3, 72, 14});
        editor->set_content(std::move(column));
        editor_ = desktop.add_window(std::move(editor));
        app.set_focus(fields[0]);
    }

    // The Editor window's frame in absolute cells.
    ckv::Rect editor_frame() const noexcept { return editor_->absolute_bounds(); }

private:
    ckv::ui::StandardRoles roles_;
    ckv::widgets::Window* editor_ = nullptr;
};

// SGR mouse reports (1-based on the wire). Button 0 is left; 32 marks motion.
std::string sgr(int button, ckv::Point cell, char final) {
    return "\x1B[<" + std::to_string(button) + ";" + std::to_string(cell.x + 1) + ";" + std::to_string(cell.y + 1) +
           final;
}
std::string press(ckv::Point cell) { return sgr(0, cell, 'M'); }
std::string drag(ckv::Point cell) { return sgr(32, cell, 'M'); }
std::string release(ckv::Point cell) { return sgr(0, cell, 'm'); }

// One cycle of the documented mix, 25 operations that leave the scene as
// they found it: ten echo keys (five characters typed into the focused
// field, five Backspaces), four focus moves (Tab, Tab, Shift+Tab,
// Shift+Tab), five menu operations (Alt+F opens File; Down, Down, Up walk it;
// Enter chooses an item that does nothing, which closes the menu and hands
// the focus back), three title-bar drag steps and three resize-grip drag
// steps, each drag returning to where it began. A lone Esc would close the
// menu too, but a legacy keyboard delivers it only after the decoder's 50 ms
// quiet deadline (docs/input-decoder.md): that wait is the protocol's
// Esc-versus-Alt ambiguity, not event-to-bytes work, so it is not scripted.
std::vector<Operation> script_cycle(ckv::Rect editor) {
    std::vector<Operation> cycle;
    for (const char* key : {"c", "k", "v", "i", "s"}) cycle.push_back({OperationClass::Echo, key});
    for (int index = 0; index < 5; ++index) cycle.push_back({OperationClass::Echo, "\x7F"});
    cycle.push_back({OperationClass::Focus, "\t"});
    cycle.push_back({OperationClass::Focus, "\t"});
    cycle.push_back({OperationClass::Focus, "\x1B[Z"});
    cycle.push_back({OperationClass::Focus, "\x1B[Z"});
    cycle.push_back({OperationClass::MenuOpen, "\x1B" "f"});
    cycle.push_back({OperationClass::MenuNavigate, "\x1B[B"});
    cycle.push_back({OperationClass::MenuNavigate, "\x1B[B"});
    cycle.push_back({OperationClass::MenuNavigate, "\x1B[A"});
    cycle.push_back({OperationClass::MenuClose, "\r"});
    const ckv::Point title{editor.x + editor.width / 2, editor.y};
    cycle.push_back({OperationClass::WindowMove, press(title) + drag({title.x + 1, title.y})});
    cycle.push_back({OperationClass::WindowMove, drag({title.x + 2, title.y})});
    cycle.push_back({OperationClass::WindowMove, drag(title) + release(title)});
    const ckv::Point grip{editor.x + editor.width - 1, editor.y + editor.height - 1};
    cycle.push_back({OperationClass::WindowResize, press(grip) + drag({grip.x + 1, grip.y + 1})});
    cycle.push_back({OperationClass::WindowResize, drag({grip.x + 2, grip.y + 1})});
    cycle.push_back({OperationClass::WindowResize, drag(grip) + release(grip)});
    return cycle;
}

std::size_t count_of(std::string_view haystack, std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t at = haystack.find(needle); at != std::string_view::npos; at = haystack.find(needle, at + 1))
        ++count;
    return count;
}

// Plays one cycle headlessly and returns the script, or prints why the
// script does not measure what it claims to and returns nothing.
std::optional<std::vector<Operation>> rehearse() {
    ckv::term::HeadlessTerminal terminal(kGrid, reference_capabilities(), /*enable_capability_probes=*/false);
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ReferenceScene scene(app);
    app.step(clock.now_nanos());
    std::vector<Operation> cycle = script_cycle(scene.editor_frame());
    const auto capture = [&app] {
        return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    };
    const std::string before = capture();
    const ckv::ui::View* const focused_before = app.focused();
    for (std::size_t index = 0; index < cycle.size(); ++index) {
        terminal.clear_written();
        terminal.inject_bytes(cycle[index].bytes, clock.now_nanos());
        app.step(clock.now_nanos());
        app.step(clock.now_nanos());
        const std::string_view written = terminal.written_bytes();
        if (count_of(written, kFrameBegin) != 1 || count_of(written, kFrameEnd) != 1) {
            std::fprintf(stderr, "rehearsal: operation %zu (%s) presented %zu frames, not one\n", index,
                         class_name(cycle[index].kind), count_of(written, kFrameEnd));
            return std::nullopt;
        }
    }
    if (capture() != before || app.focused() != focused_before) {
        std::fputs("rehearsal: one cycle of the script does not return the scene to its starting state\n", stderr);
        return std::nullopt;
    }
    return cycle;
}

// Latency samples of one class, in nanoseconds, with the bytes each frame took.
struct ClassSamples {
    std::vector<std::int64_t> nanos;
    std::uint64_t bytes = 0;
};

struct Summary {
    std::size_t count = 0;
    std::int64_t p50 = 0;
    std::int64_t p90 = 0;
    std::int64_t p99 = 0;
    std::int64_t max = 0;
    double mean_bytes = 0;
};

// Nearest-rank percentile of sorted samples: the smallest sample with at
// least `percent` of all samples at or below it.
std::int64_t percentile(const std::vector<std::int64_t>& sorted, int percent) {
    const std::size_t rank = (sorted.size() * static_cast<std::size_t>(percent) + 99) / 100;
    return sorted[std::max<std::size_t>(rank, 1) - 1];
}

Summary summarize(std::vector<std::int64_t> samples, std::uint64_t bytes) {
    Summary summary;
    if (samples.empty()) return summary;
    std::sort(samples.begin(), samples.end());
    summary.count = samples.size();
    summary.p50 = percentile(samples, 50);
    summary.p90 = percentile(samples, 90);
    summary.p99 = percentile(samples, 99);
    summary.max = samples.back();
    summary.mean_bytes = static_cast<double>(bytes) / static_cast<double>(samples.size());
    return summary;
}

// Recognizes the frame-end bracket in a byte stream that arrives in pieces.
class FrameEndMatcher {
public:
    // Feeds `bytes`; returns the offset just past the first completed
    // bracket, or npos when none completes within them.
    std::size_t feed(std::string_view bytes) noexcept {
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (bytes[index] == kFrameEnd[matched_]) {
                if (++matched_ == kFrameEnd.size()) {
                    matched_ = 0;
                    return index + 1;
                }
            } else {
                matched_ = bytes[index] == kFrameEnd[0] ? 1 : 0;
            }
        }
        return std::string_view::npos;
    }

private:
    std::size_t matched_ = 0;
};

// The master side of the session: writes input, reads output, and times the
// span between them.
class PtyHost {
public:
    explicit PtyHost(int master) : master_(master) {}

    bool write_all(std::string_view bytes) {
        while (!bytes.empty()) {
            const ssize_t written = ::write(master_, bytes.data(), bytes.size());
            if (written < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            bytes.remove_prefix(static_cast<std::size_t>(written));
        }
        return true;
    }

    // Reads until one frame has ended. Returns the steady-clock time at which
    // the read holding its final byte returned, and adds the bytes read to
    // `bytes`; nothing when the frame does not arrive in time.
    std::optional<SteadyClock::time_point> await_frame(std::uint64_t& bytes, int timeout_millis) {
        const SteadyClock::time_point deadline = SteadyClock::now() + std::chrono::milliseconds(timeout_millis);
        for (;;) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - SteadyClock::now()).count();
            if (remaining < 0) return std::nullopt;
            pollfd descriptor{master_, POLLIN, 0};
            const int ready = ::poll(&descriptor, 1, static_cast<int>(remaining));
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) return std::nullopt;
            const ssize_t count = ::read(master_, buffer_.data(), buffer_.size());
            const SteadyClock::time_point arrived = SteadyClock::now();
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return std::nullopt;
            const std::string_view chunk(buffer_.data(), static_cast<std::size_t>(count));
            bytes += chunk.size();
            const std::size_t end = matcher_.feed(chunk);
            if (end == std::string_view::npos) continue;
            // Anything after the bracket in the same read is not this frame.
            // A further frame there is one the script did not ask for.
            std::string_view rest = chunk.substr(end);
            for (std::size_t more = matcher_.feed(rest); more != std::string_view::npos; more = matcher_.feed(rest)) {
                ++extra_frames_;
                rest.remove_prefix(more);
            }
            return arrived;
        }
    }

    // Reads and discards output until the child has exited or the timeout
    // passes, so a child restoring its terminal is never blocked on a full
    // pipe. Returns the child's wait status, or nothing on timeout.
    std::optional<int> drain_until_exit(pid_t child, int timeout_millis) {
        const SteadyClock::time_point deadline = SteadyClock::now() + std::chrono::milliseconds(timeout_millis);
        for (;;) {
            int status = 0;
            if (::waitpid(child, &status, WNOHANG) == child) return status;
            if (SteadyClock::now() > deadline) return std::nullopt;
            pollfd descriptor{master_, POLLIN, 0};
            if (::poll(&descriptor, 1, 10) > 0 && ::read(master_, buffer_.data(), buffer_.size()) <= 0) {
                // The slave side has closed; the child is on its way out.
                ::usleep(1'000);
            }
        }
    }

    std::size_t extra_frames() const noexcept { return extra_frames_; }

private:
    int master_;
    std::array<char, 65536> buffer_{};
    FrameEndMatcher matcher_;
    std::size_t extra_frames_ = 0;
};

// The measured child: the reference scene on the PTY slave, run until the
// harness sends the quit chord. Never returns.
[[noreturn]] void run_child(int slave) {
    int status = EXIT_SUCCESS;
    try {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock, slave, slave, reference_capabilities(),
                                          /*enable_capability_probes=*/false);
        ckv::ui::Application app(terminal, clock);
        ReferenceScene scene(app);
        app.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "cvision_pty_latency child: %s\n", error.what());
        status = 3;
    }
    ::_exit(status);
}

struct Options {
    std::size_t operations = 10'000;
    std::size_t warmup_cycles = 4;
    std::string output = "pty_latency.json";
    std::string commit = "unrecorded";
    std::int64_t baseline_p99 = 0;
};

std::optional<std::size_t> parse_count(const char* text) {
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return std::nullopt;
    return static_cast<std::size_t>(value);
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view flag = argv[index];
        if (index + 1 >= argc) return std::nullopt;
        const char* const value = argv[++index];
        if (flag == "--operations") {
            const std::optional<std::size_t> count = parse_count(value);
            if (!count || *count == 0) return std::nullopt;
            options.operations = *count;
        } else if (flag == "--warmup-cycles") {
            const std::optional<std::size_t> count = parse_count(value);
            if (!count) return std::nullopt;
            options.warmup_cycles = *count;
        } else if (flag == "--output") {
            options.output = value;
        } else if (flag == "--commit") {
            options.commit = value;
        } else if (flag == "--baseline-p99-ns") {
            const std::optional<std::size_t> nanos = parse_count(value);
            if (!nanos || *nanos == 0) return std::nullopt;
            options.baseline_p99 = static_cast<std::int64_t>(*nanos);
        } else {
            return std::nullopt;
        }
    }
    return options;
}

std::string json_string(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
            out += escaped;
        } else {
            out += c;
        }
    }
    return out + "\"";
}

void write_summary_json(std::FILE* file, const Summary& summary) {
    std::fprintf(file,
                 "{\"count\": %zu, \"p50_ns\": %lld, \"p90_ns\": %lld, \"p99_ns\": %lld, \"max_ns\": %lld, "
                 "\"mean_frame_bytes\": %.1f}",
                 summary.count, static_cast<long long>(summary.p50), static_cast<long long>(summary.p90),
                 static_cast<long long>(summary.p99), static_cast<long long>(summary.max), summary.mean_bytes);
}

void print_summary(const char* label, const Summary& summary) {
    std::printf("%-14s %7zu %10.3f %10.3f %10.3f %10.3f %10.0f\n", label, summary.count,
                static_cast<double>(summary.p50) / 1e6, static_cast<double>(summary.p90) / 1e6,
                static_cast<double>(summary.p99) / 1e6, static_cast<double>(summary.max) / 1e6, summary.mean_bytes);
}

struct HostLoad {
    std::array<double, 3> averages{};
    bool known = false;
};

HostLoad read_load() {
    HostLoad load;
    load.known = ::getloadavg(load.averages.data(), 3) == 3;
    return load;
}

void write_load_json(std::FILE* file, const HostLoad& load) {
    if (!load.known) {
        std::fputs("null", file);
        return;
    }
    std::fprintf(file, "[%.2f, %.2f, %.2f]", load.averages[0], load.averages[1], load.averages[2]);
}

}  // namespace

int main(int argc, char** argv) {
    const std::optional<Options> parsed = parse_options(argc, argv);
    if (!parsed) {
        std::fprintf(stderr,
                     "usage: cvision_pty_latency [--operations N] [--warmup-cycles N] [--output FILE]\n"
                     "                           [--commit ID] [--baseline-p99-ns N]\n");
        return 2;
    }
    const Options& options = *parsed;

    const std::optional<std::vector<Operation>> rehearsed = rehearse();
    if (!rehearsed) return 3;
    const std::vector<Operation>& cycle = *rehearsed;

    const HostLoad load_before = read_load();

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || ::grantpt(master) != 0 || ::unlockpt(master) != 0) {
        std::perror("cvision_pty_latency: posix_openpt");
        return 3;
    }
    const char* const slave_name = ::ptsname(master);
    const int slave = slave_name != nullptr ? ::open(slave_name, O_RDWR | O_NOCTTY) : -1;
    if (slave < 0) {
        std::perror("cvision_pty_latency: open slave");
        return 3;
    }
    winsize size{};
    size.ws_col = static_cast<unsigned short>(kGrid.width);
    size.ws_row = static_cast<unsigned short>(kGrid.height);
    if (::ioctl(slave, TIOCSWINSZ, &size) != 0) {
        std::perror("cvision_pty_latency: TIOCSWINSZ");
        return 3;
    }

    std::fflush(nullptr);
    const pid_t child = ::fork();
    if (child < 0) {
        std::perror("cvision_pty_latency: fork");
        return 3;
    }
    if (child == 0) {
        ::close(master);
        // A session of its own with the slave as controlling terminal, as a
        // program started in a terminal window has.
        if (::setsid() >= 0) (void)::ioctl(slave, TIOCSCTTY, 0);
        run_child(slave);
    }
    ::close(slave);

    PtyHost host(master);
    const auto fail = [&](const char* what) {
        std::fprintf(stderr, "cvision_pty_latency: %s\n", what);
        ::kill(child, SIGKILL);
        (void)host.drain_until_exit(child, kExitTimeoutMillis);
        ::close(master);
        return 3;
    };

    std::uint64_t first_frame_bytes = 0;
    if (!host.await_frame(first_frame_bytes, kFrameTimeoutMillis)) return fail("no first frame from the child");

    std::array<ClassSamples, kClassCount> samples;
    std::vector<std::int64_t> overall;
    overall.reserve(options.operations);
    std::uint64_t overall_bytes = 0;
    const std::size_t warmup = options.warmup_cycles * cycle.size();
    for (std::size_t index = 0; index < warmup + options.operations; ++index) {
        const Operation& operation = cycle[index % cycle.size()];
        std::uint64_t bytes = 0;
        const SteadyClock::time_point written = SteadyClock::now();
        if (!host.write_all(operation.bytes)) return fail("writing input to the PTY failed");
        const std::optional<SteadyClock::time_point> presented = host.await_frame(bytes, kFrameTimeoutMillis);
        if (!presented) {
            std::fprintf(stderr, "cvision_pty_latency: operation %zu (%s) presented no frame within %d ms\n", index,
                         class_name(operation.kind), kFrameTimeoutMillis);
            return fail("aborted");
        }
        if (index < warmup) continue;
        const std::int64_t nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(*presented - written).count();
        ClassSamples& group = samples[static_cast<std::size_t>(operation.kind)];
        group.nanos.push_back(nanos);
        group.bytes += bytes;
        overall.push_back(nanos);
        overall_bytes += bytes;
    }

    if (!host.write_all(kQuitBytes)) return fail("writing the quit chord failed");
    const std::optional<int> status = host.drain_until_exit(child, kExitTimeoutMillis);
    ::close(master);
    if (!status) {
        ::kill(child, SIGKILL);
        ::waitpid(child, nullptr, 0);
        std::fputs("cvision_pty_latency: the child did not exit after the quit chord\n", stderr);
        return 3;
    }
    if (!WIFEXITED(*status) || WEXITSTATUS(*status) != EXIT_SUCCESS) {
        std::fputs("cvision_pty_latency: the child did not exit cleanly\n", stderr);
        return 3;
    }
    if (host.extra_frames() != 0) {
        std::fprintf(stderr, "cvision_pty_latency: %zu frames arrived that no operation asked for\n",
                     host.extra_frames());
        return 3;
    }

    const HostLoad load_after = read_load();
    std::array<Summary, kClassCount> class_summaries;
    for (std::size_t kind = 0; kind < kClassCount; ++kind)
        class_summaries[kind] = summarize(samples[kind].nanos, samples[kind].bytes);
    const Summary total = summarize(overall, overall_bytes);
    const bool within_limit = total.p99 < kLimitP99Nanos;
    const bool within_baseline =
        options.baseline_p99 == 0 ||
        static_cast<double>(total.p99) <= static_cast<double>(options.baseline_p99) * (1.0 + kBaselineTolerance);

    utsname host_name{};
    (void)::uname(&host_name);

    std::printf("cvision_pty_latency: %zu operations (+%zu warm-up) on a %dx%d PTY, ckVision %.*s, %s build\n",
                total.count, warmup, kGrid.width, kGrid.height, static_cast<int>(ckv::version_string().size()),
                ckv::version_string().data(), CKV_PTY_LATENCY_BUILD_TYPE);
    std::printf("host %s %s %s; load average before %.2f %.2f %.2f, after %.2f %.2f %.2f\n", host_name.sysname,
                host_name.release, host_name.machine, load_before.averages[0], load_before.averages[1],
                load_before.averages[2], load_after.averages[0], load_after.averages[1], load_after.averages[2]);
    std::printf("%-14s %7s %10s %10s %10s %10s %10s\n", "class", "count", "p50 ms", "p90 ms", "p99 ms", "max ms",
                "bytes/op");
    for (std::size_t kind = 0; kind < kClassCount; ++kind) print_summary(kClassNames[kind], class_summaries[kind]);
    print_summary("overall", total);
    std::printf("p99 %.3f ms against the %.3f ms limit: %s\n", static_cast<double>(total.p99) / 1e6,
                static_cast<double>(kLimitP99Nanos) / 1e6, within_limit ? "within" : "EXCEEDED");
    if (options.baseline_p99 != 0)
        std::printf("p99 against baseline %.3f ms (+%.0f%% allowed): %s\n",
                    static_cast<double>(options.baseline_p99) / 1e6, kBaselineTolerance * 100,
                    within_baseline ? "within" : "REGRESSED");

    std::FILE* const file = std::fopen(options.output.c_str(), "w");
    if (file == nullptr) {
        std::perror(options.output.c_str());
        return 3;
    }
    std::fprintf(file, "{\n  \"tool\": \"cvision_pty_latency\",\n  \"schema\": 1,\n");
    std::fprintf(file, "  \"ckvision_version\": %s,\n  \"commit\": %s,\n  \"build_type\": %s,\n  \"compiler\": %s,\n",
                 json_string(ckv::version_string()).c_str(), json_string(options.commit).c_str(),
                 json_string(CKV_PTY_LATENCY_BUILD_TYPE).c_str(), json_string(__VERSION__).c_str());
    std::fprintf(file, "  \"host\": {\"sysname\": %s, \"release\": %s, \"version\": %s, \"machine\": %s},\n",
                 json_string(host_name.sysname).c_str(), json_string(host_name.release).c_str(),
                 json_string(host_name.version).c_str(), json_string(host_name.machine).c_str());
    std::fputs("  \"load_average_before\": ", file);
    write_load_json(file, load_before);
    std::fputs(",\n  \"load_average_after\": ", file);
    write_load_json(file, load_after);
    std::fprintf(file, ",\n  \"terminal_grid\": [%d, %d],\n  \"capability_profile\": %s,\n", kGrid.width,
                 kGrid.height, json_string(kCapabilityProfile).c_str());
    std::fprintf(file, "  \"operations\": %zu,\n  \"warmup_operations\": %zu,\n  \"cycle_length\": %zu,\n",
                 total.count, warmup, cycle.size());
    std::fputs("  \"classes\": {\n", file);
    for (std::size_t kind = 0; kind < kClassCount; ++kind) {
        std::fprintf(file, "    %s: ", json_string(kClassNames[kind]).c_str());
        write_summary_json(file, class_summaries[kind]);
        std::fputs(kind + 1 < kClassCount ? ",\n" : "\n", file);
    }
    std::fputs("  },\n  \"overall\": ", file);
    write_summary_json(file, total);
    std::fprintf(file, ",\n  \"limit_p99_ns\": %lld,\n  \"p99_within_limit\": %s,\n",
                 static_cast<long long>(kLimitP99Nanos), within_limit ? "true" : "false");
    if (options.baseline_p99 != 0)
        std::fprintf(file, "  \"baseline_p99_ns\": %lld,\n  \"p99_within_baseline\": %s,\n",
                     static_cast<long long>(options.baseline_p99), within_baseline ? "true" : "false");
    std::fprintf(file, "  \"unrequested_frames\": %zu\n}\n", host.extra_frames());
    std::fclose(file);
    std::printf("wrote %s\n", options.output.c_str());
    return within_limit && within_baseline ? 0 : 1;
}
