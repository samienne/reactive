#pragma once

#include "bqui/bquivisibility.h"

#include <memory>
#include <optional>
#include <string>

namespace bqui::agent
{
    /**
     * @brief A bidirectional, length-prefixed framed message channel.
     *
     * Moves whole messages, not bytes: `send` writes one frame and `receive`
     * returns one, hiding partial reads/writes and the length framing. It knows
     * nothing about message contents. The local IPC implementation is a named
     * pipe on Windows and a Unix-domain socket elsewhere; a TCP transport sits
     * behind the same interface for a cross-platform, cross-host client.
     */
    class BQUI_EXPORT Transport
    {
    public:
        virtual ~Transport();

        /** @brief Send one whole message as a single frame. */
        virtual void send(std::string const& frame) = 0;

        /**
         * @brief Block until one whole message arrives.
         *
         * @return The message, or nullopt when the peer has closed the channel.
         */
        virtual std::optional<std::string> receive() = 0;

        /**
         * @brief Interrupt a blocked `receive()` so its thread can be joined.
         *
         * Called from a different thread than the one blocked in `receive()`:
         * after this returns, an in-progress `receive()` unblocks with nullopt
         * and every later `receive()` returns nullopt at once, so a dedicated
         * reader thread can be joined without hanging. It does not free the
         * underlying handle — the destructor still does that, exactly once —
         * and it is idempotent, so calling it more than once is safe. The base
         * is a no-op; the framed transports override it. Never touches the send
         * path, so a concurrent `send()` on the owning thread stays valid.
         */
        virtual void close();
    };

    /**
     * @brief Accepts incoming connections on an endpoint (the server role).
     *
     * Production only needs the app to `connect`; a listener is here so a
     * loopback test (and, conceptually, the agent side) can accept one.
     */
    class BQUI_EXPORT TransportListener
    {
    public:
        virtual ~TransportListener();

        /** @brief Block until one peer connects, returning its transport. */
        virtual std::unique_ptr<Transport> accept() = 0;
    };

    /**
     * @brief Connect to an endpoint as a client.
     *
     * The endpoint's shape selects the transport: `tcp://<host>:<port>`,
     * `<host>:<port>`, or `:<port>` (host defaults to loopback) is a TCP
     * connection; anything else is the platform's local IPC — a
     * `\\.\pipe\<name>` named pipe on Windows, or a Unix-domain socket path
     * elsewhere.
     */
    BQUI_EXPORT std::unique_ptr<Transport> connect(std::string const& endpoint);

    /** @brief Start listening on an endpoint for a client to `connect`. */
    BQUI_EXPORT std::unique_ptr<TransportListener> listen(
            std::string const& endpoint);
} // namespace bqui::agent
