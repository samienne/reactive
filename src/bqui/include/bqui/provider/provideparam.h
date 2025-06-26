#pragma once

#include "paramprovider.h"

namespace bqui::provider
{
    template <typename TTag>
    auto provideParam()
    {
        return makeParamProviderUnchecked([](BuildParams const& params)
            {
                return params.valueOrDefault<TTag>();
            });
    }
} // namespace reactive::widget

