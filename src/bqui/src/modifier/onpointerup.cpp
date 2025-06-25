#include "bqui/modifier/onpointerup.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/widget.h"

#include "bqui/eventresult.h"

#include <bq/signal/signal.h>

#include <functional>
#include <type_traits>

namespace bqui::modifier
{

AnyWidgetModifier onPointerUp(bq::signal::AnySignal<
        std::function<EventResult(ase::PointerButtonEvent const&)>
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
    return onPointerUp(bq::signal::constant(std::move(cb)));
}

AnyWidgetModifier onPointerUp(std::function<EventResult()> cb)
{
    std::function<EventResult(ase::PointerButtonEvent const& e)> f =
        std::bind(std::move(cb));
    return onPointerUp(f);
}
}

