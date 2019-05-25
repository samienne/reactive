#include "stacksizehint.h"

namespace reactive
{

SizeHintResult StackSizeHint::getWidth() const
{
    auto hints = btl::fmap(hints_, [](auto const& hint)
        {
            return hint.getWidth();
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::getHeight(float width) const
{
    auto hints = btl::fmap(hints_, [width](auto const& hint)
        {
            return hint.getHeight(width);
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::getFinalWidth(float width, float height) const
{
    auto hints = btl::fmap(hints_, [width, height](auto const& hint)
        {
            return hint.getFinalWidth(width, height);
        });

    return getLargestHint(hints);
}

StackSizeHint stackSizeHints(std::vector<SizeHint> hints)
{
    return { std::move(hints) };
}

} // namespace reactive

