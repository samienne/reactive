#include "brush.h"

#include "effectfactory.h"

#include <cassert>
#include <utility>

namespace avg
{

Brush::Brush(Color const& color) :
    content_(color)
{
}

Brush::Brush(Effect effect) :
    content_(std::move(effect))
{
}

Brush::~Brush()
{
}

Color const& Brush::getColor() const noexcept
{
    auto const* color = std::get_if<Color>(&content_);
    assert(color);
    return *color;
}

Effect const* Brush::getEffect() const noexcept
{
    return std::get_if<Effect>(&content_);
}

Brush Brush::operator*(float scale) const
{
    if (Effect const* effect = getEffect())
        return Brush(toPixels(*effect, scale));

    return *this;
}

bool Brush::operator==(Brush const& rhs) const noexcept
{
    return content_ == rhs.content_;
}

bool Brush::operator!=(Brush const& rhs) const noexcept
{
    return !(*this == rhs);
}

bool Brush::operator<(Brush const& rhs) const noexcept
{
    auto const* color = std::get_if<Color>(&content_);
    auto const* rhsColor = std::get_if<Color>(&rhs.content_);

    if (color && rhsColor)
        return *color < *rhsColor;

    return color && !rhsColor;
}

bool Brush::operator>(Brush const& rhs) const noexcept
{
    return rhs < *this;
}

std::ostream& operator<<(std::ostream& stream, Brush const& b)
{
    if (!b.getEffect())
        return stream << "Brush{" << b.getColor() << "}";

    return stream << "Brush{" << *b.getEffect() << "}";
}

bool isSameKind(Brush const& a, Brush const& b)
{
    Effect const* effect = a.getEffect();
    Effect const* other = b.getEffect();

    if (!effect && !other)
        return true;

    return effect && other && isSameKind(*effect, *other);
}

} // namespace avg
