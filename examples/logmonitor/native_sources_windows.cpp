// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "native_sources.hpp"
#include "windows_event_record.hpp"
#include "cvision/core/utf8.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winevt.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace ckv::logmonitor {
namespace {
class EventHandle {
public:
    explicit EventHandle(EVT_HANDLE value = nullptr) : value_(value) {}
    ~EventHandle() { reset(); }
    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
    EVT_HANDLE get() const noexcept { return value_; }
    void reset(EVT_HANDLE value = nullptr) { if (value_) ::EvtClose(value_); value_ = value; }
private:
    EVT_HANDLE value_;
};
class Signal {
public:
    explicit Signal(bool manual) : handle_(::CreateEventW(nullptr, manual ? TRUE : FALSE, FALSE, nullptr)) {
        if (!handle_) throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "Event Log wake creation");
    }
    ~Signal() { ::CloseHandle(handle_); }
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    HANDLE get() const noexcept { return handle_; }
    void notify() noexcept { ::SetEvent(handle_); }
private:
    HANDLE handle_;
};
std::wstring wide(std::string_view text, std::size_t limit) {
    if (text.size() > limit || text.find('\0') != std::string_view::npos || !utf8::is_valid(text)) throw std::invalid_argument("Event Log source option exceeds UTF-8 bounds");
    if (text.empty()) return {};
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "Event Log option conversion");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size) != size) throw std::runtime_error("Event Log option conversion failed");
    return result;
}
std::string_view utf8_text(std::wstring_view text, std::span<char> output, DWORD& error) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), output.data(), static_cast<int>(output.size()), nullptr, nullptr);
    if (!size) { error = ::GetLastError(); return {}; }
    return {output.data(), static_cast<std::size_t>(size)};
}
template<std::size_t Size> std::wstring_view terminated(const std::array<wchar_t, Size>& text) {
    const auto end = std::find(text.begin(), text.end(), L'\0');
    return {text.data(), static_cast<std::size_t>(end - text.begin())};
}
struct NativeState {
    const Clock& clock;
    LogSourceId identity;
    std::wstring channel, resume;
    std::shared_ptr<Signal> cancel_wake;
    std::shared_ptr<const LogWake> work_wake;
    LogMailbox output;
    std::atomic<bool> cancelled{false}, done{false};
    std::atomic<LogSourceState> state{LogSourceState::Pending};
    std::atomic<std::uint32_t> error{0}, render_error{0};
    std::atomic<std::uint64_t> render_failures{0}, oversized{0}, clipped{0};
    NativeState(const Clock& time, std::wstring path, std::wstring bookmark, LogSourceId id,
                std::shared_ptr<Signal> pulse, std::shared_ptr<const LogWake> wake)
        : clock(time), identity(id), channel(std::move(path)), resume(std::move(bookmark)), cancel_wake(std::move(pulse)), work_wake(std::move(wake)),
          output(id, 1, {8, kLogBatchBytes, LogOverflowPolicy::DropOldest}, work_wake) {}
    void cancel() noexcept { cancelled.store(true, std::memory_order_release); cancel_wake->notify(); }
    void observed(LogSourceState value, DWORD code = 0) {
        if (cancelled.load(std::memory_order_acquire)) { value = LogSourceState::Cancelled; code = 0; }
        error.store(code, std::memory_order_relaxed); state.store(value, std::memory_order_release);
        if (work_wake) work_wake->notify();
    }
    bool offer(const LogMessage& record) {
        while (!cancelled.load(std::memory_order_acquire)) {
            const auto result = output.offer(record);
            if (result == LogOfferResult::Accepted || result == LogOfferResult::Dropped) return true;
            if (result != LogOfferResult::Busy && result != LogOfferResult::WouldBlock) { observed(LogSourceState::Failed, ERROR_INVALID_DATA); return false; }
            // Contention fallback is active publication work, never an idle poll.
            ::WaitForSingleObject(cancel_wake->get(), 1);
        }
        return false;
    }
};
class WindowsFeed final : public StreamFeed {
public:
    explicit WindowsFeed(std::shared_ptr<NativeState> source) : source_(std::move(source)) { text_.reserve(4096); }
    ~WindowsFeed() override { source_->cancel(); }
    LogMailbox& publications() noexcept override { return source_->output; }
    void step(std::int64_t now) override {
        if (now < now_) return; now_ = now;
        if (source_->done.load(std::memory_order_acquire) && !closed_) closed_ = source_->output.close(source_->state.load(std::memory_order_acquire), source_->error.load(), now);
        text_ = "Windows Event Log: future-only startup unless an explicit bookmark was supplied. Retry may leave a gap or repeat supplied-bookmark history. Local filters do not change channel scope.";
        const auto number = [&](std::string_view name, auto value) { std::array<char, 32> bytes{}; const auto result = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value); text_.append(name); text_.append(bytes.data(), static_cast<std::size_t>(result.ptr - bytes.data())); };
        number("\nNative error: ", source_->error.load()); number("; message/render error: ", source_->render_error.load());
        number("; render fallbacks: ", source_->render_failures.load()); number("; oversized events: ", source_->oversized.load()); number("; clipped metadata prefixes: ", source_->clipped.load());
        if (source_->error.load() == ERROR_EVT_QUERY_RESULT_STALE) text_ += "\nStale subscription/bookmark: event continuity is lost. Retry without stale resume state or choose File/Sample.";
        if (source_->state.load() == LogSourceState::Denied) text_ += "\nChannel permission denied. Choose an accessible channel or File/Sample; no automatic elevation.";
    }
    std::optional<std::int64_t> next_deadline() const noexcept override {
        if (source_->done.load(std::memory_order_acquire) && !closed_) return now_;
        return {};
    }
    widgets::LogPanelSourceStatus diagnostic() const override {
        widgets::LogPanelSourceStatus status; status.state = source_->state.load(std::memory_order_acquire); status.native_error = source_->error.load();
        status.watch = LogWatchMode::Native; status.uncertain = true; return status;
    }
    std::string_view explanation() const noexcept override { return text_; }
    void stop(std::int64_t now) noexcept override { source_->cancel(); source_->state.store(LogSourceState::Cancelled, std::memory_order_release); closed_ = source_->output.cancel(now); }
private:
    std::shared_ptr<NativeState> source_; std::string text_; std::int64_t now_ = 0; bool closed_ = false;
};
class Worker {
public:
    Worker() : pulse(std::make_shared<Signal>(false)), thread_([this] { loop(); }) {}
    ~Worker() {
        { std::lock_guard lock(mutex_); stopping_ = true; if (job_) job_->cancel(); }
        ready_.notify_one(); thread_.join();
    }
    std::unique_ptr<StreamFeed> admit(const Clock& clock, const std::wstring& channel, const std::wstring& resume,
                                      LogSourceId id, const std::shared_ptr<const LogWake>& wake) {
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock() || stopping_ || job_) return {};
        auto state = std::make_shared<NativeState>(clock, channel, resume, id, pulse, wake);
        auto feed = std::make_unique<WindowsFeed>(state);
        job_ = std::move(state); lock.unlock(); ready_.notify_one(); return feed;
    }
    std::shared_ptr<Signal> pulse;
private:
    struct Buffers {
        std::array<wchar_t, 32768> xml{};
        std::array<wchar_t, 8192> message{};
        std::array<wchar_t, 4096> bookmark{};
        alignas(EVT_VARIANT) std::array<std::byte, 16384> values{};
        std::array<char, kLogBatchBytes> xml_utf8{};
        std::array<char, 32768> message_utf8{};
        std::array<char, 16384> bookmark_utf8{};
        std::array<char, 4096> provider_utf8{}, channel_utf8{};
        LogMessage packet;
    } buffers_;
    static std::wstring_view property_string(const EVT_VARIANT& value, std::span<const std::byte> storage) {
        if ((value.Type & EVT_VARIANT_TYPE_MASK) != EvtVarTypeString || !value.StringVal) return {};
        const auto begin = reinterpret_cast<std::uintptr_t>(storage.data()), end = begin + storage.size();
        const auto address = reinterpret_cast<std::uintptr_t>(value.StringVal);
        if (address < begin || address >= end || address % alignof(wchar_t)) return {};
        const auto size = (end - address) / sizeof(wchar_t);
        const auto found = std::find(value.StringVal, value.StringVal + size, L'\0');
        return found == value.StringVal + size ? std::wstring_view{} : std::wstring_view(value.StringVal, static_cast<std::size_t>(found - value.StringVal));
    }
    bool render(NativeState& source, EVT_HANDLE event, EVT_HANDLE context, EVT_HANDLE bookmark,
                EventHandle& publisher, std::wstring& publisher_name, DWORD& publisher_error) {
        DWORD used = 0, properties = 0, error = 0;
        if (!::EvtRender(nullptr, event, EvtRenderEventXml, static_cast<DWORD>(sizeof(buffers_.xml)), buffers_.xml.data(), &used, &properties)) return notice(source, ::GetLastError(), "[Event Log adapter: raw event exceeds rendering bound or failed to render]");
        auto xml = utf8_text(terminated(buffers_.xml), buffers_.xml_utf8, error);
        if (error) return notice(source, error, "[Event Log adapter: raw XML conversion exceeded bound or contains invalid UTF-16]");
        WindowsEventRecord record; record.xml = xml;
        std::wstring_view provider;
        const bool system_rendered = ::EvtRender(context, event, EvtRenderEventValues, static_cast<DWORD>(buffers_.values.size()), buffers_.values.data(), &used, &properties) != FALSE;
        if (system_rendered && properties == EvtSystemPropertyIdEND && used <= buffers_.values.size() && used >= properties * sizeof(EVT_VARIANT)) {
            const auto* values = reinterpret_cast<const EVT_VARIANT*>(buffers_.values.data());
            provider = property_string(values[EvtSystemProviderName], {buffers_.values.data(), used});
            record.provider = utf8_text(provider, buffers_.provider_utf8, error);
            record.channel = utf8_text(property_string(values[EvtSystemChannel], {buffers_.values.data(), used}), buffers_.channel_utf8, error);
            if (values[EvtSystemEventID].Type == EvtVarTypeUInt16) record.event_id = values[EvtSystemEventID].UInt16Val;
            if (values[EvtSystemEventRecordId].Type == EvtVarTypeUInt64) record.record_id = values[EvtSystemEventRecordId].UInt64Val;
            if (values[EvtSystemLevel].Type == EvtVarTypeByte) record.level = values[EvtSystemLevel].ByteVal;
            if (values[EvtSystemTimeCreated].Type == EvtVarTypeFileTime) record.filetime = values[EvtSystemTimeCreated].FileTimeVal;
        } else error = system_rendered ? ERROR_INVALID_DATA : ::GetLastError();
        DWORD format_error = error;
        if (!provider.empty() && !error) {
            // One bounded publisher cache, owned/closed on this worker. Reuse
            // avoids metadata I/O per event. Retry creates a fresh cache.
            if (provider.size() > 1024) format_error = ERROR_INSUFFICIENT_BUFFER;
            else if (provider != publisher_name) {
                publisher_name.assign(provider);
                const auto opened = ::EvtOpenPublisherMetadata(nullptr, publisher_name.c_str(), nullptr, 0, 0);
                publisher_error = opened ? ERROR_SUCCESS : ::GetLastError();
                publisher.reset(opened);
            }
            if (!format_error && !publisher.get()) format_error = publisher_error;
            if (!format_error) {
                if (::EvtFormatMessage(publisher.get(), event, 0, 0, nullptr, EvtFormatMessageEvent, static_cast<DWORD>(buffers_.message.size()), buffers_.message.data(), &used))
                    record.rendered_message = utf8_text(terminated(buffers_.message), buffers_.message_utf8, format_error);
                else format_error = ::GetLastError();
            }
        } else if (!format_error) format_error = ERROR_EVT_PUBLISHER_METADATA_NOT_FOUND;
        if (::EvtUpdateBookmark(bookmark, event) && ::EvtRender(nullptr, bookmark, EvtRenderBookmark, static_cast<DWORD>(sizeof(buffers_.bookmark)), buffers_.bookmark.data(), &used, &properties)) {
            DWORD bookmark_error = 0; record.bookmark = utf8_text(terminated(buffers_.bookmark), buffers_.bookmark_utf8, bookmark_error);
            if (bookmark_error && !format_error) format_error = bookmark_error;
        } else if (!format_error) format_error = ::GetLastError();
        record.render_error = format_error;
        if (format_error) { source.render_error.store(format_error); source.render_failures.fetch_add(1); }
        const auto encoded = encode_windows_event(record, buffers_.packet);
        if (!encoded.accepted) return notice(source, ERROR_INVALID_DATA, "[Event Log adapter: invalid or oversized native record]");
        if (encoded.metadata_clipped) source.clipped.fetch_add(1);
        buffers_.packet.arrival_nanos = source.clock.now_nanos(); return source.offer(buffers_.packet);
    }
    bool notice(NativeState& source, DWORD error, std::string_view message) {
        source.render_error.store(error); source.render_failures.fetch_add(1);
        if (error == ERROR_INSUFFICIENT_BUFFER) source.oversized.fetch_add(1);
        const std::array fields{LogField{"adapter", "Windows Event Log"}, LogField{"diagnostic", message}};
        buffers_.packet.set_record(message, {LogSeverity::Warning, {}, fields}); buffers_.packet.arrival_nanos = source.clock.now_nanos();
        return source.offer(buffers_.packet);
    }
    void run(NativeState& source) {
        ::ResetEvent(pulse->get());
        if (source.cancelled.load()) return;
        Signal available(true);
        EventHandle context(::EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem));
        if (!context.get()) { source.observed(LogSourceState::Failed, ::GetLastError()); return; }
        EventHandle bookmark(::EvtCreateBookmark(source.resume.empty() ? nullptr : source.resume.c_str()));
        if (!bookmark.get()) { source.observed(LogSourceState::Failed, ::GetLastError()); return; }
        const DWORD flags = EvtSubscribeStrict | (source.resume.empty() ? EvtSubscribeToFutureEvents : EvtSubscribeStartAfterBookmark);
        EventHandle subscription(::EvtSubscribe(nullptr, available.get(), source.channel.c_str(), L"*", source.resume.empty() ? nullptr : bookmark.get(), nullptr, nullptr, flags));
        if (!subscription.get()) { const auto error = ::GetLastError(); source.observed(error == ERROR_ACCESS_DENIED ? LogSourceState::Denied : LogSourceState::Failed, error); return; }
        source.observed(LogSourceState::Following);
        buffers_.packet.source = source.identity; buffers_.packet.epoch = 1;
        EventHandle publisher; std::wstring publisher_name; publisher_name.reserve(1024); DWORD publisher_error = 0;
        while (!source.cancelled.load(std::memory_order_acquire)) {
            ::ResetEvent(available.get());
            std::size_t count = 0;
            for (; count < 64 && !source.cancelled.load(); ++count) {
                EVT_HANDLE handle = nullptr; DWORD returned = 0;
                if (!::EvtNext(subscription.get(), 1, &handle, 0, 0, &returned)) {
                    const auto error = ::GetLastError();
                    if (error != ERROR_NO_MORE_ITEMS && error != ERROR_TIMEOUT) { source.observed(error == ERROR_ACCESS_DENIED ? LogSourceState::Denied : LogSourceState::Failed, error); return; }
                    break;
                }
                EventHandle event(handle); if (!returned || !event.get() || !render(source, event.get(), context.get(), bookmark.get(), publisher, publisher_name, publisher_error)) return;
            }
            if (source.cancelled.load()) break;
            if (count == 64) { std::this_thread::yield(); continue; }
            const HANDLE waits[]{pulse->get(), available.get()}; const auto result = ::WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (result == WAIT_FAILED) { source.observed(LogSourceState::Failed, ::GetLastError()); return; }
        }
    }
    void loop() noexcept {
        for (;;) {
            std::shared_ptr<NativeState> source;
            { std::unique_lock lock(mutex_); ready_.wait(lock, [this] { return stopping_ || job_; }); if (stopping_ && !job_) return; source = job_; }
            try { run(*source); } catch (const std::system_error& error) { source->observed(LogSourceState::Failed, static_cast<DWORD>(error.code().value())); }
            catch (...) { source->observed(LogSourceState::Failed, ERROR_UNHANDLED_EXCEPTION); }
            if (source->cancelled.load()) source->observed(LogSourceState::Cancelled);
            else if (source->state.load() == LogSourceState::Following) source->observed(LogSourceState::Failed, ERROR_INVALID_DATA);
            source->done.store(true, std::memory_order_release);
            source->output.close(source->state.load(), source->error.load(), source->clock.now_nanos());
            if (source->work_wake) source->work_wake->notify();
            { std::lock_guard lock(mutex_); job_.reset(); if (stopping_) return; }
        }
    }
    std::mutex mutex_; std::condition_variable ready_; std::shared_ptr<NativeState> job_; bool stopping_ = false;
    std::thread thread_;
};
struct Channel { std::wstring path; std::string title, explanation; bool available = false; };
Channel inspect_channel(std::wstring path) {
    Channel result; result.path = std::move(path); std::array<char, 4096> name{}; DWORD error = 0;
    result.title = std::string(utf8_text(result.path, name, error));
    if (error) { result.explanation = "Channel name exceeds supported bounds."; return result; }
    EventHandle config(::EvtOpenChannelConfig(nullptr, result.path.c_str(), 0));
    if (!config.get()) { result.explanation = "Channel configuration unavailable (native code " + std::to_string(::GetLastError()) + "). Choose File/Sample."; return result; }
    EVT_VARIANT enabled{}, type{}; DWORD used = 0;
    if (!::EvtGetChannelConfigProperty(config.get(), EvtChannelConfigEnabled, 0, sizeof(enabled), &enabled, &used) ||
        !::EvtGetChannelConfigProperty(config.get(), EvtChannelConfigType, 0, sizeof(type), &type, &used)) {
        result.explanation = "Channel capabilities unavailable (native code " + std::to_string(::GetLastError()) + "). Choose File/Sample."; return result;
    }
    result.available = enabled.Type == EvtVarTypeBoolean && enabled.BooleanVal && type.Type == EvtVarTypeUInt32 && (type.UInt32Val == EvtChannelTypeAdmin || type.UInt32Val == EvtChannelTypeOperational);
    result.explanation = result.available ? "Future events on this channel; subscription permissions checked on open. No automatic elevation." : "Channel is disabled or not an Admin/Operational subscription channel. Choose File/Sample.";
    return result;
}
}  // namespace
struct NativeLogSources::Impl {
    const Clock& clock; std::vector<Channel> channels; std::wstring resume;
    std::vector<std::unique_ptr<Worker>> workers;
    Impl(const Clock& time, NativeLogSourceOptions options) : clock(time), resume(wide(options.windows_bookmark, 16384)) {
        if (!options.windows_channel.empty()) channels.push_back(inspect_channel(wide(options.windows_channel, 1024)));
        else {
            channels.push_back(inspect_channel(L"Application")); channels.push_back(inspect_channel(L"System"));
            EventHandle enumeration(::EvtOpenChannelEnum(nullptr, 0));
            if (enumeration.get()) for (std::size_t index = 0; index < 256 && channels.size() < 8; ++index) {
                std::array<wchar_t, 1024> path{}; DWORD used = 0;
                if (!::EvtNextChannelPath(enumeration.get(), static_cast<DWORD>(path.size()), path.data(), &used)) break;
                const auto value = terminated(path);
                if (value == L"Application" || value == L"System" || !value.ends_with(L"/Operational")) continue;
                auto channel = inspect_channel(std::wstring(value)); if (channel.available) channels.push_back(std::move(channel));
            }
        }
        workers.reserve(8); for (int index = 0; index < 8; ++index) workers.push_back(std::make_unique<Worker>());
    }
    std::unique_ptr<StreamFeed> open(std::size_t index, LogSourceId id, std::shared_ptr<const LogWake> wake) {
        if (index >= channels.size() || !channels[index].available) return {};
        for (auto& worker : workers) if (auto feed = worker->admit(clock, channels[index].path, resume, id, wake)) return feed;
        return {};
    }
};
NativeLogSources::NativeLogSources(const Clock& clock, std::shared_ptr<const LogWake> wake, NativeLogSourceOptions options) : impl_(std::make_unique<Impl>(clock, std::move(options))) { (void)wake; }
NativeLogSources::~NativeLogSources() = default;
std::vector<NativeSourceDescriptor> NativeLogSources::catalog() {
    std::vector<NativeSourceDescriptor> result; result.reserve(impl_->channels.size());
    for (std::size_t index = 0; index < impl_->channels.size(); ++index) {
        const auto& channel = impl_->channels[index];
        result.push_back({"windows-channel-" + std::to_string(index), channel.title, channel.explanation, channel.available,
            [this, index](LogSourceId id, std::shared_ptr<const LogWake> wake) { return impl_->open(index, id, std::move(wake)); }});
    }
    return result;
}
}  // namespace ckv::logmonitor
