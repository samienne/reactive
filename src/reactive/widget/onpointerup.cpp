#include "widget/onpointerup.h"

#include "widget/setinputareas.h"
#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include "reactive/signal/signal.h"

#include "reactive/pointerbuttonevent.h"
#include "reactive/eventresult.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{

AnyWidgetModifier onPointerUp(
        AnySignal<std::function<EventResult(ase::PointerButtonEvent const&)>> cb)
{
    auto id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier([id](Instance instance, auto cb)
        {
            auto areas = instance.getInputAreas();

            if (!areas.empty()
                    && areas.back().getObbs().size() == 1
                    && areas.back().getObbs().front() == instance.getObb())
            {
                areas.back() = std::move(areas.back()).onUp(std::move(cb));
            }
            else
            {
                areas.push_back(
                        makeInputArea(id, instance.getObb()).onUp(std::move(cb))
                        );
            }

            return std::move(instance)
                .setInputAreas(std::move(areas))
                ;
        },
        std::move(cb)
        ));
}

AnyWidgetModifier onPointerUp(
        std::function<EventResult(ase::PointerButtonEvent const&)> cb)
{
    return onPointerUp(signal::constant(std::move(cb)));
}

AnyWidgetModifier onPointerUp(std::function<EventResult()> cb)
{
    std::function<EventResult(ase::PointerButtonEvent const& e)> f =
        std::bind(std::move(cb));
    return onPointerUp(f);
}
} // namespace reactive::widget

