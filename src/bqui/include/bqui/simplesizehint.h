#pragma once

#include "sizehint.h"

#include "bquivisibility.h"

namespace bqui
{
    class BQUI_EXPORT SimpleSizeHint
    {
    public:
        SimpleSizeHint(Band x, Band y);
        AxisHint getWidth() const;
        AxisHint getHeightForWidth(float) const;
        AxisHint getWidthForHeight(float) const;

    private:
        Band const horizontal_;
        Band const vertical_;
    };

    /**
     * @brief Creates static size hint.
     *
     * This is the simplest way to create a size hint. The returned sizes
     * are determined by the given parameters.
     *
     * @param x The width band.
     * @param y The height band.
     * @return SizeHint that will return the x and y bands.
     */
    BQUI_EXPORT SimpleSizeHint simpleSizeHint(Band x, Band y);

    /**
     * @brief Equivalent of simpleSizeHint(Band{x, x, x}, Band{y, y, y}).
     */
    BQUI_EXPORT SimpleSizeHint simpleSizeHint(float width, float height);

    BQUI_EXPORT SimpleSizeHint defaultSizeHint();
}
