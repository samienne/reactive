#include "pathspec.h"

#include "debug.h"

namespace avg
{

PathSpec::PathSpec() :
    start_(0.0f, 0.0f)
{
    segments_.push_back(SEGMENT_START);
    vertices_.push_back(start_);
}

PathSpec::~PathSpec()
{
}

PathSpec PathSpec::start(Vector2f v) &&
{
    if (!segments_.empty() && segments_.back() == SEGMENT_START)
        vertices_.back() = v;
    else
    {
        segments_.push_back(SEGMENT_START);
        vertices_.push_back(v);
    }

    start_ = v;

    return std::move(*this);
}

PathSpec PathSpec::start(float x, float y) &&
{
    return std::move(*this).start(Vector2f(x, y));
}

PathSpec PathSpec::lineTo(Vector2f v) &&
{
    segments_.push_back(SEGMENT_LINE);
    vertices_.push_back(v);
    return std::move(*this);
}

PathSpec PathSpec::lineTo(float x, float y) &&
{
    return std::move(*this).lineTo(Vector2f(x, y));
}

PathSpec PathSpec::conicTo(Vector2f v1, Vector2f v2) &&
{
    segments_.push_back(SEGMENT_CONIC);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    return std::move(*this);
}

PathSpec PathSpec::cubicTo(Vector2f v1, Vector2f v2, Vector2f v3) &&
{
    segments_.push_back(SEGMENT_CUBIC);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    vertices_.push_back(v3);
    return std::move(*this);
}

PathSpec PathSpec::arc(Vector2f center, float angle) &&
{
    segments_.push_back(SEGMENT_ARC);
    vertices_.push_back(center);
    vertices_.emplace_back(angle, 0.0f);

    return std::move(*this);
}

PathSpec PathSpec::close() &&
{
    if (segments_.empty() || segments_.back() == SEGMENT_START)
        return std::move(*this);

    return std::move(*this).lineTo(start_);
}

} // avg

