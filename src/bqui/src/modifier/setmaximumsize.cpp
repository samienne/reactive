#include "bqui/modifier/setmaximumsize.h"

#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

namespace bqui::modifier
{

namespace {
    SizeHintResult setMaximumSizeHintResult(SizeHintResult result, float maximum)
    {
            float min = result[0];
            float max = std::max(min, maximum);
            float fill = std::max(max, result[2]);

        return { min, max, fill };
    }

    auto setMaximumSizeImpl2(SizeHint sizeHint, avg::Vector2f maximumSize)
    {
        return mapSizeHint(std::move(sizeHint),
                [maximumSize](SizeHintResult result) -> SizeHintResult
                {
                    return setMaximumSizeHintResult(result, maximumSize.x());
                },
                [maximumSize](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    return setMaximumSizeHintResult(result, maximumSize.y());
                },
                [maximumSize](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    return setMaximumSizeHintResult(result, maximumSize.x());
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
                [maximumWidth](SizeHintResult result) -> SizeHintResult
                {
                    float min = result[0];
                    float max = std::max(maximumWidth, min);
                    float fill = std::max(max, result[2]);

                    return {{ min, max, fill }};
                },
                [](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    return result;
                },
                [maximumWidth](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = result[0];
                    float max = std::max(maximumWidth, min);
                    float fill = std::max(max, result[2]);

                    return {{ min, max, fill }};
                }
                );
    }

    auto setMaximumHeightImpl(SizeHint sizeHint, float maximumHeight)
    {
        return mapSizeHint(std::move(sizeHint),
                [](SizeHintResult result) -> SizeHintResult
                {
                    return result;
                },
                [maximumHeight](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = result[0];
                    float max = std::max(maximumHeight, min);
                    float fill = std::max(max, result[2]);

                    return {{ min, max, fill }};
                },
                [](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    return result;
                }
                );
    }
} // anonymous namespace

AnyWidgetModifier setMaximumSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    return setMaximumSizeImpl(std::move(size));
}

AnyWidgetModifier setMaximumSize(avg::Vector2f size)
{
    return setMaximumSizeImpl(bq::signal::constant(std::move(size)));
}

AnyWidgetModifier setMaximumWidth(bq::signal::AnySignal<float> width)
{
    return mapSizeHint(setMaximumWidthImpl, std::move(width));
}

AnyWidgetModifier setMaximumWidth(float width)
{
    return mapSizeHint(setMaximumWidthImpl, bq::signal::constant(width));
}

AnyWidgetModifier setMaximumHeight(bq::signal::AnySignal<float> height)
{
    return mapSizeHint(setMaximumHeightImpl, std::move(height));
}

AnyWidgetModifier setMaximumHeight(float height)
{
    return mapSizeHint(setMaximumHeightImpl, bq::signal::constant(height));
}

}
