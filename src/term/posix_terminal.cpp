// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// `deadline_nanos` in poll() is interpreted as CLOCK_MONOTONIC
// nanoseconds — this backend's own notion of "now" (D-025: platform
// backends are exactly where real time is allowed to enter the
// system). Use monotonic_now_nanos() to construct a valid deadline.
#include "cvision/term/posix_terminal.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>

#include "cvision/core/assert.hpp"
#include "cvision/term/presenter.hpp"  // sanitize_osc_text

namespace ckv::term {
namespace {

std::int64_t monotonic_now_nanos_impl() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

// --- D-024: the one sanctioned process-level facility --------------------
//
// A fixed-size, signal-handler-safe registry of live terminal sessions.
// No dynamic allocation, no locks that could deadlock in a handler.
//
// `state` is a 3-state std::atomic<int>, not a plain sig_atomic_t flag:
// kFree -> kClaiming (a slot has been exclusively won via CAS, but its
// other fields are not populated yet) -> kReady (fully populated; safe
// for the signal handler to act on). This closes two real hazards a
// simple "scan then set a flag" registration would have: (a) two
// threads racing register_session() could both pass the same slot's
// "is it free" check before either claims it, corrupting one session's
// fd/termios/restore bytes with another's; (b) even single-threaded, a
// re-entrant signal firing between "flag set" and "fields populated"
// would read partially-written data with no ordering guarantee, since
// plain (non-volatile, non-atomic) stores are not guaranteed sequenced
// before a later flag store from the compiler's point of view.
constexpr int kMaxSessions = 8;
constexpr std::size_t kRestoreBufSize = 256;
constexpr int kFree = 0;
constexpr int kClaiming = 1;
constexpr int kReady = 2;

struct SessionRecord {
    std::atomic<int> state{kFree};
    int fd = -1;
    struct termios original_termios{};
    char restore_bytes[kRestoreBufSize]{};
    std::size_t restore_len = 0;
};

SessionRecord g_sessions[kMaxSessions];
std::atomic<bool> g_handlers_installed{false};

// SIGWINCH has no terminal-session identity.  Its sole purpose here is
// therefore to interrupt a blocked poll() promptly; every PosixTerminal
// compares *its own* current geometry with last_size_ before returning,
// which prevents a resize on one PTY from becoming a spurious ResizeEvent
// in another session.
extern "C" void on_sigwinch(int) {}

// Async-signal-safe restore: write() is guaranteed safe. tcsetattr is
// NOT on POSIX's async-signal-safe function list, but is relied upon
// by essentially every real terminal-handling library (readline,
// ncurses, and this codebase) for exactly this purpose in practice,
// absent any signal-safe alternative for restoring cooked mode. This
// is a documented, deliberate, honest exception, not an oversight.
// std::atomic operations on a lock-free int are likewise not on the
// POSIX async-signal-safe list in the letter of the standard, but —
// like tcsetattr — compile to plain load/store-with-barrier
// instructions with no syscalls or blocking, and are the only
// practical way to avoid finding (a)/(b) above.
extern "C" void fatal_signal_handler(int sig) {
    for (SessionRecord& s : g_sessions) {
        if (s.state.load(std::memory_order_acquire) != kReady) continue;
        ::write(s.fd, s.restore_bytes, s.restore_len);
        ::tcsetattr(s.fd, TCSANOW, &s.original_termios);
    }
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void install_handlers_once() {
    bool expected = false;
    if (!g_handlers_installed.compare_exchange_strong(expected, true)) return;

    struct sigaction fatal{};
    fatal.sa_handler = &fatal_signal_handler;
    sigemptyset(&fatal.sa_mask);
    fatal.sa_flags = 0;
    for (const int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) ::sigaction(sig, &fatal, nullptr);

    struct sigaction winch{};
    winch.sa_handler = &on_sigwinch;
    sigemptyset(&winch.sa_mask);
    winch.sa_flags = 0;
    ::sigaction(SIGWINCH, &winch, nullptr);
}

// Returns the claimed slot index, or -1 if every slot is in use — a
// contract violation the caller CKV_ASSERTs on. Growing
// kEnterSequence/kRestoreSequence past kRestoreBufSize is likewise a
// contract violation caught here, loudly, rather than silently
// truncating a future crash's restore sequence.
int register_session(int fd, const struct termios& original, std::string_view restore_bytes) {
    CKV_ASSERT(restore_bytes.size() <= kRestoreBufSize);
    for (int i = 0; i < kMaxSessions; ++i) {
        int expected = kFree;
        if (!g_sessions[i].state.compare_exchange_strong(expected, kClaiming, std::memory_order_relaxed))
            continue;
        // Exclusively claimed: no other thread can also win this slot.
        // Safe to populate before publishing kReady.
        SessionRecord& s = g_sessions[i];
        s.fd = fd;
        s.original_termios = original;
        s.restore_len = restore_bytes.size();
        std::memcpy(s.restore_bytes, restore_bytes.data(), s.restore_len);
        s.state.store(kReady, std::memory_order_release);
        return i;
    }
    return -1;
}

void unregister_session(int slot) noexcept {
    if (slot < 0) return;
    g_sessions[slot].state.store(kFree, std::memory_order_release);
}

constexpr std::string_view kEnterSequence =
    "\x1B[?1049h"   // alternate screen, save cursor
    "\x1B[?1002h"    // button-event mouse tracking
    "\x1B[?1006h"    // SGR mouse encoding
    "\x1B[?2004h"    // bracketed paste
    "\x1B[?1004h";   // focus reporting

constexpr std::string_view kRestoreSequence =
    "\x1B[?1000l\x1B[?1002l\x1B[?1006l\x1B[?1016l"  // mouse modes off
    "\x1B[?2004l"                                     // bracketed paste off
    "\x1B[?1004l"                                     // focus reporting off
    "\x1B[?25h"                                        // cursor visible
    "\x1B[0m"                                           // style reset
    "\x1B[?1049l";                                       // leave alternate screen, restore cursor

void write_all(int fd, std::string_view bytes) {
    std::size_t off = 0;
    while (off < bytes.size()) {
        const ssize_t n = ::write(fd, bytes.data() + off, bytes.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;  // fd gone / unwritable: nothing more we can do here
        }
        off += static_cast<std::size_t>(n);
    }
}

std::string base64_encode(std::string_view data) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const std::uint32_t n = (static_cast<unsigned char>(data[i]) << 16) |
                                 (static_cast<unsigned char>(data[i + 1]) << 8) |
                                 static_cast<unsigned char>(data[i + 2]);
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += kAlphabet[(n >> 6) & 0x3F];
        out += kAlphabet[n & 0x3F];
    }
    const std::size_t remaining = data.size() - i;
    if (remaining == 1) {
        const std::uint32_t n = static_cast<unsigned char>(data[i]) << 16;
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += "==";
    } else if (remaining == 2) {
        const std::uint32_t n =
            (static_cast<unsigned char>(data[i]) << 16) | (static_cast<unsigned char>(data[i + 1]) << 8);
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += kAlphabet[(n >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

}  // namespace

PosixTerminal::PosixTerminal(int output_fd, int input_fd, Capabilities initial_caps)
    : output_fd_(output_fd),
      input_fd_(input_fd),
      caps_(initial_caps),
      decoder_(initial_caps),
      last_size_(size()) {
    // Establish the wake channel before touching the terminal.  A failed
    // construction must leave the caller's terminal entirely unchanged;
    // creating the pipe first makes that guarantee hold even when the OS
    // cannot allocate another descriptor.
    int pipe_fds[2] = {-1, -1};
    CKV_ASSERT(::pipe(pipe_fds) == 0);
    CKV_ASSERT(::fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == 0);
    CKV_ASSERT(::fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == 0);
    self_pipe_read_ = pipe_fds[0];
    self_pipe_write_ = pipe_fds[1];

    struct termios original{};
    CKV_ASSERT(::tcgetattr(input_fd_, &original) == 0);

    // Register BEFORE mutating any OS state (raw mode, alternate
    // screen). If every registry slot is already in use, CKV_ASSERT
    // fires here and the process aborts with the terminal completely
    // untouched — no restoration is owed because nothing was changed.
    // Registering only after entering raw mode/alt-screen (the
    // original ordering) left a failed construction's tty permanently
    // raw and alternate-screened with no destructor ever running to
    // undo it, breaking the "restoration guaranteed on every exit
    // path" promise for exactly the path meant to guarantee it.
    session_slot_ = register_session(input_fd_, original, kRestoreSequence);
    CKV_ASSERT(session_slot_ >= 0);
    install_handlers_once();

    struct termios raw = original;
    ::cfmakeraw(&raw);
    CKV_ASSERT(::tcsetattr(input_fd_, TCSANOW, &raw) == 0);

    write_all(output_fd_, kEnterSequence);
}

PosixTerminal::~PosixTerminal() {
    write_all(output_fd_, kRestoreSequence);
    if (session_slot_ >= 0) {
        ::tcsetattr(input_fd_, TCSANOW, &g_sessions[session_slot_].original_termios);
        unregister_session(session_slot_);
    }
    if (self_pipe_read_ >= 0) ::close(self_pipe_read_);
    if (self_pipe_write_ >= 0) ::close(self_pipe_write_);
}

Size PosixTerminal::size() const noexcept {
    struct winsize ws{};
    if (::ioctl(output_fd_, TIOCGWINSZ, &ws) != 0) return Size{80, 24};
    return Size{ws.ws_col, ws.ws_row};
}

std::vector<TerminalEvent> PosixTerminal::poll(std::int64_t deadline_nanos) {
    std::vector<TerminalEvent> events;
    const auto collect_resize = [this, &events] {
        const Size observed = size();
        if (observed == last_size_) return;
        last_size_ = observed;
        events.push_back(TerminalEvent{ResizeEvent{observed}});
    };
    // This deliberately compares on every poll rather than consuming a
    // process-global SIGWINCH flag.  Signals carry no PTY identity; a
    // per-session comparison is the only correct routing authority.
    collect_resize();
    if (!events.empty()) return events;

    std::array<struct pollfd, 2> fds{{{input_fd_, POLLIN, 0}, {self_pipe_read_, POLLIN, 0}}};
    const std::int64_t now = monotonic_now_nanos_impl();
    const std::int64_t remaining_ns = deadline_nanos - now;
    const int timeout_ms = remaining_ns <= 0 ? 0 : static_cast<int>(remaining_ns / 1'000'000);

    const int rc = ::poll(fds.data(), fds.size(), timeout_ms);
    if (rc > 0) {
        if (fds[1].revents & POLLIN) {
            char discard[64];
            while (::read(self_pipe_read_, discard, sizeof(discard)) > 0) {
            }
        }
        if (fds[0].revents & POLLIN) {
            char buf[4096];
            const ssize_t n = ::read(input_fd_, buf, sizeof(buf));
            if (n > 0) {
                std::vector<TerminalEvent> decoded =
                    decoder_.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                                  monotonic_now_nanos_impl());
                for (auto& ev : decoded) {
                    if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev))
                        caps_ = changed->capabilities;
                    events.push_back(std::move(ev));
                }
            }
        }
    } else if (rc == 0) {
        std::vector<TerminalEvent> decoded = decoder_.poll_timeout(monotonic_now_nanos_impl());
        for (auto& ev : decoded) events.push_back(std::move(ev));
    } else if (errno == EINTR) {
        // A SIGWINCH may have arrived between the first observation and
        // poll(). Observe again so this very call reports its own session's
        // resize without waiting for a subsequent frame.
        collect_resize();
    }
    return events;
}

void PosixTerminal::write(std::string_view bytes) { write_all(output_fd_, bytes); }

void PosixTerminal::set_title(std::string_view title) {
    write_all(output_fd_, "\x1B]0;" + sanitize_osc_text(title) + "\x07");
}

void PosixTerminal::bell() { write_all(output_fd_, "\x07"); }

void PosixTerminal::write_clipboard(std::string_view text) {
    if (!caps_.clipboard_write) return;
    write_all(output_fd_, "\x1B]52;c;" + base64_encode(text) + "\x07");
}

void PosixTerminal::wake() noexcept {
    const char byte = 0;
    ::write(self_pipe_write_, &byte, 1);
}

std::int64_t PosixTerminal::monotonic_now_nanos() noexcept { return monotonic_now_nanos_impl(); }

}  // namespace ckv::term
