#pragma once

#include "bqui/widget/layoutspec.h"
#include "bqui/bquivisibility.h"

#include <avg/vector.h>

#include <functional>
#include <optional>

namespace bqui
{
    /**
     * @brief @p value limited to the band's min and max; min wins over a max
     * below it.
     */
    BQUI_EXPORT float clampToBand(float value,
            widget::Constraints const& band);

    /**
     * @brief The size a window opens at: per axis the explicit size, else the
     * band's natural, else the fallback, limited to the band's min and max.
     *
     * The width is chosen first; the height band is read at that width through
     * @p heightAt. Natural and min round up and max rounds down to whole
     * pixels, and an axis is at least one pixel.
     */
    BQUI_EXPORT avg::Vector2i initialWindowSize(
            widget::Constraints const& width,
            std::function<widget::Constraints(float width)> const& heightAt,
            std::optional<avg::Vector2f> const& explicitSize,
            avg::Vector2i const& fallback);
} // namespace bqui
