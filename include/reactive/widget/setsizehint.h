#pragma once

#include "buildermodifier.h"

namespace reactive::widget
{
    template <typename TSignalSizeHint, typename = std::enable_if_t<
        IsSizeHint<signal::SignalType<TSignalSizeHint>>::value
        >
    >
    auto setSizeHint(TSignalSizeHint sizeHint)
        //-> BuilderModifier;
    {
        auto f = [sh = btl::cloneOnCopy(std::move(sizeHint))]
            (auto builder) // -> Builder
        {
            return std::move(builder)
                .setSizeHint(sh->clone())
                ;
        };

        return makeBuilderModifier(std::move(f));
    }
} // namespace reactive::widget

