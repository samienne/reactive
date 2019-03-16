#pragma once

#include "layout.h"
#include "widgetfactory.h"
#include "widget.h"

#include "signal/combine.h"
#include "signal/constant.h"
#include "signal.h"

#include <avg/drawing.h>

#include <ase/vector.h>

#include <functional>

namespace reactive
{
    template <Axis dir>
    struct MapObbs
    {
        template <typename TSizeHints>
        auto operator()(ase::Vector2f size, TSizeHints const& hints2) const
            -> std::vector<avg::Obb>
        {
            std::vector<SizeHint> hints;
            btl::forEach(hints2, [&hints](auto&& hint)
            {
                hints.push_back(hint);
            });

            std::vector<avg::Vector2f> sizes = combineSizes<dir>(size, hints);

            std::vector<avg::Obb> obbs;
            obbs.reserve(hints.size());
            float acc = dir == Axis::x ? 0.0f : size[1];

            btl::forEach(std::move(sizes), [&acc, &obbs](auto&& size)
            {
                ase::Vector2f offset;
                if (dir == Axis::x)
                {
                    offset = ase::Vector2f(acc, 0.0f);
                    acc += size[0];
                }
                else
                {
                    acc -= size[1];
                    offset = ase::Vector2f(0.0f, acc);
                }

                obbs.push_back(avg::Transform().translate(offset) *
                        avg::Obb(size));
            });

            return obbs;
        }
    };

    template <Axis dir>
    auto mapObbs(ase::Vector2f size,
            std::vector<SizeHint> const& hints)
        -> std::vector<avg::Obb>
    {
        auto sizes = combineSizes<dir>(size, hints);

        std::vector<avg::Obb> obbs;
        obbs.reserve(hints.size());
        float acc = dir == Axis::x ? 0.0f : size[1];
        for (auto const& size : sizes)
        {
            ase::Vector2f offset;
            if (dir == Axis::x)
            {
                offset = ase::Vector2f(acc, 0.0f);
                acc += size[0];
            }
            else
            {
                acc -= size[1];
                offset = ase::Vector2f(0.0f, acc);
            }

            obbs.push_back(avg::Transform().translate(offset) *
                    avg::Obb(size));
        }

        return obbs;
    }

    template <Axis dir>
    auto box(std::vector<WidgetFactory> factories)  //-> WidgetFactory
    {
        return layout(combineSizeHints<dir>, &mapObbs<dir>,
                std::move(factories));
    }

    template <Axis dir, typename... Ts>
    auto box(std::tuple<Ts...> factories) // -> WidgetFactory
    {
        return layout(
                [](auto hints)
                {
                    return combineSizeHintsTuple<dir>(std::move(hints));
                },
                MapObbs<dir>(),
                std::move(factories)
                );
    }
}

