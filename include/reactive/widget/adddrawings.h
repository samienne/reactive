#pragma once

#include "bindobb.h"
#include "binddrawing.h"
#include "setdrawing.h"
#include "widgettransformer.h"

#include <btl/foreach.h>
#include <btl/sequence.h>

namespace reactive::widget
{
    template <typename T>
    auto addDrawing(Signal<avg::Drawing, T> drawing)
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabDrawing())
            .bind([d1=std::move(drawing)](auto obb, auto d2)
            {
                auto newDrawing = signal::map(
                    [](auto obb, auto d1, auto d2) -> avg::Drawing
                    {
                        return std::move(d1) + (obb.getTransform() * std::move(d2));
                    },
                    std::move(obb),
                    std::move(d1),
                    std::move(d2)
                    );

                return setDrawing(std::move(newDrawing));
            });
    }

    template <typename TSignalDrawings, typename = std::enable_if_t<
        btl::IsSequence<SignalType<TSignalDrawings>>::value
        >
    >
    auto addDrawings(TSignalDrawings drawings)
    {
        return makeWidgetTransformer()
            .compose(bindObb(), grabDrawing())
            .values(std::move(drawings))
            .bind([](auto obb, auto drawing, auto drawings)
            {
                auto newDrawing = signal::map(
                    [](avg::Obb const& obb, auto d1, auto drawings)
                    -> avg::Drawing
                    {
                        avg::Drawing r = std::move(d1);

                        btl::forEach(std::move(drawings), [&r, &obb](auto d)
                        {
                            r = std::move(r) + (obb.getTransform() * std::move(d));
                        });

                        return r;
                    },
                    std::move(obb),
                    std::move(drawing),
                    std::move(drawings)
                    );

                return setDrawing(std::move(newDrawing));
            });
    }

} // namespace reactive::widget

