#pragma once

#include "bqui/remote/transport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace bqui::remote
{
    constexpr size_t kLengthPrefixSize = 4;

    /**
     * @brief The most bytes an outgoing queue holds before enqueue() throws.
     */
    constexpr size_t kMaxQueuedBytes = 64u * 1024u * 1024u;

    BQUI_EXPORT std::array<unsigned char, kLengthPrefixSize> encodeLength(
            uint32_t length);
    BQUI_EXPORT uint32_t decodeLength(
            std::array<unsigned char, kLengthPrefixSize> const& bytes);

    /**
     * @brief Frames whole messages over a byte stream.
     *
     * A subclass supplies the raw byte primitives; this base loops them until a
     * full length-prefixed frame is written or read, so callers see whole
     * messages even when the OS splits an I/O across several calls.
     */
    class BQUI_EXPORT StreamTransport : public Transport
    {
    public:
        void send(std::string const& frame) override;
        std::optional<std::string> receive() override;

        std::vector<std::string> receiveReady() override;
        bool disconnected() const override;

        void enqueue(std::string const& frame) override;
        bool flushReady() override;

    protected:
        /**
         * @brief Construct with an outgoing queue of at most
         * `maxQueuedBytes`.
         */
        explicit StreamTransport(size_t maxQueuedBytes = kMaxQueuedBytes);

        /**
         * @brief Write exactly `size` bytes, looping over partial writes.
         */
        virtual void writeAll(void const* data, size_t size) = 0;

        /**
         * @brief Read exactly `size` bytes, looping over partial reads.
         *
         * @return false on a clean end-of-stream before any byte of this call
         * was read.
         */
        virtual bool readAll(void* data, size_t size) = 0;

        /**
         * @brief Append whatever is readable right now to @p out without
         * blocking; return false once the peer has closed.
         *
         * May append nothing and still return true when the channel is
         * momentarily empty.
         */
        virtual bool fillReady(std::string& out) = 0;

        /**
         * @brief Take up to `size` bytes for writing without blocking.
         *
         * @return How many bytes were taken; zero when the channel takes none
         * right now. Throws when the channel has failed.
         */
        virtual size_t writeReady(void const* data, size_t size) = 0;

        /**
         * @brief True while bytes writeReady() took are still on their way out.
         *
         * Only a completion-based backend has such bytes; the base returns
         * false.
         */
        virtual bool writePending();

    private:
        void dropQueue();

        std::string readyBuffer_;
        bool disconnected_ = false;

        size_t maxQueuedBytes_;
        std::deque<std::string> queue_;
        size_t queueOffset_ = 0;
        size_t queuedBytes_ = 0;
        bool queueDropped_ = false;
    };
} // namespace bqui::remote
