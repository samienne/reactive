#include "tcptransport.h"

#include "tcpsocket.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace bqui::agent
{

std::runtime_error tcpError(char const* what)
{
    return std::runtime_error(std::string(what) + " failed: "
            + std::to_string(lastSocketError()));
}

void ensureNetworking()
{
#ifdef _WIN32
    // WSAStartup once for the process; the matching cleanup is left to exit,
    // which is the usual trade for a library that never knows it is last.
    static bool const started = []
    {
        WSADATA data;
        int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0)
            throw std::runtime_error("WSAStartup failed: "
                    + std::to_string(result));
        return true;
    }();
    (void)started;
#endif
}

std::optional<TcpAddress> parseTcpEndpoint(std::string const& endpoint)
{
    std::string rest = endpoint;
    bool explicitScheme = false;

    if (rest.rfind("tcp://", 0) == 0)
    {
        rest = rest.substr(6);
        explicitScheme = true;
    }
    else if (!rest.empty() && (rest.front() == '/' || rest.front() == '\\'))
    {
        // A Unix-socket path or a Windows pipe name — not TCP.
        return std::nullopt;
    }

    auto colon = rest.rfind(':');
    if (colon == std::string::npos)
    {
        if (explicitScheme)
            throw std::runtime_error("tcp endpoint needs a port: " + endpoint);
        return std::nullopt;
    }

    std::string portStr = rest.substr(colon + 1);
    if (portStr.empty()
            || !std::all_of(portStr.begin(), portStr.end(),
                [](unsigned char c) { return std::isdigit(c); }))
    {
        if (explicitScheme)
            throw std::runtime_error("tcp endpoint has a bad port: " + endpoint);
        return std::nullopt;
    }

    unsigned long port = std::stoul(portStr);
    if (port > 65535)
        throw std::runtime_error("tcp port out of range: " + endpoint);

    std::string host = rest.substr(0, colon);
    if (host.empty())
        host = "127.0.0.1";

    return TcpAddress{ std::move(host), static_cast<uint16_t>(port) };
}

TcpTransport::TcpTransport(TcpSocket sock) : sock_(sock) {}

TcpTransport::~TcpTransport()
{
    if (static_cast<RawSocket>(sock_) != kInvalidSocket)
        closeRawSocket(static_cast<RawSocket>(sock_));
}

void TcpTransport::writeAll(void const* data, size_t size)
{
    auto bytes = static_cast<char const*>(data);
    size_t written = 0;
    while (written < size)
    {
        size_t remaining = size - written;
        int chunk = remaining > 0x10000u ? 0x10000 : static_cast<int>(remaining);

        auto wrote = ::send(static_cast<RawSocket>(sock_), bytes + written,
                chunk, kSendFlags);
        if (wrote < 0)
        {
#ifndef _WIN32
            if (lastSocketError() == EINTR)
                continue;
#endif
            throw tcpError("send");
        }

        written += static_cast<size_t>(wrote);
    }
}

bool TcpTransport::readAll(void* data, size_t size)
{
    auto bytes = static_cast<char*>(data);
    size_t read = 0;
    while (read < size)
    {
        size_t remaining = size - read;
        int chunk = remaining > 0x10000u ? 0x10000 : static_cast<int>(remaining);

        auto got = ::recv(static_cast<RawSocket>(sock_), bytes + read, chunk, 0);
        if (got < 0)
        {
#ifndef _WIN32
            if (lastSocketError() == EINTR)
                continue;
#endif
            // A reset peer is a closed channel, not an error to propagate —
            // mirror the pipe transport's ERROR_BROKEN_PIPE handling.
            if (lastSocketError() == kConnResetError)
                return false;

            throw tcpError("recv");
        }

        if (got == 0)
            return false;

        read += static_cast<size_t>(got);
    }

    return true;
}

} // namespace bqui::agent
