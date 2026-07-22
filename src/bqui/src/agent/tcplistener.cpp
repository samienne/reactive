#include "tcptransport.h"

#include "tcpsocket.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace bqui::agent
{

namespace
{
    // Fill a loopback-family sockaddr_in from a resolved address.
    sockaddr_in makeSockAddr(TcpAddress const& address, RawSocket sock)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        if (::inet_pton(AF_INET, address.host.c_str(), &addr.sin_addr) != 1)
        {
            closeRawSocket(sock);
            throw std::runtime_error("invalid TCP host: " + address.host);
        }
        return addr;
    }
} // namespace

TcpListener::TcpListener(TcpSocket sock, uint16_t port) :
    sock_(sock), port_(port)
{
}

TcpListener::~TcpListener()
{
    if (static_cast<RawSocket>(sock_) != kInvalidSocket)
        closeRawSocket(static_cast<RawSocket>(sock_));
}

std::unique_ptr<Transport> TcpListener::accept()
{
    RawSocket sock = ::accept(static_cast<RawSocket>(sock_), nullptr, nullptr);
    if (sock == kInvalidSocket)
        throw tcpError("accept");

    return std::make_unique<TcpTransport>(static_cast<TcpSocket>(sock));
}

std::unique_ptr<Transport> tcpConnect(TcpAddress const& address)
{
    ensureNetworking();

    RawSocket sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kInvalidSocket)
        throw tcpError("socket");

    sockaddr_in addr = makeSockAddr(address, sock);
    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        closeRawSocket(sock);
        throw tcpError("connect");
    }

    return std::make_unique<TcpTransport>(static_cast<TcpSocket>(sock));
}

std::unique_ptr<TcpListener> tcpListen(TcpAddress const& address)
{
    ensureNetworking();

    RawSocket sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kInvalidSocket)
        throw tcpError("socket");

    // Let a listener rebind a recently used port rather than failing on the
    // kernel's lingering TIME_WAIT reservation.
    int yes = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<char const*>(&yes), sizeof(yes));

    sockaddr_in addr = makeSockAddr(address, sock);
    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        closeRawSocket(sock);
        throw tcpError("bind");
    }

    if (::listen(sock, 1) != 0)
    {
        closeRawSocket(sock);
        throw tcpError("listen");
    }

    // Read back the port actually bound: an ephemeral `:0` request has the OS
    // choose one, and the caller needs it to tell a client where to connect.
    sockaddr_in bound{};
    socklen_t length = sizeof(bound);
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
    {
        closeRawSocket(sock);
        throw tcpError("getsockname");
    }

    return std::make_unique<TcpListener>(static_cast<TcpSocket>(sock),
            ntohs(bound.sin_port));
}

} // namespace bqui::agent
