#include "bqui/modifier/setid.h"

#include "bqui/modifier/instancemodifier.h"

#include <bq/signal/signal.h>

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace bqui::modifier
{

AnyElementModifier setElementId(bq::signal::AnySignal<avg::UniqueId> id)
{
    return makeElementModifier(makeInstanceModifier(
        [](widget::Instance instance, avg::UniqueId const& id)
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

AnyWidgetModifier setId(bq::signal::AnySignal<avg::UniqueId> id)
{
    return makeWidgetModifier(setElementId(std::move(id)));
}

}

