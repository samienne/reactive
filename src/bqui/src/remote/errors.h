#pragma once

#include <cstddef>

namespace bqui::remote
{
    // Server error codes, in blocks: blobs -3200x, captures -3201x.
    inline constexpr int kUnknownBlob = -32001;
    inline constexpr int kBlobCapReached = -32002;
    inline constexpr int kCaptureUnsupported = -32010;
    inline constexpr int kCaptureBusy = -32011;

    /**
     * @brief How many screenshots a session may have unanswered at once, and
     * how many captures one window may hold armed, before `kCaptureBusy`.
     */
    inline constexpr size_t kMaxScreenshotsInFlight = 4;
} // namespace bqui::remote
