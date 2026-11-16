#pragma once

#include "color.h"
#include "effect.h"
#include "avgvisibility.h"

namespace avg
{
    /**
     * @brief How frostedGlass looks.
     *
     * radius is the blur radius. tint is blended over the blurred
     * backdrop by tintStrength: 0 leaves the backdrop, 1 replaces it.
     * saturation scales the colourfulness of the blurred backdrop, 1
     * keeping it. highlight is blended in by highlightStrength over the
     * highlightWidth pixels nearest the edge of the shape's bounds; the
     * default strength of 0 draws none. Only the strengths weigh tint and
     * highlight: the alpha of either colour is ignored.
     *
     * radius and highlightWidth are in logical units, like the shape the
     * effect paints, so they scale with the display.
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

    AVG_EXPORT bool operator==(FrostedGlassOptions const& lhs,
            FrostedGlassOptions const& rhs);
    AVG_EXPORT bool operator!=(FrostedGlassOptions const& lhs,
            FrostedGlassOptions const& rhs);

    /**
     * @brief Interpolates every option member by t, so the options can be
     * animated.
     */
    AVG_EXPORT FrostedGlassOptions lerp(FrostedGlassOptions const& a,
            FrostedGlassOptions const& b, float t);

    /**
     * @brief The options moved into the range frostedGlass accepts: a
     * negative or NaN radius, saturation or highlightWidth becomes 0 and
     * each strength is clamped to [0, 1], with NaN becoming 0.
     */
    AVG_EXPORT FrostedGlassOptions clampToValid(FrostedGlassOptions options);

    /**
     * @brief The divisor frostedGlass blurs at for a radius in pixels of
     * the target: the smallest of 2, 4 and 8 at which the radius is at most
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
     * Throws std::invalid_argument for a negative or NaN radius,
     * saturation or highlightWidth, or a strength outside [0, 1].
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
