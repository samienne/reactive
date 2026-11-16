#pragma once

#ifdef _WIN32

#include <cstddef>

#include <windows.h>

namespace bqui::remote
{
    /**
     * @brief Non-blocking writes to an overlapped Windows handle.
     *
     * At most one write is in flight. Its bytes are copied here first, because
     * the kernel reads them until the write completes, and `event()` signals
     * that completion so a run loop can wait on it. Does not own the handle;
     * `cancel()` must run before the handle is closed.
     */
    class OverlappedWriter
    {
    public:
        explicit OverlappedWriter(HANDLE handle);
        ~OverlappedWriter();

        OverlappedWriter(OverlappedWriter const&) = delete;
        OverlappedWriter& operator=(OverlappedWriter const&) = delete;

        /**
         * @brief Start writing up to `size` bytes.
         *
         * @return How many bytes were taken; zero while an earlier write is
         * still in flight. Throws when the handle has failed.
         */
        size_t write(void const* data, size_t size);

        /** @brief True while a write is in flight; throws if it failed. */
        bool pending();

        /** @brief Abandon a write in flight and wait for the kernel to let go. */
        void cancel();

        HANDLE event() const { return event_; }

    private:
        HANDLE handle_;
        HANDLE event_;
        OVERLAPPED overlapped_{};
        char chunk_[65536];
        bool pending_ = false;
    };
} // namespace bqui::remote

#endif
