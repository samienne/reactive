#include "bqui/widget/layoutspec.h"

#include "constraintlayout.h"

#include <utility>

namespace bqui::widget
{

static_assert(IsPureLayout<SimplePureLayout>::value, "");

bq::signal::AnySignal<Constraints> PureLayout::getWidth() const
{
    return impl_->getWidth();
}

bq::signal::AnySignal<Constraints> PureLayout::getHeightForWidth(
        bq::signal::AnySignal<LayoutSolution> widthSolution) const
{
    return impl_->getHeightForWidth(std::move(widthSolution));
}

bq::signal::AnySignal<Constraints> PureLayout::getWidthForHeight(
        bq::signal::AnySignal<LayoutSolution> heightSolution) const
{
    return impl_->getWidthForHeight(std::move(heightSolution));
}

PureLayout simplePureLayout(bq::signal::AnySignal<Constraints> width,
        WidthToConstraints heightForWidth)
{
    return PureLayout(SimplePureLayout{
            std::move(width), std::move(heightForWidth) });
}

PureLayout emptyPureLayout()
{
    return simplePureLayout(
            bq::signal::constant(Constraints()),
            [](bq::signal::AnySignal<LayoutSolution>)
            {
                return bq::signal::AnySignal<Constraints>(
                        bq::signal::constant(Constraints()));
            });
}

PureLayout pureLayoutFromSize(bq::signal::AnySignal<avg::Vector2f> size)
{
    auto shared = std::move(size).share();

    auto natural = [](float value)
    {
        Constraints c;
        c.natural = BandNatural{ value, contentStrength() };
        return c;
    };

    auto width = shared.clone().map([natural](avg::Vector2f s)
            {
                return natural(s[0]);
            });

    return simplePureLayout(
            bq::signal::AnySignal<Constraints>(std::move(width)),
            [shared, natural](bq::signal::AnySignal<LayoutSolution>)
                -> bq::signal::AnySignal<Constraints>
            {
                return shared.clone().map([natural](avg::Vector2f s)
                        {
                            return natural(s[1]);
                        });
            });
}

} // namespace bqui::widget
