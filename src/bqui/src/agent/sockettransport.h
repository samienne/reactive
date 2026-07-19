#pragma once

#ifndef _WIN32

#include "streamtransport.h"

#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>

namespace bqui::agent
{
    std::runtime_error lastError(char const* what);
    sockaddr_un makeAddress(std::string const& path);

    /** @brief A StreamTransport over a connected Unix-domain socket. */
    class SocketTransport : public StreamTransport
    {
    public:
        explicit SocketTransport(int fd);
        ~SocketTransport() override;

    protected:
        void writeAll(void const* data, size_t size) override;
        bool readAll(void* data, size_t size) override;

    private:
        int fd_;
    };
} // namespace bqui::agent

#endif
