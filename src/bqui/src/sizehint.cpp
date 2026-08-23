#include "bqui/sizehint.h"

#include <algorithm>
#include <ostream>

namespace bqui
{

static_assert(IsSizeHint<SizeHint>::value, "");

AxisHint SizeHint::getWidth() const
{
    return hint_->getWidth();
}

AxisHint SizeHint::getHeightForWidth(float width) const
{
    return hint_->getHeightForWidth(width);
}

AxisHint SizeHint::getWidthForHeight(float height) const
{
    return hint_->getWidthForHeight(height);
}

AxisHint getLargestHint(std::vector<AxisHint> const& hints)
{
    Band band;
    for (auto const& hint : hints)
    {
        band.min = std::max(band.min, hint.extent.min);
        band.natural = std::max(band.natural, hint.extent.natural);
        band.max = std::max(band.max, hint.extent.max);
        band.grow = std::max(band.grow, hint.extent.grow);
    }

    return AxisHint{ band, Anchors{} };
}

AxisHint getBaselineHint(std::vector<AxisHint> const& hints)
{
    float ascent = 0.0f;
    float descentMin = 0.0f;
    float descentNatural = 0.0f;
    float descentMax = 0.0f;
    float grow = 0.0f;
    bool anyBaseline = false;

    for (auto const& hint : hints)
    {
        float a = hint.anchors.firstBaseline.value_or(0.0f);
        if (hint.anchors.firstBaseline)
            anyBaseline = true;

        ascent = std::max(ascent, a);
        descentMin = std::max(descentMin, hint.extent.min - a);
        descentNatural = std::max(descentNatural, hint.extent.natural - a);
        descentMax = std::max(descentMax, hint.extent.max - a);
        grow = std::max(grow, hint.extent.grow);
    }

    Anchors anchors;
    if (anyBaseline)
        anchors.firstBaseline = ascent;

    return AxisHint{
        Band{
            ascent + descentMin,
            ascent + descentNatural,
            ascent + descentMax,
            grow
        },
        anchors
    };
}

std::ostream& operator<<(std::ostream& stream, Band const& band)
{
    return stream << "Band{"
        << band.min << ','
        << band.natural << ','
        << band.max << ",grow:"
        << band.grow << '}';
}
} // namespace bqui
