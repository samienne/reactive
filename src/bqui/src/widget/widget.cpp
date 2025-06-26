#include "bqui/widget//widget.h"

#include "bqui/widget/element.h"

#include "bqui/modifier/widgetmodifier.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/elementmodifier.h"

namespace bqui::widget
{
    template class Widget<std::function<AnyBuilder(BuildParams)>>;
    template class Element<bq::signal::AnySignal<Instance>>;
}

namespace bqui::modifier
{
    template class WidgetModifier<std::function<widget::AnyWidget(widget::AnyWidget)>>;
    template class BuilderModifier<std::function<widget::AnyBuilder(widget::AnyBuilder)>>;
    template class ElementModifier<std::function<widget::AnyElement(widget::AnyElement)>>;
}

