#include "stacksizehint.h"

namespace reactive
{

SizeHintResult StackSizeHint::operator()() const
{
    auto hints = btl::fmap(hints_, [](auto const& hint)
        {
            return hint();
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::operator()(float x) const
{
    auto hints = btl::fmap(hints_, [x](auto const& hint)
        {
            return hint(x);
        });

    return getLargestHint(hints);
}

SizeHintResult StackSizeHint::operator()(float x, float y) const
{
    auto hints = btl::fmap(hints_, [x, y](auto const& hint)
        {
            return hint(x, y);
        });

    return getLargestHint(hints);
}

StackSizeHint stackSizeHints(std::vector<SizeHint> const& hints)
{
    return { hints };
}

} // namespace reactive

