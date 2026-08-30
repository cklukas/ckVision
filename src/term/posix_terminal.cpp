// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// `deadline_nanos` in poll() is interpreted on the injected monotonic
// Clock shared with the owning Application. Platform time enters through
// PosixClock at construction, never through a hidden backend read.
#include "cvision/term/posix_terminal.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

#include "cvision/core/assert.hpp"
#include "cvision/core/base64.hpp"
#include "cvision/term/graphics_log.hpp"
#include "cvision/term/pointer_shape_names.hpp"
#include "cvision/term/presenter.hpp"  // sanitize_osc_text

namespace ckv::term {

// The async-signal-safe sibling of write_all (below): it retries the two
// conditions a caller can survive — EINTR and a short count — and gives up on
// everything else. write_all's answer to backpressure is poll(), which is not
// async-signal-safe, so the D-024 handler paths use this one. The loop is the
// half that matters: a short count would otherwise truncate a restore
// sequence or a diagnostic MID-ESCAPE, leaving the terminal in exactly the
// state the sequence exists to prevent. The give-up is deliberate, not
// neglect: every caller is on a path — fatal handler, suspend, resume, stderr
// diagnostic — with nowhere to report a dead descriptor and no license to
// block on one. Zero is treated as give-up too, so no descriptor that
// answers "wrote nothing" can spin a signal handler.
// At namespace scope rather than in the anonymous namespace below so the
// suite can drive the loop against a signal-interrupted pipe by extern
// declaration; nothing else should call it outside this file.
void write_all_signal_safe(int fd, const char* bytes, std::size_t count) noexcept {
    while (count != 0U) {
        const ssize_t wrote = ::write(fd, bytes, count);
        if (wrote <= 0) {
            if (wrote < 0 && errno == EINTR) continue;
            return;
        }
        bytes += wrote;
        count -= static_cast<std::size_t>(wrote);
    }
}

// Kept at namespace scope for the same focused-test reach as
// write_all_signal_safe above. poll(2) accepts milliseconds, while ckVision's
// deadline contract is nanosecond-precise. A positive fractional millisecond
// must round up: truncating it to zero turns every near-deadline wait into a
// non-blocking poll and lets the owning loop spin until the clock catches up.
int poll_timeout_milliseconds(std::int64_t now_nanos,
                              std::int64_t deadline_nanos) noexcept {
    if (deadline_nanos == std::numeric_limits<std::int64_t>::max()) return -1;
    if (deadline_nanos <= now_nanos) return 0;

    const auto remaining_nanos = static_cast<std::uint64_t>(deadline_nanos) -
                                 static_cast<std::uint64_t>(now_nanos);
    const auto remaining_millis = 1U + ((remaining_nanos - 1U) / 1'000'000U);
    const auto maximum_millis =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    return remaining_millis > maximum_millis
               ? std::numeric_limits<int>::max()
               : static_cast<int>(remaining_millis);
}

namespace {

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

static_assert(std::atomic<int>::is_always_lock_free,
              "D-024 signal-handler state requires lock-free atomic<int>");
static_assert(std::atomic<bool>::is_always_lock_free,
              "D-024 signal-handler state requires lock-free atomic<bool>");
static_assert(std::atomic<const char*>::is_always_lock_free,
              "D-024 assertion-diagnostic pointers require lock-free atomic<const char*>");
static_assert(std::atomic<std::size_t>::is_always_lock_free,
              "D-024 assertion-diagnostic sizes require lock-free atomic<size_t>");

// A kitty enhancement push, masked to what ckVision ever requests: the
// alternate-key enhancement is deliberately absent from the mask (D-047).
std::string kitty_push_sequence(int flags) {
    return "\x1B[>" + std::to_string(flags & kKittyRequestedFlags) + "u";
}

std::string make_session_enter_sequence(const Capabilities& caps, bool enable_capability_probes) {
    std::string sequence{"\x1B[?1049h"};
    // Kitty's keyboard protocol has a real session stack: push the stated
    // profile contract when the embedder gave one, else the baseline
    // (disambiguation plus event-type reports); a probing session raises
    // its request to the full set once the protocol is proven and reads
    // back what the host honoured (D-055). The matching pop in the restore
    // sequence below returns precisely to the host's prior keyboard state.
    // Do not similarly force xterm modifyOtherKeys here: its public setting
    // has no corresponding save/restore stack, so a library cannot restore
    // an arbitrary host level exactly. A ModifyOtherKeys capability remains
    // an explicit host promise.
    if (caps.keyboard_protocol == KeyboardProtocol::Kitty)
        sequence += kitty_push_sequence(caps.kitty_keyboard_flags != 0 ? caps.kitty_keyboard_flags
                                                                       : kKittyBaselineFlags);
    switch (caps.mouse_protocol) {
        case MouseProtocol::None: break;
        case MouseProtocol::X10: sequence += "\x1B[?9h"; break;
        case MouseProtocol::SGR:
            // 1003 rather than 1002: motion is reported whether or not a
            // button is held. Without it the pointer's position between
            // clicks is unknown, and a pointer shape or a hover highlight
            // has nothing to be a function of.
            sequence += "\x1B[?1003h\x1B[?1006h";
            // Only a trusted explicit pixel profile enters this mode with the
            // session. A probing session enables it at the start of each
            // bounded probe window, so a failed probe can reset it and a
            // later resize probe can establish it again.
            if (caps.pixel_mouse) sequence += "\x1B[?1016h";
            break;
    }
    if (caps.bracketed_paste) sequence += "\x1B[?2004h";
    if (caps.focus_events) sequence += "\x1B[?1004h";
    // Mode 2031 opts into unsolicited color-preference notifications. It is
    // probe policy rather than a baseline capability: curated profiles keep
    // their explicit contracts by disabling probes, and the mode is exposed
    // only after positive DECRQM/DSR evidence.
    if (enable_capability_probes) sequence += "\x1B[?2031h";
    return sequence;
}

std::string make_session_restore_sequence(const Capabilities& caps, bool enable_capability_probes) {
    std::string sequence;
    switch (caps.mouse_protocol) {
        case MouseProtocol::None: break;
        case MouseProtocol::X10: sequence += "\x1B[?9l"; break;
        case MouseProtocol::SGR:
            // 1003 and 1006 are the only SGR modes the session always
            // enters. Do not reset 1000 or 1002 merely because they are
            // other mouse tracking protocols: ckVision never set them.
            // Likewise, 1016 is entered only for an explicit pixel profile
            // or a probe window, so a non-probing cell-mouse session must
            // leave it untouched.
            sequence += "\x1B[?1003l\x1B[?1006l";
            if (caps.pixel_mouse || enable_capability_probes) sequence += "\x1B[?1016l";
            break;
    }
    if (caps.bracketed_paste) sequence += "\x1B[?2004l";
    if (caps.focus_events) sequence += "\x1B[?1004l";
    // Hand the pointer back. This belongs in the ledger the signal handler
    // replays and not merely in an orderly shutdown path: a shape is host
    // state, so an application that dies mid-hover would otherwise leave a
    // resize arrow over somebody's shell.
    if (caps.pointer_shapes) sequence += kPointerShapeResetSequence;
    // The capability probe temporarily enables mode 2026 before asking
    // DECRQM, then resets it immediately. Keep this reset in the session
    // ledger too: a fatal signal between those bytes must never leave the
    // host holding synchronized output.
    if (enable_capability_probes) sequence += "\x1B[?2026l";
    if (enable_capability_probes) sequence += "\x1B[?2031l";
    if (caps.keyboard_protocol == KeyboardProtocol::Kitty) sequence += "\x1B[<u";
    sequence += "\x1B[?25h\x1B[0m\x1B[?1049l";
    return sequence;
}

// OSC 10/11 ask for the terminal's dynamic text foreground/background. The
// background is the initial Dark/Light hint; foreground is a contrast fallback
// only when no background reply arrives. The probe briefly enables mode 2026
// before DECRQM so a set report proves synchronized-output support without
// delaying the baseline frame. DECRQM also checks mode 1016 (SGR-pixel mouse)
// and our enabled mode 2031 (live color-preference notifications). Primary DA
// advertises parameter 4 when Sixel graphics are available;
// XTSMGRAPHICS requests the Sixel color-register and geometry limits; XTWINOPS
// 16 reports the character-cell pixel size, and 14 the text area's total
// pixel size — the fallback cell-metric source for terminals (iTerm2) that
// leave 16 unanswered. CSI ? u asks whether the kitty keyboard protocol
// exists: only a terminal that answers may be switched to it, since one that
// ignores the switch would keep sending legacy sequences that the kitty
// decode path reads differently. InputDecoder turns changed replies into
// CapabilityChangedEvent values.
constexpr std::string_view kCapabilityProbeSequence =
    "\x1B]10;?\x1B\\\x1B]11;?\x1B\\\x1B[?2026h\x1B[?2026$p\x1B[?2026l\x1B[?2031$p\x1B[c\x1B[?1;4;0S\x1B[?2;4;0S\x1B[?1016$p\x1B[16t\x1B[14t\x1B[?u";
constexpr std::int64_t kCapabilityProbeTimeoutNanos = 250'000'000LL;

struct SessionRecord {
    std::atomic<int> state{kFree};
    int output_fd = -1;
    int input_fd = -1;
    struct termios original_termios{};
    struct termios raw_termios{};
    char enter_bytes[kRestoreBufSize]{};
    std::size_t enter_len = 0;
    char restore_bytes[kRestoreBufSize]{};
    std::size_t restore_len = 0;
    std::atomic<bool> resumed{false};
};

SessionRecord g_sessions[kMaxSessions];
constexpr std::array<int, 8> kManagedSignals = {
    SIGSEGV,
    SIGABRT,
    SIGBUS,
    SIGILL,
    SIGFPE,
    SIGWINCH,
    SIGTSTP,
    SIGCONT,
};
std::array<struct sigaction, kManagedSignals.size()> g_saved_signal_actions{};
std::atomic_flag g_signal_action_lock = ATOMIC_FLAG_INIT;
int g_live_session_count = 0;  // guarded by g_signal_action_lock
// The same fact as `g_live_session_count != 0`, readable without the lock.
// `publish_assertion_failure` needs it and must not take the lock to get it:
// `release_session_handlers` CKV_ASSERTs while HOLDING it, and a spinlock is
// not recursive — an assert that waited for it would hang instead of abort,
// which is a strictly worse outcome than the silence being fixed here.
std::atomic<bool> g_fatal_handler_installed{false};
std::atomic<bool> g_callback_failure{false};
std::atomic<const char*> g_assertion_expr{nullptr};
std::atomic<const char*> g_assertion_file{nullptr};
std::atomic<std::size_t> g_assertion_expr_size{0};
std::atomic<std::size_t> g_assertion_file_size{0};
std::atomic<int> g_assertion_line{0};

constexpr char kCallbackFailureDiagnostic[] = "ckVision contract violation: application callback threw\n";
constexpr char kContractViolationPrefix[] = "ckVision contract violation: ";

// SIGWINCH has no terminal-session identity.  Its sole purpose here is
// therefore to interrupt a blocked poll() promptly; every PosixTerminal
// compares *its own* current geometry with last_size_ before returning,
// which prevents a resize on one PTY from becoming a spurious ResizeEvent
// in another session.
extern "C" void on_sigwinch(int) {}

void restore_session(SessionRecord& session) noexcept {
    // A truncated restore is worse than none: the terminal is left mid-escape
    // with the screen it was about to leave. The loop, not a (void), is the
    // answer to glibc's warn_unused_result here.
    write_all_signal_safe(session.output_fd, session.restore_bytes, session.restore_len);
    ::tcsetattr(session.input_fd, TCSANOW, &session.original_termios);
}

void write_decimal(int value) noexcept {
    // Filled from the end so the digits leave in one write instead of one
    // syscall each — which is also what makes a partial write recoverable:
    // write_all_signal_safe can resume a run of bytes, not a loop's cursor.
    char digits[16];
    std::size_t at = sizeof digits;
    unsigned int magnitude = value < 0 ? static_cast<unsigned int>(-(value + 1)) + 1U
                                       : static_cast<unsigned int>(value);
    do {
        digits[--at] = static_cast<char>('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U);
    write_all_signal_safe(STDERR_FILENO, &digits[at], sizeof digits - at);
}

void write_assertion_diagnostic() noexcept {
    const char* const expr = g_assertion_expr.load(std::memory_order_acquire);
    if (expr == nullptr) return;
    const char* const file = g_assertion_file.load(std::memory_order_relaxed);
    const std::size_t expr_size = g_assertion_expr_size.load(std::memory_order_relaxed);
    const std::size_t file_size = g_assertion_file_size.load(std::memory_order_relaxed);
    const int line = g_assertion_line.load(std::memory_order_relaxed);
    // The diagnostic is best-effort — stderr may be a pipe whose reader died —
    // but a SHORT count is not the same failure as a dead fd: it truncates
    // the one message this path exists to deliver. Loop for the short
    // counts; the dead-fd give-up lives documented in the helper.
    write_all_signal_safe(STDERR_FILENO, kContractViolationPrefix,
                          sizeof(kContractViolationPrefix) - 1);
    write_all_signal_safe(STDERR_FILENO, expr, expr_size);
    write_all_signal_safe(STDERR_FILENO, " (", 2);
    if (file != nullptr) write_all_signal_safe(STDERR_FILENO, file, file_size);
    write_all_signal_safe(STDERR_FILENO, ":", 1);
    write_decimal(line);
    write_all_signal_safe(STDERR_FILENO, ")\n", 2);
}

// D-024's handler path uses only POSIX.1-2017 async-signal-safe operations:
// write(), tcsetattr(), signal/raise/kill, and lock-free atomic accesses. The
// two static assertions above make the atomic premise a build requirement
// rather than a platform assumption.
extern "C" void fatal_signal_handler(int sig) {
    for (SessionRecord& s : g_sessions) {
        if (s.state.load(std::memory_order_acquire) != kReady) continue;
        restore_session(s);
    }
    // A callback failure reaches this handler through std::abort().  The
    // diagnostic deliberately follows restoration of EVERY live session: a
    // process may own multiple distinct terminals, and writing it while even
    // one remains in its alternate screen would violate the §11 guarantee.
    if (g_assertion_expr.load(std::memory_order_acquire) != nullptr)
        write_assertion_diagnostic();
    else if (g_callback_failure.load(std::memory_order_acquire))
        write_all_signal_safe(STDERR_FILENO, kCallbackFailureDiagnostic,
                              sizeof(kCallbackFailureDiagnostic) - 1);
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

extern "C" void on_sigtstp(int) {
    // A handler that RETURNS must leave errno as it found it (POSIX XSH
    // 2.4.3): the interrupted code may be between a failed call and reading
    // its errno, and everything below is free to clobber it.
    const int saved_errno = errno;
    for (SessionRecord& session : g_sessions)
        if (session.state.load(std::memory_order_acquire) == kReady) restore_session(session);
    ::signal(SIGTSTP, SIG_DFL);

    // SIGTSTP is automatically blocked while this handler runs. Unblock it
    // before sending it again, otherwise it remains pending until after we
    // reinstall this handler and recursively enters us instead of stopping.
    sigset_t suspend_signal{};
    sigemptyset(&suspend_signal);
    sigaddset(&suspend_signal, SIGTSTP);
    ::sigprocmask(SIG_UNBLOCK, &suspend_signal, nullptr);
    ::kill(::getpid(), SIGTSTP);

    // If the default stop was ignored (for example, an orphaned process
    // group), no SIGCONT arrives to reinstall us. Keep future suspends live.
    ::signal(SIGTSTP, &on_sigtstp);
    errno = saved_errno;
}

extern "C" void on_sigcont(int) {
    const int saved_errno = errno;  // same obligation as on_sigtstp
    for (SessionRecord& session : g_sessions) {
        if (session.state.load(std::memory_order_acquire) != kReady) continue;
        ::tcsetattr(session.input_fd, TCSANOW, &session.raw_termios);
        // The sharpest of this file's eleven once-ignored writes: a short
        // count here would resume the PROGRAM with the terminal half-entered
        // — raw termios active, the enter sequence truncated mid-escape —
        // and everything drawn afterwards inherits the corruption.
        write_all_signal_safe(session.output_fd, session.enter_bytes, session.enter_len);
        session.resumed.store(true, std::memory_order_release);
    }
    ::signal(SIGTSTP, &on_sigtstp);
    errno = saved_errno;
}

struct sigaction make_signal_action(void (*handler)(int)) {
    struct sigaction action{};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return action;
}

bool install_session_handlers_unlocked() noexcept {
    const std::array<struct sigaction, kManagedSignals.size()> actions = {
        make_signal_action(&fatal_signal_handler),
        make_signal_action(&fatal_signal_handler),
        make_signal_action(&fatal_signal_handler),
        make_signal_action(&fatal_signal_handler),
        make_signal_action(&fatal_signal_handler),
        make_signal_action(&on_sigwinch),
        make_signal_action(&on_sigtstp),
        make_signal_action(&on_sigcont),
    };
    std::size_t installed = 0;
    for (; installed < kManagedSignals.size(); ++installed) {
        if (::sigaction(kManagedSignals[installed], &actions[installed], &g_saved_signal_actions[installed]) != 0)
            break;
    }
    if (installed == kManagedSignals.size()) return true;
    while (installed != 0) {
        --installed;
        ::sigaction(kManagedSignals[installed], &g_saved_signal_actions[installed], nullptr);
    }
    return false;
}

void restore_session_handlers_unlocked() noexcept {
    for (std::size_t i = 0; i < kManagedSignals.size(); ++i)
        ::sigaction(kManagedSignals[i], &g_saved_signal_actions[i], nullptr);
}

bool retain_session_handlers() noexcept {
    while (g_signal_action_lock.test_and_set(std::memory_order_acquire)) {}
    const bool installed = g_live_session_count != 0 || install_session_handlers_unlocked();
    if (installed) ++g_live_session_count;
    g_fatal_handler_installed.store(g_live_session_count != 0, std::memory_order_release);
    g_signal_action_lock.clear(std::memory_order_release);
    return installed;
}

void release_session_handlers() noexcept {
    while (g_signal_action_lock.test_and_set(std::memory_order_acquire)) {}
    CKV_ASSERT(g_live_session_count > 0);
    --g_live_session_count;
    if (g_live_session_count == 0) restore_session_handlers_unlocked();
    g_fatal_handler_installed.store(g_live_session_count != 0, std::memory_order_release);
    g_signal_action_lock.clear(std::memory_order_release);
}

// Returns the claimed slot index, or -1 if every slot is in use — a
// contract violation the caller CKV_ASSERTs on. Growing a generated session
// sequence past kRestoreBufSize is likewise a contract violation caught here,
// loudly, rather than silently
// truncating a future crash's restore sequence.
int register_session(int output_fd, int input_fd, const struct termios& original, const struct termios& raw,
                     std::string_view enter_bytes, std::string_view restore_bytes) {
    CKV_ASSERT(enter_bytes.size() <= kRestoreBufSize);
    CKV_ASSERT(restore_bytes.size() <= kRestoreBufSize);
    for (int i = 0; i < kMaxSessions; ++i) {
        int expected = kFree;
        if (!g_sessions[i].state.compare_exchange_strong(expected, kClaiming, std::memory_order_relaxed))
            continue;
        // Exclusively claimed: no other thread can also win this slot.
        // Safe to populate before publishing kReady.
        SessionRecord& s = g_sessions[i];
        s.output_fd = output_fd;
        s.input_fd = input_fd;
        s.original_termios = original;
        s.raw_termios = raw;
        s.enter_len = enter_bytes.size();
        std::memcpy(s.enter_bytes, enter_bytes.data(), s.enter_len);
        s.restore_len = restore_bytes.size();
        std::memcpy(s.restore_bytes, restore_bytes.data(), s.restore_len);
        s.resumed.store(false, std::memory_order_relaxed);
        s.state.store(kReady, std::memory_order_release);
        return i;
    }
    return -1;
}

// Appends to a live session's restore ledger. The signal handler replays
// that buffer verbatim, so this only grows it and never reorders what is
// already recorded.
void append_session_restore(int slot, std::string_view bytes) noexcept {
    if (slot < 0 || slot >= kMaxSessions) return;
    SessionRecord& record = g_sessions[slot];
    if (record.restore_len + bytes.size() > kRestoreBufSize) return;
    for (const char byte : bytes) record.restore_bytes[record.restore_len++] = byte;
}

void unregister_session(int slot) noexcept {
    if (slot < 0) return;
    g_sessions[slot].state.store(kFree, std::memory_order_release);
    release_session_handlers();
}

void write_all(int fd, std::string_view bytes) {
    std::size_t off = 0;
    while (off < bytes.size()) {
        const ssize_t n = ::write(fd, bytes.data() + off, bytes.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            // An embedding host may use a non-blocking PTY endpoint while
            // consuming its other side asynchronously. Normal terminal I/O
            // still has to deliver complete frame and session sequences, so
            // wait for writability and retry instead of silently truncating
            // output on backpressure. The D-024 signal handler intentionally
            // does not call this helper: poll() is not async-signal-safe.
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd writable{fd, POLLOUT, 0};
                int ready = 0;
                do {
                    ready = ::poll(&writable, 1, -1);
                } while (ready < 0 && errno == EINTR);
                if (ready > 0 && (writable.revents & POLLOUT)) continue;
            }
            break;  // fd gone / unwritable: nothing more we can do here
        }
        off += static_cast<std::size_t>(n);
    }
}


}  // namespace

void publish_assertion_failure_to_session(const char* expr, const char* file, int line) noexcept {
    // D-024's handler may only read immutable objects. Macro string literals
    // and __FILE__ have static storage duration, and publishing `expr` last
    // makes the preceding file/line stores visible to the SIGABRT handler.
    g_assertion_file.store(file, std::memory_order_relaxed);
    g_assertion_file_size.store(std::strlen(file), std::memory_order_relaxed);
    g_assertion_line.store(line, std::memory_order_relaxed);
    g_assertion_expr_size.store(std::strlen(expr), std::memory_order_relaxed);
    g_assertion_expr.store(expr, std::memory_order_release);
}

}  // namespace ckv::term

namespace ckv::detail {

bool publish_assertion_failure(const char* expr, const char* file, int line) noexcept {
    term::publish_assertion_failure_to_session(expr, file, line);
    // Whether anybody is going to READ what was just published. With no live
    // session there is no fatal handler to restore a screen and print it, and
    // the metadata sits in the ledger unseen — which is every headless test in
    // the fleet: they abort with no message at all, and the reader is left to
    // guess which contract broke. Saying so lets the caller speak instead.
    return term::g_fatal_handler_installed.load(std::memory_order_acquire);
}

}  // namespace ckv::detail

namespace ckv::term {

PosixTerminal::PosixTerminal(const Clock& clock, int output_fd, int input_fd, Capabilities initial_caps,
                             bool enable_capability_probes)
    : clock_(clock),
      output_fd_(output_fd),
      input_fd_(input_fd),
      observed_caps_(initial_caps),
      caps_(initial_caps),
      decoder_(initial_caps),
      last_size_(size()),
      capability_probes_enabled_(enable_capability_probes) {
    profile_kitty_flags_ = initial_caps.kitty_keyboard_flags & kKittyRequestedFlags;
    // True exactly when the enter sequence below pushes an entry onto the
    // host's kitty stack; every later stack step is guarded by it.
    kitty_push_active_ = initial_caps.keyboard_protocol == KeyboardProtocol::Kitty;
    // The decoder needs the current grid from the first byte on: it is what
    // separates cell from pixel mouse coordinates and what an XTWINOPS 14
    // reply is divided by. Kept current at every observed resize below.
    decoder_.set_cell_grid(last_size_);
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
    wait_handles_[0] = WaitHandle{WaitHandleKind::PosixFileDescriptor,
                                  static_cast<std::uintptr_t>(input_fd_)};
    wait_handles_[1] = WaitHandle{WaitHandleKind::PosixFileDescriptor,
                                  static_cast<std::uintptr_t>(self_pipe_read_)};
    wait_handle_count_ = wait_handles_.size();

    struct termios original{};
    CKV_ASSERT(::tcgetattr(input_fd_, &original) == 0);
    struct termios raw = original;
    ::cfmakeraw(&raw);

    // Register BEFORE mutating any OS state (raw mode, alternate
    // screen). If every registry slot is already in use, CKV_ASSERT
    // fires here and the process aborts with the terminal completely
    // untouched — no restoration is owed because nothing was changed.
    // Registering only after entering raw mode/alt-screen (the
    // original ordering) left a failed construction's tty permanently
    // raw and alternate-screened with no destructor ever running to
    // undo it, breaking the "restoration guaranteed on every exit
    // path" promise for exactly the path meant to guarantee it.
    const std::string session_enter = make_session_enter_sequence(caps_, capability_probes_enabled_);
    const std::string session_restore = make_session_restore_sequence(caps_, capability_probes_enabled_);
    session_slot_ = register_session(output_fd_, input_fd_, original, raw, session_enter, session_restore);
    CKV_ASSERT(session_slot_ >= 0);
    CKV_ASSERT(retain_session_handlers());

    CKV_ASSERT(::tcsetattr(input_fd_, TCSANOW, &raw) == 0);

    write_all(output_fd_, session_enter);
    begin_capability_probes();
}

PosixTerminal::~PosixTerminal() {
    restore();
    if (self_pipe_read_ >= 0) ::close(self_pipe_read_);
    if (self_pipe_write_ >= 0) ::close(self_pipe_write_);
}

Size PosixTerminal::size() const noexcept {
    struct winsize ws{};
    if (::ioctl(output_fd_, TIOCGWINSZ, &ws) != 0) return Size{80, 24};
    return Size{ws.ws_col, ws.ws_row};
}

void PosixTerminal::begin_capability_probes() {
    if (!capability_probes_enabled_) {
        configure_decoder_capability_update_policy();
        synchronize_sgr_mouse_input_policy();
        return;
    }
    // Capability replies carry no request identifier.  A fresh window must
    // therefore discard any partially accumulated probe evidence from the
    // preceding one; otherwise a late DECRPM 1016 could be paired with a
    // future XTWINOPS reply and falsely enable pixel coordinates.
    decoder_.begin_capability_probe_window(observed_caps_);
    // The deadline IS the window: it must exist before the suppression
    // policy is derived from it, or the policy concludes no window is open
    // and pixel-ambiguous reports sail through as fictitious cell clicks.
    probe_deadline_nanos_ = clock_.now_nanos() + kCapabilityProbeTimeoutNanos;
    synchronize_sgr_mouse_input_policy();
    // Enable mode 1016 for this window before querying it. A prior failed
    // window resets the mode, so this must happen on every re-probe rather
    // than only at session construction.
    if (observed_caps_.mouse_protocol == MouseProtocol::SGR && !observed_caps_.pixel_mouse)
        write_all(output_fd_, "\x1B[?1016h");
    write_all(output_fd_, kCapabilityProbeSequence);
    // Asked separately because it is the one probe whose silence is not an
    // answer. A host implementing the kitty specification replies with a
    // flag per shape and earns the CSS vocabulary; one implementing only
    // the xterm proposal reads the query as a shape name it does not have
    // and resets its pointer — harmless here, because a probe window is
    // also a re-present, and the frame that follows re-states the shape.
    if (observed_caps_.pointer_shapes)
        write_all(output_fd_, kPointerShapeQuerySequence);
}

void PosixTerminal::configure_decoder_capability_update_policy() noexcept {
    if (probe_deadline_nanos_ >= 0) {
        decoder_.set_capability_update_policy(CapabilityUpdatePolicy::AcceptProbeRefinements);
    } else if (capability_probes_enabled_) {
        // Between windows a probing session still accepts the refinements
        // whose authority a window established: the policy itself gates
        // color-scheme changes on verified 2031 and kitty flag readbacks
        // on the verified protocol, so an unverified session admits
        // nothing more than geometry measurements here.
        decoder_.set_capability_update_policy(CapabilityUpdatePolicy::AcceptVerifiedLiveRefinements);
    } else {
        decoder_.set_capability_update_policy(CapabilityUpdatePolicy::Reject);
    }
}

void PosixTerminal::synchronize_sgr_mouse_input_policy() noexcept {
    // While mode 1016 is in use but its metric has not been proved, a report's
    // numbers have no unambiguous interpretation. Consume rather than
    // delivering a potentially dangerous click at the wrong cell.
    // Only while a probe window is actually OPEN. Gating on the
    // probes-enabled setting instead would make the suppression permanent
    // on every terminal that never proves mode 1016: the window closes,
    // finish_capability_probes() lifts the suppression, and the next
    // resize-triggered re-detection puts it straight back — leaving the
    // application with no mouse at all rather than with cell-accurate
    // mouse, which is the whole point of falling back.
    // ...and only while that window has not yet elapsed. A terminal that
    // simply never answers the probe must still end up with a working
    // mouse: muting input until a reply that never comes is worse than
    // reading a report as cell coordinates, which is what the fence
    // restores the terminal to anyway.
    const bool probe_window_open =
        probe_deadline_nanos_ >= 0 && clock_.now_nanos() < probe_deadline_nanos_;
    decoder_.set_sgr_mouse_input_suppressed(probe_window_open &&
                                            observed_caps_.mouse_protocol == MouseProtocol::SGR &&
                                            !observed_caps_.pixel_mouse);
}

void PosixTerminal::finish_capability_probes() {
    probe_deadline_nanos_ = -1;
    decoder_.set_capabilities(observed_caps_);
    // If mode 1016 did not become a usable capability, return the terminal to
    // ordinary SGR cell coordinates before accepting subsequent mouse input.
    // Keeping the mode enabled would make a late pixel report look like a
    // large cell coordinate after the probe fence closes.
    if (observed_caps_.mouse_protocol == MouseProtocol::SGR && !observed_caps_.pixel_mouse) {
        write_all(output_fd_, "\x1B[?1016l");
        // Re-assert tracking immediately afterwards. Disabling one mouse
        // mode is not supposed to disturb the others, but terminals differ
        // and a host that stops reporting entirely leaves the application
        // with no mouse at all — an invisible, total failure. Re-enabling
        // costs two short sequences once per probe window and makes the
        // outcome independent of how the terminal interpreted the reset.
        write_all(output_fd_, "\x1B[?1003h\x1B[?1006h");
    }
    // Adopting the kitty keyboard protocol waits until its own reply proved
    // it exists. It reports modifiers explicitly — which is what makes a
    // macOS Option chord arrive as Alt instead of as a composed character —
    // and reports key releases, which is what lets a button stay down while
    // its key is held.
    negotiate_kitty_enhancements();
    // A resize withheld graphics while it waited for the fresh maximum
    // geometry this terminal had answered with before. The window has now
    // closed without one, so the wait is over either way: restore what DA1
    // established, with no finite bound, rather than leave a terminal that
    // draws pictures showing cell fallbacks until it is restarted. A
    // terminal answering the query once and not again is a reply that went
    // missing, never evidence that its graphics went away.
    if (withheld_sixel_graphics_) {
        withheld_sixel_graphics_ = false;
        if (!observed_caps_.sixel_graphics) {
            observed_caps_.sixel_graphics = true;
            (void)update_effective_capabilities();
        }
    }
    configure_decoder_capability_update_policy();
    decoder_.set_capabilities(observed_caps_);
    decoder_.set_sgr_mouse_input_suppressed(false);
    decoder_.require_verified_sixel_geometry(false);

    // What this session concluded about the host, written where the frames
    // are. Every question that starts "was that build/terminal/setting
    // actually in use?" is answered by the line above the frames it is
    // asking about, instead of by running the session again.
    if (graphics_log_enabled()) {
        const auto size_text = [](Size size) {
            return std::to_string(size.width) + "x" + std::to_string(size.height);
        };
        const std::string keyboard =
            caps_.keyboard_protocol == KeyboardProtocol::Kitty
                ? "kitty(flags " + std::to_string(caps_.kitty_keyboard_flags) + ")"
                : (caps_.keyboard_protocol == KeyboardProtocol::ModifyOtherKeys ? "modifyOtherKeys"
                                                                                : "legacy");
        graphics_log("terminal: sixel=" + std::string(caps_.sixel_graphics ? "yes" : "NO") +
                     " cell=" + size_text(caps_.cell_pixels) + "px grid=" + size_text(last_size_) +
                     " registers=" + std::to_string(caps_.sixel_color_registers) +
                     " max-geometry=" + size_text(caps_.sixel_max_geometry) +
                     " keyboard=" + keyboard +
                     " synchronized-output=" + (caps_.synchronized_output ? "yes" : "no") +
                     " overrides{sixel=" +
                     (overrides_.sixel_graphics ? (*overrides_.sixel_graphics ? "on" : "off") : "-") +
                     " sync=" +
                     (overrides_.synchronized_output ? (*overrides_.synchronized_output ? "on" : "off") : "-") +
                     " cell=" + (overrides_.cell_pixels ? size_text(*overrides_.cell_pixels) : "-") + "}");
    }
}

void PosixTerminal::negotiate_kitty_enhancements() {
    // Runs when a probe window closes (and again from the absorb path if
    // the protocol's proof straggles into the closing batch). What is
    // pushed is a request; only the readback that follows is an answer,
    // and it may land after the window — the decoder accepts a verified
    // session's flag readback for the session's lifetime.
    if (kitty_flags_negotiated_) return;
    if (observed_caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    kitty_flags_negotiated_ = true;
    if (kitty_push_active_) {
        // The enter sequence already pushed. An embedder that stated an
        // explicit flag contract keeps exactly that; an unstated profile
        // entered on the baseline, and this replaces our own stack entry —
        // never a second push, so the restore ledger's single pop stays
        // exact.
        if (profile_kitty_flags_ != 0) return;
        write_all(output_fd_, "\x1B[<u");
        write_all(output_fd_, kitty_push_sequence(kKittyRequestedFlags));
        write_all(output_fd_, "\x1B[?u");
        return;
    }
    // Runtime adoption: nothing of ours is on the host's stack yet. The
    // matching pop belongs in the ledger the signal handler replays;
    // pushing without recording it would strand a terminal in the protocol
    // after a crash.
    kitty_push_active_ = true;
    write_all(output_fd_, kitty_push_sequence(kKittyRequestedFlags));
    write_all(output_fd_, "\x1B[?u");
    append_session_restore(session_slot_, "\x1B[<u");
}

void PosixTerminal::maybe_demote_kitty_keyboard() {
    // All-keys-as-escape-codes without associated text would leave this
    // side reconstructing what a key typed from a keyboard layout it
    // refuses to model (D-047): a shifted or composed character would
    // arrive as a key number and nothing else. No host is known to honour
    // the one without the other; a session on one that does steps its own
    // stack entry back to the baseline and re-reads what is then in force.
    // Only our own entry is ever popped: a pre-existing host state with
    // this shape belongs to whoever set it.
    if (kitty_demoted_ || !kitty_push_active_) return;
    if (observed_caps_.keyboard_protocol != KeyboardProtocol::Kitty) return;
    const int flags = observed_caps_.kitty_keyboard_flags;
    if ((flags & kKittyReportAllKeysAsEscapeCodes) == 0 ||
        (flags & kKittyReportAssociatedText) != 0)
        return;
    kitty_demoted_ = true;
    write_all(output_fd_, "\x1B[<u");
    write_all(output_fd_, kitty_push_sequence(kKittyBaselineFlags));
    write_all(output_fd_, "\x1B[?u");
}

bool PosixTerminal::invalidate_resize_dependent_capabilities() noexcept {
    if (!capability_probes_enabled_) return false;

    const bool has_pixel_evidence = observed_caps_.cell_pixels.width != 0 ||
                                    observed_caps_.cell_pixels.height != 0 || observed_caps_.pixel_mouse;
    // Only a terminal that has actually answered the geometry query has an
    // answer worth re-establishing. One that never answers it has no
    // resize-dependent graphics state at all.
    const bool has_reported_geometry = observed_caps_.sixel_max_geometry.width != 0 ||
                                       observed_caps_.sixel_max_geometry.height != 0;
    if (!has_pixel_evidence && !has_reported_geometry) return false;

    // A resize can also mean a font or display-scale change. Retaining the
    // old conversion would turn SGR-pixel coordinates into wrong cells until
    // XTWINOPS 16 answers. The new probe window must establish both the cell
    // metric and DECRPM 1016 again; set_capabilities resets the decoder's
    // separately remembered mode proof for exactly that reason.
    observed_caps_.cell_pixels = {};
    observed_caps_.pixel_mouse = false;
    // XTSMGRAPHICS maximum geometry is the minimum of its graphics limit and
    // the terminal window (xterm control-sequences reference). Retaining a
    // runtime result across a resize could therefore permit an image larger
    // than the newly allowed geometry, so it is dropped and asked for again.
    //
    // WHETHER the terminal draws Sixel at all is not resize-dependent: a
    // terminal does not stop supporting a protocol because its window
    // changed size, and DA1 — the reply that establishes it — says nothing
    // about window size either. Clearing it here, and then declining to
    // accept DA1 again without a geometry reply, silently and permanently
    // disabled graphics on every terminal that draws Sixel without also
    // implementing XTSMGRAPHICS: the first resize turned the pictures into
    // cell fallbacks for the rest of the session, and no reply the terminal
    // was ever going to send could turn them back on. An absent optional
    // reply is not evidence of an absent capability.
    if (has_reported_geometry) {
        // This terminal answers the geometry query, so a fresh answer is
        // milliseconds away: withhold pictures until it arrives rather than
        // send one bounded by a limit that no longer applies.
        withheld_sixel_graphics_ = observed_caps_.sixel_graphics;
        observed_caps_.sixel_graphics = false;
        observed_caps_.sixel_color_registers = 0;
        observed_caps_.sixel_max_geometry = {};
        decoder_.set_capabilities(observed_caps_);
        decoder_.require_verified_sixel_geometry(true);
    } else {
        // This one does not, so there is no answer to wait for and nothing
        // about its graphics that the resize changed. Withholding here is
        // what turned every picture into a cell fallback for the rest of
        // the session.
        decoder_.set_capabilities(observed_caps_);
    }
    return update_effective_capabilities();
}

namespace {
// The character cell implied by a window pixel size, when the division
// yields something a terminal could plausibly be drawing with. Terminals
// that leave ws_xpixel/ws_ypixel unset report zero; stale or nonsensical
// values are rejected here rather than propagated into image geometry.
std::optional<Size> cell_from_window_pixels(Size window_pixels, Size grid) noexcept {
    if (window_pixels.width <= 0 || window_pixels.height <= 0) return std::nullopt;
    if (grid.width <= 0 || grid.height <= 0) return std::nullopt;
    const Size cell{window_pixels.width / grid.width, window_pixels.height / grid.height};
    if (cell.width < 2 || cell.height < 4) return std::nullopt;
    if (cell.width > 128 || cell.height > 256) return std::nullopt;
    return cell;
}
}  // namespace

Size PosixTerminal::window_pixel_size() const noexcept {
    struct winsize ws{};
    if (::ioctl(output_fd_, TIOCGWINSZ, &ws) != 0) return Size{};
    return Size{ws.ws_xpixel, ws.ws_ypixel};
}

bool PosixTerminal::update_effective_capabilities() noexcept {
    Capabilities observed = observed_caps_;
    observed.window_pixels = window_pixel_size();
    // The kernel's window pixel size, divided by the grid, is a cell metric
    // the terminal published without being asked — no round trip, so it is
    // available before the first frame rather than milliseconds into the
    // session, and it is what an image is measured against on the terminals
    // where the escape-sequence answers and the drawn result disagree.
    // XTWINOPS still wins when the window size is unset or implausible.
    if (const std::optional<Size> from_window =
            cell_from_window_pixels(observed.window_pixels, last_size_))
        observed.cell_pixels = *from_window;
    const Capabilities effective = apply_capability_overrides(observed, overrides_);
    if (effective == caps_) return false;
    caps_ = effective;
    return true;
}

void PosixTerminal::set_capability_overrides(CapabilityOverrides overrides) {
    if ((overrides.cell_pixels && (overrides.cell_pixels->width <= 0 || overrides.cell_pixels->height <= 0)) ||
        (overrides.sixel_color_registers && *overrides.sixel_color_registers <= 0)) {
        throw std::invalid_argument("terminal capability overrides require positive cell metrics and color caps");
    }
    if (overrides == overrides_) return;
    overrides_ = std::move(overrides);
    if (update_effective_capabilities()) {
        capability_change_pending_ = true;
        wake();
    }
}

std::vector<TerminalEvent> PosixTerminal::poll(std::int64_t deadline_nanos) {
    return poll(deadline_nanos, {});
}

std::vector<TerminalEvent> PosixTerminal::poll(
    std::int64_t deadline_nanos, std::span<const WaitHandle> additional_wait_handles) {
    std::vector<TerminalEvent> events;
    if (capability_change_pending_) {
        capability_change_pending_ = false;
        events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    }
    bool resumed_this_poll = false;
    if (session_slot_ >= 0 &&
        g_sessions[session_slot_].resumed.exchange(false, std::memory_order_acq_rel)) {
        resumed_this_poll = true;
        invalidate_resize_dependent_capabilities();
        begin_capability_probes();
        // A terminal can be resized while this process is stopped. Observe
        // that instance-local geometry before reporting the resume-triggered
        // capability change: Application dispatches a whole poll batch before
        // painting, so the first restored frame then has both the current
        // size and the renewed presentation policy rather than flashing an
        // old-sized frame for one step.
        const Size observed = size();
        if (observed != last_size_) {
            last_size_ = observed;
            decoder_.set_cell_grid(last_size_);
            events.push_back(TerminalEvent{ResizeEvent{observed}});
        }
        events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    }
    const auto expire_capability_probes = [this](std::int64_t now_nanos) {
        if (probe_deadline_nanos_ >= 0 && now_nanos >= probe_deadline_nanos_)
            finish_capability_probes();
    };
    const auto append_decoded = [this, &events, &expire_capability_probes](std::vector<TerminalEvent> decoded,
                                                                            std::int64_t observed_nanos) {
        expire_capability_probes(observed_nanos);
        for (auto& ev : decoded) {
            if (const auto* changed = std::get_if<CapabilityChangedEvent>(&ev)) {
                // InputDecoder emits only backend-authorized capability
                // changes, so this state update cannot retroactively make a
                // stale reply influence later bytes from the same read.
                observed_caps_ = changed->capabilities;
                // The kitty flag readback normally lands after its probe
                // window closed; a protocol proof can also straggle into
                // the batch that closed one. Raise the enhancement request
                // (once) and step back from a half-honoured set (once) as
                // that evidence arrives.
                if (probe_deadline_nanos_ < 0) negotiate_kitty_enhancements();
                maybe_demote_kitty_keyboard();
                if (!update_effective_capabilities()) continue;
                synchronize_sgr_mouse_input_policy();
                events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
                continue;
            }
            events.push_back(std::move(ev));
        }
    };
    const auto collect_resize = [this, &events] {
        const Size observed = size();
        if (observed == last_size_) return;
        last_size_ = observed;
        decoder_.set_cell_grid(last_size_);
        const bool invalidated = invalidate_resize_dependent_capabilities();
        begin_capability_probes();
        events.push_back(TerminalEvent{ResizeEvent{observed}});
        if (invalidated) events.push_back(TerminalEvent{CapabilityChangedEvent{caps_}});
    };
    // This deliberately compares on every poll rather than consuming a
    // process-global SIGWINCH flag.  Signals carry no PTY identity; a
    // per-session comparison is the only correct routing authority. A
    // resumed session already observed its own geometry above.
    if (!resumed_this_poll) collect_resize();

    // An event-bearing batch is delivered promptly — but never INSTEAD of
    // the read. Returning before the read is how a pending override (or a
    // resize, or a resume) used to cost a session its graphics: the
    // caller's next poll can be a whole frame away, a first frame can be
    // expensive, and probe replies that were buffered well inside the
    // window were then read only after it had closed — rejected as stale,
    // on a terminal that had answered within milliseconds. The batch
    // instead harvests whatever is ALREADY readable, with a zero wait, so
    // replies ride the same batch as the event that would otherwise have
    // displaced them.

    std::vector<struct pollfd> fds;
    fds.reserve(2 + additional_wait_handles.size());
    fds.push_back(pollfd{input_fd_, POLLIN, 0});
    fds.push_back(pollfd{self_pipe_read_, POLLIN, 0});
    for (const WaitHandle handle : additional_wait_handles) {
        if (handle.kind != WaitHandleKind::PosixFileDescriptor) continue;
        fds.push_back(pollfd{static_cast<int>(handle.value), POLLIN, 0});
    }
    const std::int64_t now = clock_.now_nanos();
    expire_capability_probes(now);
    // The probe window bounds reply acceptance, never the caller's wait.
    // Waking early merely to notice a missing reply would delay run()'s first
    // baseline frame — exactly what baseline-first presentation forbids.
    std::int64_t effective_deadline = deadline_nanos;
    if (!events.empty()) effective_deadline = now;
    if (const auto decoder_deadline = decoder_.next_timeout_nanos())
        effective_deadline = std::min(effective_deadline, *decoder_deadline);
    const int timeout_ms = poll_timeout_milliseconds(now, effective_deadline);

    const int rc = ::poll(fds.data(), fds.size(), timeout_ms);
    if (rc > 0) {
        if (fds[1].revents & POLLIN) {
            char discard[64];
            while (::read(self_pipe_read_, discard, sizeof(discard)) > 0) {
            }
        }
        // A PTY peer can report its final EOF as POLLHUP without POLLIN.
        // Attempt the read for either readiness form so D-040's disconnect
        // recovery cannot depend on a platform-specific revents combination.
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            char buf[4096];
            const ssize_t n = ::read(input_fd_, buf, sizeof(buf));
            if (n > 0) {
                const std::int64_t observed_nanos = clock_.now_nanos();
                // Admission is a decoding policy, not a post-processing
                // filter. Select the policy at the instant these bytes were
                // observed, before InputDecoder sees the first sequence, so
                // a late reply cannot influence a following event in this
                // same read.
                expire_capability_probes(observed_nanos);
                append_decoded(decoder_.feed(std::string_view(buf, static_cast<std::size_t>(n)), observed_nanos),
                               observed_nanos);
            } else if (n == 0) {
                // EOF is a definite terminal-input disconnect. Preserve any
                // partially received paste only as recovered TextEvent data;
                // it must never fall through as ordinary key input after the
                // stream boundary vanished (D-040).
                append_decoded(decoder_.abort_paste(), clock_.now_nanos());
            }
        }
    } else if (rc == 0) {
        const std::int64_t observed_nanos = clock_.now_nanos();
        expire_capability_probes(observed_nanos);
        append_decoded(decoder_.poll_timeout(observed_nanos), observed_nanos);
    } else if (errno == EINTR) {
        // A SIGWINCH may have arrived between the first observation and
        // poll(). Observe again so this very call reports its own session's
        // resize without waiting for a subsequent frame.
        collect_resize();
    }
    return events;
}

void PosixTerminal::restore() noexcept {
    if (session_slot_ < 0) return;
    SessionRecord& session = g_sessions[session_slot_];
    write_all(output_fd_, std::string_view(session.restore_bytes, session.restore_len));
    ::tcsetattr(input_fd_, TCSANOW, &session.original_termios);
    unregister_session(session_slot_);
    session_slot_ = -1;
    wait_handle_count_ = 0;
}

void PosixTerminal::write_diagnostic_after_restore(std::string_view message) noexcept {
    std::size_t written = 0;
    while (written < message.size()) {
        const ssize_t result = ::write(STDERR_FILENO, message.data() + written, message.size() - written);
        if (result <= 0) return;
        written += static_cast<std::size_t>(result);
    }
}

[[noreturn]] void PosixTerminal::terminate_after_callback_failure() noexcept {
    // This is handler-visible D-024 state.  bool is statically required to
    // be lock-free above, and the fixed diagnostic lives for the process, so
    // the fatal handler needs neither allocation nor non-signal-safe work.
    g_callback_failure.store(true, std::memory_order_release);
    std::abort();
}

void PosixTerminal::write(std::string_view bytes) {
    // Every byte this session writes, when CKVISION_OUTPUT_CAPTURE names a
    // file. A terminal that renders a frame wrongly cannot be argued with
    // from this side; the bytes it was given can be replayed into ckVision's
    // own decoder, which settles whether the frame or its reader was at
    // fault. Opened once, on the first write.
    if (capture_stream_ == nullptr && !capture_attempted_) {
        capture_attempted_ = true;
        if (const char* const path = std::getenv("CKVISION_OUTPUT_CAPTURE"); path != nullptr && *path != '\0')
            capture_stream_ = std::fopen(path, "wb");
    }
    if (capture_stream_ != nullptr) {
        std::fwrite(bytes.data(), 1, bytes.size(), capture_stream_);
        std::fflush(capture_stream_);
    }
 write_all(output_fd_, bytes); }

void PosixTerminal::set_title(std::string_view title) {
    write_all(output_fd_, "\x1B]0;" + sanitize_osc_text(title) + "\x07");
}

void PosixTerminal::bell() { write_all(output_fd_, "\x07"); }

void PosixTerminal::write_clipboard(std::string_view text) {
    if (!caps_.clipboard_write) return;
    write_all(output_fd_, "\x1B]52;c;" + base64::encode(text) + "\x07");
}

void PosixTerminal::wake() noexcept {
    // A lost wake is a poll() sleeping through work that is already queued,
    // so EINTR — nothing written — is retried. EAGAIN is the opposite of
    // lost: the pipe still holds an unread wake byte, so the wake this call
    // wanted is already pending; that is the self-pipe design coalescing,
    // not a failure. A one-byte pipe write cannot be short (atomic ≤
    // PIPE_BUF), so those two exhaust the outcomes worth acting on.
    const char byte = 0;
    ssize_t result = 0;
    do {
        result = ::write(self_pipe_write_, &byte, 1);
    } while (result < 0 && errno == EINTR);
}

}  // namespace ckv::term
