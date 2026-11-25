#include "brush.h"

namespace avg
{

Brush::Brush(Color const& color) :
    color_(color)
{
}

Brush::Brush(Image image, Color const& color) :
    color_(color),
    image_(std::move(image))
{
}

Brush::~Brush()
{
}

Color const& Brush::getColor() const noexcept
{
    return color_;
}

std::optional<Image> const& Brush::getImage() const noexcept
{
    return image_;
}

bool Brush::operator==(Brush const& rhs) const noexcept
{
    return color_ == rhs.color_ && image_ == rhs.image_;
}

bool Brush::operator!=(Brush const& rhs) const noexcept
{
    return !(*this == rhs);
}

bool Brush::operator<(Brush const& rhs) const noexcept
{
    if (color_ != rhs.color_)
        return color_ < rhs.color_;

    return image_ < rhs.image_;
}

bool Brush::operator>(Brush const& rhs) const noexcept
{
    return rhs < *this;
}

Brush Brush::operator*(float) const noexcept
{
    return *this;
}

} // namespace avg
