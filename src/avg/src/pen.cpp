#include "pen.h"

namespace avg
{

Pen::Pen(Brush brush, float width, JoinType join, EndType end) :
    brush_(brush),
    join_(join),
    end_(end),
    width_(width)
{
}

Pen::~Pen()
{
}

Pen Pen::operator*(float scale) const
{
    return Pen(brush_ * scale, width_ * scale, join_, end_);
}

Pen& Pen::operator*=(float scale)
{
    brush_ = brush_ * scale;
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

JoinType Pen::getJoinType() const noexcept
{
    return join_;
}

EndType Pen::getEndType() const noexcept
{
    return end_;
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

