#include "drawing.h"

#include "debug.h"
#include "effect.h"

#include <pmr/new_delete_resource.h>
#include <pmr/heap.h>

#include <stdexcept>
#include <string>
#include <utility>

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
                    },
                    [](Drawing::EffectElement const& effect) -> Rect
                    {
                        return (effect.transform *
                                avg::Obb(effect.controlBb)).getBoundingRect();
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
                    },
                    [](Drawing::EffectElement const& effect)
                        -> pmr::memory_resource*
                    {
                        return effect.inputs.get_allocator().resource();
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
                    },
                    [&](Drawing::EffectElement const& effect)
                        -> Drawing::Element
                    {
                        pmr::vector<Drawing::EffectInput> inputs(memory);
                        inputs.reserve(effect.inputs.size());
                        for (auto const& input : effect.inputs)
                        {
                            Drawing::EffectInput copy{ std::nullopt,
                                input.controlBb };
                            if (input.subDrawing)
                            {
                                copy.subDrawing.emplace(withResource(memory,
                                            **input.subDrawing), memory);
                            }
                            inputs.push_back(std::move(copy));
                        }

                        std::optional<Shape> shape;
                        if (effect.shape)
                            shape = effect.shape->with_resource(memory);

                        return Drawing::EffectElement {
                            std::move(inputs),
                            effect.pass,
                            std::move(shape),
                            effect.transform,
                            effect.controlBb
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
                },
                [&](Drawing::EffectElement& effect)
                {
                    effect.transform = avg::scale(factor) * effect.transform;
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
                },
                [&](Drawing::EffectElement& effect)
                {
                    effect.transform = t * effect.transform;
                }},
                e);
    }
} // anonymous namespace

Drawing::Drawing(pmr::memory_resource* memory) :
    elements_(memory),
    outlines_(memory)
{
}

Drawing::Drawing(pmr::memory_resource* memory, Element element) :
    elements_(memory),
    outlines_(memory)
{
    controlBb_ = combineRects(controlBb_, getElementRect(element));

    elements_.push_back(std::move(element));
}

Drawing::Drawing(pmr::vector<Element> const& elements) :
    elements_(elements),
    outlines_(elements_.get_allocator().resource()),
    controlBb_(combineDrawingRects(elements_))
{
}

Drawing::Drawing(pmr::vector<Element>&& elements) :
    elements_(std::move(elements)),
    outlines_(elements_.get_allocator().resource()),
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
    if (&drawing == this)
        return *this += Drawing(drawing);

    controlBb_ = combineRects(controlBb_, drawing.controlBb_);

    appendElements(elements_, drawing.elements_);
    outlines_ += drawing.outlines_;

    return *this;
}

Drawing Drawing::operator+(Drawing&& drawing) &&
{
    return std::move(*this += std::move(drawing));
}

Drawing& Drawing::operator+=(Drawing&& drawing)
{
    if (&drawing == this)
        return *this += Drawing(drawing);

    controlBb_ = combineRects(controlBb_, drawing.controlBb_);

    appendElements(elements_, std::move(drawing.elements_));
    outlines_ += std::move(drawing.outlines_);

    return *this;
}

Drawing Drawing::scope() &&
{
    if (elements_.empty())
        return std::move(*this);

    Rect bb = combinePaintedRects(elements_);
    Rect controlBb = controlBb_;

    Drawing result(getResource());
    auto subDrawing = result.liftSubDrawing(std::move(*this), Transform());
    result.elements_.push_back(BackdropScope{
            std::move(subDrawing),
            bb,
            avg::Transform()
            });
    result.controlBb_ = controlBb;

    return result;
}

Drawing Drawing::effect(pmr::memory_resource* memory, ElementPass pass,
        std::vector<std::optional<Drawing>> inputs, std::optional<Shape> shape)
{
    if (inputs.size() != pass.getInputCount())
    {
        throw std::invalid_argument("an effect element with "
                + std::to_string(inputs.size()) + " inputs for a pass with "
                + std::to_string(pass.getInputCount()));
    }

    Drawing result(memory);
    pmr::vector<EffectInput> elementInputs(memory);
    Rect inputBb;
    bool bounded = shape.has_value();

    for (auto& input : inputs)
    {
        if (!input)
        {
            elementInputs.push_back(EffectInput{ std::nullopt, Rect() });
            continue;
        }

        Rect const bb = combinePaintedRects(input->elements_);
        inputBb = combineRects(inputBb, bb);
        bounded = true;

        elementInputs.push_back(EffectInput{
                result.liftSubDrawing(std::move(*input), Transform()),
                bb
                });
    }

    if (!bounded)
    {
        throw std::invalid_argument("an effect element without a shape needs"
                " a sub-drawing input to bound it");
    }

    Vector2f const reach = (pass.getReach()
            * toInt(pass.getPass().output->divisor)).cast<float>();

    Rect controlBb;
    if (shape)
        controlBb = shape->getControlBb();
    else if (!inputBb.isEmpty())
    {
        controlBb = Rect(inputBb.getBottomLeft() - reach,
                inputBb.getSize() + 2.0f * reach);
    }

    result.elements_.push_back(EffectElement{
            std::move(elementInputs),
            std::move(pass),
            std::move(shape),
            Transform(),
            controlBb
            });
    result.controlBb_ = combineDrawingRects(result.elements_);

    return result;
}

Drawing Drawing::over(Drawing below, Drawing above)
{
    below.controlBb_ = combineRects(below.controlBb_, above.controlBb_);

    Drawing scoped = std::move(above).scope();
    appendElements(below.elements_, std::move(scoped.elements_));
    below.outlines_ += std::move(scoped.outlines_);

    return below;
}

Drawing Drawing::operator*(float scale) &&
{
    controlBb_ = controlBb_.scaled(scale);

    for (auto& e : elements_)
        scaleElement(scale, e);

    outlines_ = std::move(outlines_) * scale;

    return std::move(*this);
}

Drawing Drawing::clip(Rect const& r) &&
{
    outlines_ = std::move(outlines_).clip(r);

    if (controlBb_.isFullyContainedIn(r))
        return std::move(*this);

    Drawing result(getResource());

    result.elements_ = clipElements(std::move(elements_), r);
    result.outlines_ = std::move(outlines_);
    result.controlBb_ = r;

    return result;
}

Drawing Drawing::clip(Obb const& obb) &&
{
    auto outlines = takeOutlines().clip(obb);

    auto obbTInverse = obb.getTransform().inverse();
    auto obbRect = Rect(Vector2f(0.0f, 0.0f), obb.getSize());

    if ((obbTInverse * Obb(controlBb_))
            .getBoundingRect()
            .isFullyContainedIn(obbRect))
    {
        outlines_ = std::move(outlines);
        return std::move(*this);
    }

    auto d = obb.getTransform() * (obbTInverse * std::move(*this))
        .clip(obbRect);
    d.outlines_ = std::move(outlines);

    return d;
}

pmr::vector<Drawing::Element> const& Drawing::getElements() const
{
    return elements_;
}

Outlines const& Drawing::getOutlines() const
{
    return outlines_;
}

Drawing Drawing::setOutlines(Outlines outlines) &&
{
    return std::move(setOutlines(std::move(outlines)));
}

Drawing& Drawing::setOutlines(Outlines outlines) &
{
    outlines_ = std::move(outlines);
    return *this;
}

Drawing Drawing::addOutline(Shape shape) &&
{
    return std::move(addOutline(std::move(shape)));
}

Drawing& Drawing::addOutline(Shape shape) &
{
    outlines_ += std::move(shape);
    return *this;
}

Drawing Drawing::clearOutlines() &&
{
    return std::move(clearOutlines());
}

Drawing& Drawing::clearOutlines() &
{
    takeOutlines();
    return *this;
}

Outlines Drawing::takeOutlines()
{
    return std::exchange(outlines_, Outlines(getResource()));
}

pmr::heap<Drawing::SubDrawing> Drawing::liftSubDrawing(Drawing&& inner,
        Transform const& transform)
{
    if (transform != Transform())
        outlines_ += std::move(inner.outlines_).transform(transform);
    else
        outlines_ += std::move(inner.outlines_);

    SubDrawing subDrawing{ std::move(inner.elements_) };

    if (subDrawing.elements.get_allocator().resource() != getResource())
        subDrawing = withResource(getResource(), subDrawing);

    return pmr::make_heap<SubDrawing>(getResource(), std::move(subDrawing));
}

Rect Drawing::getControlBb() const
{
    return controlBb_;
}

Drawing Drawing::filterByRect(Rect const& r) &&
{
    filterElementsByRect(elements_, r);
    outlines_ = std::move(outlines_).filterByRect(r);

    controlBb_ = combineDrawingRects(elements_);

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

    drawing.outlines_ = std::move(drawing.outlines_).transform(t);

    drawing.controlBb_ = combineDrawingRects(drawing.elements_);

    return std::move(drawing);
}

} // namespace avg
