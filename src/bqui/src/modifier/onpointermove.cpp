#include "bqui/modifier/onpointermove.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/widget.h"

#include "bqui/eventresult.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{

AnyWidgetModifier onPointerMove(bq::signal::AnySignal<
        std::function<EventResult(ase::PointerMoveEvent const&)>
        > cb)
{
    auto id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier([id](widget::Instance instance,
                    auto cb)
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
    return onPointerMove(bq::signal::constant(std::move(cb)));
}

}

