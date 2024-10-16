#include "widget/setid.h"

#include "widget/instancemodifier.h"

#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{

AnyElementModifier setElementId(signal::AnySignal<avg::UniqueId> id)
{
    return makeElementModifier(makeInstanceModifier(
        [](Instance instance, avg::UniqueId const& id)
        {
            auto container = std::make_shared<avg::IdNode>(
                    id,
                    instance.getObb(),
                    instance.getRenderTree().getRoot()
                    );

            return std::move(instance)
                .setRenderTree(avg::RenderTree(std::move(container)))
                ;
        },
        std::move(id)
        ));
}

AnyWidgetModifier setId(signal::AnySignal<avg::UniqueId> id)
{
    return makeWidgetModifier(setElementId(std::move(id)));
}

} // namespace reactive::widget

