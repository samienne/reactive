#pragma once

#include "asevisibility.h"

#include <cstdint>

namespace ase
{
    /**
     * @brief The pixel rectangle a draw or clear is limited to.
     *
     * Framebuffer pixel coordinates, with the origin at the target's
     * bottom-left corner, so `{ 0, 0, width, height }` is the whole target.
     */
    struct ASE_EXPORT ScissorRect
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;

        bool operator==(ScissorRect const& rhs) const;
        bool operator!=(ScissorRect const& rhs) const;
        bool operator<(ScissorRect const& rhs) const;
    };
} // namespace ase
