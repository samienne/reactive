#include "windowsizing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bqui
{

namespace
{

int64_t initialExtent(widget::Constraints const& band,
        std::optional<float> explicitValue, int64_t fallback)
{
    float value = static_cast<float>(fallback);
    if (explicitValue)
        value = *explicitValue;
    else if (band.natural)
        value = band.natural->value;

    auto extent = static_cast<int64_t>(std::ceil(value));
    if (band.max)
        extent = std::min(extent, static_cast<int64_t>(std::floor(*band.max)));
    if (band.min)
        extent = std::max(extent, static_cast<int64_t>(std::ceil(*band.min)));

    return std::max<int64_t>(extent, 1);
}

} // namespace

float clampToBand(float value, widget::Constraints const& band)
{
    if (band.max)
        value = std::min(value, *band.max);
    if (band.min)
        value = std::max(value, *band.min);
    return value;
}

avg::Vector2i initialWindowSize(
        widget::Constraints const& width,
        std::function<widget::Constraints(float width)> const& heightAt,
        std::optional<avg::Vector2f> const& explicitSize,
        avg::Vector2i const& fallback)
{
    int64_t w = initialExtent(width,
            explicitSize ? std::make_optional((*explicitSize)[0])
                : std::nullopt,
            fallback[0]);

    int64_t h = initialExtent(heightAt(static_cast<float>(w)),
            explicitSize ? std::make_optional((*explicitSize)[1])
                : std::nullopt,
            fallback[1]);

    return avg::Vector2i(w, h);
}

} // namespace bqui
