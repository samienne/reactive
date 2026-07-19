#pragma once

#include "bqui/agent/transport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bqui::agent
{
    constexpr size_t kLengthPrefixSize = 4;

    std::array<unsigned char, kLengthPrefixSize> encodeLength(uint32_t length);
    uint32_t decodeLength(
            std::array<unsigned char, kLengthPrefixSize> const& bytes);

    /**
     * @brief Frames whole messages over a byte stream.
     *
     * A subclass supplies the raw byte primitives; this base loops them until a
     * full length-prefixed frame is written or read, so callers see whole
     * messages even when the OS splits an I/O across several calls.
     */
    class StreamTransport : public Transport
    {
    public:
        void send(std::string const& frame) override;
        std::optional<std::string> receive() override;

    protected:
        /** @brief Write exactly `size` bytes, looping over partial writes. */
        virtual void writeAll(void const* data, size_t size) = 0;

        /**
         * @brief Read exactly `size` bytes, looping over partial reads.
         *
         * @return false on a clean end-of-stream before any byte of this call
         * was read.
         */
        virtual bool readAll(void* data, size_t size) = 0;
    };
} // namespace bqui::agent
