#include "bqui/modifier/setmaximumsize.h"

#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

namespace bqui::modifier
{

namespace {
    Band setMaximumBand(Band band, float maximum)
    {
        float min = band.min;
        float natural = std::max(min, maximum);
        float max = std::max(natural, band.max);

        return Band{ min, natural, max, band.grow };
    }

    AxisHint setMaximumAxis(AxisHint hint, float maximum)
    {
        return AxisHint{ setMaximumBand(hint.extent, maximum), hint.anchors };
    }

    auto setMaximumSizeImpl2(SizeHint sizeHint, avg::Vector2f maximumSize)
    {
        return mapSizeHint(std::move(sizeHint),
                [maximumSize](AxisHint hint) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumSize.x());
                },
                [maximumSize](AxisHint hint, float) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumSize.y());
                },
                [maximumSize](AxisHint hint, float) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumSize.x());
                }
                );
    }

    template <typename T>
    AnyWidgetModifier setMaximumSizeImpl(bq::signal::Signal<T, avg::Vector2f> size)
    {
        return mapSizeHint(setMaximumSizeImpl2, std::move(size));
    }

    auto setMaximumWidthImpl(SizeHint sizeHint, float maximumWidth)
    {
        return mapSizeHint(std::move(sizeHint),
                [maximumWidth](AxisHint hint) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumWidth);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                },
                [maximumWidth](AxisHint hint, float) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumWidth);
                }
                );
    }

    auto setMaximumHeightImpl(SizeHint sizeHint, float maximumHeight)
    {
        return mapSizeHint(std::move(sizeHint),
                [](AxisHint hint) -> AxisHint
                {
                    return hint;
                },
                [maximumHeight](AxisHint hint, float) -> AxisHint
                {
                    return setMaximumAxis(hint, maximumHeight);
                },
                [](AxisHint hint, float) -> AxisHint
                {
                    return hint;
                }
                );
    }
} // anonymous namespace

AnyWidgetModifier setMaximumSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return setMaximumSizeImpl(std::move(size));
}

AnyWidgetModifier setMaximumWidth(bq::signal::AnySignal<float> width)
{
    return mapSizeHint(setMaximumWidthImpl, std::move(width));
}

AnyWidgetModifier setMaximumHeight(bq::signal::AnySignal<float> height)
{
    return mapSizeHint(setMaximumHeightImpl, std::move(height));
}

}
