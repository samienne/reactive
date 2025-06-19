#include "uniformgrid.h"

#include "widget/providebuildparams.h"

#include "layout.h"
#include "mapsizehint.h"
#include "stacksizehint.h"

#include <bq/signal/combine.h>
#include <bq/signal/signal.h>

namespace reactive
{

UniformGrid::UniformGrid(unsigned int w, unsigned int h) :
    w_(w),
    h_(h)
{
}

auto UniformGrid::cell(unsigned int x, unsigned int y,
        unsigned int w, unsigned int h,
        widget::AnyWidget widget) && -> UniformGrid
{
    cells_.push_back({x, y, w, h});
    widgets_.push_back(std::move(widget));
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

UniformGrid::operator widget::AnyWidget() &&
{
    return makeWidget([](widget::BuildParams const& params, auto widgets, auto cells,
                unsigned int w, unsigned int h)
        {
            std::vector<widget::AnyBuilder> builders;

            for (auto&& widget : widgets)
                builders.push_back(std::move(widget)(params));

            size_t i = 0;
            for (auto const& cell : cells)
            {
                auto hi = merge(
                        builders[i].getSizeHint(),
                        signal::constant(1.0f / (float)cell.w),
                        signal::constant(1.0f / (float)cell.h)
                        ).map(multiplySizeHint);

                ++i;
            }

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

            return layout(
                    std::move(mapHints),
                    std::move(mapObbs),
                    std::move(widgets)
                    );

        },
        widget::provideBuildParams(),
        std::move(widgets_),
        std::move(cells_),
        w_,
        h_
        );
}

} // reactive

