#include "pen.h"

namespace avg
{

Pen::Pen(Brush brush, float width) :
    brush_(brush),
    width_(width)
{
}

Pen::~Pen()
{
}

Pen Pen::operator*(float scale) const noexcept
{
    return Pen(brush_, width_ * scale);
}

Pen& Pen::operator*=(float scale) noexcept
{
    width_ *= scale;

    return *this;
}

Brush const& Pen::getBrush() const noexcept
{
    return brush_;
}

float Pen::getWidth() const noexcept
{
    return width_;
}

bool Pen::operator==(Pen const& rhs) const noexcept
{
    return width_ == rhs.width_ && brush_ == rhs.brush_;
}

bool Pen::operator!=(Pen const& rhs) const noexcept
{
    return width_ != rhs.width_ || brush_ != rhs.brush_;
}

bool Pen::operator<(Pen const& rhs) const noexcept
{
    if (brush_ != rhs.brush_)
        return brush_ < rhs.brush_;

    return width_ < rhs.width_;
}

bool Pen::operator>(Pen const& rhs) const noexcept
{
    if (brush_ != rhs.brush_)
        return brush_ > rhs.brush_;

    return width_ > rhs.width_;
}

} // namespace

