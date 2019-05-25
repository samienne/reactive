#pragma once

#include "sizehint.h"
#include "reactivevisibility.h"

namespace reactive
{
    class REACTIVE_EXPORT SimpleSizeHint
    {
    public:
        SimpleSizeHint(SizeHintResult x, SizeHintResult y);
        SizeHintResult getWidth() const;
        SizeHintResult getHeightForWidth(float) const;
        SizeHintResult getWidthForHeight(float) const;

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
    REACTIVE_EXPORT SimpleSizeHint simpleSizeHint(SizeHintResult x, SizeHintResult y);

    /**
     * @brief Equivalent of simpleSizeHint({{x, x, x}}, {{y, y, y}}).
     */
    REACTIVE_EXPORT SimpleSizeHint simpleSizeHint(float width, float height);
} // namespace reactive

