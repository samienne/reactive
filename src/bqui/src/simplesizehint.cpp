#include "bqui/simplesizehint.h"

namespace bqui
{

static_assert(IsSizeHint<SimpleSizeHint>::value, "");

SimpleSizeHint::SimpleSizeHint(Band x, Band y) :
    horizontal_(x),
    vertical_(y)
    {
    }

AxisHint SimpleSizeHint::getWidth() const
{
    return AxisHint{ horizontal_, Anchors{} };
}

AxisHint SimpleSizeHint::getHeightForWidth(float) const
{
    return AxisHint{ vertical_, Anchors{} };
}

AxisHint SimpleSizeHint::getWidthForHeight(float) const
{
    return AxisHint{ horizontal_, Anchors{} };
}

SimpleSizeHint simpleSizeHint(Band x, Band y)
{
    return SimpleSizeHint{ x, y };
}

SimpleSizeHint simpleSizeHint(float x, float y)
{
    return simpleSizeHint(Band{ x, x, x }, Band{ y, y, y });
}

SimpleSizeHint defaultSizeHint()
{
    return simpleSizeHint(
            Band{ 10.0f, 10000.0f, 10000.0f },
            Band{ 10.0f, 10000.0f, 10000.0f }
            );
}

} // namespace bqui
