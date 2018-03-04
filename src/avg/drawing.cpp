#include "drawing.h"

#include "debug.h"

namespace avg
{

static_assert(std::is_nothrow_move_constructible<Drawing>::value, "");
static_assert(std::is_nothrow_move_assignable<Drawing>::value, "");

Drawing::Drawing()
{
}

Drawing::Drawing(Element const& element)
{
    elements_.push_back(element);
}

Drawing::Drawing(std::vector<Element> const& elements) :
    elements_(elements)
{
}

Drawing::Drawing(std::vector<Element>&& elements) :
    elements_(std::move(elements))
{
}

Drawing::~Drawing()
{
}

Drawing Drawing::operator+(Element&& element) const &
{
    Drawing drawing;
    drawing.elements_ = elements_;
    drawing.elements_.push_back(element);

    return drawing;
}

Drawing Drawing::operator+(Element&& element) &&
{
    elements_.push_back(element);
    return Drawing(std::move(elements_));
}

Drawing& Drawing::operator+=(Element&& element)
{
    elements_.push_back(element);

    return *this;
}

Drawing Drawing::operator+(Drawing const& drawing) const &
{
    Drawing result;
    result.elements_.reserve(elements_.size() + drawing.elements_.size());

    for (auto const& element : elements_)
    {
        result.elements_.push_back(element);
    }

    for (auto const& element : drawing.elements_)
    {
        result.elements_.push_back(element);
    }

    return result;
}

Drawing Drawing::operator+(Drawing const& drawing) &&
{
    Drawing result;
    elements_.reserve(elements_.size() + drawing.elements_.size());
    result.elements_ = std::move(elements_);

    for (auto const& element : drawing.elements_)
    {
        result.elements_.push_back(element);
    }

    return result;
}

Drawing& Drawing::operator+=(Drawing const& drawing)
{
    for (auto&& e : drawing.elements_)
        elements_.push_back(e);

    return *this;
}

Drawing Drawing::operator*(float scale) const &
{
    std::vector<Element> elements;
    elements.reserve(elements_.size());
    for (auto const& element : elements_)
        elements.push_back(element * scale);

    return Drawing(std::move(elements));
}

Drawing Drawing::operator*(float scale) &&
{
    std::vector<Element> elements = std::move(elements_);
    elements.reserve(elements_.size());
    for (auto& element : elements)
        element = element * scale;

    return Drawing(std::move(elements));
}

Drawing Drawing::operator+(ase::Vector2f offset) const &
{
    std::vector<Element> elements;
    elements.reserve(elements_.size());
    for (auto const& element : elements_)
        elements.push_back(element + offset);

    return Drawing(std::move(elements));
}

Drawing Drawing::operator+(ase::Vector2f offset) &&
{
    std::vector<Element> elements = std::move(elements_);
    elements.reserve(elements_.size());
    for (auto& element : elements)
        element = element + offset;

    return Drawing(std::move(elements));
}

bool Drawing::operator==(Drawing const& rhs) const
{
    return elements_ == rhs.elements_;
}

std::vector<Drawing::Element> const& Drawing::getElements() const
{
    return elements_;
}

Drawing Drawing::transform(Transform const& t) &&
{
    for (auto&& e : elements_)
    {
        if (e.is<TextEntry>())
            e = t * e.get<TextEntry>();
        else if (e.is<Shape>())
            e = t * e.get<Shape>();
        else
            assert(false);
    }

    return std::move(*this);
}

Drawing operator*(Transform const& t, Drawing&& drawing)
{
    for (auto&& e : drawing.elements_)
    {
        if (e.is<TextEntry>())
            e = t * e.get<TextEntry>();
        else if (e.is<Shape>())
            e = t * e.get<Shape>();
        else
            assert(false);
    }

    return std::move(drawing);
}

} // namespace

