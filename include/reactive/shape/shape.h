#pragma once

#include <reactive/animate.h>

#include <reactive/widget/ondraw.h>
#include <reactive/widget/widget.h>

#include <reactive/signal/flipoptional.h>

#include <avg/animated.h>
#include <avg/brush.h>
#include <avg/drawcontext.h>
#include <avg/drawnode.h>
#include <avg/path.h>
#include <avg/pen.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/vector.h>

#include <type_traits>

namespace reactive::shape
{
    struct ShapeBuildTag {};

    namespace detail
    {
        template <typename T, typename TFunc>
        auto makeShapeUnchecked(Signal<T, TFunc>&& func);
    }

    template <typename T>
    class Shape;

    using AnyShape = Shape<AnySignal<avg::ShapeFunction>>;

    template <typename T, typename U, typename V>
    auto makeDrawModifier(
            Signal<T, avg::ShapeFunction> func,
            Signal<U, std::optional<avg::Brush>> brush,
            Signal<V, std::optional<avg::Pen>> pen
            )
    {
        return widget::onDraw([](avg::DrawContext const& context,
                        avg::Vector2f size,
                        auto func, 
                        std::optional<avg::Brush> const& brush,
                        std::optional<avg::Pen> const& pen)
                {
                    return func(context, size)
                        .fillAndStroke(
                                std::move(brush),
                                std::move(pen)
                                );
                },
                std::move(func),
                reactive::animate(std::move(brush)),
                reactive::animate(std::move(pen))
                );
    }

    template <typename T>
    class Shape
    {
    public:
        Shape(ShapeBuildTag const&, T func) :
            func_(std::move(func))
        {
        }

        template <typename U>
        auto stroke(Signal<U, avg::Pen> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        signal::constant(std::optional<avg::Brush>()),
                        std::move(pen).map([](auto p)
                            {
                                return std::make_optional(std::move(p));
                            }))
                ;
        }

        auto stroke(avg::Pen pen) &&
        {
            return stroke(signal::constant(std::move(pen)));
        }

        template <typename U>
        auto fill(Signal<U, avg::Brush> brush) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(func_.clone(),
                        std::move(brush).map([](auto b)
                            {
                                return std::make_optional(std::move(b));
                            }),
                        signal::constant(std::optional<avg::Pen>())
                        )
                ;
        }

        auto fill(avg::Brush brush) &&
        {
            return fill(std::move(brush));
        }

        auto fill(avg::Color const& color) &&
        {
            return fill(avg::Brush(color));
        }

        template <typename U>
        auto strokeToShape(Signal<T, avg::Pen> pen) && // -> Shape
        {
        }

        widget::AnyWidget fillAndStroke(std::optional<AnySignal<avg::Brush>> brush,
            std::optional<AnySignal<avg::Pen>> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        signal::flipOptional(std::move(brush)),
                        signal::flipOptional(std::move(pen)))
                ;
        }

        auto clip(Shape clipShape) // -> Shape
        {
        }

        operator AnyShape() const&
        {
            return AnyShape(ShapeBuildTag{}, func_);
        }

        operator AnyShape() &&
        {
            return AnyShape(ShapeBuildTag{}, std::move(func_));
        }

    private:
        T func_;
    };

    namespace detail
    {
        template <typename T>
        auto makeShapeUncheckedFromSignal(Signal<T, avg::ShapeFunction> func)
        {
            return Shape<Signal<T, avg::ShapeFunction>>(
                    ShapeBuildTag{},
                    std::move(func));
        }

        template <typename TFunc, typename... Ts>
        auto makeShapeUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeShapeUncheckedFromSignal(
                signal::group(std::forward<Ts>(ts)...).map(
                        [func=std::forward<TFunc>(func)](auto&&... values) mutable
                        {
                            return makeShapeFunction(
                                    func,
                                    std::forward<decltype(values)>(values)...
                                    );
                        })
                );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<
            avg::Shape,
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
