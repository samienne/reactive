#include "pathbuilder.h"

#include "debug.h"

#include <btl/cloneoncopy.h>

namespace avg
{

PathBuilder::PathBuilder() :
    start_(0.0f, 0.0f)
{
    segments_.push_back(Path::SEGMENT_START);
    vertices_.push_back(start_);
}

PathBuilder::~PathBuilder()
{
}

PathBuilder PathBuilder::start(Vector2f v) &&
{
    if (!segments_.empty() && segments_.back() == Path::SEGMENT_START)
        vertices_.back() = v;
    else
    {
        segments_.push_back(Path::SEGMENT_START);
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
    segments_.push_back(Path::SEGMENT_LINE);
    vertices_.push_back(v);

    return std::move(*this);
}

PathBuilder PathBuilder::lineTo(float x, float y) &&
{
    return std::move(*this).lineTo(Vector2f(x, y));
}

PathBuilder PathBuilder::conicTo(Vector2f v1, Vector2f v2) &&
{
    segments_.push_back(Path::SEGMENT_CONIC);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    return std::move(*this);
}

PathBuilder PathBuilder::cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&
{
    segments_.push_back(Path::SEGMENT_CUBIC);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    vertices_.push_back(v3);
    return std::move(*this);
}

PathBuilder PathBuilder::arc(Vector2f center, float angle) &&
{
    segments_.push_back(Path::SEGMENT_ARC);
    vertices_.push_back(center);
    vertices_.emplace_back(angle, 0.0f);

    return std::move(*this);
}

PathBuilder PathBuilder::close() &&
{
    if (segments_.empty() || segments_.back() == Path::SEGMENT_START)
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

