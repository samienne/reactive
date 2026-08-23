#include "bqui/widget/uniformgrid.h"

#include "constraintbox.h"
#include "constraintlayout.h"

namespace bqui::widget
{

UniformGrid::UniformGrid(unsigned int w, unsigned int h) :
    w_(w),
    h_(h)
{
}

auto UniformGrid::cell(unsigned int x, unsigned int y,
        unsigned int w, unsigned int h,
        AnyWidget widget) && -> UniformGrid
{
    cells_.push_back({x, y, w, h});
    widgets_.push_back(std::move(widget));
    return std::move(*this);
}

UniformGrid::operator AnyWidget() &&
{
    std::vector<GridCell> cells;
    cells.reserve(cells_.size());
    for (Cell const& cell : cells_)
        cells.push_back(GridCell{cell.x, cell.y, cell.w, cell.h});

    return solverUniformGrid(std::move(widgets_), std::move(cells), w_, h_);
}

}
