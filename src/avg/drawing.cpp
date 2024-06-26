#include "drawing.h"

#include "debug.h"

#include <pmr/new_delete_resource.h>
#include <pmr/heap.h>

namespace avg
{

static_assert(std::is_nothrow_move_constructible<Drawing>::value, "");
static_assert(std::is_nothrow_move_assignable<Drawing>::value, "");

namespace
{
    template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };

    template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

    // Returns a rect that covers both r1 and r2.
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
        return std::visit(Overloaded{
                    [](Drawing::ShapeElement const& shapeElement) -> Rect
                    {
                        return shapeElement.shape.getControlBb();
                    },
                    [](TextEntry const& textEntry) -> Rect
                    {
                        return textEntry.getControlBb();
                    },
                    [](Drawing::ClipElement const& clipElement) -> Rect
                    {
                        return (clipElement.transform *
                                avg::Obb(clipElement.clipRect)).getBoundingRect();
                    },
                    [](Drawing::RegionFill const& regionFill) -> Rect
                    {
                        return regionFill.region.getBoundingBox();
                    }
                    },
                e
                );
    }

    Rect combineDrawingRects(pmr::vector<Drawing::Element> const& elements)
    {
        Rect r;
        for (auto const& e : elements)
            r = combineRects(r, getElementRect(e));

        return r;
    }


    void filterElementsByRect(pmr::vector<Drawing::Element>& elements, Rect const& r)
    {
        elements.erase(
                std::remove_if(
                    elements.begin(),
                    elements.end(),
                    [&r](Drawing::Element const& e)
                    {
                        return !getElementRect(e).overlaps(r);
                    }),
                elements.end()
                );
    }

    pmr::memory_resource* getElementResource(Drawing::Element const& e)
    {
        return std::visit(Overloaded{
                    [](Drawing::ShapeElement const& shapeElement) -> pmr::memory_resource*
                    {
                        return shapeElement.shape.getResource();
                    },
                    [](TextEntry const& /*textEntry*/) -> pmr::memory_resource*
                    {
                        return pmr::new_delete_resource();
                    },
                    [](Drawing::ClipElement const& clipElement) -> pmr::memory_resource*
                    {
                        return clipElement.subDrawing.resource();
                    },
                    [](Drawing::RegionFill const& regionFill) -> pmr::memory_resource*
                    {
                        return regionFill.region.getResource();
                    }},
                e);
    }

    Drawing::Element withResource(
            pmr::memory_resource* memory,
            Drawing::Element const& e
            );

    Drawing::SubDrawing withResource(
            pmr::memory_resource* memory,
            Drawing::SubDrawing const& drawing
            )
    {
        Drawing::SubDrawing result {
            pmr::vector<Drawing::Element>(memory)
        };

        result.elements.reserve(drawing.elements.size());

        for (auto const& e : drawing.elements)
            result.elements.push_back(withResource(memory, e));

        return result;
    }

    Drawing::Element withResource(
            pmr::memory_resource* memory,
            Drawing::Element const& e
            )
    {
        return std::visit(Overloaded{
                    [&](Drawing::ShapeElement const& shapeElement) -> Drawing::Element
                    {
                        return Drawing::ShapeElement {
                            shapeElement.shape.with_resource(memory),
                            shapeElement.brush,
                            shapeElement.pen
                        };
                    },
                    [&](TextEntry const& textEntry) -> Drawing::Element
                    {
                        return textEntry;
                    },
                    [&](Drawing::ClipElement const& clipElement) -> Drawing::Element
                    {
                        return Drawing::ClipElement {
                            pmr::heap<Drawing::SubDrawing>(
                                    withResource(memory, *clipElement.subDrawing),
                                    memory
                                    ),
                            clipElement.clipRect,
                            clipElement.transform
                        };
                    },
                    [&](Drawing::RegionFill const& regionFill) -> Drawing::Element
                    {
                        return Drawing::RegionFill{
                            regionFill.region.withResource(memory),
                            regionFill.brush
                        };

                    }},
                e);
    }
} // anonymous namespace

Drawing::Drawing(pmr::memory_resource* memory) :
    elements_(memory)
{
}

Drawing::Drawing(pmr::memory_resource* memory, Element element) :
    elements_(memory)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    elements_.push_back(std::move(element));
}

Drawing::Drawing(pmr::vector<Element> const& elements) :
    elements_(elements),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::Drawing(pmr::vector<Element>&& elements) :
    elements_(std::move(elements)),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::~Drawing()
{
}

pmr::memory_resource* Drawing::getResource() const
{
    return elements_.get_allocator().resource();
}

Drawing Drawing::operator+(Element&& element) &&
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    if (getResource() == getElementResource(element))
        elements_.push_back(std::move(element));
    else
        elements_.push_back(withResource(getResource(), element));

    return std::move(*this);
}

Drawing& Drawing::operator+=(Element&& element)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    if (getResource() == getElementResource(element))
        elements_.push_back(std::move(element));
    else
        elements_.push_back(withResource(getResource(), element));

    return *this;
}

Drawing Drawing::operator+(Drawing const& drawing) &&
{
    Drawing result(getResource());
    result.controlBb_ = combineRects(controlBb_, drawing.controlBb_);
    result.elements_ = std::move(elements_);
    result.elements_.reserve(elements_.size() + drawing.elements_.size());

    if (getResource() == drawing.getResource())
    {
        for (auto const& element : drawing.elements_)
            result.elements_.push_back(element);
    }
    else
    {
        for (auto const& element : drawing.elements_)
            result.elements_.push_back(withResource(getResource(), element));
    }

    return result;
}

Drawing& Drawing::operator+=(Drawing const& drawing)
{
    controlBb_ = combineRects(controlBb_, drawing.controlBb_);

    if (getResource() == drawing.getResource())
    {
        for (auto const& element : drawing.elements_)
            elements_.push_back(element);
    }
    else
    {
        for (auto const& element : drawing.elements_)
            elements_.push_back(withResource(getResource(), element));
    }

    return *this;
}

Drawing Drawing::operator*(float scale) &&
{
    controlBb_ = controlBb_.scaled(scale);

    for (auto& e : elements_)
    {
        std::visit(Overloaded{
                [&](ShapeElement& shapeElement)
                {
                    shapeElement.shape = std::move(shapeElement.shape) * scale;
                    if (shapeElement.pen)
                        shapeElement.pen = *shapeElement.pen * scale;

                    if (shapeElement.brush)
                        shapeElement.brush = *shapeElement.brush * scale;
                },
                [&](TextEntry& textEntry)
                {
                    textEntry = std::move(textEntry) * scale;
                },
                [&](Drawing::ClipElement& /*clipElement*/)
                {
                    assert(false);
                },
                [&](Drawing::RegionFill& /*regionFill*/)
                {
                    assert(false);
                }},
                e);
    }

    return std::move(*this);
}

Drawing Drawing::clip(Rect const& r) &&
{
    if (controlBb_.isFullyContainedIn(r))
        return std::move(*this);

    Drawing result(getResource());

    filterElementsByRect(elements_, r);

    result.elements_.push_back(ClipElement{
            { pmr::make_heap<SubDrawing>(getResource(), std::move(elements_)) },
            r,
            avg::Transform()
            });

    result.controlBb_ = r;

    return result;
}

Drawing Drawing::clip(Obb const& obb) &&
{
    auto obbTInverse = obb.getTransform().inverse();
    auto obbRect = Rect(Vector2f(0.0f, 0.0f), obb.getSize());

    if ((obbTInverse * Obb(controlBb_))
            .getBoundingRect()
            .isFullyContainedIn(obbRect))
    {
        return std::move(*this);
    }

    auto d = (obbTInverse * std::move(*this))
        .clip(obbRect);

    return obb.getTransform() * std::move(d);
}

pmr::vector<Drawing::Element> const& Drawing::getElements() const
{
    return elements_;
}

Rect Drawing::getControlBb() const
{
    return controlBb_;
}

Drawing Drawing::filterByRect(Rect const& r) &&
{
    filterElementsByRect(elements_, r);

    controlBb_ = combineDrawingRects(elements_);

    return std::move(*this);
}

Drawing Drawing::transform(Transform const& t) &&
{
    for (auto&& e : elements_)
    {
        std::visit(Overloaded{
                [&](ShapeElement& shapeElement)
                {
                    shapeElement.shape = t * std::move(shapeElement.shape);
                    if (shapeElement.pen)
                        shapeElement.pen = t * std::move(*shapeElement.pen);
                    if (shapeElement.brush)
                        shapeElement.brush = t * std::move(*shapeElement.brush);
                },
                [&](TextEntry& textEntry)
                {
                    textEntry = t * std::move(textEntry);
                },
                [&](Drawing::ClipElement& clipElement)
                {
                    clipElement.transform = t * clipElement.transform;
                },
                [&](Drawing::RegionFill& regionFill)
                {
                    regionFill.region = t * std::move(regionFill.region);
                    regionFill.brush = t * std::move(regionFill.brush);
                }},
                e);
    }

    controlBb_ = combineDrawingRects(elements_);

    return std::move(*this);
}

Drawing Drawing::translate(ase::Vector2f offset) &&
{
    return std::move(*this).transform(avg::translate(offset));
}

Drawing operator*(Transform const& t, Drawing&& drawing)
{
    for (auto&& e : drawing.elements_)
    {
        std::visit(Overloaded{
                    [&](TextEntry& entry)
                    {
                        entry = t * std::move(entry);
                    },
                    [&](Drawing::ShapeElement& shapeElement)
                    {
                        shapeElement.shape = t * std::move(shapeElement.shape);
                        if (shapeElement.pen)
                            shapeElement.pen = t * std::move(*shapeElement.pen);
                        if (shapeElement.brush)
                            shapeElement.brush = t * std::move(*shapeElement.brush);
                    },
                    [&](Drawing::ClipElement& clipElement)
                    {
                        clipElement.transform = t * clipElement.transform;
                    },
                    [&](Drawing::RegionFill& regionFill)
                    {
                        regionFill.region = t * std::move(regionFill.region);
                        regionFill.brush = t * std::move(regionFill.brush);
                    }
                    }, e);
    }

    drawing.controlBb_ = combineDrawingRects(drawing.elements_);

    return std::move(drawing);
}

} // namespace avg

