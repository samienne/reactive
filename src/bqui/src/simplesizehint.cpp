#include "bqui/simplesizehint.h"

namespace bqui
{

static_assert(IsSizeHint<SimpleSizeHint>::value, "");

SimpleSizeHint::SimpleSizeHint(SizeHintResult x, SizeHintResult y) :
    horizontal_(x),
    vertical_(y)
    {
    }

SizeHintResult SimpleSizeHint::getWidth() const
{
    return horizontal_;
}

SizeHintResult SimpleSizeHint::getHeightForWidth(float) const
{
    return vertical_;
}

SizeHintResult SimpleSizeHint::getWidthForHeight(float) const
{
    return horizontal_;
}

SimpleSizeHint simpleSizeHint(SizeHintResult x, SizeHintResult y)
{
    return SimpleSizeHint{std::move(x), std::move(y)};
}

SimpleSizeHint simpleSizeHint(float x, float y)
{
    return simpleSizeHint({{x, x, x}}, {{y, y, y}});
}
} // namespace bqui

