#include "streamtransport.h"

#include <algorithm>
#include <vector>

namespace bqui::agent
{

Transport::~Transport() = default;
TransportListener::~TransportListener() = default;

// The base channel cannot be interrupted; a framed transport over a real
// handle overrides this to wake a blocked reader.
void Transport::close() {}

// Base defaults for the run-loop path; the framed transports override them.
btl::NativeHandle Transport::nativeHandle() { return {}; }
std::vector<std::string> Transport::receiveReady() { return {}; }
bool Transport::disconnected() const { return false; }

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

void StreamTransport::send(std::string const& frame)
{
    auto header = encodeLength(static_cast<uint32_t>(frame.size()));
    writeAll(header.data(), header.size());
    writeAll(frame.data(), frame.size());
}

std::optional<std::string> StreamTransport::receive()
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

std::vector<std::string> StreamTransport::receiveReady()
{
    if (!disconnected_ && !fillReady(readyBuffer_))
        disconnected_ = true;

    std::vector<std::string> frames;
    while (readyBuffer_.size() >= kLengthPrefixSize)
    {
        std::array<unsigned char, kLengthPrefixSize> header;
        std::copy_n(readyBuffer_.begin(), kLengthPrefixSize, header.begin());
        uint32_t length = decodeLength(header);

        if (readyBuffer_.size() < kLengthPrefixSize + length)
            break; // A partial frame; wait for the rest on a later call.

        frames.emplace_back(readyBuffer_, kLengthPrefixSize, length);
        readyBuffer_.erase(0, kLengthPrefixSize + length);
    }

    return frames;
}

bool StreamTransport::disconnected() const
{
    return disconnected_;
}

} // namespace bqui::agent
