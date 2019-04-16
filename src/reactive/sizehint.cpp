#include "sizehint.h"

namespace reactive
{

static_assert(IsSizeHint<SizeHint>::value, "");

SizeHintResult SizeHint::operator()() const
{
    return (*hint_)();
}

SizeHintResult SizeHint::operator()(float x) const
{
    return (*hint_)(x);
}

SizeHintResult SizeHint::operator()(float x, float y) const
{
    return (*hint_)(x, y);
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

