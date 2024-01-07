#include "widget/onpointerdown.h"

#include "widget/setinputareas.h"
#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include "eventresult.h"
#include "pointerbuttonevent.h"

#include "reactive/signal/signal.h"

#include <functional>

namespace reactive::widget
{

AnyWidgetModifier onPointerDown(
        AnySignal<std::function<EventResult(ase::PointerButtonEvent const&)>> cb)
{
    btl::UniqueId id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier([id](Instance widget, auto cb)
        {
            auto areas = widget.getInputAreas();

            if (!areas.empty()
                    && areas.back().getObbs().size() == 1
                    && areas.back().getObbs().front() == widget.getObb())
            {
                areas.back() = std::move(areas.back()).onDown(std::move(cb));
            }
            else
            {
                areas.push_back(
                        makeInputArea(id, widget.getObb())
                            .onDown(std::move(cb))
                        );
            }

            return std::move(widget)
                .setInputAreas(std::move(areas))
                ;
        },
        std::move(cb)
        ));
}

AnyWidgetModifier onPointerDown(
        std::function<EventResult(ase::PointerButtonEvent const&)> cb
        )
{
    return onPointerDown(signal::constant(std::move(cb)));
}
} // namespace reactive::widget

