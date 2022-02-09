#pragma once

#include "paramprovider.h"

namespace reactive::widget
{
    template <typename TTag>
    auto getParam()
    {
        return makeParamProviderUnchecked([](BuildParams const& params)
            {
                return params.valueOrDefault<TTag>();
            });
    }
} // namespace reactive::widget

