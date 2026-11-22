#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <cstdint>
#include <exception>
#include <vector>

namespace ase
{
    /**
     * @brief The colour pixels read back from a framebuffer.
     *
     * `pixels` holds `size[0] * size[1]` pixels, tightly packed with no row
     * padding, four bytes per pixel in the order R, G, B, A, each an unsigned
     * 8-bit value. Rows run top to bottom: the first row is the top of the
     * image as it appears on screen. All four bytes, alpha included, are
     * returned as stored in the framebuffer; for a window the colour is
     * sRGB-encoded, ready to write to an image file without conversion.
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
         * @brief The render context or framebuffer cannot read pixels back.
         */
        Unsupported,

        /**
         * @brief The backend reported an error while reading.
         */
        Failed,
    };

    /**
     * @brief The failure of a readback future, carrying why it produced no
     * pixels.
     */
    class ASE_EXPORT ReadbackFailure : public std::exception
    {
    public:
        explicit ReadbackFailure(ReadbackError error);

        ReadbackError error() const noexcept;
        char const* what() const noexcept override;

    private:
        ReadbackError error_;
    };
} // namespace ase
