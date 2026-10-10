#pragma once

#include "widgetmodifier.h"
#include "buildermodifier.h"
#include "transform.h"

namespace bqui::modifier
{
    namespace {
        template <typename T, typename U>
        auto handleGravityBuilderModifier(T&& builder, U&& size)
        {
            auto outerSize = size.share();

            auto innerSize = merge(builder.getSizeHint(), outerSize).map(
                [](SizeHint sizeHint, avg::Vector2f outerSize) -> avg::Vector2f
                {
                    AxisHint widthRequest = sizeHint.getWidth();

                    float width = std::clamp(outerSize.x(), 0.0f,
                            std::max(widthRequest.extent.natural,
                                widthRequest.extent.max));

                    AxisHint heightRequest = sizeHint.getHeightForWidth(
                            width);

                    float height = std::clamp(outerSize.y(), 0.0f,
                            std::max(heightRequest.extent.natural,
                                heightRequest.extent.max));

                    AxisHint finalWidthRequest =
                        sizeHint.getWidthForHeight(height);

                    float finalWidth = std::clamp(outerSize.x(),
                            0.0f,
                            std::max(finalWidthRequest.extent.natural,
                                finalWidthRequest.extent.max));

                    return { finalWidth, height };
                }).share();

            auto offset = merge(
                    innerSize,
                    std::move(outerSize),
                    builder.getGravity()
                    ).map(
                    [](avg::Vector2f innerSize, avg::Vector2f outerSize,
                        avg::Vector2f gravity)
                    -> avg::Transform
                {
                    return avg::Transform().translate(avg::Vector2f {
                        gravity.x() * (outerSize.x() - innerSize.x()),
                        gravity.y() * (outerSize.y() - innerSize.y())
                        });
                });

            auto element = std::move(builder)(innerSize);

            return makeBuilderFromElement(std::move(element))
                | transformBuilder(offset)
                ;
        }
    } // anonymous namespace

    inline auto handleGravity()
    {
        return makeWidgetModifierWithSize([](auto widget, auto size)
            {
                return std::move(widget)
                    | makeBuilderModifier([](auto builder, auto size)
                        {
                            return handleGravityBuilderModifier(
                                    std::move(builder), std::move(size));
                        }, std::move(size))
                    ;
            });
    }
}
