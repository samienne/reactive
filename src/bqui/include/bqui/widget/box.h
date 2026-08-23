#pragma once

#include "bqui/sizehint.h"

#include <btl/fmap.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace bqui::widget
{
    // The main-axis aggregate: the children's bands summed edge to edge. The
    // grow weights sum too, so a container fills if any of its children does.
    // The cross axis is aggregated with getLargestHint instead.
    inline auto accumulateAxisHints(std::vector<AxisHint> const& hints)
        -> AxisHint
    {
        Band band;
        for (auto const& hint : hints)
        {
            band.min += hint.extent.min;
            band.natural += hint.extent.natural;
            band.max += hint.extent.max;
            band.grow += hint.extent.grow;
        }

        return AxisHint{ band, Anchors{} };
    }

    inline auto getSizes(float size, std::vector<AxisHint> const& hints)
        -> std::vector<float>
    {
        // The interval arithmetic works over each band's {min, natural, max}
        // thresholds; grow does not enter this closed-form estimate.
        std::vector<std::array<float, 3>> bands;
        bands.reserve(hints.size());
        for (auto const& hint : hints)
            bands.push_back({{ hint.extent.min, hint.extent.natural,
                    hint.extent.max }});

        std::vector<float> result;
        result.reserve(bands.size());

        std::array<float, 3> combined{{ 0.0f, 0.0f, 0.0f }};
        for (auto const& band : bands)
            for (int i = 0; i < 3; ++i)
                combined[i] += band[i];

        std::array<float, 3> multiplier;
        for (size_t i = 0; i < multiplier.size(); ++i)
        {
            float prev = (i ? combined[i-1] : 0.0f);
            float span = combined[i] - prev;

            // Empty interval (span 0): avoid the divide; the size either
            // reaches it (1) or it does not (0).
            float m = span != 0.0f
                ? (size - prev) / span
                : (size < prev ? 0.0f : 1.0f);

            multiplier[i] = std::max(0.0f, std::min(1.0f, m));
        }

        for (auto const& band : bands)
        {
            float r = 0.0f;
            for (size_t i = 0; i < band.size(); ++i)
            {
                float prev = (i ? band[i-1] : 0.0f);
                r += (band[i] - prev) * multiplier[i];
            }
            result.push_back(r);
        }

        return result;
    }

    template <Axis dir, typename THints>
    struct AccumulateSizeHint
    {
        AxisHint getWidth() const
        {
            auto xHints = btl::fmap(hints_,
                    [](auto const& hint)
                    {
                        return hint.getWidth();
                    });

            return dir == Axis::x
                ? accumulateAxisHints(xHints)
                : getLargestHint(xHints);
        }

        AxisHint getHeightForWidth(float x) const
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

            if (dir == Axis::x)
                return baseline
                    ? getBaselineHint(yHints)
                    : getLargestHint(yHints);

            return accumulateAxisHints(yHints);
        }

        AxisHint getWidthForHeight(float height) const
        {
            auto xHints = btl::fmap(hints_,
                    [height](auto const& hint)
                    {
                        return hint.getWidthForHeight(height);
                    });

            return dir == Axis::x
                ? accumulateAxisHints(xHints)
                : getLargestHint(xHints);
        }

        THints const hints_;

        // Only meaningful for a row (dir == Axis::x): aggregate the cross axis
        // by splitting each child's band at its baseline rather than by a plain
        // maximum, so the row reports a baseline of its own.
        bool baseline = false;
    };

    template <Axis dir>
    auto accumulateSizeHints(std::vector<SizeHint> hints)
        -> AccumulateSizeHint<dir, std::vector<SizeHint>>
    {
        return AccumulateSizeHint<dir, std::vector<SizeHint>>{std::move(hints)};
    }

    // A row whose cross axis is aggregated by baseline (getBaselineHint) instead
    // of by a plain maximum, so it reports its own firstBaseline upward.
    inline auto accumulateBaselineRowHints(std::vector<SizeHint> hints)
        -> AccumulateSizeHint<Axis::x, std::vector<SizeHint>>
    {
        return AccumulateSizeHint<Axis::x, std::vector<SizeHint>>{
            std::move(hints), true };
    }
} // namespace bqui::widget
