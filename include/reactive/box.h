#pragma once

#include "layout.h"

#include "signal/signal.h"

#include <avg/drawing.h>

#include <ase/vector.h>

#include <functional>

namespace reactive
{
    inline auto accumulateSizeHintResults(std::vector<SizeHintResult> const& hints)
        -> SizeHintResult
    {
        auto result = SizeHintResult{{0.0f, 0.0f, 0.0f}};
        for (auto const& hint : hints)
            for (int i = 0; i < 3; ++i)
                result[i] += hint[i];

        return result;
    }

    inline auto getSizes(float size,
            std::vector<std::array<float, 3>> const& hints)
        -> std::vector<float>
    {
        std::vector<float> result;
        result.reserve(hints.size());

        auto combined = accumulateSizeHintResults(hints);
        std::array<float, 3> multiplier;
        for (size_t i = 0; i < multiplier.size(); ++i)
        {
            float prev = (i ? combined[i-1] : 0.0f);
            float m = (size - prev) / (combined[i] - prev);
            multiplier[i] = std::max(0.0f, std::min(1.0f, m));
        }

        for (auto const& hint : hints)
        {
            float r = 0.0f;
            for (size_t i = 0; i < hint.size(); ++i)
            {
                float prev = (i ? hint[i-1] : 0.0f);
                r += (hint[i] - prev) * multiplier[i];
            }
            result.push_back(r);
        }

        return result;
    }

    template <Axis dir, typename THints>
    struct AccumulateSizeHint
    {
        SizeHintResult getWidth() const
        {
            auto xHints = btl::fmap(hints_,
                    [](auto const& hint)
                    {
                        return hint.getWidth();
                    });

            return dir == Axis::x
                ? accumulateSizeHintResults(xHints)
                : getLargestHint(xHints);
        }

        SizeHintResult getHeightForWidth(float x) const
        {
            auto xHints = btl::fmap(hints_,
                    [](auto const& hint)
                    {
                        return hint.getWidth();
                    });

            auto xSizes = getSizes(x, xHints);

            size_t i = 0;
            auto yHints = btl::fmap(xSizes,
                    [this, &i](auto const& xSize)
                    {
                        return hints_[i++].getHeightForWidth(xSize);
                    });

            return dir == Axis::x
                ? getLargestHint(yHints)
                : accumulateSizeHintResults(yHints);
        }

        SizeHintResult getWidthForHeight(float height) const
        {
            auto xHints = btl::fmap(hints_,
                    [height](auto const& hint)
                    {
                        return hint.getWidthForHeight(height);
                    });

            return dir == Axis::x
                ? accumulateSizeHintResults(xHints)
                : getLargestHint(xHints);
        }

        //std::vector<SizeHint> const hints_;
        THints const hints_;
    };

    template <Axis dir>
    auto accumulateSizeHints(std::vector<SizeHint> hints)
        -> AccumulateSizeHint<dir, std::vector<SizeHint>>
    {
        return AccumulateSizeHint<dir, std::vector<SizeHint>>{std::move(hints)};
    }

    template <Axis dir, typename... Ts>
    auto accumulateSizeHintsTuple(std::tuple<Ts...> hints)
        -> AccumulateSizeHint<dir, std::tuple<Ts...>>
    {
        return AccumulateSizeHint<dir, std::tuple<Ts...>>{std::move(hints)};
    }

    template <Axis dir>
    auto combineSizes(ase::Vector2f size,
            std::vector<SizeHint> const& hints)
        -> std::vector<ase::Vector2f>
    {
        if (hints.empty())
            return {};

        std::vector<ase::Vector2f> result;
        result.reserve(hints.size());

        if (dir == Axis::x)
        {
            auto xHintResults = btl::fmap(hints,
                    [&](auto const& hint)
                    {
                        return hint.getWidthForHeight(size[1]);
                    });

            auto xSizes = getSizes(size[0], xHintResults);

            for (auto&& xSize : xSizes)
                result.emplace_back(xSize, size[1]);
        }
        else
        {
            auto xHintResults = btl::fmap(hints,
                    [](auto const& hint)
                    {
                        return hint.getWidth();
                    });

            auto yHintResults = btl::fmap(hints,
                    [&size](auto const& hint)
                    {
                        return hint.getHeightForWidth(size[0]);
                    });

            auto ySizes = getSizes(size[1], yHintResults);
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
    widget::AnyWidget box(std::vector<widget::AnyWidget> widgets)
    {
        return layout(accumulateSizeHints<dir>, &mapObbs<dir>,
                std::move(widgets));
    }
}

