#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    class BQUI_EXPORT UniformGrid
    {
    public:
        /**
         * @brief A grid of @p w columns by @p h rows of equally sized cells.
         *
         * @throws std::invalid_argument if @p w or @p h is zero.
         */
        UniformGrid(unsigned int w, unsigned int h);

        auto cell(unsigned int x, unsigned int y,
                unsigned int w, unsigned int h,
                widget::AnyWidget widget) && -> UniformGrid;

        operator widget::AnyWidget() &&;

    private:
        struct Cell
        {
            unsigned int x;
            unsigned int y;
            unsigned int w;
            unsigned int h;
        };

        unsigned int w_;
        unsigned int h_;
        std::vector<Cell> cells_;
        std::vector<widget::AnyWidget> widgets_;
    };

    /**
     * @brief A grid of @p w columns by @p h rows of equally sized cells.
     *
     * @throws std::invalid_argument if @p w or @p h is zero.
     */
    inline auto uniformGrid(unsigned int w, unsigned int h)
        -> UniformGrid
    {
        return UniformGrid(w, h);
    }
} // namespace bqui::widget

