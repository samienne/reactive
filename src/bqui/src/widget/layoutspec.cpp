#include "bqui/widget/layoutspec.h"

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

} // namespace bqui::widget
