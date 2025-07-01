#pragma once

#include "widgetmodifier.h"
#include "buildermodifier.h"
#include "transform.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    namespace {
        template <typename T, typename U>
        auto handleGravityBuilderModifier(T&& builder, U&& outerSize)
        {
            auto innerSize = merge(builder.getSizeHint(), outerSize).map(
                [](SizeHint sizeHint, avg::Vector2f outerSize) -> avg::Vector2f
                {
                    SizeHintResult widthRequest = sizeHint.getWidth();

                    float width = std::clamp(outerSize.x(), widthRequest[0],
                            std::max(widthRequest[1], widthRequest[2]));

                    SizeHintResult heightRequest = sizeHint.getHeightForWidth(
                            width);

                    float height = std::clamp(outerSize.y(), heightRequest[0],
                            std::max(heightRequest[1], heightRequest[2]));

                    SizeHintResult finalWidthRequest =
                        sizeHint.getWidthForHeight(height);

                    float finalWidth = std::clamp(outerSize.x(),
                            finalWidthRequest[0],
                            std::max(finalWidthRequest[1], finalWidthRequest[2]));

                    return { finalWidth, height };
                });

            auto offset = merge(innerSize, outerSize, builder.getGravity()).map(
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
