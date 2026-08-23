#pragma once

#include "sizehint.h"

namespace bqui
{
    inline AxisHint growAxisHint(AxisHint const& hint, float amount)
    {
        // Growing pushes the leading edge out by amount, so any anchor measured
        // from that edge moves with it.
        Anchors anchors = hint.anchors;
        if (anchors.firstBaseline)
            anchors.firstBaseline = *anchors.firstBaseline + amount;
        if (anchors.lastBaseline)
            anchors.lastBaseline = *anchors.lastBaseline + amount;

        return AxisHint{
            Band{
                hint.extent.min + amount * 2.0f,
                hint.extent.natural + amount * 2.0f,
                hint.extent.max + amount * 2.0f,
                hint.extent.grow
            },
            anchors
        };
    }

    template <typename THint>
    struct GrowSizeHint
    {
        AxisHint getWidth() const
        {
            return growAxisHint(hint.getWidth(), amount);
        }

        AxisHint getHeightForWidth(float width) const
        {
            return growAxisHint(hint.getHeightForWidth(width), amount);
        }

        AxisHint getWidthForHeight(float height) const
        {
            return growAxisHint(hint.getWidthForHeight(height), amount);
        }

        std::decay_t<THint> hint;
        float amount;
    };

    /**
     * @brief Grows given size hint by given amount. Returns new size hint.
     *
     * @param hint The original hint.
     * @param amount The amount to grow. Use negative to shrink.
     * @return The grown SizeHint.
     */
    template <typename THint, typename =
        std::enable_if_t<
            IsSizeHint<THint>::value
        >
    >
    GrowSizeHint<std::decay_t<THint>> growSizeHint(THint&& hint, float amount)
    {
        return GrowSizeHint<std::decay_t<THint>>{
            std::forward<THint>(hint), amount
        };
    }

}

