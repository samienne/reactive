#pragma once

#include "setrendertree.h"
#include "bindrendertree.h"
#include "bindobb.h"
#include "widgettransformer.h"

#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setId(Signal<T, avg::UniqueId> id)
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabRenderTree())
            .values(std::move(id))
            .bind([](auto obb, auto renderTree, auto id)
                {
                    auto newTree = group(std::move(obb), std::move(renderTree),
                            std::move(id)).map(
                            [](avg::Obb const& obb, avg::RenderTree renderTree,
                                avg::UniqueId id)
                            {
                                auto container = std::make_shared<avg::IdNode>(
                                        id,
                                        obb,
                                        renderTree.getRoot()
                                        );

                                return avg::RenderTree(std::move(container));

                            });

                    return setRenderTree(std::move(newTree));
                });
    }
} // namespace reactive::widget


