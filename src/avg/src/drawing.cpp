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
                    },
                    [](Drawing::BackdropScope const& scope) -> Rect
                    {
                        return (scope.transform *
                                avg::Obb(scope.controlBb)).getBoundingRect();
                    }
                    },
                e
                );
    }

    Rect getPaintedRect(Drawing::Element const& e)
    {
        Rect r = getElementRect(e);

        auto const* shapeElement = std::get_if<Drawing::ShapeElement>(&e);
        if (shapeElement && shapeElement->pen)
            r = r.enlarged(shapeElement->pen->getWidth() / 2.0f);

        return r;
    }

    Rect combinePaintedRects(pmr::vector<Drawing::Element> const& elements)
    {
        Rect r;
        for (auto const& e : elements)
            r = combineRects(r, getPaintedRect(e));

        return r;
    }

    Rect combineDrawingRects(pmr::vector<Drawing::Element> const& elements)
    {
        Rect r;
        for (auto const& e : elements)
            r = combineRects(r, getElementRect(e));

        return r;
    }

    Rect combineDrawingRects(
            pmr::vector<Drawing::Element> const& elements,
            pmr::vector<Drawing::Element> const& shadows
            )
    {
        return combineRects(
                combineDrawingRects(elements),
                combineDrawingRects(shadows)
                );
    }

    void filterElementsByRect(pmr::vector<Drawing::Element>& elements, Rect const& r)
    {
        elements.erase(
                std::remove_if(
                    elements.begin(),
                    elements.end(),
                    [&r](Drawing::Element const& e)
                    {
                        return !getPaintedRect(e).overlaps(r);
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
                    },
                    [](Drawing::BackdropScope const& scope) -> pmr::memory_resource*
                    {
                        return scope.subDrawing.resource();
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
                    },
                    [&](Drawing::BackdropScope const& scope) -> Drawing::Element
                    {
                        return Drawing::BackdropScope {
                            pmr::heap<Drawing::SubDrawing>(
                                    withResource(memory, *scope.subDrawing),
                                    memory
                                    ),
                            scope.controlBb,
                            scope.transform
                        };
                    }},
                e);
    }

    void appendElement(
            pmr::vector<Drawing::Element>& elements,
            Drawing::Element&& element
            )
    {
        auto* memory = elements.get_allocator().resource();

        if (memory == getElementResource(element))
            elements.push_back(std::move(element));
        else
            elements.push_back(withResource(memory, element));
    }

    void appendElements(
            pmr::vector<Drawing::Element>& elements,
            pmr::vector<Drawing::Element> const& source
            )
    {
        auto* memory = elements.get_allocator().resource();

        elements.reserve(elements.size() + source.size());

        if (memory == source.get_allocator().resource())
        {
            for (auto const& element : source)
                elements.push_back(element);
        }
        else
        {
            for (auto const& element : source)
                elements.push_back(withResource(memory, element));
        }
    }

    void appendElements(
            pmr::vector<Drawing::Element>& elements,
            pmr::vector<Drawing::Element>&& source
            )
    {
        elements.reserve(elements.size() + source.size());

        for (auto&& element : source)
            appendElement(elements, std::move(element));

        source.clear();
    }

    pmr::vector<Drawing::Element> clipElements(
            pmr::vector<Drawing::Element>&& elements,
            Rect const& r
            )
    {
        auto* memory = elements.get_allocator().resource();
        pmr::vector<Drawing::Element> result(memory);

        filterElementsByRect(elements, r);

        if (elements.empty())
            return result;

        result.push_back(Drawing::ClipElement{
                pmr::make_heap<Drawing::SubDrawing>(memory, std::move(elements)),
                r,
                avg::Transform()
                });

        return result;
    }

    void scaleElement(float factor, Drawing::Element& e)
    {
        std::visit(Overloaded{
                [&](Drawing::ShapeElement& shapeElement)
                {
                    shapeElement.shape = std::move(shapeElement.shape) * factor;
                    if (shapeElement.pen)
                        shapeElement.pen = *shapeElement.pen * factor;

                    if (shapeElement.brush)
                        shapeElement.brush = *shapeElement.brush * factor;
                },
                [&](TextEntry& textEntry)
                {
                    textEntry = std::move(textEntry) * factor;
                },
                [&](Drawing::ClipElement& /*clipElement*/)
                {
                    assert(false);
                },
                [&](Drawing::RegionFill& /*regionFill*/)
                {
                    assert(false);
                },
                [&](Drawing::BackdropScope& scope)
                {
                    scope.transform = avg::scale(factor) * scope.transform;
                }},
                e);
    }

    void transformElement(Transform const& t, Drawing::Element& e)
    {
        std::visit(Overloaded{
                [&](Drawing::ShapeElement& shapeElement)
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
                },
                [&](Drawing::BackdropScope& scope)
                {
                    scope.transform = t * scope.transform;
                }},
                e);
    }
} // anonymous namespace

Drawing::Drawing(pmr::memory_resource* memory) :
    elements_(memory),
    shadows_(memory)
{
}

Drawing::Drawing(pmr::memory_resource* memory, Element element) :
    elements_(memory),
    shadows_(memory)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    elements_.push_back(std::move(element));
}

Drawing::Drawing(pmr::vector<Element> const& elements) :
    elements_(elements),
    shadows_(elements_.get_allocator().resource()),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::Drawing(pmr::vector<Element>&& elements) :
    elements_(std::move(elements)),
    shadows_(elements_.get_allocator().resource()),
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
    return std::move(*this += std::move(element));
}

Drawing& Drawing::operator+=(Element&& element)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    appendElement(elements_, std::move(element));

    return *this;
}

Drawing Drawing::operator+(Drawing const& drawing) &&
{
    return std::move(*this += drawing);
}

Drawing& Drawing::operator+=(Drawing const& drawing)
{
    controlBb_ = combineRects(controlBb_, drawing.controlBb_);

    appendElements(elements_, drawing.elements_);
    appendElements(shadows_, drawing.shadows_);

    return *this;
}

Drawing Drawing::addShadow(Element&& shadow) &&
{
    return std::move(addShadow(std::move(shadow)));
}

Drawing& Drawing::addShadow(Element&& shadow) &
{
    controlBb_ = combineRects(controlBb_, getElementRect(shadow));

    appendElement(shadows_, std::move(shadow));

    return *this;
}

Drawing Drawing::flush() &&
{
    if (shadows_.empty())
        return std::move(*this);

    appendElements(shadows_, std::move(elements_));
    elements_ = std::move(shadows_);
    shadows_ = pmr::vector<Element>(getResource());

    return std::move(*this);
}

Drawing Drawing::scope() &&
{
    if (elements_.empty())
        return std::move(*this);

    Rect bb = combinePaintedRects(elements_);

    Drawing result(getResource());
    result.elements_.push_back(BackdropScope{
            pmr::make_heap<SubDrawing>(getResource(), std::move(elements_)),
            bb,
            avg::Transform()
            });
    result.shadows_ = std::move(shadows_);
    result.controlBb_ = controlBb_;

    return result;
}

Drawing Drawing::over(Drawing below, Drawing above)
{
    below.controlBb_ = combineRects(below.controlBb_, above.controlBb_);

    appendElements(below.elements_, std::move(above.shadows_));

    Drawing scoped = std::move(above).scope();
    appendElements(below.elements_, std::move(scoped.elements_));

    return below;
}

Drawing Drawing::operator*(float scale) &&
{
    controlBb_ = controlBb_.scaled(scale);

    for (auto& e : elements_)
        scaleElement(scale, e);

    for (auto& e : shadows_)
        scaleElement(scale, e);

    return std::move(*this);
}

Drawing Drawing::clip(Rect const& r) &&
{
    if (controlBb_.isFullyContainedIn(r))
        return std::move(*this);

    Drawing result(getResource());

    result.elements_ = clipElements(std::move(elements_), r);
    result.shadows_ = clipElements(std::move(shadows_), r);
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

pmr::vector<Drawing::Element> const& Drawing::getPendingShadows() const
{
    return shadows_;
}

Rect Drawing::getControlBb() const
{
    return controlBb_;
}

Drawing Drawing::filterByRect(Rect const& r) &&
{
    filterElementsByRect(elements_, r);
    filterElementsByRect(shadows_, r);

    controlBb_ = combineDrawingRects(elements_, shadows_);

    return std::move(*this);
}

Drawing Drawing::transform(Transform const& t) &&
{
    return t * std::move(*this);
}

Drawing Drawing::translate(ase::Vector2f offset) &&
{
    return std::move(*this).transform(avg::translate(offset));
}

Drawing operator*(Transform const& t, Drawing&& drawing)
{
    for (auto&& e : drawing.elements_)
        transformElement(t, e);

    for (auto&& e : drawing.shadows_)
        transformElement(t, e);

    drawing.controlBb_ = combineDrawingRects(drawing.elements_,
            drawing.shadows_);

    return std::move(drawing);
}

} // namespace avg
