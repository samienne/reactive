#include "bqui/modifier/sizevocabulary.h"

#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

#include <bq/signal/constant.h>

#include <algorithm>

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
} // anonymous namespace

AnyWidgetModifier widthAtLeast(bq::signal::AnySignal<float> width)
{
    return widthModifier(std::move(width), bandAtLeast);
}

AnyWidgetModifier widthAtLeast(float width)
{
    return widthAtLeast(bq::signal::constant(width));
}

AnyWidgetModifier widthAtMost(bq::signal::AnySignal<float> width)
{
    return widthModifier(std::move(width), bandAtMost);
}

AnyWidgetModifier widthAtMost(float width)
{
    return widthAtMost(bq::signal::constant(width));
}

AnyWidgetModifier widthExactly(bq::signal::AnySignal<float> width)
{
    return widthModifier(std::move(width), bandExactly);
}

AnyWidgetModifier widthExactly(float width)
{
    return widthExactly(bq::signal::constant(width));
}

AnyWidgetModifier heightAtLeast(bq::signal::AnySignal<float> height)
{
    return heightModifier(std::move(height), bandAtLeast);
}

AnyWidgetModifier heightAtLeast(float height)
{
    return heightAtLeast(bq::signal::constant(height));
}

AnyWidgetModifier heightAtMost(bq::signal::AnySignal<float> height)
{
    return heightModifier(std::move(height), bandAtMost);
}

AnyWidgetModifier heightAtMost(float height)
{
    return heightAtMost(bq::signal::constant(height));
}

AnyWidgetModifier heightExactly(bq::signal::AnySignal<float> height)
{
    return heightModifier(std::move(height), bandExactly);
}

AnyWidgetModifier heightExactly(float height)
{
    return heightExactly(bq::signal::constant(height));
}

AnyWidgetModifier sizeAtLeast(bq::signal::AnySignal<avg::Vector2f> size)
{
    return sizeModifier(std::move(size), bandAtLeast);
}

AnyWidgetModifier sizeAtLeast(avg::Vector2f size)
{
    return sizeAtLeast(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier sizeAtMost(bq::signal::AnySignal<avg::Vector2f> size)
{
    return sizeModifier(std::move(size), bandAtMost);
}

AnyWidgetModifier sizeAtMost(avg::Vector2f size)
{
    return sizeAtMost(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier exactSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return sizeModifier(std::move(size), bandExactly);
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
