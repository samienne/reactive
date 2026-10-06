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
            // Nothing to name: an avg::IdNode needs a child.
            if (!instance.getRenderTree().getRoot())
                return instance;

            // The wrapped node is already placed, so the IdNode gets only the
            // size; the full obb would apply the placement twice.
            auto container = std::make_shared<avg::IdNode>(
                    id,
                    avg::Obb(instance.getSize()),
                    instance.getRenderTree().getRoot()
                    );

            auto introspection = instance.getIntrospection();
            introspection.id = id;

            return std::move(instance)
                .setRenderTree(avg::RenderTree(std::move(container)))
                .setIntrospection(std::move(introspection))
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

