#include "outlines.h"

#include "pathbuilder.h"

#include <iterator>
#include <utility>

namespace avg
{

static_assert(std::is_nothrow_move_constructible<Outlines>::value, "");
static_assert(std::is_nothrow_move_assignable<Outlines>::value, "");

namespace
{
    Shape makeRectShape(pmr::memory_resource* memory, Rect const& r)
    {
        return Shape(PathBuilder(memory)
                .start(r.getBottomLeft())
                .lineTo(r.getBottomRight())
                .lineTo(r.getTopRight())
                .lineTo(r.getTopLeft())
                .close()
                .build());
    }
} // anonymous namespace

Outlines::Outlines(pmr::memory_resource* memory) :
    shapes_(memory)
{
}

Outlines::Outlines(pmr::memory_resource* memory, Shape shape) :
    shapes_(memory)
{
    shapes_.push_back(std::move(shape));
}

pmr::memory_resource* Outlines::getResource() const
{
    return shapes_.get_allocator().resource();
}

Outlines& Outlines::operator+=(Shape shape)
{
    shapes_.push_back(std::move(shape));
    return *this;
}

Outlines& Outlines::operator+=(Outlines const& outlines)
{
    if (&outlines == this)
        return *this += Outlines(outlines);

    shapes_.insert(shapes_.end(), outlines.shapes_.begin(),
            outlines.shapes_.end());
    return *this;
}

Outlines& Outlines::operator+=(Outlines&& outlines)
{
    if (&outlines == this)
        return *this += Outlines(outlines);

    if (shapes_.empty())
    {
        shapes_ = std::move(outlines.shapes_);
        return *this;
    }

    shapes_.insert(shapes_.end(),
            std::make_move_iterator(outlines.shapes_.begin()),
            std::make_move_iterator(outlines.shapes_.end()));
    return *this;
}

Outlines Outlines::operator+(Outlines const& outlines) &&
{
    *this += outlines;
    return std::move(*this);
}

Outlines Outlines::clip(Rect const& r) &&
{
    return std::move(*this).clip(Obb(r));
}

Outlines Outlines::clip(Obb const& obb) &&
{
    if (shapes_.empty())
        return std::move(*this);

    if (obb.getSize()[0] <= 0.0f || obb.getSize()[1] <= 0.0f)
    {
        shapes_.clear();
        return std::move(*this);
    }

    auto toObb = obb.getTransform().inverse();
    auto obbRect = Rect(Vector2f(0.0f, 0.0f), obb.getSize());

    pmr::vector<Shape> clipped(getResource());

    for (auto& shape : shapes_)
    {
        auto bounds = (toObb * Obb(shape.getControlBb())).getBoundingRect();

        if (bounds.isFullyContainedIn(obbRect))
        {
            clipped.push_back(std::move(shape));
        }
        else if (bounds.overlaps(obbRect))
        {
            clipped.push_back(std::move(shape).intersect(
                        makeRectShape(getResource(), obbRect)
                        .transform(obb.getTransform())));
        }
    }

    shapes_ = std::move(clipped);

    return std::move(*this);
}

Outlines Outlines::filterByRect(Rect const& r) &&
{
    pmr::vector<Shape> kept(getResource());

    for (auto& shape : shapes_)
    {
        if (shape.getControlBb().overlaps(r))
            kept.push_back(std::move(shape));
    }

    shapes_ = std::move(kept);

    return std::move(*this);
}

Outlines Outlines::transform(Transform const& t) &&
{
    for (auto& shape : shapes_)
        shape = std::move(shape).transform(t);

    return std::move(*this);
}

Outlines Outlines::operator*(float scale) &&
{
    for (auto& shape : shapes_)
        shape = std::move(shape) * scale;

    return std::move(*this);
}

pmr::vector<Shape> const& Outlines::getShapes() const
{
    return shapes_;
}

bool Outlines::isEmpty() const
{
    return shapes_.empty();
}

Rect Outlines::getControlBb() const
{
    Rect result;

    for (auto const& shape : shapes_)
        result += shape.getControlBb();

    return result;
}

} // namespace avg
