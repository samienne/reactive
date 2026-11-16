#pragma once

#include "remote/streamtransport.h"

#include <btl/nativehandle.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace bqui::remote::testkit
{
    // An OS handle a run loop can wait on, held ready or idle for good.
    class FixedHandle
    {
    public:
        enum class Direction { Readable, Writable };

        FixedHandle(Direction direction, bool ready);
        ~FixedHandle();

        FixedHandle(FixedHandle const&) = delete;
        FixedHandle& operator=(FixedHandle const&) = delete;

        btl::NativeHandle native() const;

    private:
#ifdef _WIN32
        void* event_;
#else
        int fds_[2];
        Direction direction_;
#endif
    };

    // A StreamTransport whose channel is a script, so a test controls exactly
    // what each write takes without a real channel or large payloads.
    class ScriptedTransport : public StreamTransport
    {
    public:
        static constexpr size_t kTakeAll = SIZE_MAX;

        explicit ScriptedTransport(size_t maxQueuedBytes = kMaxQueuedBytes,
                bool writable = true);

        // What each writeReady() call takes, in order: a byte count (zero
        // would block), or nullopt to fail. Past the script, every call takes
        // `afterScript`.
        std::deque<std::optional<size_t>> writes;
        std::optional<size_t> afterScript = kTakeAll;

        // What the first receiveReady() finds, and whether the peer has then
        // closed its side.
        std::string incoming;
        bool incomingEnds = false;

        // Every byte the writes took, in order.
        std::string wire;

        btl::NativeHandle nativeHandle() override;
        btl::NativeHandle writableHandle() override;

    protected:
        void writeAll(void const* data, size_t size) override;
        bool readAll(void* data, size_t size) override;
        bool fillReady(std::string& out) override;
        size_t writeReady(void const* data, size_t size) override;

    private:
        FixedHandle readable_;
        FixedHandle writable_;
    };

    // Half-close a TCP client: it stops sending but still reads.
    void shutdownSending(Transport& client);

    std::string framed(std::string const& message);

    // The whole frames in `bytes`; a trailing partial frame is left out.
    std::vector<std::string> unframed(std::string const& bytes);
} // namespace bqui::remote::testkit
