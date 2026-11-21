#pragma once

#include "bqui/bquivisibility.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace bqui::remote
{
    /**
     * @brief Encode an 8-bit RGB image as a PNG file.
     *
     * `rgb` holds `width * height` pixels, three bytes each, tightly packed
     * with the top row first. The colour is taken as sRGB-encoded and the
     * file says so. Throws `std::invalid_argument` for a zero or oversized
     * dimension and `std::runtime_error` if compression fails.
     */
    BQUI_EXPORT std::string encodePng(std::uint32_t width,
            std::uint32_t height, std::uint8_t const* rgb);
} // namespace bqui::remote
