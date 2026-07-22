#pragma once

#ifdef _WIN32

#include "streamtransport.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

#include <windows.h>

namespace bqui::agent
{
    std::runtime_error lastError(char const* what);

    /** @brief Connect to / listen on a Windows named-pipe endpoint. */
    std::unique_ptr<Transport> pipeConnect(std::string const& endpoint);
    std::unique_ptr<TransportListener> pipeListen(std::string const& endpoint);

    /**
     * @brief A StreamTransport over an overlapped Windows named-pipe handle.
     *
     * The handle is opened with `FILE_FLAG_OVERLAPPED` deliberately: a
     * synchronous handle serialises all I/O, so a reader thread parked in
     * `ReadFile` would block a concurrent `WriteFile` (the reply) on the same
     * handle — the session's whole point is to send while a reader is parked.
     * Overlapped I/O lets a read and a write be in flight at once, and a
     * per-transport stop event lets `close()` wake a parked read with no race.
     */
    class PipeTransport : public StreamTransport
    {
    public:
        explicit PipeTransport(HANDLE handle);
        ~PipeTransport() override;

        void close() override;

    protected:
        void writeAll(void const* data, size_t size) override;
        bool readAll(void* data, size_t size) override;

    private:
        HANDLE handle_;
        // Separate completion events for a read and a write, so the two can be
        // in flight at the same time without sharing signalling state.
        HANDLE readEvent_;
        HANDLE writeEvent_;
        // Manual-reset: once close() sets it, it stays set, so a read issued
        // after close() sees it immediately and every later read bails too.
        HANDLE stopEvent_;
        std::atomic<bool> closing_{ false };
    };
} // namespace bqui::agent

#endif
