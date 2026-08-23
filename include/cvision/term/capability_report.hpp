// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A human-readable account of what this terminal reported and what
// ckVision concluded from it. Terminals disagree with their own
// self-descriptions in the field — iTerm2 answers DECRPM 1016 as
// "permanently reset" and then sends pixel mouse coordinates anyway —
// so an application needs to be able to show, and a user to paste, the
// evidence rather than argue from behaviour. One builder here keeps that
// report identical wherever it is shown.
#pragma once

#include <string>
#include <vector>

#include "cvision/term/capabilities.hpp"

namespace ckv::term {

// One probed or derived fact: what was asked, what came back.
struct CapabilityReportEntry {
    std::string name;    // "SIXEL graphics"
    std::string value;   // "yes", "10x21 px", "not reported"
    std::string source;  // the query it came from, e.g. "DA1 (CSI c)"
};

// Every capability ckVision probes for, in a stable order. `grid` is the
// terminal's current cell grid; pass {0,0} if unknown.
std::vector<CapabilityReportEntry> capability_report(const Capabilities& caps, Size grid);

// The same content as one plain-text block, suitable for a clipboard
// export or a bug report.
std::string capability_report_text(const Capabilities& caps, Size grid);

}  // namespace ckv::term
