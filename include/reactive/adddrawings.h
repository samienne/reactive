#pragma once

#include "widgetmap.h"

#include <btl/sequence.h>

namespace reactive
{
    template <typename TSignal>
    auto addDrawing(TSignal drawing)
    {
        return makeWidgetMap<avg::Obb, avg::Drawing>(
                [d1=std::move(drawing)]
                (auto obb, auto d2)
                {
                    return std::move(d1) + (obb.getTransform() * std::move(d2));
                });
    }

    template <typename TSignalDrawings, typename = std::enable_if_t<
        btl::IsSequence<SignalType<TSignalDrawings>>::value
        >
    >
    auto addDrawings(TSignalDrawings drawings)
    {
        return makeWidgetMap<ObbTag, DrawingTag>(
                []
                (avg::Obb const& obb, auto d1, auto drawings)
                //-> std::tuple<avg::Obb, avg::Drawing>
                -> avg::Drawing
                {
                    avg::Drawing r = std::move(d1);
                    //for (auto&& d : drawings)
                    btl::forEach(std::move(drawings), [&r, &obb](auto d)
                    {
                        r = std::move(r) + (obb.getTransform() * std::move(d));
                    });

                    //return std::make_tuple(std::move(obb), std::move(r));
                    return r;
                },
                std::move(drawings)
                );
    }

} // namespace reactive

