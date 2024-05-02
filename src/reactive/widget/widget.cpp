#include "widget/widget.h"
#include "widget/buildermodifier.h"
#include "widget/elementmodifier.h"
#include "widget/element.h"

namespace reactive::widget
{
    template class Widget<std::function<AnyBuilder(BuildParams)>>;
    template class WidgetModifier<std::function<AnyWidget(AnyWidget)>>;
    template class BuilderModifier<std::function<AnyBuilder(AnyBuilder)>>;
    template class Element<signal2::AnySignal<Instance>>;
    template class ElementModifier<std::function<AnyElement(AnyElement)>>;
} // namespace reactive::widget

