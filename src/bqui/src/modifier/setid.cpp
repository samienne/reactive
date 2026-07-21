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
            // An instance that draws nothing has no root to name, and an
            // avg::IdNode without a child is a node that cannot be drawn.
            // Contributing nothing is what the rest of the render tree already
            // does with such an instance (avg::ContainerNode::addChild).
            if (!instance.getRenderTree().getRoot())
                return std::move(instance);

            // Naming a tree must not move it. The node being wrapped already
            // carries wherever the instance sits, so an IdNode that repeated
            // the instance's obb would apply that placement a second time on
            // the way down; only the size is the IdNode's to state.
            auto container = std::make_shared<avg::IdNode>(
                    id,
                    avg::Obb(instance.getSize()),
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

