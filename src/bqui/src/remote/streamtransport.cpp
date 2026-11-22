#include "streamtransport.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace bqui::remote
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
void Transport::enqueue(std::string const& frame) { send(frame); }
bool Transport::flushReady() { return true; }
btl::NativeHandle Transport::writableHandle() { return {}; }

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

StreamTransport::StreamTransport(size_t maxQueuedBytes) :
    maxQueuedBytes_(maxQueuedBytes)
{
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

void StreamTransport::enqueue(std::string const& frame)
{
    if (queueDropped_)
        throw std::runtime_error("the outgoing queue was dropped");

    size_t size = kLengthPrefixSize + frame.size();
    if (size > maxQueuedBytes_ - queuedBytes_)
    {
        dropQueue();
        throw std::runtime_error("outgoing queue passed "
                + std::to_string(maxQueuedBytes_) + " bytes");
    }

    auto header = encodeLength(static_cast<uint32_t>(frame.size()));

    std::string bytes;
    bytes.reserve(size);
    bytes.append(header.begin(), header.end());
    bytes.append(frame);

    queue_.push_back(std::move(bytes));
    queuedBytes_ += size;
}

bool StreamTransport::flushReady()
{
    try
    {
        while (!queue_.empty())
        {
            std::string const& front = queue_.front();
            size_t wrote = writeReady(front.data() + queueOffset_,
                    front.size() - queueOffset_);
            if (wrote == 0)
                break;

            queueOffset_ += wrote;
            queuedBytes_ -= wrote;
            if (queueOffset_ == front.size())
            {
                queue_.pop_front();
                queueOffset_ = 0;
            }
        }

        return queue_.empty() && !writePending();
    }
    catch (...)
    {
        dropQueue();
        throw;
    }
}

bool StreamTransport::writePending()
{
    return false;
}

void StreamTransport::dropQueue()
{
    queue_.clear();
    queueOffset_ = 0;
    queuedBytes_ = 0;
    queueDropped_ = true;
}

} // namespace bqui::remote
