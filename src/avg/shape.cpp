#include "shape.h"

#include "obb.h"

namespace avg
{

Shape::Shape(pmr::memory_resource* memory) :
    path_(memory)
{
}

Shape::Shape(Path path, std::optional<Brush> brush, std::optional<Pen> pen) :
    path_(std::move(path)),
    brush_(std::move(brush)),
    pen_(std::move(pen))
{
}

Shape::~Shape()
{
}

pmr::memory_resource* Shape::getResource() const
{
    return path_.getResource();
}

Shape Shape::setPath(Path const& path) &&
{
    path_ = path;
    return std::move(*this);
}

Shape Shape::setBrush(std::optional<Brush> const& brush) &&
{
    brush_ = brush;
    return std::move(*this);
}

Shape Shape::setPen(std::optional<Pen> const& pen) &&
{
    pen_ = pen;
    return std::move(*this);
}

Path const& Shape::getPath() const
{
    return path_;
}

std::optional<Pen> const& Shape::getPen() const
{
    return pen_;
}

std::optional<Brush> const& Shape::getBrush() const
{
    return brush_;
}

Rect Shape::getControlBb() const
{
    return path_.getControlBb();
}

Obb Shape::getControlObb() const
{
    return path_.getControlBb();
}

Shape Shape::operator*(float scale) const &
{
    return Shape(getResource())
        .setPath(path_ * scale)
        .setBrush(brush_ ? *brush_ * scale : std::optional<Brush>(std::nullopt))
        .setPen(pen_ ? *pen_ * scale : std::optional<Pen>(std::nullopt));
}

Shape Shape::operator*(float scale) &&
{
    path_ = std::move(path_) * scale;
    brush_ = brush_ ? *brush_ * scale : std::optional<Brush>(std::nullopt);
    pen_ = pen_ ? *pen_ * scale : std::optional<Pen>(std::nullopt);
    return std::move(*this);
}

Shape Shape::operator+(Vector2f offset) const &
{
    return Shape(getResource())
        .setPath(path_ + offset)
        .setBrush(brush_)
        .setPen(pen_);
}

Shape Shape::operator+(Vector2f offset) &&
{
    path_ = std::move(path_) + offset;
    return std::move(*this);
}

Shape& Shape::operator*=(float scale)
{
    path_ *= scale;
    pen_ = pen_ ? *pen_ * scale : std::optional<Pen>(std::nullopt);
    return *this;
}

Shape& Shape::operator+=(Vector2f offset)
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

Shape Shape::with_resource(pmr::memory_resource* memory) const
{
    return Shape(path_.with_resource(memory), brush_, pen_);
}

/*
Shape operator*(Transform const& t, Shape const& rhs)
{
    return Shape(t * rhs.path_, t * rhs.brush_, t * rhs.pen_);
}
*/
} // namespace

