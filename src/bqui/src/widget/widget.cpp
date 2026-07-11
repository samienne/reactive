#include "bqui/widget//widget.h"

#include "bqui/widget/element.h"

#include "bqui/modifier/widgetmodifier.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/elementmodifier.h"

namespace bqui::widget
{
    template class BQUI_EXPORT_TEMPLATE Widget<std::function<AnyBuilder(BuildParams)>>;
    template class BQUI_EXPORT_TEMPLATE Element<bq::signal::AnySignal<Instance>>;
}

namespace bqui::modifier
{
    template class BQUI_EXPORT_TEMPLATE WidgetModifier<std::function<widget::AnyWidget(widget::AnyWidget)>>;
    template class BQUI_EXPORT_TEMPLATE BuilderModifier<std::function<widget::AnyBuilder(widget::AnyBuilder)>>;
    template class BQUI_EXPORT_TEMPLATE ElementModifier<std::function<widget::AnyElement(widget::AnyElement)>>;
}

