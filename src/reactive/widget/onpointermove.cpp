#include "widget/onpointermove.h"

#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include "reactive/signal/signal.h"

#include "reactive/pointermoveevent.h"
#include "reactive/eventresult.h"

namespace reactive::widget
{

AnyWidgetModifier onPointerMove(signal::AnySignal<
        std::function<EventResult(ase::PointerMoveEvent const&)>
        > cb)
{
    auto id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier([id](Instance instance, auto cb)
            {
                auto areas = instance.getInputAreas();
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == instance.getObb())
                {
                    areas.back() = std::move(areas.back()).onMove(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, instance.getObb()).onMove(std::move(cb))
                            );
                }

                return std::move(instance)
                    .setInputAreas(std::move(areas))
                    ;
            },
            std::move(cb)
            ));
}

AnyWidgetModifier onPointerMove(
        std::function<EventResult(ase::PointerMoveEvent const&)> cb
        )
{
    return onPointerMove(signal::constant(std::move(cb)));
}

} // namespace reactive::widget

