#ifndef _WIN32

#include "sockettransport.h"

#include <memory>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <unistd.h>

namespace bqui::agent
{

namespace
{

class SocketListener : public TransportListener
{
public:
    SocketListener(int fd, std::string path) :
        fd_(fd), path_(std::move(path))
    {
    }

    ~SocketListener() override
    {
        if (fd_ >= 0)
            ::close(fd_);

        ::unlink(path_.c_str());
    }

    std::unique_ptr<Transport> accept() override
    {
        int fd = ::accept(fd_, nullptr, nullptr);
        if (fd < 0)
            throw lastError("accept");

        return std::make_unique<SocketTransport>(fd);
    }

private:
    int fd_;
    std::string path_;
};

} // namespace

std::unique_ptr<Transport> connect(std::string const& endpoint)
{
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        throw lastError("socket");

    auto address = makeAddress(endpoint);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) < 0)
    {
        ::close(fd);
        throw lastError("connect");
    }

    return std::make_unique<SocketTransport>(fd);
}

std::unique_ptr<TransportListener> listen(std::string const& endpoint)
{
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        throw lastError("socket");

    // A stale socket file from a previous run would block bind().
    ::unlink(endpoint.c_str());

    auto address = makeAddress(endpoint);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        ::close(fd);
        throw lastError("bind");
    }

    if (::listen(fd, 1) < 0)
    {
        ::close(fd);
        throw lastError("listen");
    }

    return std::make_unique<SocketListener>(fd, endpoint);
}

} // namespace bqui::agent

#endif
