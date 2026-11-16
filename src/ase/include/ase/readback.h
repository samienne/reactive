#pragma once

#include "vector.h"

#include <btl/result.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

namespace ase
{
    /**
     * @brief The colour pixels read back from a framebuffer.
     *
     * `pixels` holds `size[0] * size[1]` pixels, tightly packed with no row
     * padding, four bytes per pixel in the order R, G, B, A, each an unsigned
     * 8-bit value. Rows run top to bottom: the first row is the top of the
     * image as it appears on screen. Colour bytes are as stored in the
     * framebuffer; for a window that is sRGB-encoded, ready to write to an
     * image file without conversion. Alpha is premultiplied and has no
     * meaning for a window, which renders over an opaque clear; treat the
     * image as opaque RGB.
     *
     * `size` is in pixels: a window's logical size times its scaling factor.
     */
    struct ReadbackImage
    {
        Vector2i size = Vector2i(0, 0);
        std::vector<std::uint8_t> pixels;
    };

    /**
     * @brief Why a readback produced no pixels.
     */
    enum class ReadbackError
    {
        /**
         * @brief The render context or window cannot read pixels back.
         */
        Unsupported,

        /**
         * @brief The request was dropped before a frame served it, such as
         * when its window was destroyed.
         */
        Cancelled,

        /**
         * @brief The backend reported an error while reading.
         */
        Failed,
    };

    /**
     * @brief The pixels of a readback, or why there are none.
     */
    using ReadbackResult = btl::Result<ReadbackImage, ReadbackError>;

    /**
     * @brief Receives a readback's result exactly once, on any thread.
     */
    using ReadbackCallback = std::function<void(ReadbackResult)>;

    /**
     * @brief A window's presented frame, read back.
     *
     * `presentIndex` counts the window's presents from 1 and names the frame
     * uniquely; `frameTime` is that frame's `Frame::time`, which a frame
     * stepped with zero elapsed time shares with the one before it.
     */
    struct FrameCapture
    {
        std::uint64_t presentIndex = 0;
        std::chrono::microseconds frameTime{ 0 };
        ReadbackImage image;
    };

    /**
     * @brief A window's captured frame, or why there is none.
     */
    using CaptureResult = btl::Result<FrameCapture, ReadbackError>;

    /**
     * @brief Receives a capture's result exactly once, on any thread.
     */
    using CaptureCallback = std::function<void(CaptureResult)>;
} // namespace ase
