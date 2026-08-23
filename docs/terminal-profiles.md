<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Terminal capability profiles

`ckv::term::capabilities_for_profile` provides named, conservative capability
baselines. A profile is an explicit host-construction choice; ckVision never
reads `TERM`, a multiplexer environment variable, or a configuration file to
guess one. Hosts may instead pass a fully explicit `Capabilities` value to
`PosixTerminal`. With an existing `PosixClock clock`,
`PosixTerminal(clock, output_fd, input_fd, TerminalProfile::LinuxConsole)` and
`HeadlessTerminal(size, TerminalProfile::LinuxConsole)` are the direct
construction forms; named profiles disable raw runtime probe refinement by
default. Headless scripts can still inject an explicit `CapabilityChangedEvent`
when a scenario deliberately models a trusted host-policy change.

All currently shipped profiles use the documented narrow East-Asian-Ambiguous
width convention. A host with an explicit wide convention may report it through
`Capabilities::ambiguous_width_is_wide`; ckVision retains its own deterministic
logical cell width either way. Until D-OPEN-7 has a published, interoperable
width-reporting protocol, the Presenter treats every non-ASCII grapheme as a
cursor-synchronization boundary for every profile and re-addresses the next
cell absolutely. Therefore a terminal disagreement can affect only the
grapheme just emitted, never shift the rest of a line.

| Profile | Guaranteed baseline | Deliberately not assumed |
|---|---|---|
| `ModernVt` | 256 colors, SGR mouse, bracketed paste, focus reporting | graphics, pixel metrics/mouse, clipboard, synchronized output, color scheme |
| `TmuxConservative` | 256-color text and keyboard | mouse, focus/paste forwarding, graphics, clipboard, pixel geometry, synchronized output |
| `ScreenConservative` | 16-color text and keyboard | all optional xterm extensions, including mouse, focus/paste forwarding, graphics and pixel geometry |
| `LinuxConsole` | 16-color text and keyboard | all optional xterm extensions; mouse is disabled under D-OPEN-6 because a mouse daemon is not a zero-dependency library facility |

The tmux and screen profiles are intentionally lower bounds. Their actual
behavior is configuration- and outer-terminal-dependent, so a host with
version-specific evidence can pass an explicit stronger profile. The Linux
console documentation specifies a VT102/ECMA-48 subset and X10 mouse reporting
only when a mouse-aware userspace component supplies updates; that does not
justify claiming SGR or pixel mouse support.

`PosixTerminal` enters only input modes authorized by the selected capabilities
and resets only modes it established, in addition to its unconditional
alternate-screen and raw-mode lifecycle. In particular, a non-probing
`ModernVt` session resets its 1003/1006 SGR modes but does not alter the
distinct 1000 or 1002 mouse protocols or pixel mode 1016. An SGR profile also requests mode 1016
only in a bounded capability probe (or when an explicit trusted pixel profile
requires it); that request alone never turns on
`Capabilities::pixel_mouse`. When runtime probing is enabled, the backend also
enters DEC mode 2031 as its explicit probe subscription and restores it on
exit; `Capabilities::color_scheme_notifications` remains false until a
positive report proves the host will send live color-preference changes.
Runtime capability probes are normally disabled for a curated profile so the
explicit host contract remains authoritative.

The `ModernVt` PTY contract exercises both halves of that baseline together:
the session enables the documented modes, then ordinary terminal traffic yields
a focus event, sanitized bracketed-paste text, and a cell-space SGR mouse
event. Since its profile does not assert pixel metrics, that mouse event has no
pixel coordinate. The conservative profiles instead consume those unrequested
extension streams without allowing their payload to reach application input.
Their real-PTY contracts additionally send unsolicited focus, bracketed-paste,
and SGR-mouse streams immediately before an ordinary key: all three streams
are consumed, while that key still reaches the application. This distinguishes
a conservative boundary from a decoder that merely disables every input path.
An explicit scripted `CapabilityChangedEvent` is different from an
unrequested raw reply: it is host policy. Deterministic terminals apply that
new profile to their public capability report and decoder before interpreting
later scripted bytes; a later conservative transition withdraws those input
extensions again.

An explicit profile may instead set both `pixel_mouse` and a nonzero
`cell_pixels` metric when the host has already established SGR-pixel reporting.
Each SGR mouse event then carries the terminal's pixel position as well as the
cell position derived from that metric; the POSIX PTY contract exercises that
dual-space delivery and the corresponding 1003/1006/1016 session restoration.
The SGR wheel button codes map to the four directional `MouseButton` values:
up/down (64/65) and left/right (66/67). Widgets that only scroll vertically
may ignore the horizontal events, but a backend or terminal view never
collapses them into vertical motion.
An SGR session enters mode 1003, not 1002, so motion is reported whether or
not a button is held (D-049). That is what makes the pointer's position known
between clicks, which is what a pointer shape and a hover highlight are
functions of; 1002 would report motion only during a drag. The extra traffic
is one report per cell crossed, and a report that hovers the same view as the
last one is resolved and dropped before it reaches a widget.

Such a session also writes mouse pointer shapes (OSC 22) and resets the shape
on exit from the same ledger a fatal signal replays. Unlike every other
capability here, that is not gated on the host having said it can: the
protocol's support query is optional, hosts implementing only the original
xterm proposal draw the shapes while never replying, and an unrecognized OSC
is discarded — so silence is not evidence of absence, and writing it costs
nothing where it is not understood. The query is still sent, and a host that
answers earns the CSS vocabulary and per-shape degradation; a host that says
nothing gets the X11 cursorfont names both host families accept.
`CapabilityOverrides::pointer_shapes` silences it for a host observed to do
something other than ignore it. Profiles with no mouse never emit it at all,
since a shape is a statement about where the pointer is and they are never
told.

An authoritative non-pixel profile never enables mode 1016. A probing SGR
session enables it only while obtaining its DECRPM proof; until that proof and
an XTWINOPS metric agree, SGR reports are consumed rather than misread as cell
coordinates. If the bounded probe expires without a usable pixel capability,
ckVision resets mode 1016 before ordinary SGR input is delivered again.

XTSMGRAPHICS maximum Sixel geometry is window-sensitive: xterm constrains it
by both its graphics limit and the current window. Accordingly, a probing
session withdraws previously runtime-proven graphics and its palette/geometry
limits when the terminal resizes. The mandatory text fallback remains visible
until a fresh finite geometry reply re-establishes the raster capability; a
DA1 Sixel advertisement alone is insufficient after that boundary.

An explicit custom `Capabilities` value may select `MouseProtocol::X10` for a
host that documents that legacy press-only protocol. ckVision enables DECSET 9
for that session and decodes only its three-byte-offset press reports; it does
not infer release, motion, or pixel-position support from X10.
