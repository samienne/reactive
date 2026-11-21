#include "bqui/modifier/setanchor.h"

#include "pureconstraint.h"

#include <utility>

namespace bqui::modifier
{

AnyWidgetModifier setAnchor(widget::XAnchorKey key,
        bq::signal::AnySignal<widget::Anchor> anchor)
{
    return detail::pureAnchorModifier(detail::PureAxis::horizontal, key.id(),
            std::move(anchor));
}

AnyWidgetModifier setAnchor(widget::YAnchorKey key,
        bq::signal::AnySignal<widget::Anchor> anchor)
{
    return detail::pureAnchorModifier(detail::PureAxis::vertical, key.id(),
            std::move(anchor));
}

} // namespace bqui::modifier
