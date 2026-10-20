// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "logmonitor_app.hpp"
#include "native_json.hpp"
#include "cvision/core/process_stream.hpp"
namespace ckv::logmonitor {
// Installed Apple log help returns EX_USAGE (64) with valid help on current
// macOS. Accept it only for this explicit help probe, never for a stream exit.
bool supports_native_json(NativeJsonSchema schema, const core::ProcessRunResult& result) noexcept;
// One framing quantum (<=16 KiB, <=4 envelopes) per supplied-time turn.
// Session ownership and explicit output loss are independent of raw pipe flow.
class ProcessJsonFeed final : public StreamFeed {
public:
    ProcessJsonFeed(LogSourceId source, NativeJsonSchema schema,
                    std::unique_ptr<core::ProcessStreamSession> session,
                    std::shared_ptr<const LogWake> wake, std::string explanation);
    ~ProcessJsonFeed() override;
    LogMailbox& publications() noexcept override;
    void step(std::int64_t now) override;
    std::optional<std::int64_t> next_deadline() const noexcept override;
    widgets::LogPanelSourceStatus diagnostic() const override;
    std::string_view explanation() const noexcept override;
    void stop(std::int64_t now) noexcept override;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}  // namespace ckv::logmonitor
