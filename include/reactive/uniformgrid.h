#pragma once

#include "widget/widget.h"

#include "reactivevisibility.h"

namespace reactive
{
    class REACTIVE_EXPORT UniformGrid
    {
    public:
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

    inline auto uniformGrid(unsigned int w, unsigned int h)
        -> UniformGrid
    {
        return UniformGrid(w, h);
    }
}

