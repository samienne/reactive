#include "scriptedtransport.h"

#ifdef _WIN32
#   include <btl/win32/nativehandle_win32.h>
#else
#   include <btl/posix/nativehandle_posix.h>

#   include <fcntl.h>
#   include <sys/socket.h>
#   include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <stdexcept>

namespace bqui::remote::testkit
{

#ifdef _WIN32
FixedHandle::FixedHandle(Direction, bool ready) :
    event_(CreateEventA(nullptr, TRUE, ready ? TRUE : FALSE, nullptr))
{
    if (!event_)
        throw std::runtime_error("CreateEvent failed");
}

FixedHandle::~FixedHandle()
{
    CloseHandle(static_cast<HANDLE>(event_));
}

btl::NativeHandle FixedHandle::native() const
{
    return btl::fromHandle(static_cast<HANDLE>(event_));
}
#else
// A pipe: its read end is readable once a byte is in it, and its write end is
// writable until it is full.
FixedHandle::FixedHandle(Direction direction, bool ready) :
    direction_(direction)
{
    if (::pipe(fds_) != 0)
        throw std::runtime_error("pipe failed");

    char byte = 0;
    if (direction == Direction::Readable && ready
            && ::write(fds_[1], &byte, 1) != 1)
    {
        throw std::runtime_error("write failed");
    }

    if (direction == Direction::Writable && !ready)
    {
        ::fcntl(fds_[1], F_SETFL, ::fcntl(fds_[1], F_GETFL) | O_NONBLOCK);
        std::array<char, 4096> block{};
        while (::write(fds_[1], block.data(), block.size()) > 0)
        {
        }
        while (::write(fds_[1], &byte, 1) > 0)
        {
        }
    }
}

FixedHandle::~FixedHandle()
{
    ::close(fds_[0]);
    ::close(fds_[1]);
}

btl::NativeHandle FixedHandle::native() const
{
    return btl::fromFd(direction_ == Direction::Readable ? fds_[0] : fds_[1]);
}
#endif

ScriptedTransport::ScriptedTransport(size_t maxQueuedBytes, bool writable) :
    StreamTransport(maxQueuedBytes),
    readable_(FixedHandle::Direction::Readable, false),
    writable_(FixedHandle::Direction::Writable, writable)
{
}

btl::NativeHandle ScriptedTransport::nativeHandle()
{
    return readable_.native();
}

btl::NativeHandle ScriptedTransport::writableHandle()
{
    return writable_.native();
}

void ScriptedTransport::writeAll(void const* data, size_t size)
{
    wire.append(static_cast<char const*>(data), size);
}

bool ScriptedTransport::readAll(void*, size_t)
{
    return false;
}

bool ScriptedTransport::fillReady(std::string& out)
{
    out += incoming;
    incoming.clear();
    return !incomingEnds;
}

size_t ScriptedTransport::writeReady(void const* data, size_t size)
{
    std::optional<size_t> step = afterScript;
    if (!writes.empty())
    {
        step = writes.front();
        writes.pop_front();
    }

    if (!step)
        throw std::runtime_error("scripted write failure");

    size_t taken = size < *step ? size : *step;
    wire.append(static_cast<char const*>(data), taken);
    return taken;
}

void shutdownSending(Transport& client)
{
#ifdef _WIN32
    ::shutdown(btl::toSocket(client.nativeHandle()), SD_SEND);
#else
    ::shutdown(btl::toFd(client.nativeHandle()), SHUT_WR);
#endif
}

std::string framed(std::string const& message)
{
    auto header = encodeLength(static_cast<uint32_t>(message.size()));
    return std::string(header.begin(), header.end()) + message;
}

std::vector<std::string> unframed(std::string const& bytes)
{
    std::vector<std::string> frames;
    size_t at = 0;
    while (bytes.size() - at >= kLengthPrefixSize)
    {
        std::array<unsigned char, kLengthPrefixSize> header;
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                kLengthPrefixSize, header.begin());
        size_t length = decodeLength(header);
        if (bytes.size() - at - kLengthPrefixSize < length)
            break;

        frames.emplace_back(bytes, at + kLengthPrefixSize, length);
        at += kLengthPrefixSize + length;
    }

    return frames;
}

} // namespace bqui::remote::testkit
