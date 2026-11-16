#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/shapefunction.h>

#include <optional>
#include <utility>

namespace bqui::shape
{
    template <typename T>
    class Shape;
}

namespace bqui::modifier
{
    /**
     * @brief Replaces the widget's outline with the shape function's
     * value in the widget's size; std::nullopt leaves the widget none.
     */
    BQUI_EXPORT AnyWidgetModifier outline(
            bq::signal::AnySignal<std::optional<avg::ShapeFunction>> outline);

    /**
     * @brief Adds the shape function's value in the widget's size to the
     * widget's outline.
     */
    BQUI_EXPORT AnyWidgetModifier addOutline(
            bq::signal::AnySignal<avg::ShapeFunction> outline);

    /**
     * @brief Removes the widget's outline.
     *
     * @code
     * shape::rectangle().fill(white) | noOutline()
     * @endcode
     */
    BQUI_EXPORT AnyWidgetModifier noOutline();

    /**
     * @brief Replaces the widget's outline with the shape in the widget's
     * size.
     *
     * @code
     * label("Hi") | outline(shape::circle())
     * @endcode
     */
    template <typename T>
    AnyWidgetModifier outline(shape::Shape<T> shape)
    {
        return outline(std::move(shape).getShapeFunction().map(
                    [](avg::ShapeFunction func)
                    {
                        return std::make_optional(std::move(func));
                    }));
    }

    /**
     * @brief Adds the shape in the widget's size to the widget's outline.
     *
     * @code
     * shape::circle().fill(white) | addOutline(shape::rectangle())
     * @endcode
     */
    template <typename T>
    AnyWidgetModifier addOutline(shape::Shape<T> shape)
    {
        return addOutline(bq::signal::AnySignal<avg::ShapeFunction>(
                    std::move(shape).getShapeFunction()));
    }
}
