#include "bqui/modifier/onpointerdown.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/widget.h"

#include "bqui/eventresult.h"
#include "bqui/pointerbuttonevent.h"

#include <bq/signal/signal.h>

#include <functional>

namespace bqui::modifier
{

AnyWidgetModifier onPointerDown(bq::signal::AnySignal<
        std::function<EventResult(PointerButtonEvent const&)>
        > cb)
{
    btl::UniqueId id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier([id](widget::Instance widget,
                    auto cb)
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
        std::function<EventResult(PointerButtonEvent const&)> cb
        )
{
    return onPointerDown(bq::signal::constant(std::move(cb)));
}
}

