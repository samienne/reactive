#include "bqui/stacksizehint.h"

namespace bqui
{

SizeHintResult StackSizeHint::getWidth() const
{
    auto hints = btl::fmap(hints_, [](auto const& hint)
        {
            return hint.getWidth();
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::getHeightForWidth(float width) const
{
    auto hints = btl::fmap(hints_, [width](auto const& hint)
        {
            return hint.getHeightForWidth(width);
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::getWidthForHeight(float height) const
{
    auto hints = btl::fmap(hints_, [height](auto const& hint)
        {
            return hint.getWidthForHeight(height);
        });

    return getLargestHint(hints);
}

StackSizeHint stackSizeHints(std::vector<SizeHint> hints)
{
    return { std::move(hints) };
}

} // namespace bqui

