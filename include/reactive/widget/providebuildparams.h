#pragma once

#include "buildparams.h"
#include "paramprovider.h"

namespace reactive::widget
{
    inline auto provideBuildParams()
    {
        return makeParamProviderUnchecked([](BuildParams const& params)
                -> BuildParams const&
            {
                return params;
            });
    }
} // namespace reactive::widget
