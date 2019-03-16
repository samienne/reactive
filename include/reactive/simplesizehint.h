#pragma once

#include "sizehint.h"

#include <btl/visibility.h>

namespace reactive
{
    class BTL_VISIBLE SimpleSizeHint
    {
    public:
        SimpleSizeHint(SizeHintResult x, SizeHintResult y);
        SizeHintResult operator()() const;
        SizeHintResult operator()(float) const;
        SizeHintResult operator()(float, float) const;

    private:
        SizeHintResult const horizontal_;
        SizeHintResult const vertical_;
    };

    /**
     * @brief Creates static size hint.
     *
     * This is the simplest way to create a size hint. The returned sizes
     * are determined by the given parameters.
     *
     * @param x The hints on the X-axis.
     * @param y The hints on the Y-axis.
     * @return SizeHint that will return the x and y hints.
     */
    BTL_VISIBLE SimpleSizeHint simpleSizeHint(SizeHintResult x, SizeHintResult y);

    /**
     * @brief Equivalent of simpleSizeHint({{x, x, x}}, {{y, y, y}}).
     */
    BTL_VISIBLE SimpleSizeHint simpleSizeHint(float x, float y);
} // namespace reactive

