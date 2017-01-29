#pragma once

#include "pipe.h"

#include <btl/collection.h>

namespace reactive
{
    namespace stream
    {
        template <typename T>
        auto bindEvents(btl::collection<T> collection)
        {
            auto p = pipe<btl::persistent_collection_event<T>>();

            auto c = btl::observe(std::move(collection),
                    [handle=std::move(p.handle)](auto event)
                    {
                        handle.push(std::move(event));
                    });

            auto s = std::move(p.stream).fmap([c=std::move(c)](auto event)
                    {
                        return std::move(event);
                    });

            return s;
        }
    } // stream
} // reactive

