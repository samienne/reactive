#include "bqui/modifier/setminimumsize.h"

#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

namespace bqui::modifier
{

namespace {
    Band setMinimumBand(Band band, float minimum)
    {
        float min = minimum;
        float natural = std::max(min, band.natural);
        float max = std::max(natural, band.max);

        return Band{ min, natural, max, band.grow };
    }

    AxisHint setMinimumAxis(AxisHint hint, float minimum)
    {
        return AxisHint{ setMinimumBand(hint.extent, minimum), hint.anchors };
    }

    auto setMinimumSizeImpl2(SizeHint sizeHint, avg::Vector2f minimumSize)
    {
        return mapSizeHint(std::move(sizeHint),
                [minimumSize](AxisHint hint) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumSize.x());
                },
                [minimumSize](AxisHint hint, float) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumSize.y());
                },
                [minimumSize](AxisHint hint, float) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumSize.x());
                }
                );
    }

    template <typename T>
    AnyWidgetModifier setMinimumSizeImpl(bq::signal::Signal<T, avg::Vector2f> size)
    {
        return mapSizeHint(setMinimumSizeImpl2, std::move(size));
    }

    auto setMinimumWidthImpl(SizeHint sizeHint, float minimumWidth)
    {
        return mapSizeHint(std::move(sizeHint),
                [minimumWidth](AxisHint hint) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumWidth);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                },
                [minimumWidth](AxisHint hint, float) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumWidth);
                }
                );
    }

    auto setMinimumHeightImpl(SizeHint sizeHint, float minimumHeight)
    {
        return mapSizeHint(std::move(sizeHint),
                [](AxisHint hint) -> AxisHint
                {
                    return hint;
                },
                [minimumHeight](AxisHint hint, float) -> AxisHint
                {
                    return setMinimumAxis(hint, minimumHeight);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                }
                );
    }
} // anonymous namespace

AnyWidgetModifier setMinimumSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return setMinimumSizeImpl(std::move(size));
}

AnyWidgetModifier setMinimumSize(avg::Vector2f size)
{
    return setMinimumSizeImpl(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier setMinimumWidth(bq::signal::AnySignal<float> width)
{
    return mapSizeHint(setMinimumWidthImpl, std::move(width));
}

AnyWidgetModifier setMinimumWidth(float width)
{
    return mapSizeHint(setMinimumWidthImpl, bq::signal::constant(width));
}

AnyWidgetModifier setMinimumHeight(bq::signal::AnySignal<float> height)
{
    return mapSizeHint(setMinimumHeightImpl, std::move(height));
}

AnyWidgetModifier setMinimumHeight(float height)
{
    return mapSizeHint(setMinimumHeightImpl, bq::signal::constant(height));
}

}
