#include "bqui/shape/shape.h"

#include <avg/brush.h>
#include <avg/drawing.h>
#include <avg/pen.h>
#include <avg/shape.h>

#include <optional>
#include <utility>

namespace bqui::shape::detail
{

namespace
{
    bool isOpaque(std::optional<avg::Brush> const& brush)
    {
        return brush && !brush->getEffect()
            && brush->getColor().getAlpha() >= 1.0f;
    }

    std::optional<avg::Shape> makeOutline(avg::Shape const& shape,
            std::optional<avg::Brush> const& brush,
            std::optional<avg::Pen> const& pen)
    {
        if (isOpaque(brush))
        {
            if (!pen)
                return shape;

            return avg::Shape(shape).add(avg::Shape(shape).strokeToShape(*pen));
        }

        if (pen && isOpaque(pen->getBrush()))
            return avg::Shape(shape).strokeToShape(*pen);

        return std::nullopt;
    }
} // anonymous namespace

avg::Drawing drawShape(avg::Shape shape,
        std::optional<avg::Brush> const& brush,
        std::optional<avg::Pen> const& pen)
{
    auto outline = makeOutline(shape, brush, pen);
    auto drawing = std::move(shape).fillAndStroke(brush, pen);

    if (outline)
        drawing.addOutline(std::move(*outline));

    return drawing;
}

} // namespace bqui::shape::detail
