#include "bqui/modifier/alignguide.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/guideaccess.h"

#include <avg/rendertree/uniqueid.h>

#include <utility>

namespace bqui::modifier
{

namespace
{
    AnyWidgetModifier alignEdge(avg::UniqueId guide, widget::GuideEdge edge)
    {
        return makeWidgetModifier(makeBuilderModifier(
                [guide, edge](auto builder)
                {
                    return std::move(builder).addGuideAlignment(
                            widget::GuideAlignment{ guide, edge });
                }));
    }
} // namespace

AnyWidgetModifier alignLeft(widget::XGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::left);
}

AnyWidgetModifier alignRight(widget::XGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::right);
}

AnyWidgetModifier alignCenterX(widget::XGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::centerX);
}

AnyWidgetModifier alignTop(widget::YGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::top);
}

AnyWidgetModifier alignBottom(widget::YGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::bottom);
}

AnyWidgetModifier alignCenterY(widget::YGuide guide)
{
    return alignEdge(widget::GuideAccess::id(guide), widget::GuideEdge::centerY);
}

} // namespace bqui::modifier
