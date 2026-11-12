#pragma once

#include "color.h"
#include "effect.h"
#include "avgvisibility.h"

namespace avg
{
    /**
     * @brief How frostedGlass looks.
     *
     * radius is the blur radius in screen pixels. tint is blended over the
     * blurred backdrop by tintStrength: 0 leaves the backdrop, 1 replaces
     * it. saturation scales the colourfulness of the blurred backdrop, 1
     * keeping it. highlight is blended in by highlightStrength over the
     * highlightWidth pixels nearest the edge of the shape's bounds; the
     * default strength of 0 draws none. Only the strengths weigh tint and
     * highlight: the alpha of either colour is ignored.
     */
    struct AVG_EXPORT FrostedGlassOptions
    {
        float radius = 24.0f;
        Color tint = Color(1.0f, 1.0f, 1.0f, 1.0f);
        float tintStrength = 0.2f;
        float saturation = 1.2f;
        Color highlight = Color(1.0f, 1.0f, 1.0f, 1.0f);
        float highlightStrength = 0.0f;
        float highlightWidth = 1.5f;
    };

    /**
     * @brief The divisor frostedGlass blurs at for a radius in screen
     * pixels: the smallest of 2, 4 and 8 at which the radius is at most
     * 16 pixels of the blurred target, and 8 for any larger radius.
     */
    AVG_EXPORT Divisor frostedGlassDivisor(float radius);

    /**
     * @brief Blurs the backdrop behind the shape and tints it.
     *
     *     FrostedGlassOptions options;
     *     options.radius = 32.0f;
     *     Effect glass = frostedGlass(options);
     *
     * Throws std::invalid_argument for a negative radius, saturation or
     * highlightWidth, or a strength outside [0, 1].
     */
    AVG_EXPORT Effect frostedGlass(
            FrostedGlassOptions const& options = FrostedGlassOptions());

    /**
     * @brief Blends colour over the backdrop behind the shape.
     *
     *     Effect dim = tint(Color(0.0f, 0.0f, 0.0f), 0.5f);
     *
     * The blend weight is strength times the colour's alpha, so an opaque
     * colour at strength 1 paints the shape in the colour and a
     * translucent one tints less; frostedGlass, unlike this, ignores its
     * tint's alpha. The result is opaque. Throws std::invalid_argument for
     * a strength outside [0, 1].
     */
    AVG_EXPORT Effect tint(Color const& colour, float strength);
}
