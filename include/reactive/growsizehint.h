#pragma once

#include "sizehint.h"

namespace reactive
{
    inline SizeHintResult growSizeHintResult(SizeHintResult const& result,
            float amount)
    {
        return SizeHintResult{
            {
                result[0] + amount * 2.0f,
                result[1] + amount * 2.0f,
                result[2] + amount * 2.0f
            }
        };
    }

    template <typename THint>
    struct GrowSizeHint
    {
        SizeHintResult operator()() const
        {
            return growSizeHintResult(hint(), amount);
        }

        SizeHintResult operator()(float x) const
        {
            return growSizeHintResult(hint(x), amount);
        }

        SizeHintResult operator()(float x, float y) const
        {
            return growSizeHintResult(hint(x, y), amount);
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
            std::forward<THint>(hint), amount};
    }

} // namespace reactive
