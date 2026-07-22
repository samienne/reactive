#pragma once

#include "streamtransport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace bqui::agent
{
    // A connected socket handle. Held as an intptr so this header pulls in no
    // platform socket headers: it is a SOCKET on Windows and a file descriptor
    // on POSIX, both of which fit.
    using TcpSocket = std::intptr_t;

    /** @brief A runtime_error carrying the platform's last socket error. */
    std::runtime_error tcpError(char const* what);

    /** @brief Initialise the platform networking stack (Winsock) once. */
    void ensureNetworking();

    /** @brief A resolved TCP endpoint. */
    struct TcpAddress
    {
        std::string host;
        uint16_t port;
    };

    /**
     * @brief Recognise and parse a TCP endpoint string.
     *
     * Accepts `tcp://<host>:<port>`, a bare `<host>:<port>`, and `:<port>`
     * (host defaults to loopback). Returns nullopt for a pipe or Unix-socket
     * endpoint, so the dispatcher falls back to the platform default. A
     * malformed `tcp://` endpoint throws.
     *
     * Exported so the transport test can drive TCP dispatch directly.
     */
    BQUI_EXPORT std::optional<TcpAddress> parseTcpEndpoint(
            std::string const& endpoint);

    /** @brief A StreamTransport over a connected TCP socket. */
    class TcpTransport : public StreamTransport
    {
    public:
        explicit TcpTransport(TcpSocket sock);
        ~TcpTransport() override;

    protected:
        void writeAll(void const* data, size_t size) override;
        bool readAll(void* data, size_t size) override;

    private:
        TcpSocket sock_;
    };

    /**
     * @brief A TransportListener bound to a TCP port.
     *
     * Exposes the bound `port`, so a caller that asked for an ephemeral port
     * (`:0`) can read back the port the OS chose and hand it to a client.
     */
    class BQUI_EXPORT TcpListener : public TransportListener
    {
    public:
        TcpListener(TcpSocket sock, uint16_t port);
        ~TcpListener() override;

        std::unique_ptr<Transport> accept() override;

        /** @brief The port this listener is bound to. */
        uint16_t port() const { return port_; }

    private:
        TcpSocket sock_;
        uint16_t port_;
    };

    std::unique_ptr<Transport> tcpConnect(TcpAddress const& address);

    /** @brief Exported so the transport test can read back an ephemeral port. */
    BQUI_EXPORT std::unique_ptr<TcpListener> tcpListen(TcpAddress const& address);
} // namespace bqui::agent
