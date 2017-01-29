#include "shape.h"

namespace avg
{

Shape::Shape()
{
}

/*Shape::Shape(Path const& path, btl::option<Brush> const& brush,
        btl::option<Pen> const& pen) :
    path_(path),
    brush_(brush),
    pen_(pen)
{
}*/

Shape::~Shape()
{
}

Shape Shape::setPath(Path const& path) &&
{
    path_ = path;
    return std::move(*this);
}

Shape Shape::setBrush(btl::option<Brush> const& brush) &&
{
    brush_ = brush;
    return std::move(*this);
}

Shape Shape::setPen(btl::option<Pen> const& pen) &&
{
    pen_ = pen;
    return std::move(*this);
}

Path const& Shape::getPath() const
{
    return path_;
}

btl::option<Pen> const& Shape::getPen() const
{
    return pen_;
}

btl::option<Brush> const& Shape::getBrush() const
{
    return brush_;
}

Shape Shape::operator*(float scale) const &
{
    return Shape()
        .setPath(path_ * scale)
        .setBrush(brush_ * scale)
        .setPen(pen_ * scale);
}

Shape Shape::operator*(float scale) &&
{
    return Shape()
        .setPath(std::move(path_) * scale)
        .setBrush(brush_ * scale)
        .setPen(pen_ * scale);
}

Shape Shape::operator+(ase::Vector2f offset) const &
{
    return Shape()
        .setPath(path_ + offset)
        .setBrush(brush_)
        .setPen(pen_);
}

Shape Shape::operator+(ase::Vector2f offset) &&
{
    return Shape()
        .setPath(std::move(path_) + offset)
        .setBrush(brush_)
        .setPen(pen_);
}

Shape& Shape::operator*=(float scale)
{
    path_ *= scale;
    pen_ *= scale;
    return *this;
}

Shape& Shape::operator+=(ase::Vector2f offset)
{
    path_ += offset;
    return *this;
}

bool Shape::operator==(Shape const& rhs) const
{
    return pen_ == rhs.pen_ && brush_ == rhs.brush_
        && path_ == rhs.path_;
}

bool Shape::operator!=(Shape const& rhs) const
{
    return pen_ != rhs.pen_ || brush_ != rhs.brush_
        || path_ != rhs.path_;
}

} // namespace

