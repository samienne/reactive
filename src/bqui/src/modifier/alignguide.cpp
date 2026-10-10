#include "bqui/modifier/alignguide.h"

#include "pureconstraint.h"

#include "widget/guideaccess.h"

#include "bqui/widget/layoutspec.h"

#include <utility>

namespace bqui::modifier
{

namespace
{
    using detail::PureAxis;

    AnyWidgetModifier alignX(widget::XGuide const& guide, float fraction)
    {
        return detail::pureGuideModifier(PureAxis::horizontal,
                widget::GuideAccess::variable(guide),
                widget::Anchor{ fraction, 0.0f });
    }

    AnyWidgetModifier alignY(widget::YGuide const& guide, float fraction)
    {
        return detail::pureGuideModifier(PureAxis::vertical,
                widget::GuideAccess::variable(guide),
                widget::Anchor{ fraction, 0.0f });
    }
} // namespace

AnyWidgetModifier alignLeft(widget::XGuide guide)
{
    return alignX(guide, 0.0f);
}

AnyWidgetModifier alignRight(widget::XGuide guide)
{
    return alignX(guide, 1.0f);
}

AnyWidgetModifier alignCenterX(widget::XGuide guide)
{
    return alignX(guide, 0.5f);
}

AnyWidgetModifier alignTop(widget::YGuide guide)
{
    return alignY(guide, 0.0f);
}

AnyWidgetModifier alignBottom(widget::YGuide guide)
{
    return alignY(guide, 1.0f);
}

AnyWidgetModifier alignCenterY(widget::YGuide guide)
{
    return alignY(guide, 0.5f);
}

AnyWidgetModifier alignBaseline(widget::YGuide guide)
{
    return alignAnchor(std::move(guide), widget::baselineAnchor);
}

AnyWidgetModifier alignAnchor(widget::XGuide guide, std::string name)
{
    return detail::pureGuideAnchorModifier(PureAxis::horizontal,
            widget::GuideAccess::variable(guide), std::move(name));
}

AnyWidgetModifier alignAnchor(widget::YGuide guide, std::string name)
{
    return detail::pureGuideAnchorModifier(PureAxis::vertical,
            widget::GuideAccess::variable(guide), std::move(name));
}

} // namespace bqui::modifier
