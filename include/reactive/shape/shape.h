#pragma once

#include "btl/bindarguments.h"
#include "reactive/animate.h"

#include <reactive/widget/ondraw.h>
#include <reactive/widget/widget.h>

#include <avg/vector.h>
#include <avg/brush.h>
#include <avg/drawcontext.h>
#include <avg/path.h>
#include <avg/pen.h>
#include <avg/shape.h>
#include <type_traits>

namespace reactive::shape
{
    struct ShapeBuildTag {};

    template <typename TFunc>
    class Shape;

    using AnyShape = Shape<std::function<avg::Path(avg::DrawContext const&, avg::Vector2f)>>;

    template <typename TFunc>
    class Shape
    {
    public:
        Shape(ShapeBuildTag const&, TFunc&& func) : func_(std::move(func))
        {
        }

        template <typename T>
        auto stroke(Signal<T, avg::Pen> pen) // -> Widget
        {
            return widget::makeWidget()
                | widget::onDraw(
                    [](avg::DrawContext const& context, avg::Vector2f size,
                        auto func, avg::Pen pen)
                    {
                        return context.drawing()
                            + avg::Shape(func(context, size), std::nullopt, std::move(pen))
                            ;
                    },
                    func_,
                    animate(std::move(pen))
                    )
                ;
        }

        auto stroke(avg::Pen pen)
        {
            return stroke(signal::constant(std::move(pen)));
        }

        template <typename T>
        auto fill(Signal<T, avg::Brush> brush) // -> Widget
        {
            return widget::makeWidget()
                | widget::onDraw(
                    [](avg::DrawContext const& context, avg::Vector2f size,
                        auto func, avg::Brush brush)
                    {
                        return context.drawing()
                            + avg::Shape(func(context, size), std::move(brush), std::nullopt)
                            ;
                    },
                    func_,
                    animate(std::move(brush))
                    )
                ;
        }

        auto fill(avg::Brush brush)
        {
            return fill(std::move(brush));
        }

        auto fill(avg::Color const& color)
        {
            return fill(avg::Brush(color));
        }

        template <typename T>
        auto strokeToShape(Signal<T, avg::Pen> pen) // -> Shape
        {
        }

        auto fillAndStroke(avg::Brush const& brush, avg::Pen const& pen) // -> Widget
        {
        }

        template <typename T>
        auto clip(Shape<T> shape) // -> Shape
        {
        }

    private:
        TFunc func_;
    };

    namespace detail
    {
        template <typename TFunc>
        auto makeShapeUnchecked(TFunc&& func)
        {
            return Shape<std::decay_t<TFunc>>(std::forward<TFunc>(
                        ShapeBuildTag{},
                        func
                        ));
        }

        template <typename TFunc, typename... Ts>
        auto makeShapeUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeShapeUnchecked(btl::bindArguments(
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        ));
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<
            avg::Path,
            TFunc,
            avg::DrawContext const&,
            avg::Vector2f,
            avg::AnimatedTypeT<std::decay_t<
                signal::SignalType<widget::ParamProviderTypeT<std::decay_t<Ts>>>
            >>...
        >>>
    auto makeShape(TFunc&& func, Ts&&... ts)
    {
        return detail::makeShapeUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }
} // namespace reactive::shape
