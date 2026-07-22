#ifndef _WIN32

#include "sockettransport.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>

namespace bqui::agent
{

std::runtime_error lastError(char const* what)
{
    return std::runtime_error(std::string(what) + " failed: "
            + std::strerror(errno));
}

sockaddr_un makeAddress(std::string const& path)
{
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (path.size() >= sizeof(address.sun_path))
        throw std::runtime_error("endpoint path too long");

    std::memcpy(address.sun_path, path.data(), path.size());
    return address;
}

SocketTransport::SocketTransport(int fd) : fd_(fd) {}

SocketTransport::~SocketTransport()
{
    if (fd_ >= 0)
        ::close(fd_);
}

void SocketTransport::close()
{
    // shutdown() (not close()) is the race-free wake: it leaves the fd valid —
    // so a concurrent read() cannot alias a reused descriptor — while turning
    // a blocked read() into an immediate EOF and making every later read()
    // return EOF too. The destructor still closes the fd exactly once.
    if (fd_ >= 0)
        ::shutdown(fd_, SHUT_RDWR);
}

void SocketTransport::writeAll(void const* data, size_t size)
{
    auto bytes = static_cast<char const*>(data);
    size_t written = 0;
    while (written < size)
    {
        ssize_t wrote = ::write(fd_, bytes + written, size - written);
        if (wrote < 0)
        {
            if (errno == EINTR)
                continue;

            throw lastError("write");
        }

        written += static_cast<size_t>(wrote);
    }
}

bool SocketTransport::readAll(void* data, size_t size)
{
    auto bytes = static_cast<char*>(data);
    size_t read = 0;
    while (read < size)
    {
        ssize_t got = ::read(fd_, bytes + read, size - read);
        if (got < 0)
        {
            if (errno == EINTR)
                continue;

            throw lastError("read");
        }

        if (got == 0)
            return false;

        read += static_cast<size_t>(got);
    }

    return true;
}

} // namespace bqui::agent

#endif
