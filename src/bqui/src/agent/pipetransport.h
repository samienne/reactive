#pragma once

#ifdef _WIN32

#include "streamtransport.h"

#include <stdexcept>

#include <windows.h>

namespace bqui::agent
{
    std::runtime_error lastError(char const* what);

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
