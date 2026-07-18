#include "bqui/agent/transport.h"

#include <array>
#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/socket.h>
#   include <sys/un.h>
#   include <unistd.h>
#   include <cstring>
#   include <cerrno>
#endif

namespace bqui::agent
{

Transport::~Transport() = default;
TransportListener::~TransportListener() = default;

namespace
{

constexpr size_t kLengthPrefixSize = 4;

std::array<unsigned char, kLengthPrefixSize> encodeLength(uint32_t length)
{
    return {
        static_cast<unsigned char>((length >> 24) & 0xff),
        static_cast<unsigned char>((length >> 16) & 0xff),
        static_cast<unsigned char>((length >> 8) & 0xff),
        static_cast<unsigned char>(length & 0xff)
    };
}

uint32_t decodeLength(std::array<unsigned char, kLengthPrefixSize> const& bytes)
{
    return (static_cast<uint32_t>(bytes[0]) << 24)
        | (static_cast<uint32_t>(bytes[1]) << 16)
        | (static_cast<uint32_t>(bytes[2]) << 8)
        | static_cast<uint32_t>(bytes[3]);
}

/**
 * @brief Frames whole messages over a byte stream.
 *
 * A subclass supplies the raw byte primitives; this base loops them until a
 * full length-prefixed frame is written or read, so callers see whole messages
 * even when the OS splits an I/O across several calls.
 */
class StreamTransport : public Transport
{
public:
    void send(std::string const& frame) override
    {
        auto header = encodeLength(static_cast<uint32_t>(frame.size()));
        writeAll(header.data(), header.size());
        writeAll(frame.data(), frame.size());
    }

    std::optional<std::string> receive() override
    {
        std::array<unsigned char, kLengthPrefixSize> header;
        if (!readAll(header.data(), header.size()))
            return std::nullopt;

        uint32_t length = decodeLength(header);

        std::string frame(length, '\0');
        if (length > 0 && !readAll(&frame[0], length))
            return std::nullopt;

        return frame;
    }

protected:
    // Write exactly `size` bytes, looping over partial writes.
    virtual void writeAll(void const* data, size_t size) = 0;

    // Read exactly `size` bytes, looping over partial reads. Returns false on a
    // clean end-of-stream before any byte of this call was read.
    virtual bool readAll(void* data, size_t size) = 0;
};

#ifdef _WIN32

std::runtime_error lastError(char const* what)
{
    return std::runtime_error(std::string(what) + " failed: "
            + std::to_string(GetLastError()));
}

class PipeTransport : public StreamTransport
{
public:
    explicit PipeTransport(HANDLE handle) : handle_(handle) {}

    ~PipeTransport() override
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }

protected:
    void writeAll(void const* data, size_t size) override
    {
        auto bytes = static_cast<char const*>(data);
        size_t written = 0;
        while (written < size)
        {
            DWORD wrote = 0;
            if (!WriteFile(handle_, bytes + written,
                        static_cast<DWORD>(size - written), &wrote, nullptr))
                throw lastError("WriteFile");

            written += wrote;
        }
    }

    bool readAll(void* data, size_t size) override
    {
        auto bytes = static_cast<char*>(data);
        size_t read = 0;
        while (read < size)
        {
            DWORD got = 0;
            if (!ReadFile(handle_, bytes + read,
                        static_cast<DWORD>(size - read), &got, nullptr))
            {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE)
                    return false;

                throw lastError("ReadFile");
            }

            if (got == 0)
                return false;

            read += got;
        }

        return true;
    }

private:
    HANDLE handle_;
};

class PipeListener : public TransportListener
{
public:
    explicit PipeListener(std::string endpoint) : endpoint_(std::move(endpoint))
    {
    }

    std::unique_ptr<Transport> accept() override
    {
        HANDLE handle = CreateNamedPipeA(
                endpoint_.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                65536, 65536, 0, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
            throw lastError("CreateNamedPipe");

        if (!ConnectNamedPipe(handle, nullptr)
                && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            CloseHandle(handle);
            throw lastError("ConnectNamedPipe");
        }

        return std::make_unique<PipeTransport>(handle);
    }

private:
    std::string endpoint_;
};

} // namespace

std::unique_ptr<Transport> connect(std::string const& endpoint)
{
    // Race with the server: the pipe may not exist yet, or all its instances
    // may be busy between accepts. Retry briefly rather than failing.
    for (;;)
    {
        HANDLE handle = CreateFileA(
                endpoint.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (handle != INVALID_HANDLE_VALUE)
            return std::make_unique<PipeTransport>(handle);

        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
            Sleep(10);
        else if (error == ERROR_PIPE_BUSY)
            WaitNamedPipeA(endpoint.c_str(), 5000);
        else
            throw lastError("CreateFile");
    }
}

std::unique_ptr<TransportListener> listen(std::string const& endpoint)
{
    return std::make_unique<PipeListener>(endpoint);
}

#else

namespace
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

class SocketTransport : public StreamTransport
{
public:
    explicit SocketTransport(int fd) : fd_(fd) {}

    ~SocketTransport() override
    {
        if (fd_ >= 0)
            ::close(fd_);
    }

protected:
    void writeAll(void const* data, size_t size) override
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

    bool readAll(void* data, size_t size) override
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

private:
    int fd_;
};

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

#endif

} // namespace bqui::agent
