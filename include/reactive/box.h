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
    template <Axis dir, typename THints>
    struct CombineSizeHint
    {
        SizeHintResult operator()() const
        {
            auto xHints = btl::fmap(hints_,
                    [](auto const& hint)
                    {
                        return hint();
                    });

            return dir == Axis::x
                ? combinePartialHints(xHints)
                : getLargestHint(xHints);
        }

        SizeHintResult operator()(float x) const
        {
            auto xHints = btl::fmap(hints_,
                    [](auto const& hint)
                    {
                        return hint();
                    });

            auto xSizes = getSizes(x, xHints);

            size_t i = 0;
            auto yHints = btl::fmap(xSizes,
                    [this, &i](auto const& xSize)
                    {
                        return hints_[i++](xSize);
                    });

            return dir == Axis::x
                ? getLargestHint(yHints)
                : combinePartialHints(yHints);
        }

        SizeHintResult operator()(float x, float y) const
        {
            auto xHints = btl::fmap(hints_,
                    [x, y](auto const& hint)
                    {
                        return hint(x, y);
                    });

            return dir == Axis::x
                ? combinePartialHints(xHints)
                : getLargestHint(xHints);
        }

        //std::vector<SizeHint> const hints_;
        THints const hints_;
    };

    template <Axis dir>
    auto combineSizeHints(std::vector<SizeHint> hints)
        -> CombineSizeHint<dir, std::vector<SizeHint>>
    {
        return CombineSizeHint<dir, std::vector<SizeHint>>{std::move(hints)};
    }

    template <Axis dir, typename... Ts>
    auto combineSizeHintsTuple(std::tuple<Ts...> hints)
        -> CombineSizeHint<dir, std::tuple<Ts...>>
    {
        return CombineSizeHint<dir, std::tuple<Ts...>>{std::move(hints)};
    }

    static_assert(IsSizeHint<CombineSizeHint<Axis::x,
            std::vector<SizeHint>>>::value, "");

    template <Axis dir>
    BTL_HIDDEN auto combineSizes(ase::Vector2f size,
            std::vector<SizeHint> const& hints)
        -> std::vector<ase::Vector2f>
    {
        if (hints.empty())
            return {};

        std::vector<ase::Vector2f> result;
        result.reserve(hints.size());

        auto xHints = btl::fmap(hints,
                [](auto const& hint)
                {
                    return hint();
                });

        if (dir == Axis::x)
        {
            auto xSizes = getSizes(size[0], xHints);
            for (auto&& xSize : xSizes)
                result.emplace_back(xSize, size[1]);
        }
        else
        {
            auto yHints = btl::fmap(hints,
                    [&size](auto const& hint)
                    {
                        return hint(size[0]);
                    });

            auto ySizes = getSizes(size[1], yHints);
            for (auto&& ySize : ySizes)
                result.emplace_back(size[0], ySize);
        }

        return result;
    }

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

