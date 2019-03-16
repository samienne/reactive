#include "uniformgrid.h"

#include "layout.h"
#include "mapsizehint.h"

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
        WidgetFactory factory) && -> UniformGrid
{
    cells_.push_back({x, y, w, h});
    factories_.push_back(std::move(factory));
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
            [x](SizeHintResult result, float, float) -> SizeHintResult
            {
                return {{ result[0] * x, result[1] * x, result[2] * x }};
            }
            );
}

UniformGrid::operator WidgetFactory() &&
{
    std::vector<Signal<SizeHint>> hints;
    hints.reserve(cells_.size());

    auto cells = std::move(cells_);
    auto factories = std::move(factories_);
    size_t i = 0;
    for (auto const& cell : cells)
    {
        auto hi = signal::map(multiplySizeHint,
                factories[i].getSizeHint(),
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
        return multiplySizeHint(stackHints(hints), (float)w, (float)h);
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
    for (auto&& factory : factories)
    {
        factory = std::move(factory)
            | setSizeHint(std::move(hints[i++]))
            ;
    }

    auto r = layout(
            std::move(mapHints),
            std::move(mapObbs),
            std::move(factories)
            );

    return r;
}

#if 1
WidgetFactory layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        std::vector<WidgetFactory> factories)
{
    auto hints = btl::fmap(factories, [](auto const& factory)
            {
                return factory.getSizeHint();
            });

    auto hintsSignal = share(
            signal::combine(std::move(hints))
            );

    auto widgetMap = [obbMap=std::move(obbMap),
            hintsSignal, factories=btl::cloneOnCopy(std::move(factories))]
                (auto w)
        // -> Widget
    {
        auto obbs = share(signal::map(obbMap, w.getSize(), hintsSignal));

        size_t index = 0;
        auto widgets = btl::fmap(*factories, [&index, &obbs](auto&& f)
            {
                auto t = signal::map([index](
                            std::vector<avg::Obb> const& obbs)
                        {
                            return obbs.at(index).getTransform();
                        }, obbs);

                auto size = signal::map([index](
                            std::vector<avg::Obb> const& obbs)
                        {
                            return obbs.at(index).getSize();
                        }, obbs);

                auto factory = f.clone()
                    | transform(std::move(t));

                ++index;

                return std::move(factory)(std::move(size));
            });

        return std::move(w)
            | addWidgets(std::move(widgets));
    };

    return makeWidgetFactory()
        | mapWidget(std::move(widgetMap))
        | setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
}
#endif

} // reactive

