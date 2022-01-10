#include "uniformgrid.h"

#include "layout.h"
#include "mapsizehint.h"
#include "stacksizehint.h"

#include "signal/combine.h"
#include "signal/constant.h"

namespace reactive
{

UniformGrid::UniformGrid(unsigned int w, unsigned int h) :
    w_(w),
    h_(h)
{
}

auto UniformGrid::cell(unsigned int x, unsigned int y,
        unsigned int w, unsigned int h,
        widget::Builder builder) && -> UniformGrid
{
    cells_.push_back({x, y, w, h});
    builders_.push_back(std::move(builder));
    return std::move(*this);
}

auto multiplySizeHint(SizeHint const& sizeHint, float x, float y) -> SizeHint
{
    return mapSizeHint(sizeHint,
            [x](SizeHintResult result) -> SizeHintResult
            {
                return {{ result[0] * x, result[1] * x, result[2] * x }};
            },
            [y](SizeHintResult result, float) -> SizeHintResult
            {
                return {{ result[0] * y, result[1] * y, result[2] * y }};
            },
            [x](SizeHintResult result, float) -> SizeHintResult
            {
                return {{ result[0] * x, result[1] * x, result[2] * x }};
            }
            );
}

UniformGrid::operator widget::Builder() &&
{
    std::vector<AnySignal<SizeHint>> hints;
    hints.reserve(cells_.size());

    auto cells = std::move(cells_);
    auto builders = std::move(builders_);
    size_t i = 0;
    for (auto const& cell : cells)
    {
        auto hi = signal::map(multiplySizeHint,
                builders[i].getSizeHint(),
                signal::constant(1.0f / (float)cell.w),
                signal::constant(1.0f / (float)cell.h)
                );

        hints.push_back(std::move(hi));

        ++i;
    }

    auto w = w_;
    auto h = h_;
    auto mapHints = [w, h](std::vector<SizeHint> const& hints)
        -> SizeHint
    {
        return multiplySizeHint(stackSizeHints(hints), (float)w, (float)h);
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

    i = 0;
    for (auto&& builder : builders)
    {
        builder = std::move(builder)
            | widget::setSizeHint(std::move(hints[i++]))
            ;
    }

    auto r = layout(
            std::move(mapHints),
            std::move(mapObbs),
            std::move(builders)
            );

    return r;
}

} // reactive

