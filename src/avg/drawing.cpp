#include "drawing.h"

#include "debug.h"

namespace avg
{

static_assert(std::is_nothrow_move_constructible<Drawing>::value, "");
static_assert(std::is_nothrow_move_assignable<Drawing>::value, "");

namespace
{
    Rect combineRects(Rect const& r1, Rect const& r2)
    {
        if (r1.isEmpty())
            return r2;

        if (r2.isEmpty())
            return r1;

        float x1 = std::min(r1.getLeft(), r2.getLeft());
        float y1 = std::min(r1.getBottom(), r2.getBottom());
        float x2 = std::max(r1.getRight(), r2.getRight());
        float y2 = std::max(r1.getTop(), r2.getTop());

        return Rect(Vector2f(x1, y1), Vector2f(x2-x1, y2-y1));
    }

    Rect getElementRect(Drawing::Element const& e)
    {
        if (e.is<Shape>())
            return e.get<Shape>().getControlBb();
        else if (e.is<TextEntry>())
            return e.get<TextEntry>().getControlBb();
        else
            assert(false);

        return Rect();
    }

    Rect combineDrawingRects(std::vector<Drawing::Element> const& elements)
    {
        Rect r;
        for (auto const& e : elements)
        {
            r = combineRects(r, getElementRect(e));
        }

        return r;
    }
} // anonymous namespace

Drawing::Drawing()
{
}

Drawing::Drawing(Element const& element)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    elements_.push_back(element);
}

Drawing::Drawing(std::vector<Element> const& elements) :
    elements_(elements),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::Drawing(std::vector<Element>&& elements) :
    elements_(std::move(elements)),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::~Drawing()
{
}

Drawing Drawing::operator+(Element&& element) const &
{
    Drawing drawing;
    drawing.controlBb_ = combineRects(controlBb_, getElementRect(element));
    drawing.elements_ = elements_;
    drawing.elements_.push_back(std::move(element));

    return drawing;
}

Drawing Drawing::operator+(Element&& element) &&
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));
    elements_.push_back(std::move(element));

    return std::move(*this);
}

Drawing& Drawing::operator+=(Element&& element)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));
    elements_.push_back(element);

    return *this;
}

Drawing Drawing::operator+(Drawing const& drawing) const &
{
    Drawing result;

    result.controlBb_ = combineRects(controlBb_, drawing.controlBb_);
    result.elements_.reserve(elements_.size() + drawing.elements_.size());

    for (auto const& element : elements_)
        result.elements_.push_back(element);

    for (auto const& element : drawing.elements_)
        result.elements_.push_back(element);


    return result;
}

Drawing Drawing::operator+(Drawing const& drawing) &&
{
    Drawing result;
    result.controlBb_ = combineRects(controlBb_, drawing.controlBb_);
    elements_.reserve(elements_.size() + drawing.elements_.size());
    result.elements_ = std::move(elements_);

    for (auto const& element : drawing.elements_)
        result.elements_.push_back(element);

    return result;
}

Drawing& Drawing::operator+=(Drawing const& drawing)
{
    controlBb_ = combineRects(controlBb_, drawing.controlBb_);

    for (auto&& e : drawing.elements_)
        elements_.push_back(e);

    return *this;
}

Drawing Drawing::operator*(float scale) const &
{
    Drawing result;

    result.elements_.reserve(elements_.size());
    result.controlBb_ = controlBb_.scaled(scale);

    for (auto const& element : elements_)
        result.elements_.push_back(element * scale);

    return result;
}

Drawing Drawing::operator*(float scale) &&
{
    controlBb_ = controlBb_.scaled(scale);

    for (auto& element : elements_)
        element = element * scale;

    return std::move(*this);
}

Drawing Drawing::operator+(ase::Vector2f offset) const &
{
    Drawing result;

    result.elements_.reserve(elements_.size());
    result.controlBb_ = Rect(controlBb_.getBottomLeft() + offset,
            controlBb_.getSize());

    for (auto const& element : elements_)
        result.elements_.push_back(element + offset);

    return result;
}

Drawing Drawing::operator+(ase::Vector2f offset) &&
{
    controlBb_ = Rect(controlBb_.getBottomLeft() + offset,
            controlBb_.getSize());

    elements_.reserve(elements_.size());

    for (auto& element : elements_)
        element = element + offset;

    return std::move(*this);
}

bool Drawing::operator==(Drawing const& rhs) const
{
    return elements_ == rhs.elements_;
}

std::vector<Drawing::Element> const& Drawing::getElements() const
{
    return elements_;
}

Rect Drawing::getControlBb() const
{
    return controlBb_;
}

Drawing Drawing::filterByRect(Rect const& r) &&
{
    elements_.erase(
            std::remove_if(
                elements_.begin(),
                elements_.end(),
                [&r](Element const& e)
                {
                    return !getElementRect(e).overlaps(r);
                }),
            elements_.end()
            );

    controlBb_ = combineDrawingRects(elements_);

    return std::move(*this);
}

Drawing Drawing::transform(Transform const& t) &&
{
    for (auto&& e : elements_)
    {
        if (e.is<TextEntry>())
            e = t * std::move(e.get<TextEntry>());
        else if (e.is<Shape>())
            e = t * std::move(e.get<Shape>());
        else
            assert(false);
    }

    controlBb_ = combineDrawingRects(elements_);

    return std::move(*this);
}

Drawing operator*(Transform const& t, Drawing&& drawing)
{
    for (auto&& e : drawing.elements_)
    {
        if (e.is<TextEntry>())
            e = t * std::move(e.get<TextEntry>());
        else if (e.is<Shape>())
            e = t * std::move(e.get<Shape>());
        else
            assert(false);
    }

    drawing.controlBb_ = combineDrawingRects(drawing.elements_);

    return std::move(drawing);
}

} // namespace

