#include "pathbuilder.h"

#include "debug.h"

#include <btl/cloneoncopy.h>

namespace avg
{

PathBuilder::PathBuilder(pmr::memory_resource* memory) :
    segments_(memory),
    vertices_(memory),
    start_(0.0f, 0.0f)
{
    segments_.push_back(Path::SegmentType::start);
    vertices_.push_back(start_);
}

PathBuilder::~PathBuilder()
{
}

pmr::memory_resource* PathBuilder::getResource() const
{
    return segments_.get_allocator().resource();
}

PathBuilder PathBuilder::start(Vector2f v) &&
{
    if (!segments_.empty() && segments_.back() == Path::SegmentType::start)
        vertices_.back() = v;
    else
    {
        segments_.push_back(Path::SegmentType::start);
        vertices_.push_back(v);
    }

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
    return std::move(*this);
}

PathBuilder PathBuilder::cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&
{
    segments_.push_back(Path::SegmentType::cubic);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    vertices_.push_back(v3);
    return std::move(*this);
}

PathBuilder PathBuilder::arc(Vector2f center, float angle) &&
{
    segments_.push_back(Path::SegmentType::arc);
    vertices_.push_back(center);
    vertices_.emplace_back(angle, 0.0f);

    return std::move(*this);
}

PathBuilder PathBuilder::close() &&
{
    if (segments_.empty() || segments_.back() == Path::SegmentType::start)
        return std::move(*this);

    return std::move(*this).lineTo(start_);
}

Path PathBuilder::build() &&
{
    return Path(std::move(segments_), std::move(vertices_));
}

Path PathBuilder::build() const&
{
    return Path(btl::clone(segments_), btl::clone(vertices_));
}

} // avg

