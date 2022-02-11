#include "pathbuilder.h"

#include "pen.h"
#include "brush.h"
#include "shape.h"

#include "debug.h"

#include <btl/cloneoncopy.h>

namespace avg
{

PathBuilder::PathBuilder(pmr::memory_resource* memory) :
    memory_(memory),
    data_(memory),
    segments_(memory),
    vertices_(memory),
    start_(0.0f, 0.0f)
{
    segments_.push_back(Path::SegmentType::start);
    vertices_.push_back(start_);

    data_.push(Path::StartSegment{ Path::SegmentType::start, { 0.0f, 0.0f } });
    newPoly_ = true;
}

pmr::memory_resource* PathBuilder::getResource() const
{
    return memory_;
}

PathBuilder PathBuilder::start(Vector2f v) &&
{
    if (newPoly_)
    {
        vertices_.back() = v;

        data_.pop(sizeof(Path::StartSegment));
    }
    else
    {
        segments_.push_back(Path::SegmentType::start);
        vertices_.push_back(v);

    }

    data_.push(Path::StartSegment{ Path::SegmentType::start, v });
    newPoly_ = true;
    start_ = v;

    return std::move(*this);
}

PathBuilder PathBuilder::start(float x, float y) &&
{
    return std::move(*this).start(Vector2f(x, y));
}

PathBuilder PathBuilder::lineTo(Vector2f v) &&
{
    segments_.push_back(Path::SegmentType::line);
    vertices_.push_back(v);

    data_.push(Path::LineSegment{ Path::SegmentType::line, v });
    newPoly_ = false;

    return std::move(*this);
}

PathBuilder PathBuilder::lineTo(float x, float y) &&
{
    return std::move(*this).lineTo(Vector2f(x, y));
}

PathBuilder PathBuilder::conicTo(Vector2f v1, Vector2f v2) &&
{
    segments_.push_back(Path::SegmentType::conic);
    vertices_.push_back(v1);
    vertices_.push_back(v2);

    data_.push(Path::ConicSegment{ Path::SegmentType::conic, v1, v2 });
    newPoly_ = false;

    return std::move(*this);
}

PathBuilder PathBuilder::cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&
{
    segments_.push_back(Path::SegmentType::cubic);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    vertices_.push_back(v3);

    data_.push(Path::CubicSegment{ Path::SegmentType::cubic, v1, v2, v3 });
    newPoly_ = false;

    return std::move(*this);
}

PathBuilder PathBuilder::arc(Vector2f center, float angle) &&
{
    segments_.push_back(Path::SegmentType::arc);
    vertices_.push_back(center);
    vertices_.emplace_back(angle, 0.0f);

    data_.push(Path::ArcSegment{ Path::SegmentType::arc, center, angle });
    newPoly_ = false;

    return std::move(*this);
}

PathBuilder PathBuilder::close() &&
{
    if (newPoly_)
        return std::move(*this);

    return std::move(*this).lineTo(start_);
}

Path PathBuilder::build() &&
{
    return Path(std::move(data_));
}

Path PathBuilder::build() const&
{
    return Path(btl::Buffer(data_));
}

Shape PathBuilder::buildShape() &&
{
    return Shape(Path(std::move(data_)), std::nullopt, std::nullopt);
}

Shape PathBuilder::buildShape(avg::Brush brush) &&
{
    return Shape(Path(std::move(data_)), std::make_optional(std::move(brush)), std::nullopt);
}

Shape PathBuilder::buildShape(avg::Brush brush, avg::Pen pen) &&
{
    return Shape(Path(std::move(data_)), std::make_optional(std::move(brush)),
            std::make_optional(std::move(pen)));
}

Shape PathBuilder::buildShape(avg::Pen pen) &&
{
    return Shape(Path(std::move(data_)), std::nullopt, std::make_optional(std::move(pen)));
}

} // avg

