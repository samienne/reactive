#pragma once

#include "widgetfactory.h"

namespace reactive
{
    class UniformGrid
    {
    public:
        UniformGrid(unsigned int w, unsigned int h);

        auto cell(unsigned int x, unsigned int y,
                unsigned int w, unsigned int h,
                WidgetFactory factory) && -> UniformGrid;

        operator WidgetFactory() &&;

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
        std::vector<WidgetFactory> factories_;
    };

    inline auto uniformGrid(unsigned int w, unsigned int h) -> UniformGrid
    {
        return UniformGrid(w, h);
    }
}

