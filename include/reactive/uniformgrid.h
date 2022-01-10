#pragma once

#include "widget/builder.h"

#include "reactivevisibility.h"

namespace reactive
{
    class REACTIVE_EXPORT UniformGrid
    {
    public:
        UniformGrid(unsigned int w, unsigned int h);

        auto cell(unsigned int x, unsigned int y,
                unsigned int w, unsigned int h,
                widget::Builder builder) && -> UniformGrid;

        operator widget::Builder() &&;

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
        std::vector<widget::Builder> builders_;
    };

    inline auto uniformGrid(unsigned int w, unsigned int h)
        -> UniformGrid
    {
        return UniformGrid(w, h);
    }
}

