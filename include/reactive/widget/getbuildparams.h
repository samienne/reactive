#pragma once

#include "buildparams.h"
#include "paramprovider.h"

namespace reactive::widget
{
    inline auto getBuildParams()
    {
        return makeParamProviderUnchecked([](BuildParams const& params) -> BuildParams const&
            {
                return params;
            });
    }
} // namespace reactive::widget
