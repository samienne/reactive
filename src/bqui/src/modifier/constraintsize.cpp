#include "bqui/modifier/constraintsize.h"

#include "pureconstraint.h"

#include "bqui/widget/boxvariables.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/strength.h>

#include <utility>
#include <vector>

namespace bqui::modifier
{

namespace
{
    // The pure-solver constraint each size word names on the strength hierarchy a
    // pure region solves on: a fixed extent is a strong equality (above the weak
    // 100 default, below a required bound), a min or max a required inequality.
    std::vector<arrange::Constraint> equalConstraint(arrange::Expression extent,
            float value)
    {
        return {
            (std::move(extent) == arrange::Expression(static_cast<double>(value)))
            | arrange::Strength::strong()
        };
    }

    std::vector<arrange::Constraint> atLeastConstraint(arrange::Expression extent,
            float value)
    {
        return {
            std::move(extent) >= arrange::Expression(static_cast<double>(value))
        };
    }

    std::vector<arrange::Constraint> atMostConstraint(arrange::Expression extent,
            float value)
    {
        return {
            std::move(extent) <= arrange::Expression(static_cast<double>(value))
        };
    }

    // A pure-solver leaf constraint on one axis, built from the box's own extent
    // on that axis and one of the size-word constraint shapes above.
    template <typename TConstraint>
    AnyWidgetModifier pureWidth(bq::signal::AnySignal<float> value,
            TConstraint constraint)
    {
        return detail::pureConstraintModifier(detail::PureAxis::horizontal,
                std::move(value),
                [constraint](widget::BoxVariables const& box, float v)
                {
                    return constraint(box.width(), v);
                });
    }

    template <typename TConstraint>
    AnyWidgetModifier pureHeight(bq::signal::AnySignal<float> value,
            TConstraint constraint)
    {
        return detail::pureConstraintModifier(detail::PureAxis::vertical,
                std::move(value),
                [constraint](widget::BoxVariables const& box, float v)
                {
                    return constraint(box.height(), v);
                });
    }

    // The both-axes counterpart: a pure-solver constraint of shape @p constraint
    // on each axis, split off @p size's two components.
    template <typename TConstraint>
    AnyWidgetModifier pureSize(bq::signal::AnySignal<avg::Vector2f> size,
            TConstraint constraint)
    {
        auto shared = std::move(size).share();
        auto width = shared.clone().map(
                [](avg::Vector2f s) { return s.x(); });
        auto height = shared.clone().map(
                [](avg::Vector2f s) { return s.y(); });

        return detail::composeModifiers(
                pureWidth(std::move(width), constraint),
                pureHeight(std::move(height), constraint));
    }

    // Fill pulls a child's extent toward this large value, so it grows to
    // whatever room its container leaves. The value only has to exceed any
    // extent a real layout reaches; the container's own weak fill caps where the
    // child actually settles.
    constexpr double fillExtent = 1.0e6;

    // Fill sits above the weak 100 default so a filled child grows in preference
    // to an untagged one, and below the container's weak fill so it never pushes
    // past the room the container leaves.
    arrange::Strength fillStrength()
    {
        return arrange::Strength::weak(0.5);
    }
} // namespace

AnyWidgetModifier fixedWidth(bq::signal::AnySignal<float> width)
{
    return pureWidth(std::move(width), equalConstraint);
}

AnyWidgetModifier fixedWidth(float width)
{
    return fixedWidth(bq::signal::constant(width));
}

AnyWidgetModifier fixedHeight(bq::signal::AnySignal<float> height)
{
    return pureHeight(std::move(height), equalConstraint);
}

AnyWidgetModifier fixedHeight(float height)
{
    return fixedHeight(bq::signal::constant(height));
}

AnyWidgetModifier fixedSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return pureSize(std::move(size), equalConstraint);
}

AnyWidgetModifier fixedSize(avg::Vector2f size)
{
    return fixedSize(bq::signal::constant(size));
}

AnyWidgetModifier minWidth(bq::signal::AnySignal<float> width)
{
    return pureWidth(std::move(width), atLeastConstraint);
}

AnyWidgetModifier minWidth(float width)
{
    return minWidth(bq::signal::constant(width));
}

AnyWidgetModifier minHeight(bq::signal::AnySignal<float> height)
{
    return pureHeight(std::move(height), atLeastConstraint);
}

AnyWidgetModifier minHeight(float height)
{
    return minHeight(bq::signal::constant(height));
}

AnyWidgetModifier minSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return pureSize(std::move(size), atLeastConstraint);
}

AnyWidgetModifier minSize(avg::Vector2f size)
{
    return minSize(bq::signal::constant(size));
}

AnyWidgetModifier maxWidth(bq::signal::AnySignal<float> width)
{
    return pureWidth(std::move(width), atMostConstraint);
}

AnyWidgetModifier maxWidth(float width)
{
    return maxWidth(bq::signal::constant(width));
}

AnyWidgetModifier maxHeight(bq::signal::AnySignal<float> height)
{
    return pureHeight(std::move(height), atMostConstraint);
}

AnyWidgetModifier maxHeight(float height)
{
    return maxHeight(bq::signal::constant(height));
}

AnyWidgetModifier maxSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return pureSize(std::move(size), atMostConstraint);
}

AnyWidgetModifier maxSize(avg::Vector2f size)
{
    return maxSize(bq::signal::constant(size));
}

AnyWidgetModifier fillWidth()
{
    return detail::pureConstraintModifier(detail::PureAxis::horizontal,
            bq::signal::constant(0.0f),
            [](widget::BoxVariables const& box, float)
            {
                return std::vector<arrange::Constraint>{
                    (box.width() >= arrange::Expression(fillExtent))
                    | fillStrength()
                };
            });
}

AnyWidgetModifier fillHeight()
{
    return detail::pureConstraintModifier(detail::PureAxis::vertical,
            bq::signal::constant(0.0f),
            [](widget::BoxVariables const& box, float)
            {
                return std::vector<arrange::Constraint>{
                    (box.height() >= arrange::Expression(fillExtent))
                    | fillStrength()
                };
            });
}

} // namespace bqui::modifier
