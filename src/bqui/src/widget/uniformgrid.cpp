#include "bqui/widget/uniformgrid.h"

#include "bqui/widget/layout.h"

#include "bqui/stacksizehint.h"

#include <stdexcept>

namespace bqui::widget
{

UniformGrid::UniformGrid(unsigned int w, unsigned int h) :
    w_(w),
    h_(h)
{
    if (w == 0 || h == 0)
    {
        throw std::invalid_argument("UniformGrid: a grid needs at least one "
                "column and one row.");
    }
}

auto UniformGrid::cell(unsigned int x, unsigned int y,
        unsigned int w, unsigned int h,
        AnyWidget widget) && -> UniformGrid
{
    cells_.push_back({x, y, w, h});
    widgets_.push_back(std::move(widget));
    return std::move(*this);
}

namespace
{

SizeHintResult scaleResult(SizeHintResult result, float factor)
{
    return {{ result[0] * factor, result[1] * factor, result[2] * factor }};
}

/**
 * @brief The hint scaled by x horizontally and y vertically.
 */
struct ScaledSizeHint
{
    SizeHintResult getWidth() const
    {
        return scaleResult(hint.getWidth(), x);
    }

    SizeHintResult getHeightForWidth(float width) const
    {
        return scaleResult(hint.getHeightForWidth(width / x), y);
    }

    SizeHintResult getWidthForHeight(float height) const
    {
        return scaleResult(hint.getWidthForHeight(height / y), x);
    }

    SizeHint hint;
    float x;
    float y;
};

} // anonymous namespace

UniformGrid::operator AnyWidget() &&
{
    return makeWidget([](auto widgets, auto cells,
                unsigned int w, unsigned int h)
        {
            auto mapHints = [w, h, cells](std::vector<SizeHint> const& hints)
                -> SizeHint
            {
                std::vector<SizeHint> perCell;
                perCell.reserve(hints.size());

                for (size_t i = 0; i < hints.size(); ++i)
                {
                    auto const& cell = cells[i];

                    // A child with no cells to span is given no room and so
                    // asks nothing of the grid.
                    if (cell.w == 0 || cell.h == 0)
                        continue;

                    perCell.push_back(ScaledSizeHint{
                            hints[i],
                            1.0f / (float)cell.w,
                            1.0f / (float)cell.h
                            });
                }

                return ScaledSizeHint{
                    stackSizeHints(std::move(perCell)),
                    (float)w,
                    (float)h
                    };
            };

            auto mapObbs = [w, h, cells](ase::Vector2f size,
                    std::vector<SizeHint> const& hints)
                -> std::vector<avg::Obb>
            {
                if (hints.empty())
                    return {};

                auto cellSize = ase::Vector2f(
                        size[0] / (float)w,
                        size[1] / (float)h);

                std::vector<avg::Obb> obbs;
                for (auto const& cell : cells)
                {
                    auto t = avg::Transform().translate(
                            (float)cell.x * cellSize[0],
                            (float)cell.y * cellSize[1]);

                    obbs.push_back(
                            t * avg::Obb(ase::Vector2f(
                                    (float)cell.w * cellSize[0],
                                    (float)cell.h * cellSize[1])));
                }

                return obbs;
            };

            return layout(
                    std::move(mapHints),
                    std::move(mapObbs),
                    std::move(widgets)
                    );

        },
        std::move(widgets_),
        std::move(cells_),
        w_,
        h_
        );
}

}

