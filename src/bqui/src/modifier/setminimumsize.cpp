#include "bqui/modifier/setminimumsize.h"

#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/mapsizehint.h"

#include "bqui/mapsizehint.h"

namespace bqui::modifier
{

namespace {
    auto setMinimumSizeImpl2(SizeHint sizeHint, avg::Vector2f minimumSize)
    {
        return mapSizeHint(std::move(sizeHint),
                [minimumSize](SizeHintResult result) -> SizeHintResult
                {
                    float min = minimumSize.x();
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

                    return {{ min, max, fill }};
                },
                [minimumSize](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = minimumSize.y();
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

                    return {{ min, max, fill }};
                },
                [minimumSize](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = minimumSize.x();
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

                    return {{ min, max, fill }};
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
                [minimumWidth](SizeHintResult result) -> SizeHintResult
                {
                    float min = minimumWidth;
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

                    return {{ min, max, fill }};
                },
                [](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    return result;
                },
                [minimumWidth](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = minimumWidth;
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

                    return {{ min, max, fill }};
                }
                );
    }

    auto setMinimumHeightImpl(SizeHint sizeHint, float minimumHeight)
    {
        return mapSizeHint(std::move(sizeHint),
                [](SizeHintResult result) -> SizeHintResult
                {
                    return result;
                },
                [minimumHeight](SizeHintResult result, float)
                    -> SizeHintResult
                {
                    float min = minimumHeight;
                    float max = std::max(min, result[0]);
                    float fill = std::max(max, result[1]);

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
