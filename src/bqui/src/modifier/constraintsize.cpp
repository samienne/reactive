#include "bqui/modifier/constraintsize.h"

#include "pureconstraint.h"

#include "widget/constraintlayout.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <arrange/strength.h>

#include <utility>

namespace bqui::modifier
{

namespace
{
    using detail::PureAxis;

    // A fixed size is a strong preference: above the weakest default, and at the
    // same strength as the min/max bounds, so it ties rather than loses to one.
    arrange::Strength fixedStrength()
    {
        return arrange::Strength::strong();
    }

    // The both-axes counterpart of a single-axis band word: apply @p make on
    // each axis, splitting @p size's two components.
    template <typename Make>
    AnyWidgetModifier bothAxes(bq::signal::AnySignal<avg::Vector2f> size,
            Make make)
    {
        auto shared = std::move(size).share();
        auto width = shared.clone().map(
                [](avg::Vector2f s) { return s.x(); });
        auto height = shared.clone().map(
                [](avg::Vector2f s) { return s.y(); });

        return detail::composeModifiers(
                make(PureAxis::horizontal, std::move(width)),
                make(PureAxis::vertical, std::move(height)));
    }
} // namespace

AnyWidgetModifier fixedWidth(bq::signal::AnySignal<float> width)
{
    return detail::pureNaturalModifier(PureAxis::horizontal, fixedStrength(),
            std::move(width));
}

AnyWidgetModifier fixedWidth(float width)
{
    return fixedWidth(bq::signal::constant(width));
}

AnyWidgetModifier fixedHeight(bq::signal::AnySignal<float> height)
{
    return detail::pureNaturalModifier(PureAxis::vertical, fixedStrength(),
            std::move(height));
}

AnyWidgetModifier fixedHeight(float height)
{
    return fixedHeight(bq::signal::constant(height));
}

AnyWidgetModifier fixedSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return bothAxes(std::move(size),
            [](PureAxis axis, bq::signal::AnySignal<float> value)
            {
                return detail::pureNaturalModifier(axis, fixedStrength(),
                        std::move(value));
            });
}

AnyWidgetModifier fixedSize(avg::Vector2f size)
{
    return fixedSize(bq::signal::constant(size));
}

AnyWidgetModifier minWidth(bq::signal::AnySignal<float> width)
{
    return detail::pureMinModifier(PureAxis::horizontal, std::move(width));
}

AnyWidgetModifier minWidth(float width)
{
    return minWidth(bq::signal::constant(width));
}

AnyWidgetModifier minHeight(bq::signal::AnySignal<float> height)
{
    return detail::pureMinModifier(PureAxis::vertical, std::move(height));
}

AnyWidgetModifier minHeight(float height)
{
    return minHeight(bq::signal::constant(height));
}

AnyWidgetModifier minSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return bothAxes(std::move(size), &detail::pureMinModifier);
}

AnyWidgetModifier minSize(avg::Vector2f size)
{
    return minSize(bq::signal::constant(size));
}

AnyWidgetModifier maxWidth(bq::signal::AnySignal<float> width)
{
    return detail::pureMaxModifier(PureAxis::horizontal, std::move(width));
}

AnyWidgetModifier maxWidth(float width)
{
    return maxWidth(bq::signal::constant(width));
}

AnyWidgetModifier maxHeight(bq::signal::AnySignal<float> height)
{
    return detail::pureMaxModifier(PureAxis::vertical, std::move(height));
}

AnyWidgetModifier maxHeight(float height)
{
    return maxHeight(bq::signal::constant(height));
}

AnyWidgetModifier maxSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return bothAxes(std::move(size), &detail::pureMaxModifier);
}

AnyWidgetModifier maxSize(avg::Vector2f size)
{
    return maxSize(bq::signal::constant(size));
}

AnyWidgetModifier defaultSize()
{
    return detail::pureContentDefaultModifier();
}

AnyWidgetModifier defaultSize(avg::Vector2f size)
{
    return bothAxes(bq::signal::constant(size),
            [](PureAxis axis, bq::signal::AnySignal<float> value)
            {
                return detail::pureNaturalModifier(axis,
                        widget::contentStrength(), std::move(value));
            });
}

AnyWidgetModifier fill()
{
    return grow(1.0f);
}

AnyWidgetModifier grow(float weight)
{
    return detail::pureFillModifier(weight);
}

} // namespace bqui::modifier
