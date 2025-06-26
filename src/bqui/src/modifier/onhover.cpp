#include "bqui/modifier/onhover.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/widget.h"

#include <bq/signal/signal.h>

#include <ase/hoverevent.h>

namespace bqui::modifier
{

AnyWidgetModifier onHover(
        bq::signal::AnySignal<std::function<void(HoverEvent const&)>> cb,
        bq::signal::AnySignal<avg::Obb> area)
{
    auto id = btl::makeUniqueId();

    return makeWidgetModifier(makeInstanceModifier(
            [id](widget::Instance instance, avg::Obb const& area, auto cb)
            {
                auto areas = instance.getInputAreas();
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == area)
                {
                    areas.back() = std::move(areas.back())
                        .onHover(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, area).onHover(std::move(cb))
                            );
                }

                return std::move(instance)
                    .setInputAreas(std::move(areas))
                    ;
            },
            std::move(area),
            std::move(cb)
            ));
}

AnyWidgetModifier onHover(bq::signal::AnySignal<
        std::function<void(HoverEvent const&)>
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
                areas.back() = std::move(areas.back()).onHover(std::move(cb));
            }
            else
            {
                areas.push_back(
                        makeInputArea(id, instance.getObb()).onHover(std::move(cb))
                        );
            }

            return std::move(instance)
                .setInputAreas(std::move(areas))
                ;
        },
        std::move(cb)
        ));
}

AnyWidgetModifier onHover(
        std::function<void(HoverEvent const&)> cb
        )
{
    return onHover(bq::signal::constant(std::move(cb)));
}

AnyWidgetModifier onHover(bq::signal::InputHandle<bool> handle)
{
    return makeWidgetModifier([](auto widget, auto handle)
            {
                return std::move(widget)
                    | onHover([handle=std::move(handle)](HoverEvent const& e) mutable
                            {
                                handle.set(e.hover);
                            });
            },
            std::move(handle)
            );
}

AnyWidgetModifier onHover(bq::signal::AnySignal<avg::Obb> obb,
        bq::signal::InputHandle<bool> handle)
{
    return makeWidgetModifier([](auto widget, auto obb, auto handle)
        {
            return std::move(widget)
                | onHover(bq::signal::constant([handle=std::move(handle)]
                    (HoverEvent const& e) mutable
                    {
                        handle.set(e.hover);
                    }), std::move(obb)
                    )
                ;
        },
        std::move(obb),
        std::move(handle)
        );
}

}

