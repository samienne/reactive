#include "shape.h"

#include "obb.h"
#include "region.h"
#include "drawing.h"

namespace avg
{

namespace
{
    Shape::Operation withResource(Shape::Operation&& operation,
            pmr::memory_resource* memory);

    Shape::Element withResource(Shape::Element&& element,
            pmr::memory_resource* memory)
    {
        if (std::holds_alternative<Path>(element))
        {
            if (memory == std::get<Path>(element).getResource())
                return std::move(element);

            return std::move(std::get<Path>(element)).with_resource(memory);
        }
        else if (std::holds_alternative<Shape::Operation>(element))
        {
            return withResource(
                    std::move(std::get<Shape::Operation>(element)),
                    memory
                    );
        }
        else
        {
            assert(false);
        }

        return std::move(element);
    }

    Shape::SubElement withResource(
            Shape::SubElement&& subElement,
            pmr::memory_resource* memory)
    {
        return {
            subElement.transform,
            withResource(std::move(subElement.element), memory),
        };
    }

    Shape::Operation withResource(Shape::Operation&& operation,
            pmr::memory_resource* memory)
    {
        if (memory == operation.elements.get_allocator().resource())
            return std::move(operation);

        pmr::vector<Shape::SubElement> elements(memory);

        for(auto&& e : operation.elements)
        {
            elements.emplace_back(withResource(std::move(e), memory));
        }

        return { operation.type, std::move(elements) };
    }

    Shape withOperation(
            OperationType type,
            Shape::SubElement&& lhs,
            Shape::SubElement&& rhs,
            pmr::memory_resource* memory
            )
    {
        if (std::holds_alternative<Shape::Operation>(lhs.element))
        {
            auto&& op = std::get<Shape::Operation>(lhs.element);
            if (op.type == type)
            {
                rhs.transform = lhs.transform.inverse() * rhs.transform;

                auto newOp = withResource(std::move(op), memory);
                newOp.elements.push_back(withResource(std::move(rhs), memory));

                return Shape(memory, Shape::SubElement{
                        Transform(),
                        std::move(newOp)
                        });
            }
        }

        Shape::Operation op{
            OperationType::intersect,
            pmr::vector<Shape::SubElement>(
                    {
                        withResource(std::move(lhs), memory),
                        withResource(std::move(rhs), memory)
                    },
                    memory
                    )
        };

        return Shape(memory, Shape::SubElement{Transform(), std::move(op)});
    }

    Rect getControlBb(Transform transform, Shape::Element const& element);

    Rect getControlBb(Transform transform, Shape::SubElement const& subElement)
    {
        return getControlBb(transform * subElement.transform, subElement.element);
    }

    Rect getControlBb(Transform transform, Shape::Element const& element)
    {
        Rect result;

        if (std::holds_alternative<Path>(element))
        {
            return (transform * avg::Obb(std::get<Path>(element).getControlBb()))
                .getBoundingRect();
        }
        else if (std::holds_alternative<Shape::Operation>(element))
        {
            auto const& op = std::get<Shape::Operation>(element);
            switch (op.type)
            {
            case OperationType::add:
                for (auto const& e : op.elements)
                {
                    result += getControlBb(transform * e.transform, e);
                }

                break;

            case OperationType::intersect:
                for (auto const& e : op.elements)
                {
                    result = result.intersected(
                            getControlBb(transform * e.transform, e)
                            );
                }

                break;

            case OperationType::subtract:
                if (!op.elements.empty())
                    result = getControlBb(
                            transform * op.elements.front().transform,
                            op.elements.front()
                            );

                break;
            }
        }

        return result;
    }

    Region fillSubElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Shape::SubElement const& subElement);

    Region fillOperationToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Shape::Operation const& operation)
    {
        if (operation.elements.empty())
            return Region(memory);

        Region result = fillSubElementToRegion(memory, pointSize, transform,
                operation.elements.front());

        for (auto i = operation.elements.begin() + 1;
                i != operation.elements.end(); ++i)
        {
            switch (operation.type)
            {
            case OperationType::add:
                result |= fillSubElementToRegion(memory, pointSize, transform,
                        *i);
                break;
            case OperationType::intersect:
                result &= fillSubElementToRegion(memory, pointSize, transform,
                        *i);
                break;
            case OperationType::subtract:
                result -= fillSubElementToRegion(memory, pointSize, transform,
                        *i);
                break;
            }
        }

        return result;
    }

    Region fillElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Shape::Element const& element)
    {
        if (std::holds_alternative<Path>(element))
        {
            return (transform * std::get<Path>(element)).fillRegion(
                    memory,
                    FillRule::FILL_EVENODD,
                    pointSize
                    );
        }
        else if (std::holds_alternative<Shape::Operation>(element))
        {
            return fillOperationToRegion(memory, pointSize, transform,
                    std::get<Shape::Operation>(element));
        }

        assert(false && "Unknown sub element type");

        return Region(memory);
    }

    Region fillSubElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Shape::SubElement const& subElement)
    {
        return fillElementToRegion(memory, pointSize,
                transform *  subElement.transform, subElement.element);
    }

    Region strokeSubElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Pen const& pen,
            Shape::SubElement const& subElement);

    Region strokeOperationToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Pen const& pen,
            Shape::Operation operation)
    {
        if (operation.elements.empty())
            return Region(memory);

        Region result = strokeSubElementToRegion(memory, pointSize, transform,
                pen, operation.elements.front());

        for (auto i = operation.elements.begin() + 1;
                i != operation.elements.end(); ++i)
        {
            switch (operation.type)
            {
            case OperationType::add:
                result |= strokeSubElementToRegion(memory, pointSize,
                        transform, pen, *i);
                break;
            case OperationType::intersect:
                result &= strokeSubElementToRegion(memory, pointSize,
                        transform, pen, *i);
                break;
            case OperationType::subtract:
                result -= strokeSubElementToRegion(memory, pointSize,
                        transform, pen, *i);
                break;
            }
        }

        return result;
    }

    Region strokeElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Pen const& pen,
            Shape::Element const& element)
    {
        if (std::holds_alternative<Path>(element))
        {
            return (transform * std::get<Path>(element)).offsetRegion(
                    memory, pen.getJoinType(), pen.getEndType(),
                    pen.getWidth(), pointSize);
        }
        else if (std::holds_alternative<Shape::Operation>(element))
        {
            return strokeOperationToRegion(memory, pointSize, transform,
                    pen, std::get<Shape::Operation>(element));
        }

        assert(false && "Unknown sub element type");

        return Region(memory);
    }

    Region strokeSubElementToRegion(
            pmr::memory_resource* memory,
            Vector2f pointSize,
            Transform transform,
            Pen const& pen,
            Shape::SubElement const& subElement)
    {
        auto t = transform * subElement.transform;

        return strokeElementToRegion(memory, pointSize,
                t, t * pen, subElement.element);
    }

} // anonymous namespace

Shape::Shape(Path path) :
    memory_(path.getResource()),
    root_{Transform(), std::move(path)}
{
}

pmr::memory_resource* Shape::getResource() const
{
    return memory_;
}

Shape Shape::intersect(Shape&& rhs) &&
{
    return withOperation(
            OperationType::intersect,
            std::move(root_),
            std::move(rhs.root_),
            memory_
            );
}

Shape Shape::add(Shape&& rhs) &&
{
    return withOperation(
            OperationType::add,
            std::move(root_),
            std::move(rhs.root_),
            memory_
            );
}

Shape Shape::subtract(Shape&& rhs) &&
{
    return withOperation(
            OperationType::subtract,
            std::move(root_),
            std::move(rhs.root_),
            memory_
            );
}

Drawing Shape::stroke(Pen const& pen) &&
{
    return Drawing(memory_, Drawing::ShapeElement {
            std::move(*this), std::nullopt, pen });
}

Drawing Shape::fill(Brush const& brush) &&
{
    return Drawing(memory_, Drawing::ShapeElement { std::move(*this),
                brush, std::nullopt });
}

Drawing Shape::fillAndStroke(std::optional<Brush> const& brush,
        std::optional<Pen> const& pen) &&
{
    return Drawing(memory_, Drawing::ShapeElement { std::move(*this),
                brush, pen });
}

Shape Shape::transform(Transform t) &&
{
    return t * std::move(*this);
}

Rect Shape::getControlBb() const
{
    return avg::getControlBb(Transform(), root_);
}

Shape Shape::operator*(float scale) const &
{
    return Shape(
            memory_,
            Shape::SubElement{
                avg::scale(scale) * root_.transform,
                root_.element
                }
            );
}

Shape Shape::operator*(float scale) &&
{
    return Shape(
            memory_,
            Shape::SubElement{
                avg::scale(scale) * root_.transform,
                std::move(root_.element),
                }
            );
}

Shape& Shape::operator*=(float scale)
{
    root_.transform = avg::scale(scale) * root_.transform;
    return *this;
}

Shape& Shape::operator+=(Shape&& rhs)
{
    root_ = withOperation(
            OperationType::add,
            std::move(root_),
            std::move(rhs.root_),
            memory_
            ).root_;

    return *this;
}

Shape Shape::with_resource(pmr::memory_resource* memory) const&
{
    if (memory == memory_)
        return *this;

    return Shape(*this).with_resource(memory);
}

Shape Shape::with_resource(pmr::memory_resource* memory) &&
{
    if (memory == memory_)
        return std::move(*this);

    root_ = withResource(std::move(root_), memory);
    memory_ = memory;

    return std::move(*this);
}

Region Shape::strokeToRegion(pmr::memory_resource* memory,
        Pen const& pen, Vector2f pointSize) const
{
    return strokeSubElementToRegion(memory, pointSize, Transform(), pen, root_);
}

Region Shape::fillToRegion(pmr::memory_resource* memory,
                Vector2f pointSize) const
{
    return fillSubElementToRegion(memory, pointSize, Transform(), root_);
}

Shape operator*(Transform const& t, Shape const& rhs)
{
    return Shape(rhs.memory_,
            Shape::SubElement{
                t * rhs.root_.transform,
                rhs.root_.element
                }
            );
}

Shape::Shape(pmr::memory_resource* memory, SubElement&& root) :
    memory_(memory),
    root_(std::move(root))
{
}

} // namespace

