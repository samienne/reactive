#include "sizehint.h"

namespace reactive
{

static_assert(IsSizeHint<SizeHint>::value, "");

SizeHintResult SizeHint::getWidth() const
{
    return hint_->getWidth();
}

SizeHintResult SizeHint::getHeight(float width) const
{
    return hint_->getHeight(width);
}

SizeHintResult SizeHint::getFinalWidth(float width, float height) const
{
    return hint_->getFinalWidth(width, height);
}

SizeHintResult getLargestHint(std::vector<SizeHintResult> const& hints)
{
    auto result = SizeHintResult{{0.0f, 0.0f, 0.0f}};
    for (auto const& hint : hints)
        for (int i = 0; i < 3; ++i)
            result[i] = std::max(result[i], hint[i]);

    return result;
}

std::ostream& operator<<(std::ostream& stream,
        SizeHintResult const& h)
{
    stream << "SizeHintResult{"
        << "h:{";

    for (int i = 0; i < 3; ++i)
        stream << h[i] << (i < 2 ? "," :"");

    return stream << "}";
}
} // namespace reactive

