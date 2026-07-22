#pragma once

#ifdef _WIN32

#include "streamtransport.h"

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

    /** @brief A StreamTransport over a Windows named-pipe handle. */
    class PipeTransport : public StreamTransport
    {
    public:
        explicit PipeTransport(HANDLE handle);
        ~PipeTransport() override;

    protected:
        void writeAll(void const* data, size_t size) override;
        bool readAll(void* data, size_t size) override;

    private:
        HANDLE handle_;
    };
} // namespace bqui::agent

#endif
