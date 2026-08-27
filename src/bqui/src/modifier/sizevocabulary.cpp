#include "bqui/modifier/sizevocabulary.h"

#include "pureconstraint.h"

#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

#include "bqui/widget/boxvariables.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <arrange/constraint.h>
#include <arrange/expression.h>
#include <arrange/strength.h>

#include <algorithm>
#include <vector>

namespace bqui::modifier
{

namespace
{
    // Each transform tightens or targets one edge of the {min, natural, max}
    // band and then clamps the other two only as far as needed to keep
    // min <= natural <= max, so every call leaves the band coherent and the
    // add-only ones stay order-independent. The grow weight is carried through
    // untouched; only growWeight sets it.

    Band bandAtLeast(Band band, float value)
    {
        float min = std::max(band.min, value);
        float natural = std::max(band.natural, min);
        float max = std::max(band.max, natural);

        return Band{ min, natural, max, band.grow };
    }

    Band bandAtMost(Band band, float value)
    {
        float max = std::min(band.max, value);
        float natural = std::min(band.natural, max);
        float min = std::min(band.min, natural);

        return Band{ min, natural, max, band.grow };
    }

    Band bandExactly(Band band, float value)
    {
        return Band{ value, value, value, band.grow };
    }

    Band bandPrefer(Band band, float value)
    {
        float natural = std::min(band.max, std::max(band.min, value));

        return Band{ band.min, natural, band.max, band.grow };
    }

    Band bandGrow(Band band, float weight)
    {
        return Band{ band.min, band.natural, band.max, weight };
    }

    template <typename TBand>
    AxisHint applyBand(AxisHint hint, float value, TBand band)
    {
        return AxisHint{ band(hint.extent, value), hint.anchors };
    }

    // A width modifier rewrites the width band (getWidth and getWidthForHeight)
    // and leaves the height band untouched; a height modifier does the reverse.

    template <typename TBand>
    auto widthImpl(SizeHint sizeHint, float value, TBand band)
    {
        return mapSizeHint(std::move(sizeHint),
                [value, band](AxisHint hint) -> AxisHint
                {
                    return applyBand(hint, value, band);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value, band);
                }
                );
    }

    template <typename TBand>
    auto heightImpl(SizeHint sizeHint, float value, TBand band)
    {
        return mapSizeHint(std::move(sizeHint),
                [](AxisHint hint) -> AxisHint
                {
                    return hint;
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value, band);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                }
                );
    }

    template <typename TBand>
    auto sizeImpl(SizeHint sizeHint, avg::Vector2f value, TBand band)
    {
        return mapSizeHint(std::move(sizeHint),
                [value, band](AxisHint hint) -> AxisHint
                {
                    return applyBand(hint, value.x(), band);
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value.y(), band);
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value.x(), band);
                }
                );
    }

    // A grow weight is a property of the widget, not of an axis: it fills in
    // whichever direction its container distributes, so it is written onto both
    // bands.
    template <typename TBand>
    auto growImpl(SizeHint sizeHint, float value, TBand band)
    {
        return mapSizeHint(std::move(sizeHint),
                [value, band](AxisHint hint) -> AxisHint
                {
                    return applyBand(hint, value, band);
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value, band);
                },
                [value, band](AxisHint hint, float) -> AxisHint
                {
                    return applyBand(hint, value, band);
                }
                );
    }

    template <typename TBand>
    AnyWidgetModifier widthModifier(bq::signal::AnySignal<float> width, TBand band)
    {
        return mapSizeHint(
                [band](SizeHint hint, float value)
                {
                    return widthImpl(std::move(hint), value, band);
                },
                std::move(width));
    }

    template <typename TBand>
    AnyWidgetModifier heightModifier(bq::signal::AnySignal<float> height,
            TBand band)
    {
        return mapSizeHint(
                [band](SizeHint hint, float value)
                {
                    return heightImpl(std::move(hint), value, band);
                },
                std::move(height));
    }

    template <typename TBand>
    AnyWidgetModifier sizeModifier(bq::signal::AnySignal<avg::Vector2f> size,
            TBand band)
    {
        return mapSizeHint(
                [band](SizeHint hint, avg::Vector2f value)
                {
                    return sizeImpl(std::move(hint), value, band);
                },
                std::move(size));
    }

    template <typename TBand>
    AnyWidgetModifier growModifier(bq::signal::AnySignal<float> weight, TBand band)
    {
        return mapSizeHint(
                [band](SizeHint hint, float value)
                {
                    return growImpl(std::move(hint), value, band);
                },
                std::move(weight));
    }

    // The same {min, natural, max} band words, cast as the pure-solver
    // constraint each names on the strength hierarchy a pure region solves on:
    // an exact size is a strong equality (above the weak 100 default, below a
    // required bound), an at-least or at-most a required inequality. A pure
    // region reads these; the banded path ignores them and reads the band the
    // sibling mapSizeHint modifier wrote. Together under one modifier the same
    // vocabulary drives either context.

    std::vector<arrange::Constraint> exactlyConstraint(arrange::Expression extent,
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
    // on that axis and one of the band-word constraint shapes above.
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

    // A band modifier and the pure-solver constraint that names the same intent,
    // under one modifier: the band drives the shipped per-container path, the
    // constraint drives a pure region, and each context ignores the other's half.
    AnyWidgetModifier bandAndPure(AnyWidgetModifier band, AnyWidgetModifier pure)
    {
        return detail::composeModifiers(std::move(band), std::move(pure));
    }

    // The both-axes counterpart: the already-built band modifier joined with a
    // pure-solver constraint of shape @p constraint on each axis, split off @p
    // size's two components.
    template <typename TConstraint>
    AnyWidgetModifier sizeAndPure(AnyWidgetModifier band,
            bq::signal::AnySignal<avg::Vector2f> size, TConstraint constraint)
    {
        auto shared = std::move(size).share();
        auto width = shared.clone().map(
                [](avg::Vector2f s) { return s.x(); });
        auto height = shared.clone().map(
                [](avg::Vector2f s) { return s.y(); });

        return bandAndPure(
                bandAndPure(std::move(band),
                    pureWidth(std::move(width), constraint)),
                pureHeight(std::move(height), constraint));
    }
} // anonymous namespace

AnyWidgetModifier widthAtLeast(bq::signal::AnySignal<float> width)
{
    auto shared = std::move(width).share();
    return bandAndPure(
            widthModifier(shared.clone(), bandAtLeast),
            pureWidth(shared.clone(), atLeastConstraint));
}

AnyWidgetModifier widthAtLeast(float width)
{
    return widthAtLeast(bq::signal::constant(width));
}

AnyWidgetModifier widthAtMost(bq::signal::AnySignal<float> width)
{
    auto shared = std::move(width).share();
    return bandAndPure(
            widthModifier(shared.clone(), bandAtMost),
            pureWidth(shared.clone(), atMostConstraint));
}

AnyWidgetModifier widthAtMost(float width)
{
    return widthAtMost(bq::signal::constant(width));
}

AnyWidgetModifier widthExactly(bq::signal::AnySignal<float> width)
{
    auto shared = std::move(width).share();
    return bandAndPure(
            widthModifier(shared.clone(), bandExactly),
            pureWidth(shared.clone(), exactlyConstraint));
}

AnyWidgetModifier widthExactly(float width)
{
    return widthExactly(bq::signal::constant(width));
}

AnyWidgetModifier heightAtLeast(bq::signal::AnySignal<float> height)
{
    auto shared = std::move(height).share();
    return bandAndPure(
            heightModifier(shared.clone(), bandAtLeast),
            pureHeight(shared.clone(), atLeastConstraint));
}

AnyWidgetModifier heightAtLeast(float height)
{
    return heightAtLeast(bq::signal::constant(height));
}

AnyWidgetModifier heightAtMost(bq::signal::AnySignal<float> height)
{
    auto shared = std::move(height).share();
    return bandAndPure(
            heightModifier(shared.clone(), bandAtMost),
            pureHeight(shared.clone(), atMostConstraint));
}

AnyWidgetModifier heightAtMost(float height)
{
    return heightAtMost(bq::signal::constant(height));
}

AnyWidgetModifier heightExactly(bq::signal::AnySignal<float> height)
{
    auto shared = std::move(height).share();
    return bandAndPure(
            heightModifier(shared.clone(), bandExactly),
            pureHeight(shared.clone(), exactlyConstraint));
}

AnyWidgetModifier heightExactly(float height)
{
    return heightExactly(bq::signal::constant(height));
}

AnyWidgetModifier sizeAtLeast(bq::signal::AnySignal<avg::Vector2f> size)
{
    auto shared = std::move(size).share();
    return sizeAndPure(sizeModifier(shared.clone(), bandAtLeast),
            shared.clone(), atLeastConstraint);
}

AnyWidgetModifier sizeAtLeast(avg::Vector2f size)
{
    return sizeAtLeast(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier sizeAtMost(bq::signal::AnySignal<avg::Vector2f> size)
{
    auto shared = std::move(size).share();
    return sizeAndPure(sizeModifier(shared.clone(), bandAtMost),
            shared.clone(), atMostConstraint);
}

AnyWidgetModifier sizeAtMost(avg::Vector2f size)
{
    return sizeAtMost(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier exactSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    auto shared = std::move(size).share();
    return sizeAndPure(sizeModifier(shared.clone(), bandExactly),
            shared.clone(), exactlyConstraint);
}

AnyWidgetModifier exactSize(avg::Vector2f size)
{
    return exactSize(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier preferWidth(bq::signal::AnySignal<float> width)
{
    return widthModifier(std::move(width), bandPrefer);
}

AnyWidgetModifier preferWidth(float width)
{
    return preferWidth(bq::signal::constant(width));
}

AnyWidgetModifier preferHeight(bq::signal::AnySignal<float> height)
{
    return heightModifier(std::move(height), bandPrefer);
}

AnyWidgetModifier preferHeight(float height)
{
    return preferHeight(bq::signal::constant(height));
}

AnyWidgetModifier growWeight(bq::signal::AnySignal<float> weight)
{
    return growModifier(std::move(weight), bandGrow);
}

AnyWidgetModifier growWeight(float weight)
{
    return growWeight(bq::signal::constant(weight));
}

}
